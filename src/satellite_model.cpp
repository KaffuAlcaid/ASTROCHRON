#include "satellite_model.h"
#include "catalog_model.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <cmath>
#include <algorithm>
#include <limits>

namespace {
QString formatTime(double seconds, const QTimeZone &zone, const char *format = "MM-dd HH:mm:ss")
{
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(seconds * 1000), zone).toString(QString::fromLatin1(format));
}
QString number(double value, int decimals = 2) { return QString::number(value, 'f', decimals); }
QVariantMap stateMap(const Orbit::State &state)
{
    return {{"time", state.time}, {"latitude", state.latitude}, {"longitude", state.longitude},
        {"altitude", state.altitude}, {"azimuth", state.azimuth}, {"elevation", state.elevation},
        {"range", state.range}, {"speed", state.speed}, {"rangeRate", state.rangeRate}, {"elevationRate", state.elevationRate},
        {"sunElevation", state.sunElevation}, {"illumination", state.illumination}, {"phaseAngle", state.phaseAngle}};
}

qint64 stationFor(const Orbit::Satellite &satellite)
{
    switch (satellite.number) {
    case 25544: case 36086: case 49044: return 25544;
    case 48274: case 53239: case 54216: return 48274;
    default: return 0;
    }
}

bool sameOrbit(const Orbit::Satellite &a, const Orbit::Satellite &b)
{
    for (const auto *key : {"EPOCH", "MEAN_MOTION", "ECCENTRICITY", "INCLINATION", "RA_OF_ASC_NODE",
                            "ARG_OF_PERICENTER", "MEAN_ANOMALY", "BSTAR", "MEAN_MOTION_DOT", "MEAN_MOTION_DDOT"})
        if (a.elements.value(key) != b.elements.value(key)) return false;
    return true;
}

qint64 visitingStation(const QString &name)
{
    for (const auto *prefix : {"TIANZHOU-", "SHENZHOU-"})
        if (name.startsWith(QLatin1String(prefix), Qt::CaseInsensitive)) return 48274;
    for (const auto *prefix : {"CREW DRAGON ", "DRAGON ", "CYGNUS ", "PROGRESS-MS ", "SOYUZ-MS "})
        if (name.startsWith(QLatin1String(prefix), Qt::CaseInsensitive)) return 25544;
    return 0;
}
}

SatelliteModel::SatelliteModel(QObject *parent) : QAbstractListModel(parent)
{
    connect(this, &SatelliteModel::selectionChanged, this, &SatelliteModel::detailsChanged);
    connect(this, &SatelliteModel::catalogChanged, this, &SatelliteModel::localizedChanged);
    connect(this, &SatelliteModel::selectionChanged, this, [this] {
        m_photometryStatus = {}; emit photometryChanged();
    });
    QFile magnitudes(":/photometry/qs.mag");
    int skipped = 0;
    if (magnitudes.open(QIODevice::ReadOnly))
        m_defaultPhotometry = readMagnitudes(magnitudes, {0, 0, QStringLiteral("Mike McCants / QuickSat"), false, QStringLiteral("2020-09-14")}, skipped);
    m_defaultPhotometry.insert(48274, {0.87, 0, QStringLiteral("SeeSat-L / Jay Respler"), false, QStringLiteral("2022-08-03"), 0, QStringLiteral("2021-035A")});
    m_pool.setMaxThreadCount(2);
    m_watchlist = m_settings.value("watchlist/ids", QStringList{"25544", "48274"}).toStringList();
    m_watchlist.removeDuplicates();
    m_watchOrder = m_settings.value("watchlist/order", 0).toInt() == 1 ? 1 : 0;
    m_watchTimer.setSingleShot(true); m_watchTimer.setInterval(100);
    connect(&m_watchTimer, &QTimer::timeout, this, &SatelliteModel::calculateWatchPredictions);
    m_frequency = m_settings.value("radio/frequencyMHz", 145.8).toDouble();
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(directory);
    m_database = QSqlDatabase::addDatabase("QSQLITE", QStringLiteral("orbits-%1").arg(reinterpret_cast<quintptr>(this)));
    m_database.setDatabaseName(directory + "/orbits.sqlite");
    if (!m_database.open()) { setStatus([error = m_database.lastError().text()] { return tr("轨道资料库打开失败：") + error; }); return; }
    m_snapshotRetention = m_settings.value("data/snapshotRetention", 10).toInt() == 5 ? 5 : 10;
    m_autoCleanup = m_settings.value("data/autoCleanup", false).toBool();
    if (!initializeDatabase()) { m_database.close(); return; }
    QSqlQuery query(m_database);
    if (query.exec("SELECT norad, magnitude, phase, source, manual, source_date, recorded_at FROM photometry"))
        while (query.next()) m_photometry.insert(query.value(0).toLongLong(), {query.value(1).toDouble(), query.value(2).toInt(), query.value(3).toString(), query.value(4).toBool(), query.value(5).toString(), query.value(6).toLongLong()});
}

SatelliteModel::~SatelliteModel()
{
    if (m_watchCancel) m_watchCancel->store(true);
    m_pool.clear();
    m_pool.waitForDone();
    const auto name = m_database.connectionName();
    m_database.close();
    m_database = {};
    QSqlDatabase::removeDatabase(name);
}

bool SatelliteModel::initializeDatabase()
{
    QSqlQuery query(m_database);
    QString error;
    const auto exec = [&](const QString &sql) {
        if (query.exec(sql)) return true;
        error = query.lastError().text(); return false;
    };
    if (!exec("PRAGMA user_version") || !query.next()) {
        setStatus([error] { return tr("数据库初始化失败：%1").arg(error); }); return false;
    }
    int version = query.value(0).toInt();
    query.finish();
    if (version > 2) {
        setStatus([] { return tr("数据库版本较高，请使用对应版本的 ASTROCHRON"); }); return false;
    }
    if (m_database.tables().isEmpty() && !exec("PRAGMA auto_vacuum=INCREMENTAL")) {
        setStatus([error] { return tr("数据库初始化失败：%1").arg(error); }); return false;
    }
    while (version < 2) {
        bool ok = m_database.transaction();
        if (!ok) error = m_database.lastError().text();
        if (ok && version == 0) {
            ok = exec("CREATE TABLE IF NOT EXISTS snapshots (id INTEGER PRIMARY KEY, group_key TEXT NOT NULL, acquired INTEGER NOT NULL, source TEXT NOT NULL, payload BLOB NOT NULL)")
                && exec("CREATE TABLE IF NOT EXISTS photometry (norad INTEGER PRIMARY KEY, magnitude REAL NOT NULL, phase INTEGER NOT NULL, source TEXT NOT NULL, manual INTEGER NOT NULL)");
            const auto columns = m_database.record("photometry");
            if (ok && !columns.contains("source_date")) ok = exec("ALTER TABLE photometry ADD COLUMN source_date TEXT NOT NULL DEFAULT ''");
            if (ok && !columns.contains("recorded_at")) ok = exec("ALTER TABLE photometry ADD COLUMN recorded_at INTEGER NOT NULL DEFAULT 0");
        } else if (ok && version == 1) {
            ok = exec("ALTER TABLE snapshots ADD COLUMN origin TEXT NOT NULL DEFAULT 'online'")
                && exec("ALTER TABLE snapshots ADD COLUMN pinned INTEGER NOT NULL DEFAULT 0")
                && exec("UPDATE snapshots SET origin='import' WHERE group_key='local'")
                && exec("CREATE INDEX snapshots_group_acquired ON snapshots(group_key, acquired DESC, id DESC)");
        }
        if (ok) ok = exec(QStringLiteral("PRAGMA user_version=%1").arg(version + 1));
        if (ok && !m_database.commit()) { ok = false; error = m_database.lastError().text(); }
        if (!ok) {
            m_database.rollback();
            setStatus([error] { return tr("数据库升级失败：%1").arg(error); }); return false;
        }
        ++version;
    }
    return true;
}

QVariantMap SatelliteModel::storageInfo() const
{
    if (!m_database.isOpen()) return {};
    QSqlQuery query(m_database);
    qint64 count = 0, pinned = 0, imported = 0, pages = 0, free = 0, pageSize = 0;
    if (query.exec("SELECT COUNT(*), SUM(pinned), SUM(origin='import' OR group_key='local') FROM snapshots") && query.next()) {
        count = query.value(0).toLongLong(); pinned = query.value(1).toLongLong(); imported = query.value(2).toLongLong();
    }
    if (query.exec("PRAGMA page_count") && query.next()) pages = query.value(0).toLongLong();
    if (query.exec("PRAGMA freelist_count") && query.next()) free = query.value(0).toLongLong();
    if (query.exec("PRAGMA page_size") && query.next()) pageSize = query.value(0).toLongLong();
    return {{"count", count}, {"pinned", pinned}, {"imported", imported}, {"bytes", pages * pageSize}, {"freeBytes", free * pageSize}};
}

QVariantList SatelliteModel::cleanupCandidates() const
{
    QVariantList result;
    if (!m_database.isOpen()) return result;
    QSet<qint64> used;
    for (const auto &source : m_sources) used.insert(source.snapshot);
    for (const auto &source : m_groupSources) used.insert(source.snapshot);
    QHash<QString, int> counts;
    QHash<QString, QString> names;
    for (const auto &value : groups()) { const auto group = value.toMap(); names.insert(group.value("key").toString(), group.value("name").toString()); }
    QSqlQuery query(m_database);
    if (!query.exec("SELECT id, group_key, acquired, source, length(payload) FROM snapshots WHERE origin='online' AND group_key<>'local' AND pinned=0 ORDER BY group_key, acquired DESC, id DESC")) return result;
    while (query.next()) {
        const auto group = query.value(1).toString();
        const auto id = query.value(0).toLongLong();
        if (++counts[group] <= m_snapshotRetention || used.contains(id)) continue;
        result.append(QVariantMap{{"id", id}, {"group", names.value(group, group)}, {"source", query.value(3)},
            {"acquired", formatTime(query.value(2).toDouble(), m_clock ? QTimeZone(m_clock->timeZone().toUtf8()) : QTimeZone::systemTimeZone(), "yyyy-MM-dd HH:mm:ss")},
            {"size", number(query.value(4).toLongLong() / 1024.0, 1) + " KiB"}});
    }
    return result;
}

bool SatelliteModel::snapshotPinned() const
{
    if (!m_database.isOpen()) return false;
    QSqlQuery query(m_database);
    query.prepare("SELECT pinned FROM snapshots WHERE id=?"); query.addBindValue(snapshotId());
    return query.exec() && query.next() && query.value(0).toBool();
}

