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
#include <QStandardPaths>
#include <QUrlQuery>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>

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
        {"sunElevation", state.sunElevation}, {"illumination", state.illumination}};
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
    m_pool.setMaxThreadCount(2);
    m_watchlist = m_settings.value("watchlist/ids", QStringList{"25544", "48274"}).toStringList();
    m_watchlist.removeDuplicates();
    m_frequency = m_settings.value("radio/frequencyMHz", 145.8).toDouble();
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(directory);
    m_database = QSqlDatabase::addDatabase("QSQLITE", QStringLiteral("orbits-%1").arg(reinterpret_cast<quintptr>(this)));
    m_database.setDatabaseName(directory + "/orbits.sqlite");
    if (!m_database.open()) { setStatus(QStringLiteral("轨道资料库打开失败：") + m_database.lastError().text()); return; }
    QSqlQuery query(m_database);
    if (!query.exec("CREATE TABLE IF NOT EXISTS snapshots (id INTEGER PRIMARY KEY, group_key TEXT NOT NULL, acquired INTEGER NOT NULL, source TEXT NOT NULL, payload BLOB NOT NULL)"))
        setStatus(QStringLiteral("轨道资料库初始化失败：") + query.lastError().text());
}

SatelliteModel::~SatelliteModel()
{
    m_pool.clear();
    m_pool.waitForDone();
    const auto name = m_database.connectionName();
    m_database.close();
    m_database = {};
    QSqlDatabase::removeDatabase(name);
}

int SatelliteModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : static_cast<int>(m_rows.size()); }
QHash<int, QByteArray> SatelliteModel::roleNames() const
{
    return {{IdRole, "satelliteId"}, {NameRole, "satelliteName"}, {OriginalRole, "originalName"}, {ElevationRole, "elevationText"}};
}
QVariant SatelliteModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return {};
    const auto &satellite = m_satellites[m_rows[index.row()]];
    switch (role) {
    case IdRole: return QString::number(satellite.number);
    case NameRole: return Orbit::displayName(satellite);
    case OriginalRole: return satellite.name;
    case ElevationRole: return m_elevations.contains(satellite.number) ? number(m_elevations.value(satellite.number), 1) + QStringLiteral("°") : QStringLiteral("待计算");
    default: return {};
    }
}

QVariantList SatelliteModel::groups() const
{
    QVariantList result;
    const std::pair<const char *, const char *> groups[] = {
        {"catalog", "本地完整目录"}, {"active", "活动卫星（CelesTrak）"}, {"stations", "空间站"}, {"visual", "明亮目标"},
        {"weather", "气象卫星"}, {"noaa", "美国气象卫星"}, {"goes", "地球静止气象卫星"},
        {"resource", "地球资源卫星"}, {"sarsat", "搜救卫星"}, {"dmc", "灾害监测卫星"},
        {"starlink", "星链"}, {"oneweb", "一网"}, {"iridium-NEXT", "铱星二代"},
        {"qianfan", "千帆"}, {"hulianwang", "互联网低轨"}, {"kuiper", "柯伊伯"}, {"sar", "合成孔径雷达"},
        {"intelsat", "国际通信卫星"}, {"geo", "地球同步卫星"}, {"amateur", "业余无线电卫星"},
        {"gnss", "全球导航卫星"}, {"gps-ops", "GPS 运行组"}, {"glo-ops", "GLONASS 运行组"},
        {"galileo", "伽利略"}, {"beidou", "北斗"}, {"science", "科学卫星"},
        {"engineering", "技术试验卫星"}, {"education", "教育卫星"}, {"cubesat", "立方星"},
        {"radar", "雷达标定卫星"}, {"other", "其他卫星"}, {"last-30-days", "最近发射"}
    };
    for (const auto &[key, name] : groups) result.append(QVariantMap{{"key", QString::fromLatin1(key)}, {"name", QString::fromUtf8(name)}});
    result.append(QVariantMap{{"key", "local"}, {"name", QStringLiteral("本地轨道文件")}});
    return result;
}

