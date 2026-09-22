#include "satellite_model.h"

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
#include <cmath>

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
        {"range", state.range}, {"speed", state.speed}, {"rangeRate", state.rangeRate},
        {"sunElevation", state.sunElevation}, {"illumination", state.illumination}};
}
}

SatelliteModel::SatelliteModel(QObject *parent) : QAbstractListModel(parent)
{
    m_pool.setMaxThreadCount(1);
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
        {"stations", "空间站"}, {"visual", "明亮目标"}, {"active", "在轨活动卫星"},
        {"weather", "气象卫星"}, {"noaa", "美国气象卫星"}, {"goes", "地球静止气象卫星"},
        {"resource", "地球资源卫星"}, {"sarsat", "搜救卫星"}, {"dmc", "灾害监测卫星"},
        {"starlink", "星链"}, {"oneweb", "一网"}, {"iridium-NEXT", "铱星二代"},
        {"intelsat", "国际通信卫星"}, {"geo", "地球同步卫星"}, {"amateur", "业余无线电卫星"},
        {"gnss", "全球导航卫星"}, {"gps-ops", "全球定位系统"}, {"glo-ops", "格洛纳斯"},
        {"galileo", "伽利略"}, {"beidou", "北斗"}, {"science", "科学卫星"},
        {"engineering", "技术试验卫星"}, {"education", "教育卫星"}, {"cubesat", "立方星"},
        {"radar", "雷达标定卫星"}, {"other", "其他卫星"}, {"last-30-days", "最近发射"}
    };
    for (const auto &[key, name] : groups) result.append(QVariantMap{{"key", QString::fromLatin1(key)}, {"name", QString::fromUtf8(name)}});
    result.append(QVariantMap{{"key", "local"}, {"name", QStringLiteral("本地轨道文件")}});
    return result;
}

void SatelliteModel::setClock(AppState *clock)
{
    if (m_clock == clock) return;
    if (m_clock) disconnect(m_clock, nullptr, this, nullptr);
    m_clock = clock;
    if (clock) {
        connect(clock, &AppState::timeChanged, this, [this] {
            if (std::abs(m_clock->unixTime() - m_lastTime) > 3) ++m_revision;
            m_lastTime = m_clock->unixTime();
            if (std::abs(m_clock->referenceTime() - m_trackReference) >= 60) m_needTrack = true;
            requestFrame();
        });
        connect(clock, &AppState::observerChanged, this, &SatelliteModel::invalidate);
        QTimer::singleShot(0, this, [this] { setGroup(m_settings.value("catalog/group", "stations").toString()); });
    }
    emit clockChanged();
}

void SatelliteModel::setSearch(const QString &value) { if (m_search == value) return; m_search = value; filter(); emit catalogChanged(); }
void SatelliteModel::filter()
{
    beginResetModel();
    m_rows.clear();
    const auto search = m_search.trimmed();
    for (int i = 0; i < m_satellites.size(); ++i) {
        const auto &satellite = m_satellites[i];
        if (search.isEmpty() || satellite.name.contains(search, Qt::CaseInsensitive) || Orbit::displayName(satellite).contains(search) ||
            QString::number(satellite.number).contains(search) || satellite.internationalId.contains(search, Qt::CaseInsensitive)) m_rows.append(i);
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
    QSqlQuery query(m_database);
    query.prepare("SELECT id FROM snapshots WHERE group_key = ? ORDER BY id DESC LIMIT 1");
    query.addBindValue(group);
    if (query.exec() && query.next()) loadSnapshot(query.value(0).toLongLong());
    else {
        m_source.clear(); m_snapshot = 0;
        install({});
        if (group != "local") refresh();
        else setStatus(QStringLiteral("请选择轨道文件"));
    }
    emit catalogChanged();
}

void SatelliteModel::setStatus(const QString &value) { m_status = value; emit statusChanged(); }
void SatelliteModel::refresh()
{
    if (m_downloading || m_group == "local") return;
    const QString group = m_group;
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
        if (group == m_group) {
            install(std::move(satellites));
            emit catalogChanged();
        }
        setStatus(skipped ? QStringLiteral("已获取 %1 个目标，%2 条根数格式异常").arg(count).arg(skipped) : QStringLiteral("已获取 %1 个目标").arg(count));
    });
}

bool SatelliteModel::store(const QByteArray &payload, const QString &source, const QString &group)
{
    QSqlQuery query(m_database);
    query.prepare("INSERT INTO snapshots (group_key, acquired, source, payload) VALUES (?, ?, ?, ?)");
    query.addBindValue(group); query.addBindValue(QDateTime::currentSecsSinceEpoch()); query.addBindValue(source); query.addBindValue(payload);
    if (!query.exec()) { setStatus(QStringLiteral("轨道资料保存失败：") + query.lastError().text()); return false; }
    if (group == m_group) { m_snapshot = query.lastInsertId().toLongLong(); m_source = source; }
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
    setStatus(QStringLiteral("已导入 %1 个目标%2").arg(m_satellites.size()).arg(skipped ? QStringLiteral("，%1 条根数格式异常").arg(skipped) : QString()));
}