bool SatelliteModel::snapshotImported() const
{
    return m_sources.value(m_selected).group == "local";
}

void SatelliteModel::pinSnapshot(qint64 id, bool pinned)
{
    if (m_storageBusy || id <= 0) return;
    QSqlQuery query(m_database);
    query.prepare("UPDATE snapshots SET pinned=? WHERE id=? AND origin='online' AND group_key<>'local'");
    query.addBindValue(pinned ? 1 : 0); query.addBindValue(id);
    if (!query.exec()) m_storageStatus = [error = query.lastError().text()] { return tr("快照保存失败：%1").arg(error); };
    else m_storageStatus = {};
    emit sourceChanged(); emit storageChanged();
}

void SatelliteModel::setSnapshotRetention(int count)
{
    count = count == 5 ? 5 : 10;
    if (m_snapshotRetention == count) return;
    m_snapshotRetention = count; m_settings.setValue("data/snapshotRetention", count);
    emit storageChanged();
}

bool SatelliteModel::pruneSnapshots()
{
    if (!m_database.isOpen() || m_storageBusy) return false;
    const auto candidates = cleanupCandidates();
    if (candidates.isEmpty()) return true;
    QString error;
    bool ok = m_database.transaction();
    if (!ok) error = m_database.lastError().text();
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM snapshots WHERE id=? AND pinned=0 AND origin='online' AND group_key<>'local'");
    for (const auto &entry : candidates) {
        if (!ok) break;
        query.bindValue(0, entry.toMap().value("id"));
        if (!query.exec()) { ok = false; error = query.lastError().text(); }
    }
    query.finish();
    if (ok && !m_database.commit()) { ok = false; error = m_database.lastError().text(); }
    if (!ok) {
        m_database.rollback();
        m_storageStatus = [error] { return tr("历史资料清理失败：%1").arg(error); };
    } else {
        m_storageStatus = [count = candidates.size()] { return tr("已清理 %1 份在线历史快照").arg(count); };
        compactDatabase(false);
    }
    emit sourceChanged(); emit storageChanged();
    return ok;
}

bool SatelliteModel::enableCleanup()
{
    if (!pruneSnapshots()) return false;
    m_autoCleanup = true; m_settings.setValue("data/autoCleanup", true);
    emit storageChanged(); return true;
}

void SatelliteModel::disableCleanup()
{
    m_autoCleanup = false; m_settings.setValue("data/autoCleanup", false);
    emit storageChanged();
}

void SatelliteModel::compactDatabase(bool full)
{
    if (m_storageBusy || !m_database.isOpen()) return;
    if (m_downloading) {
        if (full) { m_storageStatus = [] { return tr("轨道下载完成后可整理数据库"); }; emit storageChanged(); }
        return;
    }
    if (!full) {
        const auto info = storageInfo();
        QSqlQuery mode(m_database);
        if (!mode.exec("PRAGMA auto_vacuum") || !mode.next() || mode.value(0).toInt() != 2
            || info.value("freeBytes").toLongLong() < 16 * 1024 * 1024
            || info.value("freeBytes").toDouble() < info.value("bytes").toDouble() * 0.2) return;
    }
    m_storageBusy = true; m_storageStatus = [] { return tr("正在整理数据库"); };
    emit storageChanged();
    m_pool.start([this, path = m_database.databaseName(), full] {
        const auto connection = QUuid::createUuid().toString();
        QString error;
        {
            auto database = QSqlDatabase::addDatabase("QSQLITE", connection);
            database.setDatabaseName(path); database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=3000");
            if (!database.open()) error = database.lastError().text();
            else {
                QSqlQuery query(database);
                if (full) {
                    if (!query.exec("PRAGMA auto_vacuum=INCREMENTAL") || !query.exec("VACUUM")) error = query.lastError().text();
                } else {
                    while (error.isEmpty()) {
                        if (!query.exec("PRAGMA freelist_count") || !query.next()) { error = query.lastError().text(); break; }
                        const auto remaining = query.value(0).toLongLong();
                        query.finish();
                        if (remaining == 0) break;
                        if (!query.exec("PRAGMA incremental_vacuum(256)")) error = query.lastError().text();
                        while (query.next()) {}
                    }
                }
            }
            database.close();
        }
        QSqlDatabase::removeDatabase(connection);
        QMetaObject::invokeMethod(this, [this, error] {
            m_storageBusy = false;
            m_storageStatus = error.isEmpty() ? std::function<QString()>([] { return tr("数据库已整理"); })
                : std::function<QString()>([error] { return tr("数据库整理失败：%1").arg(error); });
            emit storageChanged();
        }, Qt::QueuedConnection);
    }, -1);
}

int SatelliteModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : static_cast<int>(m_rows.size()); }
QHash<int, QByteArray> SatelliteModel::roleNames() const
{
    return {{IdRole, "satelliteId"}, {NameRole, "satelliteName"}, {OriginalRole, "originalName"}, {ElevationRole, "elevationText"}, {PredictionRole, "predictionText"}};
}
QVariant SatelliteModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return {};
    const auto &satellite = m_satellites[m_rows[index.row()]];
    switch (role) {
    case IdRole: return QString::number(satellite.number);
    case NameRole: return Orbit::displayName(satellite);
    case OriginalRole: return satellite.name;
    case ElevationRole: return m_elevations.contains(satellite.number) ? number(m_elevations.value(satellite.number), 1) + QStringLiteral("°") : tr("待计算");
    case PredictionRole: return m_watchSummaries.value(satellite.number, m_clock && m_clock->hasObserver() ? tr("预报计算中") : tr("请选择观测地点"));
    default: return {};
    }
}

QVariantList SatelliteModel::groups() const
{
    QVariantList result;
    const std::pair<const char *, const char *> groups[] = {
        {"catalog", QT_TR_NOOP("本地完整目录")}, {"active", QT_TR_NOOP("活动卫星（CelesTrak）")}, {"stations", QT_TR_NOOP("空间站")}, {"visual", QT_TR_NOOP("明亮目标")},
        {"weather", QT_TR_NOOP("气象卫星")}, {"noaa", QT_TR_NOOP("美国气象卫星")}, {"goes", QT_TR_NOOP("地球静止气象卫星")},
        {"resource", QT_TR_NOOP("地球资源卫星")}, {"sarsat", QT_TR_NOOP("搜救卫星")}, {"dmc", QT_TR_NOOP("灾害监测卫星")},
        {"starlink", QT_TR_NOOP("星链")}, {"oneweb", QT_TR_NOOP("一网")}, {"iridium-NEXT", QT_TR_NOOP("铱星二代")},
        {"qianfan", QT_TR_NOOP("千帆")}, {"hulianwang", QT_TR_NOOP("互联网低轨")}, {"kuiper", QT_TR_NOOP("柯伊伯")}, {"sar", QT_TR_NOOP("合成孔径雷达")},
        {"intelsat", QT_TR_NOOP("国际通信卫星")}, {"geo", QT_TR_NOOP("地球同步卫星")}, {"amateur", QT_TR_NOOP("业余无线电卫星")},
        {"gnss", QT_TR_NOOP("全球导航卫星")}, {"gps-ops", QT_TR_NOOP("GPS 运行组")}, {"glo-ops", QT_TR_NOOP("GLONASS 运行组")},
        {"galileo", QT_TR_NOOP("伽利略")}, {"beidou", QT_TR_NOOP("北斗")}, {"science", QT_TR_NOOP("科学卫星")},
        {"engineering", QT_TR_NOOP("技术试验卫星")}, {"education", QT_TR_NOOP("教育卫星")}, {"cubesat", QT_TR_NOOP("立方星")},
        {"radar", QT_TR_NOOP("雷达标定卫星")}, {"other", QT_TR_NOOP("其他卫星")}, {"last-30-days", QT_TR_NOOP("最近发射")}
    };
    for (const auto &[key, name] : groups) result.append(QVariantMap{{"key", QString::fromLatin1(key)}, {"name", tr(name)}});
    result.append(QVariantMap{{"key", "local"}, {"name", tr("本地轨道文件")}});
    return result;
}

bool SatelliteModel::usingLocalConstellation() const
{
    return !m_groupMembers.contains(m_group)
        && (m_group == "gps-ops" || m_group == "glo-ops" || m_group == "galileo" || m_group == "beidou");
}

QVariantMap SatelliteModel::groupInfo() const
{
    if (m_group == "catalog") return {{"description", tr("各来源按卫星编号合并，空间站组合体合并显示")}};
    const auto source = m_groupSources.value(m_group);
    const bool operational = m_group == "gps-ops" || m_group == "glo-ops";
    return {{"description", usingLocalConstellation() ? tr("当前显示：本地目录中的%1对象").arg(CatalogModel::constellationName(m_group))
                                        : operational ? tr("运行组采用 CelesTrak 分组，实时导航健康状态以系统公告为准")
                                        : m_group == "local" ? tr("所选本地轨道文件中的对象") : tr("CelesTrak 所选分组中的对象")},
        {"localMembers", usingLocalConstellation()},
        {"acquired", source.acquired ? QDateTime::fromSecsSinceEpoch(source.acquired, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm:ss 'UTC'") : QString()},
        {"source", source.url}, {"loaded", m_groupMembers.contains(m_group)},
        {"count", static_cast<int>(m_groupMembers.value(m_group).size())}};
}

void SatelliteModel::setClock(AppState *clock)
{
    if (m_clock == clock) return;
    if (m_clock) disconnect(m_clock, nullptr, this, nullptr);
    m_clock = clock;
    if (clock) {
        const auto reschedule = [this] { m_calculationTime = 0; m_gnssTime = 0; requestFrame(); };
        connect(clock, &AppState::calculationFrequencyChanged, this, reschedule);
        connect(clock, &AppState::updateFrequencyChanged, this, reschedule);
        connect(clock, &AppState::localizedChanged, this, [this] {
            updateWatchRows();
            emit planChanged();
            emit storageChanged();
            emit localizedChanged(); emit detailsChanged(); emit frameChanged(); emit trajectoryChanged();
            emit previewChanged(); emit sourceChanged(); emit statusChanged(); emit photometryChanged();
            if (!m_rows.isEmpty()) emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1));
        });
        connect(clock, &AppState::timeChanged, this, [this] {
            const double watchTime = m_clock->unixTime();
            if (static_cast<qint64>(std::floor(watchTime / 86400)) != m_watchRequestedDay) requestWatchPredictions();
            if (watchTime >= m_watchNextBoundary || watchTime < m_watchClockTime || watchTime - m_watchClockTime > 3) updateWatchRows();
            m_watchClockTime = watchTime;
            if (std::abs(m_clock->unixTime() - m_lastTime) > 3) {
                ++m_revision;
                if (previewActive()) { ++m_previewRevision; m_previewTime = 0; }
            }
            m_lastTime = m_clock->unixTime();
            if (std::abs(m_clock->referenceTime() - m_trackReference) >= 60) m_needTrack = true;
            if (previewActive() && !m_previewBusy && std::abs(m_clock->unixTime() - m_previewTime) >= (m_previewMode == 1 ? 60 : 10)) refreshPreview();
            requestFrame();
        });
        connect(clock, &AppState::observerChanged, this, [this] {
            requestWatchPredictions();
            invalidate();
            if (previewActive()) { ++m_previewRevision; refreshPreview(); }
        });
        QTimer::singleShot(0, this, [this] {
            if (!m_database.isOpen()) return;
            QSqlQuery query(m_database);
            if (query.exec("SELECT id FROM snapshots WHERE id IN (SELECT MAX(id) FROM snapshots GROUP BY group_key) ORDER BY acquired")) {
                QVector<qint64> ids;
                while (query.next()) ids.append(query.value(0).toLongLong());
                for (const auto id : ids) loadSnapshot(id);
            }
            if (!m_groupMembers.contains("active")) setGroup("active");
        });
    }
    emit clockChanged();
}