QVariantMap SatelliteModel::groupInfo() const
{
    if (m_group == "catalog") return {{"description", QStringLiteral("各来源按卫星编号合并，空间站组合体合并显示")}};
    const auto source = m_groupSources.value(m_group);
    const bool operational = m_group == "gps-ops" || m_group == "glo-ops";
    return {{"description", operational ? QStringLiteral("运行组采用 CelesTrak 分组，实时导航健康状态以系统公告为准")
                                        : m_group == "local" ? QStringLiteral("所选本地轨道文件中的对象") : QStringLiteral("CelesTrak 所选分组中的对象")},
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
        connect(clock, &AppState::timeChanged, this, [this] {
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
            invalidate();
            if (previewActive()) { ++m_previewRevision; refreshPreview(); }
        });
        QTimer::singleShot(0, this, [this] {
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
        else setStatus(QStringLiteral("请选择轨道文件"));
    }
    emit catalogChanged();
}

void SatelliteModel::setStatus(const QString &value) { m_status = value; emit statusChanged(); }
void SatelliteModel::refresh()
{
    if (m_downloading || m_group == "local") return;
    const QString group = m_group == "catalog" ? QStringLiteral("active") : m_group;
    const QString setting = "catalog/lastAttempt/" + group;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const auto elapsed = now - m_settings.value(setting, 0).toLongLong();
    if (elapsed < 7200) {
        setStatus(QStringLiteral("下次可更新：%1").arg(QDateTime::fromSecsSinceEpoch(now + 7200 - elapsed).toString("HH:mm")));
        return;
    }
    m_settings.setValue(setting, now);
    QUrl url("https://celestrak.org/NORAD/elements/gp.php");
    QUrlQuery parameters; parameters.addQueryItem("GROUP", group); parameters.addQueryItem("FORMAT", "json"); url.setQuery(parameters);
    QNetworkRequest request(url); request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::UserAgentHeader, "ASTROCHRON/0.1 (satellite observation)");
    auto *reply = m_network.get(request);
    m_downloading = true;
    setStatus(QStringLiteral("正在获取轨道数据"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, group, url] {
        reply->deleteLater(); m_downloading = false;
        if (reply->error() != QNetworkReply::NoError) { setStatus(QStringLiteral("轨道数据获取失败：") + reply->errorString()); return; }
        const auto payload = reply->readAll();
        QString error; int skipped = 0;
        auto satellites = Orbit::parse(payload, error, skipped);
        if (satellites.isEmpty()) { setStatus(error.isEmpty() ? QStringLiteral("数据源当前没有目标") : error); return; }
        const auto count = satellites.size();
        if (!store(payload, url.toString(), group)) return;
        install(std::move(satellites), Source{group, url.toString(), m_snapshot});
        const auto summary = QStringLiteral("目录已获取 %1 条根数").arg(count);
        setStatus(summary + (skipped ? QStringLiteral("，%1 条根数格式异常").arg(skipped) : QString()));
    });
}

bool SatelliteModel::store(const QByteArray &payload, const QString &source, const QString &group)
{
    QSqlQuery query(m_database);
    query.prepare("INSERT INTO snapshots (group_key, acquired, source, payload) VALUES (?, ?, ?, ?)");
    query.addBindValue(group); query.addBindValue(QDateTime::currentSecsSinceEpoch()); query.addBindValue(source); query.addBindValue(payload);
    if (!query.exec()) { setStatus(QStringLiteral("轨道资料保存失败：") + query.lastError().text()); return false; }
    m_snapshot = query.lastInsertId().toLongLong(); m_source = source;
    return true;
}

void SatelliteModel::importFile(const QUrl &url)
{
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) { setStatus(QStringLiteral("轨道文件打开失败：") + file.errorString()); return; }
    const auto payload = file.readAll();
    QString error; int skipped = 0;
    auto satellites = Orbit::parse(payload, error, skipped);
    if (satellites.isEmpty()) { setStatus(error.isEmpty() ? QStringLiteral("文件内没有卫星根数") : error); return; }
    if (!store(payload, QFileInfo(file).fileName(), "local")) return;
    setGroup("local");
    setStatus(QStringLiteral("轨道文件已加入目录%1").arg(skipped ? QStringLiteral("，%1 条根数格式异常").arg(skipped) : QString()));
}

void SatelliteModel::exportSelected(const QUrl &url)
{
    const auto *satellite = selected(); if (!satellite) return;
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { setStatus(QStringLiteral("文件保存失败：") + file.errorString()); return; }
    QJsonArray elements;
    for (const int member : m_members.value(satellite->number)) elements.append(m_satellites[member].elements);
    const auto bytes = QJsonDocument(elements).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) { setStatus(QStringLiteral("轨道文件写入失败：") + file.errorString()); return; }
    setStatus(QStringLiteral("轨道根数已保存"));
}