void SatelliteModel::exportSelected(const QUrl &url)
{
    const auto *satellite = selected(); if (!satellite) return;
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { setStatus(QStringLiteral("文件保存失败：") + file.errorString()); return; }
    const auto bytes = QJsonDocument(QJsonArray{satellite->elements}).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) { setStatus(QStringLiteral("轨道文件写入失败：") + file.errorString()); return; }
    setStatus(QStringLiteral("轨道根数已保存"));
}

QVariantList SatelliteModel::snapshots() const
{
    QVariantList result;
    QSqlQuery query(m_database);
    query.prepare("SELECT id, acquired, source FROM snapshots WHERE group_key = ? ORDER BY id DESC");
    query.addBindValue(m_group);
    if (query.exec()) while (query.next()) result.append(QVariantMap{{"id", query.value(0)},
        {"label", QDateTime::fromSecsSinceEpoch(query.value(1).toLongLong(), m_clock ? QTimeZone(m_clock->timeZone().toUtf8()) : QTimeZone::systemTimeZone()).toString("yyyy-MM-dd HH:mm:ss")}, {"source", query.value(2)}});
    return result;
}

void SatelliteModel::loadSnapshot(qint64 id)
{
    QSqlQuery query(m_database);
    query.prepare("SELECT payload, source FROM snapshots WHERE id = ? AND group_key = ?"); query.addBindValue(id); query.addBindValue(m_group);
    if (!query.exec() || !query.next()) return;
    QString error; int skipped = 0;
    auto satellites = Orbit::parse(query.value(0).toByteArray(), error, skipped);
    if (satellites.isEmpty()) { setStatus(error); return; }
    m_snapshot = id; m_source = query.value(1).toString();
    install(std::move(satellites));
    setStatus(QStringLiteral("已载入 %1 个目标").arg(m_satellites.size()));
    emit catalogChanged();
}

void SatelliteModel::install(QVector<Orbit::Satellite> satellites)
{
    beginResetModel(); m_rows.clear(); m_satellites = std::move(satellites); endResetModel();
    m_elevations.clear();
    if (!selected()) m_selected = m_satellites.isEmpty() ? 0 : m_satellites[0].number;
    for (const auto &satellite : m_satellites) if (satellite.number == 25544 && m_selected == m_satellites[0].number) { m_selected = 25544; break; }
    filter(); invalidate(); emit selectionChanged(); emit catalogChanged();
}
const Orbit::Satellite *SatelliteModel::selected() const
{
    for (const auto &satellite : m_satellites) if (satellite.number == m_selected) return &satellite;
    return nullptr;
}
void SatelliteModel::select(const QString &id)
{
    const auto value = id.toLongLong(); if (value == m_selected) return;
    bool exists = false; for (const auto &satellite : m_satellites) if (satellite.number == value) exists = true;
    if (!exists) return;
    m_selected = value; invalidate(); emit selectionChanged();
}
void SatelliteModel::invalidate()
{
    ++m_revision; m_needTrack = true;
    m_observation.clear(); m_trajectory.clear(); m_passes.clear(); m_markers.clear();
    emit frameChanged(); emit trajectoryChanged(); requestFrame();
}

void SatelliteModel::setReceiveFrequency(double value)
{
    if (!std::isfinite(value) || value < 0 || value > 1000000) return;
    m_frequency = value; m_settings.setValue("radio/frequencyMHz", value);
    requestFrame();
}