void SatelliteModel::setSearch(const QString &value) { if (m_search == value) return; m_search = value; filter(); emit watchlistChanged(); }
void SatelliteModel::filter()
{
    beginResetModel();
    m_rows.clear();
    const auto search = m_search.trimmed();
    for (const auto &id : m_watchlist) {
        const int i = m_index.value(m_owners.value(id.toLongLong(), id.toLongLong()), -1);
        if (i < 0) continue;
        for (const int member : m_members.value(m_satellites[i].number)) {
            const auto &satellite = m_satellites[member];
            const bool stationAlias = satellite.number == 48274 && QStringLiteral("天宫 中国空间站 天和 TIANHE CSS").contains(search, Qt::CaseInsensitive);
            if (search.isEmpty() || satellite.name.contains(search, Qt::CaseInsensitive) || Orbit::displayName(satellite).contains(search) ||
                stationAlias || QString::number(satellite.number).contains(search) || satellite.internationalId.contains(search, Qt::CaseInsensitive)) {
                m_rows.append(i);
                break;
            }
        }
    }
    endResetModel();
    sortWatchRows();
}

void SatelliteModel::setWatchOrder(int order)
{
    order = order == 1 ? 1 : 0;
    if (m_watchOrder == order) return;
    m_watchOrder = order; m_settings.setValue("watchlist/order", order);
    sortWatchRows(); emit watchlistChanged();
}

void SatelliteModel::sortWatchRows()
{
    auto sorted = m_rows;
    const auto position = [&](int row) { return m_watchlist.indexOf(QString::number(m_satellites[row].number)); };
    std::stable_sort(sorted.begin(), sorted.end(), [&](int a, int b) {
        if (m_watchOrder == 1) {
            const double ta = m_watchNextTimes.value(m_satellites[a].number, std::numeric_limits<double>::infinity());
            const double tb = m_watchNextTimes.value(m_satellites[b].number, std::numeric_limits<double>::infinity());
            if (ta != tb) return ta < tb;
        }
        return position(a) < position(b);
    });
    for (qsizetype i = 0; i < sorted.size(); ++i) {
        const auto from = m_rows.indexOf(sorted[i]);
        if (from == i) continue;
        beginMoveRows({}, static_cast<int>(from), static_cast<int>(from), {}, static_cast<int>(i));
        m_rows.move(from, i);
        endMoveRows();
    }
}

void SatelliteModel::requestWatchPredictions()
{
    if (!m_clock) return;
    m_watchRequestedDay = static_cast<qint64>(std::floor(m_clock->unixTime() / 86400));
    ++m_watchGeneration;
    if (m_watchCancel) m_watchCancel->store(true);
    m_watchTimer.start();
}

void SatelliteModel::calculateWatchPredictions()
{
    if (m_watchBusy || !m_clock || !m_clock->hasObserver()) return;
    const auto day = static_cast<qint64>(std::floor(m_clock->unixTime() / 86400));
    const Orbit::Observer observer{m_clock->observerLatitude(), m_clock->observerLongitude(), m_clock->ellipsoidHeight() / 1000, m_clock->minimumElevation()};
    if (!std::isfinite(observer.heightKm)) return;
    if (day != m_watchDay || observer.latitude != m_watchObserver.latitude || observer.longitude != m_watchObserver.longitude
        || observer.heightKm != m_watchObserver.heightKm || observer.minimumElevation != m_watchObserver.minimumElevation) m_watchPredictions.clear();
    m_watchDay = day; m_watchObserver = observer;
    QVector<Orbit::Satellite> pending;
    QSet<qint64> ids;
    for (const auto &key : m_watchlist) {
        const auto id = m_owners.value(key.toLongLong(), key.toLongLong());
        if (ids.contains(id) || !m_index.contains(id)) continue;
        ids.insert(id);
        const auto &satellite = m_satellites[m_index.value(id)];
        const auto existing = m_watchPredictions.constFind(id);
        if (existing == m_watchPredictions.cend() || !sameOrbit(satellite, existing->satellite)) {
            m_watchPredictions.remove(id); pending.append(satellite);
        }
    }
    for (auto it = m_watchPredictions.begin(); it != m_watchPredictions.end();)
        if (!ids.contains(it.key())) it = m_watchPredictions.erase(it); else ++it;
    updateWatchRows();
    if (pending.isEmpty()) return;
    m_watchBusy = true;
    m_watchCancel = std::make_shared<std::atomic_bool>(false);
    const auto generation = m_watchGeneration;
    m_pool.start([this, pending, observer, day, generation, cancel = m_watchCancel] {
        QHash<qint64, WatchPrediction> results;
        // The previous day supplies rise times for passes already in progress.
        for (const auto &satellite : pending) {
            if (cancel->load()) break;
            results.insert(satellite.number, {satellite, Orbit::predictPasses(satellite, (day - 1) * 86400.0, (day + 2) * 86400.0, observer)});
        }
        QMetaObject::invokeMethod(this, [this, generation, results = std::move(results)]() mutable {
            m_watchBusy = false;
            if (generation == m_watchGeneration) {
                for (auto it = results.begin(); it != results.end(); ++it) m_watchPredictions.insert(it.key(), std::move(it.value()));
                updateWatchRows();
            } else m_watchTimer.start();
        }, Qt::QueuedConnection);
    }, -1);
}

void SatelliteModel::updateWatchRows()
{
    if (!m_clock) return;
    const double time = m_clock->unixTime();
    const QTimeZone zone(m_clock->timeZone().toUtf8());
    const auto date = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(time), zone).date();
    QHash<qint64, QString> summaries;
    QHash<qint64, double> times;
    m_watchNextBoundary = date.addDays(1).startOfDay(zone).toSecsSinceEpoch();
    for (const auto &key : m_watchlist) {
        const auto id = m_owners.value(key.toLongLong(), key.toLongLong());
        const auto forecast = m_watchPredictions.constFind(id);
        if (forecast == m_watchPredictions.cend()) continue;
        QString text = tr("24 h 内暂无过境");
        for (const auto &pass : forecast->passes) {
            for (double change : {pass.rise.time - 86400, pass.rise.time, pass.set.time})
                if (change > time) m_watchNextBoundary = std::min(m_watchNextBoundary, change);
            if (pass.set.time <= time || pass.rise.time >= time + 86400) continue;
            times.insert(id, pass.rise.time <= time ? 0 : pass.rise.time);
            if (pass.startsBeforeWindow || pass.endsAfterWindow) text = tr("持续高于最低高度角");
            else {
                const auto rise = QDateTime::fromSecsSinceEpoch(qRound64(pass.rise.time), zone);
                text = pass.rise.time <= time ? tr("正在过境") : tr("下一次 %1").arg(rise.toString(rise.date() == date ? "HH:mm" : "MM-dd HH:mm"));
                text += tr(" · 峰值 %1°").arg(number(pass.peak.elevation, 1));
            }
            const bool optical = std::any_of(pass.visibleIntervals.begin(), pass.visibleIntervals.end(), [time](const auto &interval) {
                return interval[1] > time && interval[0] < time + 86400;
            });
            if (optical) text += tr(" · 光学");
            for (const auto &interval : pass.visibleIntervals)
                for (double change : {interval[0] - 86400, interval[1]})
                    if (change > time) m_watchNextBoundary = std::min(m_watchNextBoundary, change);
            break;
        }
        summaries.insert(id, text);
    }
    if (summaries == m_watchSummaries && times == m_watchNextTimes) return;
    m_watchSummaries = std::move(summaries); m_watchNextTimes = std::move(times);
    sortWatchRows();
    if (!m_rows.isEmpty()) emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {PredictionRole});
}

void SatelliteModel::setGroup(const QString &group)
{
    bool known = false;
    for (const auto &entry : groups()) if (entry.toMap().value("key").toString() == group) known = true;
    if (!known) return;
    m_group = group;
    m_settings.setValue("catalog/group", group);
    if (group == "catalog") { emit catalogChanged(); return; }
    QSqlQuery query(m_database);
    query.prepare("SELECT id FROM snapshots WHERE group_key = ? ORDER BY id DESC LIMIT 1");
    query.addBindValue(group);
    if (query.exec() && query.next()) loadSnapshot(query.value(0).toLongLong());
    else {
        if (group != "local") refresh();
        else setStatus([] { return tr("请选择轨道文件"); });
    }
    emit catalogChanged();
}