QVariantList SatelliteModel::snapshots() const
{
    QVariantList result;
    if (!m_database.isOpen()) return result;
    QSqlQuery query(m_database);
    query.prepare("SELECT id, acquired, source FROM snapshots WHERE group_key = ? ORDER BY id DESC");
    query.addBindValue(m_sources.value(m_selected).group.isEmpty() ? m_group : m_sources.value(m_selected).group);
    if (query.exec()) while (query.next()) result.append(QVariantMap{{"id", query.value(0)},
        {"label", QDateTime::fromSecsSinceEpoch(query.value(1).toLongLong(), m_clock ? QTimeZone(m_clock->timeZone().toUtf8()) : QTimeZone::systemTimeZone()).toString("yyyy-MM-dd HH:mm:ss")}, {"source", query.value(2)}});
    return result;
}

void SatelliteModel::loadSnapshot(qint64 id)
{
    QSqlQuery query(m_database);
    query.prepare("SELECT payload, source, group_key FROM snapshots WHERE id = ?"); query.addBindValue(id);
    if (!query.exec() || !query.next()) return;
    QString error; int skipped = 0;
    auto satellites = Orbit::parse(query.value(0).toByteArray(), error, skipped);
    if (satellites.isEmpty()) { setStatus(error); return; }
    m_snapshot = id; m_source = query.value(1).toString();
    install(std::move(satellites), Source{query.value(2).toString(), m_source, id});
    setStatus(QStringLiteral("本地轨道数据已载入"));
    emit catalogChanged();
}