void SatelliteModel::requestFrame()
{
    if (!m_clock || m_satellites.isEmpty()) return;
    if (m_busy) { m_pending = true; return; }
    const auto satellites = m_satellites;
    const auto id = m_selected;
    const auto revision = m_revision;
    const double time = m_clock->unixTime(), reference = m_clock->referenceTime(), frequency = m_frequency;
    const Orbit::Observer observer{m_clock->observerLatitude(), m_clock->observerLongitude(), m_clock->ellipsoidHeight() / 1000.0, m_clock->minimumElevation()};
    const QTimeZone zone(m_clock->timeZone().toUtf8());
    const bool calculateTrack = m_needTrack;
    const bool hasHeight = m_clock->hasObserverHeight();
    if (!std::isfinite(observer.heightKm)) { setStatus(QStringLiteral("大地水准面数据读取失败")); return; }
    m_busy = true; m_pending = false; m_needTrack = false;
    emit statusChanged();
    m_pool.start([this, satellites, id, revision, time, reference, frequency, observer, zone, calculateTrack, hasHeight] {
        QVariantList markers, trajectory, passes;
        QVariantMap observation;
        QHash<qint64, double> elevations;
        const auto sun = Orbit::sunAt(time);
        for (const auto &satellite : satellites) {
            const auto state = Orbit::propagate(satellite, time, observer, sun);
            if (!state) continue;
            elevations.insert(satellite.number, state->elevation);
            auto marker = stateMap(*state);
            marker.insert("id", QString::number(satellite.number));
            markers.append(marker);
            if (satellite.number != id) continue;
            observation = marker;
            observation.insert("name", Orbit::displayName(satellite));
            observation.insert("originalName", satellite.name);
            observation.insert("lighting", Orbit::illuminationName(state->illumination));
            observation.insert("direction", Orbit::directionName(state->azimuth));
            observation.insert("motion", state->rangeRate < 0 ? QStringLiteral("正在接近") : QStringLiteral("正在远离"));
            observation.insert("epochAge", (time - satellite.epoch) / 86400.0);
            observation.insert("doppler", -state->rangeRate / 299792.458 * frequency * 1e6);
            observation.insert("heightEstimated", !hasHeight);
            observation.insert("visibility", state->elevation < observer.minimumElevation ? QStringLiteral("低于最低高度角")
                : state->illumination != 0 ? QStringLiteral("位于地影") : state->sunElevation > -6 ? QStringLiteral("天空较亮") : QStringLiteral("具备光照观测条件"));
            if (calculateTrack) {
                const auto track = Orbit::track(satellite, reference - 43200, reference + 43200, observer);
                for (const auto &sample : track.samples) trajectory.append(stateMap(sample));
                for (const auto &pass : track.passes) {
                    QStringList visible;
                    for (const auto &interval : pass.visibleIntervals)
                        visible.append(formatTime(interval[0], zone, "HH:mm:ss") + " - " + formatTime(interval[1], zone, "HH:mm:ss"));
                    passes.append(QVariantMap{{"start", pass.rise.time}, {"peak", pass.peak.time}, {"end", pass.set.time},
                        {"startText", (pass.startsBeforeWindow ? QStringLiteral("早于 ") : QString()) + formatTime(pass.rise.time, zone)},
                        {"peakText", formatTime(pass.peak.time, zone)},
                        {"endText", (pass.endsAfterWindow ? QStringLiteral("晚于 ") : QString()) + formatTime(pass.set.time, zone)},
                        {"startAz", Orbit::directionName(pass.rise.azimuth) + " " + number(pass.rise.azimuth, 1) + QStringLiteral("°")},
                        {"endAz", Orbit::directionName(pass.set.azimuth) + " " + number(pass.set.azimuth, 1) + QStringLiteral("°")},
                        {"maximum", number(pass.peak.elevation, 1) + QStringLiteral("°")},
                        {"duration", number((pass.set.time - pass.rise.time) / 60, 1) + QStringLiteral(" 分")},
                        {"range", number(pass.peak.range, 0) + QStringLiteral(" 千米")},
                        {"optical", visible.isEmpty() ? QStringLiteral("光照条件欠佳") : visible.join(" / ")}});
                }
            }
        }
        QMetaObject::invokeMethod(this, [this, revision, reference, calculateTrack, markers = std::move(markers), observation = std::move(observation),
            trajectory = std::move(trajectory), passes = std::move(passes), elevations = std::move(elevations)]() mutable {
            m_busy = false;
            if (revision == m_revision) {
                m_markers = std::move(markers); m_observation = std::move(observation); m_elevations = std::move(elevations);
                emit frameChanged();
                if (!m_rows.isEmpty()) emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {ElevationRole});
                if (calculateTrack) { m_trajectory = std::move(trajectory); m_passes = std::move(passes); m_trackReference = reference; emit trajectoryChanged(); }
            } else if (calculateTrack) m_needTrack = true;
            emit statusChanged();
            if (m_pending || revision != m_revision) requestFrame();
        }, Qt::QueuedConnection);
    });
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
    add(QStringLiteral("历元（协调世界时）"), e.value("EPOCH").toString());
    add(QStringLiteral("轨道倾角"), field("INCLINATION", 4, QStringLiteral("°")));
    add(QStringLiteral("升交点赤经"), field("RA_OF_ASC_NODE", 4, QStringLiteral("°")));
    add(QStringLiteral("偏心率"), field("ECCENTRICITY", 7));
    add(QStringLiteral("近地点幅角"), field("ARG_OF_PERICENTER", 4, QStringLiteral("°")));
    add(QStringLiteral("平近点角"), field("MEAN_ANOMALY", 4, QStringLiteral("°")));
    add(QStringLiteral("每日绕地圈数"), field("MEAN_MOTION", 8, QStringLiteral(" 圈/日")));
    add(QStringLiteral("轨道周期"), number(1440.0 / e.value("MEAN_MOTION").toDouble(), 3) + QStringLiteral(" 分"));
    add(QStringLiteral("阻力项"), scientific("BSTAR") + QStringLiteral(" /地球半径"));
    add(QStringLiteral("历元圈数"), field("REV_AT_EPOCH", 0));
    add(QStringLiteral("根数集编号"), field("ELEMENT_SET_NO", 0));
    add(QStringLiteral("平均运动一阶项"), scientific("MEAN_MOTION_DOT") + QStringLiteral(" 圈/日²"));
    add(QStringLiteral("平均运动二阶项"), scientific("MEAN_MOTION_DDOT") + QStringLiteral(" 圈/日³"));
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