void SatelliteModel::setStatus(std::function<QString()> value) { m_status = std::move(value); emit statusChanged(); }
void SatelliteModel::refresh()
{
    if (m_downloading || m_group == "local") return;
    const QString group = m_group == "catalog" ? QStringLiteral("active") : m_group;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery latest(m_database);
    latest.prepare("SELECT MAX(acquired) FROM snapshots WHERE group_key = ?");
    latest.addBindValue(group);
    const qint64 acquired = latest.exec() && latest.next() ? latest.value(0).toLongLong() : 0;
    const qint64 nextRequest = std::max(acquired ? acquired + 7200 : 0, m_retryAfter.value(group));
    if (now < nextRequest) {
        setStatus([nextRequest] { return tr("下次可更新：%1").arg(QDateTime::fromSecsSinceEpoch(nextRequest).toString("HH:mm:ss")); });
        return;
    }
    QUrl url("https://celestrak.org/NORAD/elements/gp.php");
    QUrlQuery parameters; parameters.addQueryItem("GROUP", group); parameters.addQueryItem("FORMAT", "json"); url.setQuery(parameters);
    QNetworkRequest request(url); request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::UserAgentHeader, "ASTROCHRON/0.1 (satellite observation)");
    auto *reply = m_network.get(request);
    m_downloading = true;
    setStatus([] { return tr("正在获取轨道数据"); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, group, url] {
        reply->deleteLater(); m_downloading = false;
        const auto finishedAt = QDateTime::currentSecsSinceEpoch();
        m_retryAfter[group] = finishedAt + 60;
        const auto retryHeader = reply->rawHeader("Retry-After");
        if (!retryHeader.isEmpty()) {
            bool seconds = false;
            const auto delay = retryHeader.toLongLong(&seconds);
            const auto retryTime = seconds ? finishedAt + std::max<qint64>(delay, 0)
                : QDateTime::fromString(QString::fromLatin1(retryHeader), Qt::RFC2822Date).toSecsSinceEpoch();
            m_retryAfter[group] = std::max(m_retryAfter.value(group), retryTime);
        }
        if (reply->error() != QNetworkReply::NoError) {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (httpStatus >= 500)
                setStatus([httpStatus] { return tr("CelesTrak 服务暂时异常（HTTP %1），本地轨道资料仍可使用").arg(httpStatus); });
            else if (httpStatus > 0)
                setStatus([httpStatus] { return tr("CelesTrak 请求失败（HTTP %1），本地轨道资料仍可使用").arg(httpStatus); });
            else
                setStatus([error = static_cast<int>(reply->error())] { return tr("轨道数据获取失败：网络连接异常（错误 %1），本地轨道资料仍可使用").arg(error); });
            return;
        }
        const auto payload = reply->readAll();
        QString error; int skipped = 0;
        auto satellites = Orbit::parse(payload, error, skipped);
        if (satellites.isEmpty()) { setStatus([error] { return error.isEmpty() ? tr("数据源当前没有目标") : error; }); return; }
        const auto count = satellites.size();
        if (!store(payload, url.toString(), group)) return;
        m_retryAfter.remove(group);
        install(std::move(satellites), Source{group, url.toString(), m_snapshot});
        setStatus([count, skipped] { return tr("目录已获取 %1 条根数").arg(count) + (skipped ? tr("，%1 条根数格式异常").arg(skipped) : QString()); });
    });
}

bool SatelliteModel::store(const QByteArray &payload, const QString &source, const QString &group)
{
    if (m_storageBusy) { setStatus([] { return tr("正在整理数据库，请稍后保存资料"); }); return false; }
    QSqlQuery query(m_database);
    query.prepare("INSERT INTO snapshots (group_key, acquired, source, payload, origin) VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(group); query.addBindValue(QDateTime::currentSecsSinceEpoch()); query.addBindValue(source); query.addBindValue(payload);
    query.addBindValue(group == "local" ? "import" : "online");
    if (!query.exec()) { setStatus([error = query.lastError().text()] { return tr("轨道资料保存失败：") + error; }); return false; }
    m_snapshot = query.lastInsertId().toLongLong(); m_source = source;
    return true;
}

void SatelliteModel::importFile(const QUrl &url)
{
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) { setStatus([error = file.errorString()] { return tr("轨道文件打开失败：") + error; }); return; }
    const auto payload = file.readAll();
    QString error; int skipped = 0;
    auto satellites = Orbit::parse(payload, error, skipped);
    if (satellites.isEmpty()) { setStatus([error] { return error.isEmpty() ? tr("文件内没有卫星根数") : error; }); return; }
    if (!store(payload, QFileInfo(file).fileName(), "local")) return;
    setGroup("local");
    setStatus([skipped] { return tr("轨道文件已加入目录%1").arg(skipped ? tr("，%1 条根数格式异常").arg(skipped) : QString()); });
}

void SatelliteModel::exportSelected(const QUrl &url)
{
    const auto *satellite = selected(); if (!satellite) return;
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { setStatus([error = file.errorString()] { return tr("文件保存失败：") + error; }); return; }
    QJsonArray elements;
    for (const int member : m_members.value(satellite->number)) elements.append(m_satellites[member].elements);
    const auto bytes = QJsonDocument(elements).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) { setStatus([error = file.errorString()] { return tr("轨道文件写入失败：") + error; }); return; }
    setStatus([] { return tr("轨道根数已保存"); });
}

void SatelliteModel::preparePlan()
{
    const auto *satellite = selected();
    if (!satellite || !m_clock || !m_clock->hasObserver()) return;
    const auto revision = ++m_planRevision;
    m_plan = {};
    m_plan.satellite = *satellite;
    m_plan.observer = {m_clock->observerLatitude(), m_clock->observerLongitude(), m_clock->ellipsoidHeight() / 1000, m_clock->minimumElevation()};
    m_plan.observerName = m_clock->observerName(); m_plan.zone = QTimeZone(m_clock->timeZone().toUtf8());
    m_plan.height = m_clock->observerHeight(); m_plan.heightKnown = m_clock->hasObserverHeight();
    m_plan.source = sourceText();
    m_plan.start = std::floor(m_clock->unixTime()); m_plan.end = m_plan.start + 43200;
    m_planStatus = {};
    if (!std::isfinite(m_plan.observer.heightKm)) {
        m_planBusy = false;
        m_planStatus = [] { return tr("大地水准面数据读取失败"); };
        emit planChanged(); return;
    }
    m_planBusy = true;
    emit planChanged();
    m_pool.start([this, revision, plan = m_plan] {
        auto passes = Orbit::predictPasses(plan.satellite, plan.start, plan.end, plan.observer);
        QMetaObject::invokeMethod(this, [this, revision, passes = std::move(passes)]() mutable {
            if (revision != m_planRevision) return;
            m_plan.passes = std::move(passes); m_planBusy = false;
            if (m_plan.passes.isEmpty()) m_planStatus = [] { return tr("12 h 内暂无满足高度角条件的过境"); };
            emit planChanged();
        }, Qt::QueuedConnection);
    }, -1);
}

QVariantMap SatelliteModel::planInfo() const
{
    if (!m_plan.satellite.number) return {};
    return {{"name", Orbit::displayName(m_plan.satellite)}, {"observer", m_plan.observerName},
        {"start", formatTime(m_plan.start, m_plan.zone, "yyyy-MM-dd HH:mm:ss")},
        {"end", formatTime(m_plan.end, m_plan.zone, "yyyy-MM-dd HH:mm:ss")},
        {"zone", QString::fromUtf8(m_plan.zone.id())}, {"minimum", m_plan.observer.minimumElevation},
        {"fileName", QStringLiteral("ASTROCHRON-%1-%2").arg(m_plan.satellite.number).arg(formatTime(m_plan.start, QTimeZone::UTC, "yyyyMMdd-HHmmss"))}};
}

QVariantList SatelliteModel::planPasses() const
{
    QVariantList result;
    for (const auto &pass : m_plan.passes)
        result.append(QVariantMap{{"start", formatTime(pass.rise.time, m_plan.zone)}, {"end", formatTime(pass.set.time, m_plan.zone)},
            {"peak", formatTime(pass.peak.time, m_plan.zone)}, {"maximum", number(pass.peak.elevation, 1) + QStringLiteral("°")},
            {"partial", pass.startsBeforeWindow || pass.endsAfterWindow}, {"optical", !pass.visibleIntervals.isEmpty()}});
    return result;
}