qint64 SatelliteModel::snapshotId() const { return m_sources.value(m_selected).snapshot; }
QString SatelliteModel::sourceText() const { return m_sources.value(m_selected).url; }

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
        const auto key = CatalogModel::constellation(m_satellites[index]);
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
    if (!previewActive() || !m_clock) return;
    if (m_previewBusy) { m_previewPending = true; return; }
    QVector<Orbit::Satellite> satellites;
    const auto members = m_groupMembers.value(m_previewGroup);
    for (const int index : m_targets)
        if (CatalogModel::constellation(m_satellites[index]) == m_previewGroup &&
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
        QVector<Orbit::Sun> suns;
        const int steps = mode == 1 ? 30 : 0;
        for (int step = 0; step <= steps; ++step) suns.append(Orbit::sunAt(time + step * 30));
        for (const auto &satellite : satellites) {
            std::optional<Orbit::State> previous;
            for (int step = 0; step <= steps; ++step) {
                const auto state = Orbit::propagate(satellite, time + step * 30, observer, suns[step]);
                if (!state) { previous.reset(); continue; }
                if (state->elevation >= observer.minimumElevation) { candidates.insert(satellite.number); break; }
                // Refine an intervening peak so short grazing passes can enter the preview.
                if (previous && previous->elevationRate > 0 && state->elevationRate < 0) {
                    double left = previous->time, right = state->time;
                    for (int iteration = 0; iteration < 12; ++iteration) {
                        const double a = left + (right - left) / 3, b = right - (right - left) / 3;
                        const auto first = Orbit::propagate(satellite, a, observer, Orbit::sunAt(a));
                        const auto second = Orbit::propagate(satellite, b, observer, Orbit::sunAt(b));
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
    if (!m_clock || m_satellites.isEmpty()) return;
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
    const double time = m_clock->unixTime(), reference = m_clock->referenceTime();
    const bool calculateGnss = std::abs(time - m_gnssTime) >= 1;
    QVector<Orbit::Satellite> gnss;
    if (calculateGnss) for (const int index : m_gnssTargets) gnss.append(m_satellites[index]);
    const Orbit::Observer observer{m_clock->observerLatitude(), m_clock->observerLongitude(), m_clock->ellipsoidHeight() / 1000.0, m_clock->minimumElevation()};
    const QTimeZone zone(m_clock->timeZone().toUtf8());
    const bool calculateTrack = m_needTrack;
    const bool hasHeight = m_clock->hasObserverHeight();
    const auto previewCandidates = m_previewCandidates;
    const int previewMode = m_previewMode;
    if (!std::isfinite(observer.heightKm)) { setStatus(QStringLiteral("大地水准面数据读取失败")); return; }
    m_busy = true; m_pending = false; m_needTrack = false;
    emit statusChanged();
    m_pool.start([this, satellites, gnss, calculateGnss, id, revision, time, reference, observer, zone, calculateTrack, hasHeight, watchIds, previewCandidates, previewMode] {
        QVariantList markers, trajectory, passes, shadowEvents;
        QVariantList gnssMarkers;
        QVariantMap observation;
        QHash<qint64, double> elevations;
        const auto sun = Orbit::sunAt(time);
        for (const auto &satellite : gnss) {
            if (const auto state = Orbit::propagate(satellite, time, observer, sun))
                gnssMarkers.append(QVariantMap{{"id", QString::number(satellite.number)}, {"latitude", state->latitude}, {"longitude", state->longitude}});
        }
        int previewCount = 0;
        for (const auto &satellite : satellites) {
            const auto state = Orbit::propagate(satellite, time, observer, sun);
            if (!state) continue;
            elevations.insert(satellite.number, state->elevation);
            auto marker = stateMap(*state);
            marker.insert("id", QString::number(satellite.number));
            const bool inPreview = previewCandidates.contains(satellite.number) && (previewMode != 0 || state->elevation >= observer.minimumElevation);
            if (inPreview) ++previewCount;
            if (watchIds.contains(satellite.number) || satellite.number == id || inPreview) markers.append(marker);
            if (satellite.number != id) continue;
            observation = marker;
            observation.insert("name", Orbit::displayName(satellite));
            observation.insert("originalName", satellite.name);
            observation.insert("lighting", Orbit::illuminationName(state->illumination));
            observation.insert("direction", Orbit::directionName(state->azimuth));
            observation.insert("motion", state->rangeRate < 0 ? QStringLiteral("正在接近") : QStringLiteral("正在远离"));
            observation.insert("epochAge", (time - satellite.epoch) / 86400.0);
            observation.insert("heightEstimated", !hasHeight);
            observation.insert("visibility", state->elevation < observer.minimumElevation ? QStringLiteral("低于最低高度角")
                : state->illumination != 0 ? QStringLiteral("位于地影") : state->sunElevation > -6 ? QStringLiteral("天空较亮") : QStringLiteral("具备光照观测条件"));
            if (calculateTrack) {
                const auto track = Orbit::track(satellite, reference - 43200, reference + 43200, observer);
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
                            const auto stateAt = Orbit::propagate(satellite, middle, observer, Orbit::sunAt(middle));
                            if (!stateAt) break;
                            if ((stateAt->illumination >= boundary) == before) left = middle; else right = middle;
                        }
                        const double eventTime = (left + right) / 2;
                        shadowEvents.append(QVariantMap{{"time", eventTime}, {"timeText", formatTime(eventTime, zone)}, {"entering", after}, {"umbra", boundary == 2},
                            {"name", (after ? QStringLiteral("进入") : QStringLiteral("离开")) + (boundary == 2 ? QStringLiteral("本影") : QStringLiteral("半影"))}});
                    }
                }
                std::sort(shadowEvents.begin(), shadowEvents.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value("time").toDouble() < b.toMap().value("time").toDouble(); });
                for (const auto &pass : track.passes) {
                    QStringList visible;
                    for (const auto &interval : pass.visibleIntervals)
                        visible.append(formatTime(interval[0], zone, "HH:mm:ss") + " - " + formatTime(interval[1], zone, "HH:mm:ss"));
                    passes.append(QVariantMap{{"start", pass.rise.time}, {"peak", pass.peak.time}, {"end", pass.set.time},
                        {"startClipped", pass.startsBeforeWindow}, {"endClipped", pass.endsAfterWindow},
                        {"startText", (pass.startsBeforeWindow ? QStringLiteral("早于 ") : QString()) + formatTime(pass.rise.time, zone)},
                        {"peakText", formatTime(pass.peak.time, zone)},
                        {"endText", (pass.endsAfterWindow ? QStringLiteral("晚于 ") : QString()) + formatTime(pass.set.time, zone)},
                        {"startAz", Orbit::directionName(pass.rise.azimuth) + " " + number(pass.rise.azimuth, 1) + QStringLiteral("°")},
                        {"endAz", Orbit::directionName(pass.set.azimuth) + " " + number(pass.set.azimuth, 1) + QStringLiteral("°")},
                        {"maximum", number(pass.peak.elevation, 1) + QStringLiteral("°")},
                        {"duration", number((pass.set.time - pass.rise.time) / 60, 1) + QStringLiteral(" min")},
                        {"range", number(pass.peak.range, 0) + QStringLiteral(" km")}, {"hasOptical", !visible.isEmpty()},
                        {"optical", visible.isEmpty() ? QStringLiteral("光照条件欠佳") : visible.join(" / ")}});
                }
            }
        }
        QMetaObject::invokeMethod(this, [this, revision, reference, calculateTrack, markers = std::move(markers), observation = std::move(observation),
            trajectory = std::move(trajectory), passes = std::move(passes), shadowEvents = std::move(shadowEvents), elevations = std::move(elevations), previewCount,
            gnssMarkers = std::move(gnssMarkers), calculateGnss, time]() mutable {
            m_busy = false;
            if (revision == m_revision) {
                m_markers = std::move(markers); m_observation = std::move(observation); m_elevations = std::move(elevations);
                m_previewCount = previewCount;
                if (calculateGnss) { m_gnssMarkers = std::move(gnssMarkers); m_gnssTime = time; emit gnssChanged(); }
                if (m_observation.contains("rangeRate"))
                    m_observation.insert("doppler", -m_observation.value("rangeRate").toDouble() / 299792.458 * m_frequency * 1e6);
                emit frameChanged();
                if (!m_rows.isEmpty()) emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {ElevationRole});
                if (calculateTrack) { m_trajectory = std::move(trajectory); m_passes = std::move(passes); m_shadowEvents = std::move(shadowEvents); m_trackReference = reference; emit trajectoryChanged(); }
            } else if (calculateTrack) m_needTrack = true;
            emit statusChanged();
            if (m_pending || revision != m_revision) requestFrame();
        }, Qt::QueuedConnection);
    });
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
        return e.contains(key) ? QString::number(e.value(key).toDouble(), 'f', precision) + unit : QStringLiteral("暂无数据");
    };
    const auto scientific = [&e](const char *key) { return e.contains(key) ? QString::number(e.value(key).toDouble(), 'e', 7) : QStringLiteral("暂无数据"); };
    QVariantList result;
    const auto add = [&result](const QString &label, const QString &value) { result.append(QVariantMap{{"label", label}, {"value", value}}); };
    add(QStringLiteral("卫星编号"), QString::number(satellite->number));
    add(QStringLiteral("国际编号"), satellite->internationalId.isEmpty() ? QStringLiteral("暂无数据") : satellite->internationalId);
    add(QStringLiteral("名称"), satellite->name);
    const auto constellation = CatalogModel::constellation(*satellite);
    if (constellation == "gps-ops" || constellation == "glo-ops" || constellation == "galileo" || constellation == "beidou") {
        add(QStringLiteral("PRN"), m_prns.value(satellite->number, QStringLiteral("暂无数据")));
        add(QStringLiteral("轨道面"), m_planes.value(satellite->number, QStringLiteral("暂无数据")));
    }
    add(QStringLiteral("历元（协调世界时）"), e.value("EPOCH").toString());
    add(QStringLiteral("轨道倾角"), field("INCLINATION", 4, QStringLiteral("°")));
    add(QStringLiteral("升交点赤经"), field("RA_OF_ASC_NODE", 4, QStringLiteral("°")));
    add(QStringLiteral("偏心率"), field("ECCENTRICITY", 7));
    add(QStringLiteral("近地点幅角"), field("ARG_OF_PERICENTER", 4, QStringLiteral("°")));
    add(QStringLiteral("平近点角"), field("MEAN_ANOMALY", 4, QStringLiteral("°")));
    add(QStringLiteral("每日绕地圈数"), field("MEAN_MOTION", 8, QStringLiteral(" rev/d")));
    add(QStringLiteral("轨道周期"), number(1440.0 / e.value("MEAN_MOTION").toDouble(), 3) + QStringLiteral(" min"));
    add(QStringLiteral("阻力项"), scientific("BSTAR") + QStringLiteral(" R_E^-1"));
    add(QStringLiteral("历元圈数"), field("REV_AT_EPOCH", 0));
    add(QStringLiteral("根数集编号"), field("ELEMENT_SET_NO", 0));
    add(QStringLiteral("平均运动一阶项"), scientific("MEAN_MOTION_DOT") + QStringLiteral(" rev/d²"));
    add(QStringLiteral("平均运动二阶项"), scientific("MEAN_MOTION_DDOT") + QStringLiteral(" rev/d³"));
    return result;
}
void SatelliteModel::copyDetails()
{
    QStringList lines;
    for (const auto &entry : orbitFields()) {
        const auto field = entry.toMap(); lines.append(field.value("label").toString() + ": " + field.value("value").toString());
    }
    QGuiApplication::clipboard()->setText(lines.join('\n'));
    setStatus(QStringLiteral("轨道参数已复制"));
}