bool SatelliteModel::exportPlan(const QUrl &url, const QString &format)
{
    if (m_planBusy || m_plan.passes.isEmpty() || !url.isLocalFile() || (format != "csv" && format != "ics")) return false;
    const auto iso = [](double time, const QTimeZone &zone) {
        return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(std::floor(time)), zone).toString(Qt::ISODate);
    };
    const auto utc = [](double time) {
        return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(std::floor(time)), QTimeZone::UTC).toString("yyyyMMdd'T'HHmmss'Z'");
    };
    QByteArray output;
    const auto csvRow = [&](const QStringList &values) {
        QStringList escaped;
        for (auto value : values) {
            value.replace('"', "\"\"");
            escaped.append('"' + value + '"');
        }
        output += escaped.join(',').toUtf8() + "\r\n";
    };
    const auto icsText = [](QString value) {
        return value.replace("\r\n", "\n").replace('\r', '\n').replace("\\", "\\\\")
            .replace("\n", "\\n").replace(";", "\\;").replace(",", "\\,");
    };
    const auto icsLine = [&](const QString &line) {
        const auto bytes = line.toUtf8();
        qsizetype offset = 0;
        while (offset < bytes.size()) {
            qsizetype count = std::min<qsizetype>(offset ? 74 : 75, bytes.size() - offset);
            while (offset + count < bytes.size() && (static_cast<unsigned char>(bytes[offset + count]) & 0xc0) == 0x80) --count;
            if (offset) output += ' ';
            output += QByteArrayView(bytes).sliced(offset, count); output += "\r\n";
            offset += count;
        }
    };
    const auto name = Orbit::displayName(m_plan.satellite);
    const auto epoch = Orbit::formatEpoch(m_plan.satellite.epoch) + 'Z';
    const auto zone = QString::fromUtf8(m_plan.zone.id());
    const auto location = tr("%1（%2°, %3°）").arg(m_plan.observerName, number(m_plan.observer.latitude, 6), number(m_plan.observer.longitude, 6));
    if (format == "csv") {
        output = QByteArray::fromHex("efbbbf");
        csvRow({tr("卫星"), "NORAD ID", "COSPAR", tr("开始（UTC）"), tr("最高点（UTC）"), tr("结束（UTC）"),
            tr("最高高度角（°）"), tr("持续时间（s）"), tr("开始方位角（°）"), tr("结束方位角（°）"),
            tr("光学条件"), tr("光学区间（UTC）"), tr("轨道历元（UTC）"), tr("观测地点"), tr("纬度（°）"), tr("经度（°）"),
            tr("海拔（m）"), tr("最低高度角（°）"), tr("时区"), tr("开始（当地时间）"), tr("最高点（当地时间）"), tr("结束（当地时间）"),
            tr("起点截断"), tr("终点截断"), tr("数据来源")});
    } else {
        icsLine("BEGIN:VCALENDAR"); icsLine("VERSION:2.0"); icsLine("PRODID:-//ASTROCHRON//Observation Plan//EN");
        icsLine("CALSCALE:GREGORIAN");
    }
    for (const auto &pass : m_plan.passes) {
        QStringList optical;
        for (const auto &interval : pass.visibleIntervals)
            optical.append(iso(interval[0], QTimeZone::UTC) + " / " + iso(interval[1], QTimeZone::UTC));
        if (format == "csv") {
            csvRow({name, QString::number(m_plan.satellite.number), m_plan.satellite.internationalId,
                iso(pass.rise.time, QTimeZone::UTC), iso(pass.peak.time, QTimeZone::UTC), iso(pass.set.time, QTimeZone::UTC),
                number(pass.peak.elevation, 1), number(pass.set.time - pass.rise.time, 1), number(pass.rise.azimuth, 1), number(pass.set.azimuth, 1),
                optical.isEmpty() ? tr("条件欠佳") : tr("具备光学条件"), optical.join("; "), epoch, m_plan.observerName,
                number(m_plan.observer.latitude, 6), number(m_plan.observer.longitude, 6), m_plan.heightKnown ? number(m_plan.height, 1) : QString(),
                number(m_plan.observer.minimumElevation, 1), zone, iso(pass.rise.time, m_plan.zone), iso(pass.peak.time, m_plan.zone), iso(pass.set.time, m_plan.zone),
                pass.startsBeforeWindow ? "1" : "0", pass.endsAfterWindow ? "1" : "0", m_plan.source});
        } else {
            const bool partial = pass.startsBeforeWindow || pass.endsAfterWindow;
            const auto title = partial ? tr("ASTROCHRON · %1观测时段（时段内最高 %2°）").arg(name, number(pass.peak.elevation, 1))
                : tr("ASTROCHRON · %1过境（峰值 %2°）").arg(name, number(pass.peak.elevation, 1));
            const auto identity = QStringLiteral("%1|%2|%3|%4|%5|%6|%7").arg(m_plan.satellite.number).arg(epoch,
                number(m_plan.observer.latitude, 8), number(m_plan.observer.longitude, 8), number(m_plan.observer.heightKm, 6),
                number(m_plan.observer.minimumElevation, 3), utc(pass.rise.time));
            QStringList description{
                (partial ? tr("时段内最高点：%1") : tr("最高点：%1")).arg(iso(pass.peak.time, m_plan.zone)),
                tr("开始方位：%1 %2°；结束方位：%3 %4°").arg(Orbit::directionName(pass.rise.azimuth), number(pass.rise.azimuth, 1), Orbit::directionName(pass.set.azimuth), number(pass.set.azimuth, 1)),
                tr("最低高度角：%1°").arg(number(m_plan.observer.minimumElevation, 1)),
                tr("当地时间：%1 / %2；时区：%3").arg(iso(pass.rise.time, m_plan.zone), iso(pass.set.time, m_plan.zone), zone),
                tr("轨道历元（UTC）：%1").arg(epoch), tr("观测地点：%1").arg(location),
                tr("海拔：%1").arg(m_plan.heightKnown ? number(m_plan.height, 1) + " m" : tr("待填写")),
                tr("光学区间（UTC）：%1").arg(optical.isEmpty() ? tr("条件欠佳") : optical.join("; ")),
                tr("数据来源：%1").arg(m_plan.source)};
            if (pass.startsBeforeWindow) description.append(tr("时段开始前已高于最低高度角"));
            if (pass.endsAfterWindow) description.append(tr("时段结束时仍高于最低高度角"));
            icsLine("BEGIN:VEVENT");
            icsLine("UID:" + QUuid::createUuidV5(QUuid("{cb64f4af-cd0e-4d96-806d-1d73cf11da90}"), identity.toUtf8()).toString(QUuid::WithoutBraces) + "@astrochron");
            icsLine("DTSTAMP:" + utc(QDateTime::currentSecsSinceEpoch()));
            icsLine("DTSTART:" + utc(pass.rise.time));
            icsLine("DTEND:" + utc(std::max(std::floor(pass.set.time), std::floor(pass.rise.time) + 1)));
            icsLine("SUMMARY:" + icsText(title)); icsLine("LOCATION:" + icsText(location));
            icsLine("DESCRIPTION:" + icsText(description.join('\n'))); icsLine("END:VEVENT");
        }
    }
    if (format == "ics") icsLine("END:VCALENDAR");
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly) || file.write(output) != output.size() || !file.commit()) {
        m_planStatus = [error = file.errorString()] { return tr("观测计划保存失败：%1").arg(error); };
        emit planChanged(); return false;
    }
    m_planStatus = [] { return tr("观测计划已保存"); };
    emit planChanged(); return true;
}

QVariantList SatelliteModel::snapshots() const
{
    QVariantList result;
    if (!m_database.isOpen()) return result;
    QSqlQuery query(m_database);
    query.prepare("SELECT id, acquired, source, pinned, origin FROM snapshots WHERE group_key = ? ORDER BY acquired DESC, id DESC");
    query.addBindValue(m_sources.value(m_selected).group.isEmpty() ? m_group : m_sources.value(m_selected).group);
    if (query.exec()) while (query.next()) result.append(QVariantMap{{"id", query.value(0)},
        {"label", QDateTime::fromSecsSinceEpoch(query.value(1).toLongLong(), m_clock ? QTimeZone(m_clock->timeZone().toUtf8()) : QTimeZone::systemTimeZone()).toString("yyyy-MM-dd HH:mm:ss")
            + (query.value(3).toBool() ? tr(" · 已固定") : QString())}, {"source", query.value(2)}, {"pinned", query.value(3).toBool()}, {"imported", query.value(4).toString() == "import"}});
    return result;
}

void SatelliteModel::loadSnapshot(qint64 id)
{
    if (m_storageBusy) return;
    QSqlQuery query(m_database);
    query.prepare("SELECT payload, source, group_key FROM snapshots WHERE id = ?"); query.addBindValue(id);
    if (!query.exec() || !query.next()) return;
    QString error; int skipped = 0;
    auto satellites = Orbit::parse(query.value(0).toByteArray(), error, skipped);
    if (satellites.isEmpty()) { setStatus([error] { return error; }); return; }
    m_snapshot = id; m_source = query.value(1).toString();
    install(std::move(satellites), Source{query.value(2).toString(), m_source, id});
    setStatus([] { return tr("本地轨道数据已载入"); });
    emit catalogChanged();
}

qint64 SatelliteModel::snapshotId() const { return m_sources.value(m_selected).snapshot; }
QString SatelliteModel::sourceText() const { return m_sources.value(m_selected).url; }
QString SatelliteModel::elementEpoch() const
{
    const auto *satellite = selected();
    return satellite ? satellite->elements.value("EPOCH").toString() : QString();
}

QString SatelliteModel::constellationKey(const Orbit::Satellite &satellite) const
{
    const auto key = CatalogModel::constellation(satellite);
    if (key != "other") return key;
    for (const auto *group : {"gps-ops", "glo-ops", "galileo", "beidou"})
        if (m_groupMembers.value(QLatin1String(group)).contains(satellite.number)) return QLatin1String(group);
    return key;
}

void SatelliteModel::install(QVector<Orbit::Satellite> satellites, Source source)
{
    QSqlQuery query(m_database);
    query.prepare("SELECT acquired FROM snapshots WHERE id = ?");
    query.addBindValue(source.snapshot);
    if (query.exec() && query.next()) source.acquired = query.value(0).toLongLong();
    m_groupSources.insert(source.group, source);
    beginResetModel(); m_rows.clear();
    QSet<qint64> sourceMembers;
    static const QRegularExpression prnPattern(QStringLiteral("PRN\\s*([A-Z]?\\d+)"), QRegularExpression::CaseInsensitiveOption);
    for (auto &satellite : satellites) {
        const auto id = satellite.number;
        const auto prn = prnPattern.match(satellite.name);
        if (satellite.elements.contains("PRN")) m_prns[id] = satellite.elements.value("PRN").toVariant().toString();
        else if (prn.hasMatch()) m_prns[id] = prn.captured(1);
        if (satellite.elements.contains("ORBITAL_PLANE")) m_planes[id] = satellite.elements.value("ORBITAL_PLANE").toVariant().toString();
        sourceMembers.insert(id);
        m_sources.insert(id, source);
        if (m_index.contains(id)) m_satellites[m_index.value(id)] = std::move(satellite);
        else { m_index.insert(id, static_cast<int>(m_satellites.size())); m_satellites.append(std::move(satellite)); }
    }
    m_groupMembers[source.group] = sourceMembers;
    endResetModel();
    m_targets.clear(); m_owners.clear(); m_members.clear();
    QHash<qint64, int> indices;
    for (int i = 0; i < m_satellites.size(); ++i) indices.insert(m_satellites[i].number, i);
    QVector<int> stationRecords;
    for (int i = 0; i < m_satellites.size(); ++i) {
        const auto &satellite = m_satellites[i];
        const qint64 station = stationFor(satellite);
        m_owners.insert(satellite.number, station && indices.contains(station) ? station : satellite.number);
        if (station && indices.contains(station)) stationRecords.append(i);
    }
    // Visiting spacecraft reappear separately when the source supplies an independent orbit.
    for (int i = 0; i < m_satellites.size(); ++i) {
        const auto &satellite = m_satellites[i];
        if (!stationFor(satellite)) {
            const auto station = visitingStation(satellite.name);
            if (station && indices.contains(station)) for (const int candidate : stationRecords) {
                if (stationFor(m_satellites[candidate]) == station && sameOrbit(satellite, m_satellites[candidate])) {
                    m_owners[satellite.number] = station;
                    break;
                }
            }
        }
        m_members[m_owners.value(satellite.number)].append(i);
    }
    for (int i = 0; i < m_satellites.size(); ++i)
        if (m_owners.value(m_satellites[i].number) == m_satellites[i].number) m_targets.append(i);
    m_gnssTargets.clear();
    for (const int index : m_targets) {
        const auto key = constellationKey(m_satellites[index]);
        if (key != "gps-ops" && key != "glo-ops" && key != "galileo" && key != "beidou") continue;
        if (!m_groupMembers.contains(key) || m_groupMembers.value(key).contains(m_satellites[index].number)) m_gnssTargets.append(index);
    }
    m_elevations.clear();
    m_selected = m_owners.value(m_selected, 0);
    if (!m_selected) for (const auto &id : m_watchlist) {
        if (indices.contains(id.toLongLong())) { m_selected = m_owners.value(id.toLongLong(), id.toLongLong()); break; }
    }
    if (selectedWatched()) m_lastWatchedSelection = m_selected;
    filter(); invalidate(); emit selectionChanged(); emit catalogChanged(); emit watchlistChanged(); emit sourceChanged();
    requestWatchPredictions();
    if (m_autoCleanup && !m_storageBusy) pruneSnapshots();
    emit storageChanged();
    if (previewActive()) { ++m_previewRevision; refreshPreview(); }
}
const Orbit::Satellite *SatelliteModel::selected() const
{
    const int index = m_index.value(m_selected, -1);
    return index >= 0 ? &m_satellites[index] : nullptr;
}
void SatelliteModel::select(const QString &id)
{
    const auto value = m_owners.value(id.toLongLong(), 0); if (!value || value == m_selected) return;
    bool exists = false; for (const auto &satellite : m_satellites) if (satellite.number == value) exists = true;
    if (!exists) return;
    m_selected = value;
    if (selectedWatched()) m_lastWatchedSelection = value;
    invalidate(); emit selectionChanged(); emit watchlistChanged(); emit sourceChanged();
}

bool SatelliteModel::isWatched(const QString &id) const
{
    return m_watchlist.contains(QString::number(m_owners.value(id.toLongLong(), id.toLongLong())));
}

void SatelliteModel::setWatched(const QString &id, bool watched)
{
    const qint64 number = m_owners.value(id.toLongLong(), id.toLongLong());
    const QString key = QString::number(number);
    if (watched == m_watchlist.contains(key) || (watched && !m_index.contains(number))) return;
    if (watched) m_watchlist.append(key); else m_watchlist.removeAll(key);
    m_settings.setValue("watchlist/ids", m_watchlist);
    requestWatchPredictions();
    ++m_revision;
    filter(); emit watchlistChanged();
    if ((!watched && m_selected == number && !previewActive()) || !m_selected) {
        m_selected = 0;
        for (const auto &candidate : m_watchlist) if (m_index.contains(candidate.toLongLong())) { m_selected = candidate.toLongLong(); break; }
        m_lastWatchedSelection = m_selected;
        invalidate(); emit selectionChanged(); emit sourceChanged();
    } else requestFrame();
}

QString SatelliteModel::previewName() const { return CatalogModel::constellationName(m_previewGroup); }

void SatelliteModel::previewConstellation(const QString &key)
{
    m_previewGroup = key; m_previewMode = 0;
    m_previewCandidates.clear(); m_previewCount = 0; m_previewTotal = 0; m_previewTime = 0;
    ++m_previewRevision; ++m_revision;
    emit previewChanged();
    refreshPreview();
}

void SatelliteModel::setPreviewMode(int mode)
{
    mode = qBound(0, mode, 2);
    if (m_previewMode == mode) return;
    m_previewMode = mode;
    ++m_previewRevision; ++m_revision;
    m_previewTime = 0;
    emit previewChanged();
    refreshPreview();
}

void SatelliteModel::closePreview()
{
    m_previewGroup.clear(); m_previewCandidates.clear(); m_previewCount = 0;
    ++m_previewRevision; ++m_revision;
    emit previewChanged();
    if (!selectedWatched()) {
        m_selected = 0;
        const QString previous = QString::number(m_lastWatchedSelection);
        if (m_watchlist.contains(previous)) m_selected = m_lastWatchedSelection;
        else for (const auto &id : m_watchlist) if (m_index.contains(id.toLongLong())) { m_selected = id.toLongLong(); break; }
        invalidate(); emit selectionChanged(); emit watchlistChanged(); emit sourceChanged();
    } else requestFrame();
}

void SatelliteModel::refreshPreview()
{
    if (!previewActive() || !m_clock || !m_clock->hasObserver()) return;
    if (m_previewBusy) { m_previewPending = true; return; }
    QVector<Orbit::Satellite> satellites;
    const auto members = m_groupMembers.value(m_previewGroup);
    for (const int index : m_targets)
        if (constellationKey(m_satellites[index]) == m_previewGroup &&
            (!m_groupMembers.contains(m_previewGroup) || members.contains(m_satellites[index].number))) satellites.append(m_satellites[index]);
    m_previewTotal = static_cast<int>(satellites.size());
    if (m_previewMode == 2) {
        m_previewCandidates.clear();
        for (const auto &satellite : satellites) m_previewCandidates.insert(satellite.number);
        m_previewTime = m_clock->unixTime(); ++m_revision;
        emit previewChanged(); requestFrame(); return;
    }
    const auto generation = m_previewRevision;
    const int mode = m_previewMode;
    const double time = m_clock->unixTime();
    const Orbit::Observer observer{m_clock->observerLatitude(), m_clock->observerLongitude(), m_clock->ellipsoidHeight() / 1000.0, m_clock->minimumElevation()};
    if (!std::isfinite(observer.heightKm)) return;
    m_previewBusy = true; m_previewPending = false;
    emit previewChanged();
    m_pool.start([this, satellites, generation, mode, time, observer] {
        QSet<qint64> candidates;
        const auto geometry = Orbit::observerGeometry(observer);
        QVector<Orbit::PropagationContext> contexts;
        const int steps = mode == 1 ? 30 : 0;
        for (int step = 0; step <= steps; ++step) contexts.append(Orbit::propagationContext(time + step * 30, geometry));
        for (const auto &satellite : satellites) {
            std::optional<Orbit::State> previous;
            for (int step = 0; step <= steps; ++step) {
                const auto state = Orbit::look(satellite, contexts[step]);
                if (!state) { previous.reset(); continue; }
                if (state->elevation >= observer.minimumElevation) { candidates.insert(satellite.number); break; }
                // Refine an intervening peak so short grazing passes can enter the preview.
                if (previous && previous->elevationRate > 0 && state->elevationRate < 0) {
                    double left = previous->time, right = state->time;
                    for (int iteration = 0; iteration < 12; ++iteration) {
                        const double a = left + (right - left) / 3, b = right - (right - left) / 3;
                        const auto first = Orbit::look(satellite, Orbit::propagationContext(a, geometry));
                        const auto second = Orbit::look(satellite, Orbit::propagationContext(b, geometry));
                        if (!first || !second) break;
                        if (std::max(first->elevation, second->elevation) >= observer.minimumElevation) { candidates.insert(satellite.number); break; }
                        if (first->elevation < second->elevation) left = a; else right = b;
                    }
                    if (candidates.contains(satellite.number)) break;
                }
                previous = state;
            }
        }
        QMetaObject::invokeMethod(this, [this, candidates = std::move(candidates), generation, time]() mutable {
            m_previewBusy = false;
            if (generation == m_previewRevision && previewActive()) {
                m_previewCandidates = std::move(candidates);
                m_previewTime = time; ++m_revision;
                requestFrame();
            }
            emit previewChanged();
            if (previewActive() && (m_previewPending || generation != m_previewRevision)) refreshPreview();
        }, Qt::QueuedConnection);
    });
}
void SatelliteModel::invalidate()
{
    ++m_revision; m_needTrack = true;
    m_trackCache = {};
    m_gnssTime = 0;
    m_observation.clear(); m_trajectory.clear(); m_passes.clear(); m_markers.clear(); m_shadowEvents.clear();
    emit frameChanged(); emit trajectoryChanged(); requestFrame();
}

void SatelliteModel::setReceiveFrequency(double value)
{
    if (!std::isfinite(value) || value < 0 || value > 1000000) return;
    if (m_frequency == value) return;
    m_frequency = value; m_settings.setValue("radio/frequencyMHz", value);
    emit receiveFrequencyChanged();
    if (m_observation.contains("rangeRate"))
        m_observation.insert("doppler", -m_observation.value("rangeRate").toDouble() / 299792.458 * m_frequency * 1e6);
    emit frameChanged();
}

void SatelliteModel::requestFrame()
{
    if (!m_clock || !m_clock->hasObserver() || m_satellites.isEmpty()) return;
    const double time = m_clock->unixTime();
    const double calculationInterval = 1.0 / m_clock->calculationFrequency();
    if (m_clock->live() && m_revision == m_calculationRevision && !m_needTrack && std::abs(time - m_calculationTime) < calculationInterval) return;
    if (m_busy) { m_pending = true; return; }
    QVector<Orbit::Satellite> satellites;
    QSet<qint64> watchIds;
    for (const auto &id : m_watchlist) watchIds.insert(m_owners.value(id.toLongLong(), id.toLongLong()));
    auto ids = watchIds;
    ids.unite(m_previewCandidates);
    if (m_selected) ids.insert(m_selected);
    for (const auto id : ids) {
        const int index = m_index.value(id, -1);
        if (index >= 0) satellites.append(m_satellites[index]);
    }
    const auto id = m_selected;
    const auto revision = m_revision;
    const double reference = m_clock->referenceTime();
    const bool interpolate = m_clock->live() && m_clock->updateFrequency() > m_clock->calculationFrequency();
    const bool interpolateGnss = m_clock->live() && m_clock->updateFrequency() > 1;
    const bool calculateGnss = std::abs(time - m_gnssTime) >= 1;
    QVector<Orbit::Satellite> gnss;
    if (calculateGnss) for (const int index : m_gnssTargets) gnss.append(m_satellites[index]);
    const Orbit::Observer observer{m_clock->observerLatitude(), m_clock->observerLongitude(), m_clock->ellipsoidHeight() / 1000.0, m_clock->minimumElevation()};
    const QTimeZone zone(m_clock->timeZone().toUtf8());
    const bool calculateTrack = m_needTrack;
    const auto previousTrack = calculateTrack ? m_trackCache : Orbit::Track{};
    const bool hasHeight = m_clock->hasObserverHeight();
    const auto previewCandidates = m_previewCandidates;
    const int previewMode = m_previewMode;
    if (!std::isfinite(observer.heightKm)) { setStatus([] { return tr("大地水准面数据读取失败"); }); return; }
    m_busy = true; m_pending = false; m_needTrack = false;
    m_calculationTime = time; m_calculationRevision = revision;
    emit statusChanged();
    m_pool.start([this, satellites, gnss, calculateGnss, id, revision, time, reference, observer, zone, calculateTrack, previousTrack, hasHeight, watchIds, previewCandidates, previewMode, calculationInterval, interpolate, interpolateGnss] {
        QVariantList markers, trajectory, passes, shadowEvents;
        QVariantList gnssMarkers;
        QVariantMap observation;
        QHash<qint64, double> elevations;
        const auto geometry = Orbit::observerGeometry(observer);
        const auto context = Orbit::propagationContext(time, geometry);
        const auto nextContext = interpolate ? Orbit::propagationContext(time + calculationInterval, geometry) : context;
        const auto nextGnssContext = calculateGnss && interpolateGnss ? Orbit::propagationContext(time + 1, geometry) : context;
        const auto addNextPosition = [](QVariantMap &marker, const Orbit::Satellite &satellite, const Orbit::PropagationContext &next) {
            if (const auto point = Orbit::position(satellite, next)) {
                marker.insert("nextTime", point->time); marker.insert("nextLatitude", point->latitude);
                marker.insert("nextLongitude", point->longitude); marker.insert("nextAltitude", point->altitude);
            }
        };
        Orbit::Track trackCache;
        for (const auto &satellite : gnss) {
            if (const auto state = Orbit::position(satellite, context)) {
                QVariantMap marker{{"id", QString::number(satellite.number)}, {"time", time}, {"latitude", state->latitude}, {"longitude", state->longitude}};
                if (interpolateGnss) addNextPosition(marker, satellite, nextGnssContext);
                gnssMarkers.append(marker);
            }
        }
        int previewCount = 0;
        for (const auto &satellite : satellites) {
            const auto state = Orbit::propagate(satellite, context);
            if (!state) continue;
            elevations.insert(satellite.number, state->elevation);
            auto marker = stateMap(*state);
            marker.insert("id", QString::number(satellite.number));
            const bool inPreview = previewCandidates.contains(satellite.number) && (previewMode != 0 || state->elevation >= observer.minimumElevation);
            if (inPreview) ++previewCount;
            if (watchIds.contains(satellite.number) || satellite.number == id || inPreview) {
                auto displayed = marker;
                if (interpolate) addNextPosition(displayed, satellite, nextContext);
                markers.append(displayed);
            }
            if (satellite.number != id) continue;
            observation = marker;
            observation.insert("originalName", satellite.name);
            observation.insert("epochAge", (time - satellite.epoch) / 86400.0);
            observation.insert("heightEstimated", !hasHeight);
            if (calculateTrack) {
                trackCache = Orbit::track(satellite, reference - 43200, reference + 43200, observer, previousTrack);
                const auto &track = trackCache;
                for (const auto &sample : track.samples) trajectory.append(stateMap(sample));
                for (qsizetype i = 1; i < track.samples.size(); ++i) {
                    const auto &a = track.samples[i - 1], &b = track.samples[i];
                    if (b.time - a.time > 31) continue;
                    for (const int boundary : {1, 2}) {
                        const bool before = a.illumination >= boundary, after = b.illumination >= boundary;
                        if (before == after) continue;
                        double left = a.time, right = b.time;
                        while (right - left > 0.5) {
                            const double middle = (left + right) / 2;
                            const auto stateAt = Orbit::propagate(satellite, Orbit::propagationContext(middle, geometry));
                            if (!stateAt) break;
                            if ((stateAt->illumination >= boundary) == before) left = middle; else right = middle;
                        }
                        const double eventTime = (left + right) / 2;
                        shadowEvents.append(QVariantMap{{"time", eventTime}, {"timeText", formatTime(eventTime, zone)}, {"entering", after}, {"umbra", boundary == 2}});
                    }
                }
                std::sort(shadowEvents.begin(), shadowEvents.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value("time").toDouble() < b.toMap().value("time").toDouble(); });
                for (const auto &pass : track.passes) {
                    QStringList visible;
                    for (const auto &interval : pass.visibleIntervals)
                        visible.append(formatTime(interval[0], zone, "HH:mm:ss") + " - " + formatTime(interval[1], zone, "HH:mm:ss"));
                    passes.append(QVariantMap{{"start", pass.rise.time}, {"peak", pass.peak.time}, {"end", pass.set.time},
                        {"startClipped", pass.startsBeforeWindow}, {"endClipped", pass.endsAfterWindow},
                        {"startText", formatTime(pass.rise.time, zone)},
                        {"peakText", formatTime(pass.peak.time, zone)},
                        {"endText", formatTime(pass.set.time, zone)},
                        {"startAzimuth", pass.rise.azimuth}, {"endAzimuth", pass.set.azimuth},
                        {"maximum", number(pass.peak.elevation, 1) + QStringLiteral("°")},
                        {"duration", number((pass.set.time - pass.rise.time) / 60, 1) + QStringLiteral(" min")},
                        {"range", number(pass.peak.range, 0) + QStringLiteral(" km")}, {"hasOptical", !visible.isEmpty()},
                        {"optical", visible.join(" / ")}});
                }
            }
        }
        QMetaObject::invokeMethod(this, [this, revision, reference, calculateTrack, markers = std::move(markers), observation = std::move(observation),
            trajectory = std::move(trajectory), passes = std::move(passes), shadowEvents = std::move(shadowEvents), trackCache = std::move(trackCache), elevations = std::move(elevations), previewCount,
            gnssMarkers = std::move(gnssMarkers), calculateGnss, time]() mutable {
            m_busy = false;
            if (revision == m_revision) {
                m_markers = std::move(markers); m_observation = std::move(observation); m_elevations = std::move(elevations);
                m_previewCount = previewCount;
                if (calculateGnss) { m_gnssMarkers = std::move(gnssMarkers); m_gnssTime = time; emit gnssChanged(); }
                if (m_observation.contains("rangeRate"))
                    m_observation.insert("doppler", -m_observation.value("rangeRate").toDouble() / 299792.458 * m_frequency * 1e6);
                updateMagnitude();
                emit frameChanged();
                if (!m_rows.isEmpty()) emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {ElevationRole});
                if (calculateTrack) {
                    m_trajectory = std::move(trajectory); m_passes = std::move(passes); m_shadowEvents = std::move(shadowEvents);
                    m_trackCache = std::move(trackCache); m_trackReference = reference; emit trajectoryChanged();
                }
            } else if (calculateTrack) m_needTrack = true;
            emit statusChanged();
            if (m_pending || revision != m_revision) requestFrame();
        }, Qt::QueuedConnection);
    });
}

const SatelliteModel::Photometry *SatelliteModel::selectedPhotometry() const
{
    const auto custom = m_photometry.constFind(m_selected);
    if (custom != m_photometry.cend()) return &custom.value();
    const auto base = m_defaultPhotometry.constFind(m_selected);
    const auto *satellite = selected();
    // Historical QuickSat provisional numbers can overlap later NORAD assignments.
    if (base == m_defaultPhotometry.cend() || !satellite || base->internationalId.isEmpty()
        || base->internationalId != satellite->internationalId) return nullptr;
    return &base.value();
}

QVariantMap SatelliteModel::photometry() const
{
    const auto *entry = selectedPhotometry();
    if (!entry) return {};
    const auto base = m_defaultPhotometry.constFind(m_selected);
    const auto *satellite = selected();
    const bool hasDefault = base != m_defaultPhotometry.cend() && satellite && !base->internationalId.isEmpty() && base->internationalId == satellite->internationalId;
    const auto source = !m_photometry.contains(m_selected) && m_selected == 48274 ? tr("SeeSat-L / Jay Respler（2022 年构型）")
        : entry->manual && entry->source == QStringLiteral("手动填写") ? tr("手动填写") : entry->source;
    return {{"magnitude", entry->magnitude}, {"phase", entry->phase}, {"source", source}, {"manual", entry->manual},
        {"builtin", !m_photometry.contains(m_selected)}, {"hasOverride", m_photometry.contains(m_selected)}, {"hasDefault", hasDefault},
        {"sourceDate", entry->sourceDate}, {"recordedAt", entry->recordedAt ? QDateTime::fromSecsSinceEpoch(entry->recordedAt, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm:ss 'UTC'") : QString()}};
}

bool SatelliteModel::setPhotometry(double magnitude, int phase, const QString &source, const QString &sourceDate)
{
    if (!selected() || !std::isfinite(magnitude) || magnitude < -30 || magnitude > 30 || (phase != 0 && phase != 90)) return false;
    const auto date = sourceDate.trimmed();
    if (!date.isEmpty() && (!QDate::fromString(date, Qt::ISODate).isValid() || QDate::fromString(date, Qt::ISODate).toString(Qt::ISODate) != date)) {
        m_photometryStatus = [] { return tr("请填写有效的资料日期（YYYY-MM-DD）"); };
        emit photometryChanged(); return false;
    }
    const QString label = source.trimmed().isEmpty() ? QStringLiteral("手动填写") : source.trimmed();
    const auto recordedAt = QDateTime::currentSecsSinceEpoch();
    QSqlQuery query(m_database);
    query.prepare("INSERT OR REPLACE INTO photometry (norad, magnitude, phase, source, manual, source_date, recorded_at) VALUES (?, ?, ?, ?, 1, ?, ?)");
    query.addBindValue(m_selected); query.addBindValue(magnitude); query.addBindValue(phase); query.addBindValue(label);
    query.addBindValue(date.isEmpty() ? QStringLiteral("") : date); query.addBindValue(recordedAt);
    if (!query.exec()) {
        m_photometryStatus = [error = query.lastError().text()] { return tr("星等参数保存失败：") + error; };
        emit photometryChanged(); return false;
    }
    m_photometry.insert(m_selected, {magnitude, phase, label, true, date, recordedAt});
    m_photometryStatus = [] { return tr("星等参数已保存"); };
    updateMagnitude(); emit frameChanged(); emit photometryChanged();
    return true;
}

bool SatelliteModel::clearPhotometry()
{
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM photometry WHERE norad = ?"); query.addBindValue(m_selected);
    if (!query.exec()) {
        m_photometryStatus = [error = query.lastError().text()] { return tr("星等参数清除失败：") + error; };
        emit photometryChanged(); return false;
    }
    m_photometry.remove(m_selected);
    m_photometryStatus = [hasDefault = selectedPhotometry() != nullptr] { return hasDefault ? tr("使用内置星等资料") : tr("星等参数已清除"); };
    updateMagnitude(); emit frameChanged(); emit photometryChanged();
    return true;
}

QHash<qint64, SatelliteModel::Photometry> SatelliteModel::readMagnitudes(QIODevice &input, const Photometry &metadata, int &skipped)
{
    QHash<qint64, Photometry> entries;
    static const QRegularExpression designation(QStringLiteral("^(\\d{2})\\s+(\\d{1,3})([A-Z]{1,3})$"));
    while (!input.atEnd()) {
        const auto line = QString::fromUtf8(input.readLine());
        if (line.trimmed().isEmpty()) continue;
        bool validId = false, validMagnitude = false;
        const auto id = line.left(5).toLongLong(&validId);
        const double magnitude = line.mid(33, 4).trimmed().toDouble(&validMagnitude);
        // QuickSat columns 34-37: magnitude at 1000 km and full phase; 20 means unknown.
        if (line.size() < 37 || !validId || id <= 0 || !validMagnitude || !std::isfinite(magnitude)
            || magnitude == 20 || magnitude < -30 || magnitude > 30) { ++skipped; continue; }
        auto entry = metadata;
        entry.magnitude = magnitude;
        const auto match = designation.match(line.mid(8, 8).trimmed());
        if (match.hasMatch()) {
            const int year = match.captured(1).toInt();
            entry.internationalId = QStringLiteral("%1-%2%3").arg(year < 57 ? 2000 + year : 1900 + year)
                .arg(match.captured(2).rightJustified(3, QLatin1Char('0')), match.captured(3));
        }
        entries.insert(id, entry);
    }
    return entries;
}

bool SatelliteModel::importMagnitudes(const QUrl &url, const QString &sourceDate)
{
    const auto fail = [this](std::function<QString()> message) { m_photometryStatus = std::move(message); emit photometryChanged(); return false; };
    const auto date = sourceDate.trimmed();
    if (!date.isEmpty() && (!QDate::fromString(date, Qt::ISODate).isValid() || QDate::fromString(date, Qt::ISODate).toString(Qt::ISODate) != date))
        return fail([] { return tr("请填写有效的资料日期（YYYY-MM-DD）"); });
    const auto recordedAt = QDateTime::currentSecsSinceEpoch();
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) return fail([error = file.errorString()] { return tr("星等表打开失败：") + error; });
    int skipped = 0, preserved = 0;
    auto entries = readMagnitudes(file, {0, 0, QStringLiteral("QuickSat · ") + QFileInfo(file).fileName(), false, date, recordedAt}, skipped);
    if (entries.isEmpty()) return fail([] { return tr("文件中没有可用的 QuickSat 星等记录"); });
    if (!m_database.transaction()) return fail([error = m_database.lastError().text()] { return tr("星等资料库写入失败：") + error; });
    QSqlQuery query(m_database);
    query.prepare("INSERT OR REPLACE INTO photometry (norad, magnitude, phase, source, manual, source_date, recorded_at) VALUES (?, ?, ?, ?, 0, ?, ?)");
    for (auto it = entries.begin(); it != entries.end();) {
        if (m_photometry.value(it.key()).manual) { ++preserved; it = entries.erase(it); continue; }
        query.bindValue(0, it.key()); query.bindValue(1, it->magnitude); query.bindValue(2, it->phase); query.bindValue(3, it->source);
        query.bindValue(4, date.isEmpty() ? QStringLiteral("") : date); query.bindValue(5, recordedAt);
        if (!query.exec()) {
            m_database.rollback(); return fail([error = query.lastError().text()] { return tr("星等表导入失败：") + error; });
        }
        ++it;
    }
    if (!m_database.commit()) { m_database.rollback(); return fail([error = m_database.lastError().text()] { return tr("星等表保存失败：") + error; }); }
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) m_photometry.insert(it.key(), it.value());
    m_photometryStatus = [count = entries.size(), preserved, skipped] { return tr("已导入 %1 条星等记录，保留 %2 条手动参数，跳过 %3 行空缺或格式异常记录").arg(count).arg(preserved).arg(skipped); };
    updateMagnitude(); emit frameChanged(); emit photometryChanged();
    return true;
}

void SatelliteModel::updateMagnitude()
{
    m_observation.remove("magnitude");
    if (!m_observation.contains("range")) return;
    int status = 0;
    const auto *entry = selectedPhotometry();
    if (!entry) status = 1;
    else if (m_observation.value("elevation").toDouble() <= 0) status = 2;
    else if (m_observation.value("illumination").toInt() == 2) status = 3;
    else if (m_observation.value("illumination").toInt() == 1) status = 4;
    else {
        Orbit::State state;
        state.range = m_observation.value("range").toDouble();
        state.phaseAngle = m_observation.value("phaseAngle").toDouble();
        state.elevation = m_observation.value("elevation").toDouble();
        const auto magnitude = Orbit::apparentMagnitude(state, entry->magnitude, entry->phase);
        if (magnitude) m_observation.insert("magnitude", *magnitude);
        else status = 5;
    }
    m_observation.insert("magnitudeStatusCode", status);
}

QVariantMap SatelliteModel::observation() const
{
    auto result = m_observation;
    if (result.isEmpty()) return result;
    if (const auto *satellite = selected()) result.insert("name", Orbit::displayName(*satellite));
    const int illumination = result.value("illumination").toInt();
    result.insert("lighting", Orbit::illuminationName(illumination));
    result.insert("direction", Orbit::directionName(result.value("azimuth").toDouble()));
    result.insert("motion", result.value("rangeRate").toDouble() < 0 ? tr("正在接近") : tr("正在远离"));
    result.insert("visibility", result.value("elevation").toDouble() < (m_clock ? m_clock->minimumElevation() : 10) ? tr("低于最低高度角")
        : illumination != 0 ? tr("位于地影") : result.value("sunElevation").toDouble() > -6 ? tr("天空较亮") : tr("具备光照观测条件"));
    const char *magnitudeStates[] = {"", QT_TR_NOOP("暂无参考星等"), QT_TR_NOOP("地平线下"), QT_TR_NOOP("本影内"), QT_TR_NOOP("半影内"), QT_TR_NOOP("相位接近 180°")};
    result.insert("magnitudeStatus", tr(magnitudeStates[qBound(0, result.value("magnitudeStatusCode").toInt(), 5)]));
    return result;
}

QVariantList SatelliteModel::passes() const
{
    auto result = m_passes;
    for (auto &entry : result) {
        auto pass = entry.toMap();
        if (pass.value("startClipped").toBool()) pass["startText"] = tr("早于 ") + pass.value("startText").toString();
        if (pass.value("endClipped").toBool()) pass["endText"] = tr("晚于 ") + pass.value("endText").toString();
        const double start = pass.value("startAzimuth").toDouble(), end = pass.value("endAzimuth").toDouble();
        pass["startAz"] = Orbit::directionName(start) + " " + number(start, 1) + QStringLiteral("°");
        pass["endAz"] = Orbit::directionName(end) + " " + number(end, 1) + QStringLiteral("°");
        if (!pass.value("hasOptical").toBool()) pass["optical"] = tr("光照条件欠佳");
        entry = pass;
    }
    return result;
}

QVariantList SatelliteModel::shadowEvents() const
{
    auto result = m_shadowEvents;
    for (auto &entry : result) {
        auto event = entry.toMap();
        event["name"] = (event.value("entering").toBool() ? tr("进入") : tr("离开")) + (event.value("umbra").toBool() ? tr("本影") : tr("半影"));
        entry = event;
    }
    return result;
}

QVariantList SatelliteModel::relatedObjects() const
{
    QVariantList result;
    for (const int index : m_members.value(m_selected)) {
        const auto &satellite = m_satellites[index];
        result.append(QVariantMap{{"id", QString::number(satellite.number)}, {"name", Orbit::displayName(satellite)},
            {"originalName", satellite.name}, {"internationalId", satellite.internationalId}});
    }
    return result;
}

QVariantList SatelliteModel::orbitFields() const
{
    const auto *satellite = selected(); if (!satellite) return {};
    const auto &e = satellite->elements;
    const auto field = [&e](const char *key, int precision, const QString &unit = {}) {
        return e.contains(key) ? QString::number(e.value(key).toDouble(), 'f', precision) + unit : tr("暂无数据");
    };
    const auto scientific = [&e](const char *key) { return e.contains(key) ? QString::number(e.value(key).toDouble(), 'e', 7) : tr("暂无数据"); };
    QVariantList result;
    const auto add = [&result](const QString &label, const QString &value, bool epoch = false) { result.append(QVariantMap{{"label", label}, {"value", value}, {"epoch", epoch}}); };
    add(tr("卫星编号"), QString::number(satellite->number));
    add(tr("国际编号"), satellite->internationalId.isEmpty() ? tr("暂无数据") : satellite->internationalId);
    add(tr("名称"), satellite->name);
    const auto constellation = constellationKey(*satellite);
    if (constellation == "gps-ops" || constellation == "glo-ops" || constellation == "galileo" || constellation == "beidou") {
        add(QStringLiteral("PRN"), m_prns.value(satellite->number, tr("暂无数据")));
        add(tr("轨道面"), m_planes.value(satellite->number, tr("暂无数据")));
    }
    add(tr("历元（协调世界时）"), e.value("EPOCH").toString(), true);
    add(tr("轨道倾角"), field("INCLINATION", 4, QStringLiteral("°")));
    add(tr("升交点赤经"), field("RA_OF_ASC_NODE", 4, QStringLiteral("°")));
    add(tr("偏心率"), field("ECCENTRICITY", 7));
    add(tr("近地点幅角"), field("ARG_OF_PERICENTER", 4, QStringLiteral("°")));
    add(tr("平近点角"), field("MEAN_ANOMALY", 4, QStringLiteral("°")));
    add(tr("每日绕地圈数"), field("MEAN_MOTION", 8, QStringLiteral(" rev/d")));
    add(tr("轨道周期"), number(1440.0 / e.value("MEAN_MOTION").toDouble(), 3) + QStringLiteral(" min"));
    add(tr("阻力项"), scientific("BSTAR") + QStringLiteral(" R_E^-1"));
    add(tr("历元圈数"), field("REV_AT_EPOCH", 0));
    add(tr("根数集编号"), field("ELEMENT_SET_NO", 0));
    add(tr("平均运动一阶项"), scientific("MEAN_MOTION_DOT") + QStringLiteral(" rev/d²"));
    add(tr("平均运动二阶项"), scientific("MEAN_MOTION_DDOT") + QStringLiteral(" rev/d³"));
    return result;
}
void SatelliteModel::copyDetails()
{
    QStringList lines;
    for (const auto &entry : orbitFields()) {
        const auto field = entry.toMap(); lines.append(field.value("label").toString() + ": " + field.value("value").toString());
    }
    QGuiApplication::clipboard()->setText(lines.join('\n'));
    setStatus([] { return tr("轨道参数已复制"); });
}
