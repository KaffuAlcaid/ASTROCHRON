#include "satellite_controller.h"
#include "orbital_elements_parser.h"
#include "orbit_propagator.h"
#include "satellite_labels.h"
#include "satellite_photometry.h"
#include "observation_plan_exporter.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QSaveFile>
#include <cmath>
#include <algorithm>

namespace {
QString formatTime(double seconds, const QTimeZone &zone, const char *format = "MM-dd HH:mm:ss")
{
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(seconds * 1000), zone)
        .toString(QString::fromLatin1(format));
}
QString number(double value, int decimals = 2)
{
    return QString::number(value, 'f', decimals);
}
QVariantMap stateMap(const Orbit::State &state)
{
    return {{"time", state.time},
            {"latitude", state.latitude},
            {"longitude", state.longitude},
            {"altitude", state.altitude},
            {"azimuth", state.azimuth},
            {"elevation", state.elevation},
            {"range", state.range},
            {"speed", state.speed},
            {"rangeRate", state.rangeRate},
            {"elevationRate", state.elevationRate},
            {"sunElevation", state.sunElevation},
            {"illumination", state.illumination},
            {"phaseAngle", state.phaseAngle}};
}

QString databasePath()
{
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(directory);
    return directory + "/orbits.sqlite";
}

} // namespace

SatelliteController::SatelliteController(QObject *parent)
    : QObject(parent), m_database(databasePath()), m_snapshots(m_database.connection()), m_watchlist(m_catalog),
      m_photometry(m_database.connection())
{
    connect(this, &SatelliteController::selectionChanged, this, &SatelliteController::detailsChanged);
    connect(this, &SatelliteController::catalogChanged, this, &SatelliteController::localizedChanged);
    connect(this, &SatelliteController::selectionChanged, this, [this] {
        m_photometry.resetStatus();
        emit photometryChanged();
    });
    m_pool.setMaxThreadCount(2);
    connect(&m_watchlist, &WatchlistModel::changed, this, &SatelliteController::watchlistChanged);
    m_watchTimer.setSingleShot(true);
    m_watchTimer.setInterval(100);
    connect(&m_watchTimer, &QTimer::timeout, this, &SatelliteController::calculateWatchPredictions);
    m_frequency = m_settings.value("radio/frequencyMHz", 145.8).toDouble();
    std::function<QString()> error;
    if (!m_database.open(error)) {
        setStatus(std::move(error));
        return;
    }
    m_snapshotRetention = m_settings.value("data/snapshotRetention", 10).toInt() == 5 ? 5 : 10;
    m_autoCleanup = m_settings.value("data/autoCleanup", false).toBool();
    m_photometry.load();
}

SatelliteController::~SatelliteController()
{
    if (m_watchCancel)
        m_watchCancel->store(true);
    m_pool.clear();
    m_pool.waitForDone();
}

QVariantMap SatelliteController::storageInfo() const
{
    if (!m_database.isOpen())
        return {};
    const auto info = m_snapshots.storageInfo();
    return {{"count", info.count},
            {"pinned", info.pinned},
            {"imported", info.imported},
            {"bytes", info.bytes},
            {"freeBytes", info.freeBytes}};
}

QVariantList SatelliteController::cleanupCandidates() const
{
    QVariantList result;
    if (!m_database.isOpen())
        return result;
    QHash<QString, QString> names;
    for (const auto &value : groups()) {
        const auto group = value.toMap();
        names.insert(group.value("key").toString(), group.value("name").toString());
    }
    const auto zone = m_clock ? QTimeZone(m_clock->timeZone().toUtf8()) : QTimeZone::systemTimeZone();
    for (const auto &snapshot : m_snapshots.cleanupCandidates(m_snapshotRetention, m_catalog.usedSnapshots())) {
        const auto &source = snapshot.source;
        result.append(QVariantMap{{"id", source.snapshot},
                                  {"group", names.value(source.group, source.group)},
                                  {"source", source.url},
                                  {"acquired", formatTime(source.acquired, zone, "yyyy-MM-dd HH:mm:ss")},
                                  {"size", number(snapshot.bytes / 1024.0, 1) + " KiB"}});
    }
    return result;
}

bool SatelliteController::snapshotPinned() const
{
    return m_database.isOpen() && m_snapshots.isPinned(snapshotId());
}

bool SatelliteController::snapshotImported() const
{
    return m_catalog.source(m_selected).group == "local";
}

void SatelliteController::pinSnapshot(qint64 id, bool pinned)
{
    if (m_storageBusy || id <= 0)
        return;
    QString error;
    if (!m_snapshots.setPinned(id, pinned, error))
        m_storageStatus = [error] { return tr("快照保存失败：%1").arg(error); };
    else
        m_storageStatus = {};
    emit sourceChanged();
    emit storageChanged();
}

void SatelliteController::setSnapshotRetention(int count)
{
    count = count == 5 ? 5 : 10;
    if (m_snapshotRetention == count)
        return;
    m_snapshotRetention = count;
    m_settings.setValue("data/snapshotRetention", count);
    emit storageChanged();
}

bool SatelliteController::pruneSnapshots()
{
    if (!m_database.isOpen() || m_storageBusy)
        return false;
    const auto candidates = m_snapshots.cleanupCandidates(m_snapshotRetention, m_catalog.usedSnapshots());
    if (candidates.isEmpty())
        return true;
    QString error;
    const bool ok = m_snapshots.remove(candidates, error);
    if (!ok)
        m_storageStatus = [error] { return tr("历史资料清理失败：%1").arg(error); };
    else {
        m_storageStatus = [count = candidates.size()] { return tr("已清理 %1 份在线历史快照").arg(count); };
        compactDatabase(false);
    }
    emit sourceChanged();
    emit storageChanged();
    return ok;
}

bool SatelliteController::enableCleanup()
{
    if (!pruneSnapshots())
        return false;
    m_autoCleanup = true;
    m_settings.setValue("data/autoCleanup", true);
    emit storageChanged();
    return true;
}

void SatelliteController::disableCleanup()
{
    m_autoCleanup = false;
    m_settings.setValue("data/autoCleanup", false);
    emit storageChanged();
}

void SatelliteController::compactDatabase(bool full)
{
    if (m_storageBusy || !m_database.isOpen())
        return;
    if (m_downloading) {
        if (full) {
            m_storageStatus = [] { return tr("轨道下载完成后可整理数据库"); };
            emit storageChanged();
        }
        return;
    }
    if (!full && !m_snapshots.needsCompaction())
        return;
    m_storageBusy = true;
    m_storageStatus = [] { return tr("正在整理数据库"); };
    emit storageChanged();
    m_pool.start(
        [this, path = m_database.path(), full] {
            const auto error = SnapshotRepository::compact(path, full);
            QMetaObject::invokeMethod(
                this,
                [this, error] {
                    m_storageBusy = false;
                    m_storageStatus =
                        error.isEmpty()
                            ? std::function<QString()>([] { return tr("数据库已整理"); })
                            : std::function<QString()>([error] { return tr("数据库整理失败：%1").arg(error); });
                    emit storageChanged();
                },
                Qt::QueuedConnection);
        },
        -1);
}

QVariantList SatelliteController::groups() const
{
    QVariantList result;
    const std::pair<const char *, const char *> groups[] = {{"catalog", QT_TR_NOOP("本地完整目录")},
                                                            {"active", QT_TR_NOOP("活动卫星（CelesTrak）")},
                                                            {"stations", QT_TR_NOOP("空间站")},
                                                            {"visual", QT_TR_NOOP("明亮目标")},
                                                            {"weather", QT_TR_NOOP("气象卫星")},
                                                            {"noaa", QT_TR_NOOP("美国气象卫星")},
                                                            {"goes", QT_TR_NOOP("地球静止气象卫星")},
                                                            {"resource", QT_TR_NOOP("地球资源卫星")},
                                                            {"sarsat", QT_TR_NOOP("搜救卫星")},
                                                            {"dmc", QT_TR_NOOP("灾害监测卫星")},
                                                            {"starlink", QT_TR_NOOP("星链")},
                                                            {"oneweb", QT_TR_NOOP("一网")},
                                                            {"iridium-NEXT", QT_TR_NOOP("铱星二代")},
                                                            {"qianfan", QT_TR_NOOP("千帆")},
                                                            {"hulianwang", QT_TR_NOOP("互联网低轨")},
                                                            {"kuiper", QT_TR_NOOP("柯伊伯")},
                                                            {"sar", QT_TR_NOOP("合成孔径雷达")},
                                                            {"intelsat", QT_TR_NOOP("国际通信卫星")},
                                                            {"geo", QT_TR_NOOP("地球同步卫星")},
                                                            {"amateur", QT_TR_NOOP("业余无线电卫星")},
                                                            {"gnss", QT_TR_NOOP("全球导航卫星")},
                                                            {"gps-ops", QT_TR_NOOP("GPS 运行组")},
                                                            {"glo-ops", QT_TR_NOOP("GLONASS 运行组")},
                                                            {"galileo", QT_TR_NOOP("伽利略")},
                                                            {"beidou", QT_TR_NOOP("北斗")},
                                                            {"science", QT_TR_NOOP("科学卫星")},
                                                            {"engineering", QT_TR_NOOP("技术试验卫星")},
                                                            {"education", QT_TR_NOOP("教育卫星")},
                                                            {"cubesat", QT_TR_NOOP("立方星")},
                                                            {"radar", QT_TR_NOOP("雷达标定卫星")},
                                                            {"other", QT_TR_NOOP("其他卫星")},
                                                            {"last-30-days", QT_TR_NOOP("最近发射")}};
    for (const auto &[key, name] : groups)
        result.append(QVariantMap{{"key", QString::fromLatin1(key)}, {"name", tr(name)}});
    result.append(QVariantMap{{"key", "local"}, {"name", tr("本地轨道文件")}});
    return result;
}

bool SatelliteController::usingLocalConstellation() const
{
    return !m_catalog.hasGroup(m_group) && SatelliteCatalog::isNavigationGroup(m_group);
}

QVariantMap SatelliteController::groupInfo() const
{
    if (m_group == "catalog")
        return {{"description", tr("各来源按卫星编号合并，空间站组合体合并显示")}};
    const auto source = m_catalog.groupSource(m_group);
    const bool operational = m_group == "gps-ops" || m_group == "glo-ops";
    return {{"description", usingLocalConstellation()
                                ? tr("当前显示：本地目录中的%1对象").arg(SatelliteCatalog::constellationName(m_group))
                            : operational        ? tr("运行组采用 CelesTrak 分组，实时导航健康状态以系统公告为准")
                            : m_group == "local" ? tr("所选本地轨道文件中的对象")
                                                 : tr("CelesTrak 所选分组中的对象")},
            {"localMembers", usingLocalConstellation()},
            {"acquired",
             source.acquired
                 ? QDateTime::fromSecsSinceEpoch(source.acquired, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm:ss 'UTC'")
                 : QString()},
            {"source", source.url},
            {"loaded", m_catalog.hasGroup(m_group)},
            {"count", static_cast<int>(m_catalog.groupMembers(m_group).size())}};
}

void SatelliteController::setClock(AppState *clock)
{
    if (m_clock == clock)
        return;
    if (m_clock)
        disconnect(m_clock, nullptr, this, nullptr);
    m_clock = clock;
    if (clock) {
        const auto reschedule = [this] {
            m_calculationTime = 0;
            m_gnssTime = 0;
            requestFrame();
        };
        connect(clock, &AppState::calculationFrequencyChanged, this, reschedule);
        connect(clock, &AppState::updateFrequencyChanged, this, reschedule);
        connect(clock, &AppState::localizedChanged, this, [this] {
            updateWatchRows();
            emit planChanged();
            emit storageChanged();
            emit localizedChanged();
            emit detailsChanged();
            emit frameChanged();
            emit trajectoryChanged();
            emit previewChanged();
            emit sourceChanged();
            emit statusChanged();
            emit photometryChanged();
            m_watchlist.retranslate();
        });
        connect(clock, &AppState::timeChanged, this, [this] {
            const double watchTime = m_clock->unixTime();
            if (static_cast<qint64>(std::floor(watchTime / 86400)) != m_watchRequestedDay)
                requestWatchPredictions();
            if (watchTime >= m_watchNextBoundary || watchTime < m_watchClockTime || watchTime - m_watchClockTime > 3)
                updateWatchRows();
            m_watchClockTime = watchTime;
            if (std::abs(m_clock->unixTime() - m_lastTime) > 3) {
                ++m_revision;
                if (previewActive()) {
                    ++m_previewRevision;
                    m_previewTime = 0;
                }
            }
            m_lastTime = m_clock->unixTime();
            if (std::abs(m_clock->referenceTime() - m_trackReference) >= 60)
                m_needTrack = true;
            if (previewActive() && !m_previewBusy &&
                std::abs(m_clock->unixTime() - m_previewTime) >= (m_previewMode == 1 ? 60 : 10))
                refreshPreview();
            requestFrame();
        });
        m_watchlist.setHasObserver(clock->hasObserver());
        connect(clock, &AppState::observerChanged, this, [this] {
            m_watchlist.setHasObserver(m_clock->hasObserver());
            requestWatchPredictions();
            invalidate();
            if (previewActive()) {
                ++m_previewRevision;
                refreshPreview();
            }
        });
        QTimer::singleShot(0, this, [this] {
            if (!m_database.isOpen())
                return;
            for (const auto id : m_snapshots.latestPerGroup())
                loadSnapshot(id);
            if (!m_catalog.hasGroup("active"))
                setGroup("active");
        });
    }
    emit clockChanged();
}

void SatelliteController::requestWatchPredictions()
{
    if (!m_clock)
        return;
    m_watchRequestedDay = static_cast<qint64>(std::floor(m_clock->unixTime() / 86400));
    ++m_watchGeneration;
    if (m_watchCancel)
        m_watchCancel->store(true);
    m_watchTimer.start();
}

void SatelliteController::calculateWatchPredictions()
{
    if (m_watchBusy || !m_clock || !m_clock->hasObserver())
        return;
    const auto day = static_cast<qint64>(std::floor(m_clock->unixTime() / 86400));
    const Orbit::Observer observer{m_clock->observerLatitude(), m_clock->observerLongitude(),
                                   m_clock->ellipsoidHeight() / 1000, m_clock->minimumElevation()};
    if (!std::isfinite(observer.heightKm))
        return;
    if (day != m_watchDay || observer.latitude != m_watchObserver.latitude ||
        observer.longitude != m_watchObserver.longitude || observer.heightKm != m_watchObserver.heightKm ||
        observer.minimumElevation != m_watchObserver.minimumElevation)
        m_watchPredictions.clear();
    m_watchDay = day;
    m_watchObserver = observer;
    QVector<Orbit::Satellite> pending;
    QSet<qint64> ids;
    for (const auto &key : m_watchlist.ids()) {
        const auto id = m_catalog.ownerOf(key.toLongLong());
        if (ids.contains(id) || !m_catalog.find(id))
            continue;
        ids.insert(id);
        const auto &satellite = *m_catalog.find(id);
        const auto existing = m_watchPredictions.constFind(id);
        if (existing == m_watchPredictions.cend() || !Orbit::sameElements(satellite, existing->satellite)) {
            m_watchPredictions.remove(id);
            pending.append(satellite);
        }
    }
    for (auto it = m_watchPredictions.begin(); it != m_watchPredictions.end();)
        if (!ids.contains(it.key()))
            it = m_watchPredictions.erase(it);
        else
            ++it;
    updateWatchRows();
    if (pending.isEmpty())
        return;
    m_watchBusy = true;
    m_watchCancel = std::make_shared<std::atomic_bool>(false);
    const auto generation = m_watchGeneration;
    m_pool.start(
        [this, pending, observer, day, generation, cancel = m_watchCancel] {
            QHash<qint64, WatchPrediction> results;
            // The previous day supplies rise times for passes already in progress.
            for (const auto &satellite : pending) {
                if (cancel->load())
                    break;
                results.insert(satellite.number, {satellite, Orbit::predictPasses(satellite, (day - 1) * 86400.0,
                                                                                  (day + 2) * 86400.0, observer)});
            }
            QMetaObject::invokeMethod(
                this,
                [this, generation, results = std::move(results)]() mutable {
                    m_watchBusy = false;
                    if (generation == m_watchGeneration) {
                        for (auto it = results.begin(); it != results.end(); ++it)
                            m_watchPredictions.insert(it.key(), std::move(it.value()));
                        updateWatchRows();
                    } else
                        m_watchTimer.start();
                },
                Qt::QueuedConnection);
        },
        -1);
}

void SatelliteController::updateWatchRows()
{
    if (!m_clock)
        return;
    const double time = m_clock->unixTime();
    const QTimeZone zone(m_clock->timeZone().toUtf8());
    const auto date = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(time), zone).date();
    QHash<qint64, QString> summaries;
    QHash<qint64, double> times;
    m_watchNextBoundary = date.addDays(1).startOfDay(zone).toSecsSinceEpoch();
    for (const auto &key : m_watchlist.ids()) {
        const auto id = m_catalog.ownerOf(key.toLongLong());
        const auto forecast = m_watchPredictions.constFind(id);
        if (forecast == m_watchPredictions.cend())
            continue;
        QString text = tr("24 h 内暂无过境");
        for (const auto &pass : forecast->passes) {
            for (double change : {pass.rise.time - 86400, pass.rise.time, pass.set.time})
                if (change > time)
                    m_watchNextBoundary = std::min(m_watchNextBoundary, change);
            if (pass.set.time <= time || pass.rise.time >= time + 86400)
                continue;
            times.insert(id, pass.rise.time <= time ? 0 : pass.rise.time);
            if (pass.startsBeforeWindow || pass.endsAfterWindow)
                text = tr("持续高于最低高度角");
            else {
                const auto rise = QDateTime::fromSecsSinceEpoch(qRound64(pass.rise.time), zone);
                text = pass.rise.time <= time
                           ? tr("正在过境")
                           : tr("下一次 %1").arg(rise.toString(rise.date() == date ? "HH:mm" : "MM-dd HH:mm"));
                text += tr(" · 峰值 %1°").arg(number(pass.peak.elevation, 1));
            }
            const bool optical =
                std::any_of(pass.visibleIntervals.begin(), pass.visibleIntervals.end(),
                            [time](const auto &interval) { return interval[1] > time && interval[0] < time + 86400; });
            if (optical)
                text += tr(" · 光学");
            for (const auto &interval : pass.visibleIntervals)
                for (double change : {interval[0] - 86400, interval[1]})
                    if (change > time)
                        m_watchNextBoundary = std::min(m_watchNextBoundary, change);
            break;
        }
        summaries.insert(id, text);
    }
    m_watchlist.setPredictions(std::move(summaries), std::move(times));
}

void SatelliteController::setGroup(const QString &group)
{
    bool known = false;
    for (const auto &entry : groups())
        if (entry.toMap().value("key").toString() == group)
            known = true;
    if (!known)
        return;
    m_group = group;
    m_settings.setValue("catalog/group", group);
    if (group == "catalog") {
        emit catalogChanged();
        return;
    }
    const auto snapshot = m_snapshots.latestId(group);
    if (snapshot)
        loadSnapshot(snapshot);
    else {
        if (group != "local")
            refresh();
        else
            setStatus([] { return tr("请选择轨道文件"); });
    }
    emit catalogChanged();
}

void SatelliteController::setStatus(std::function<QString()> value)
{
    m_status = std::move(value);
    emit statusChanged();
}
void SatelliteController::refresh()
{
    if (m_downloading || m_group == "local")
        return;
    const QString group = m_group == "catalog" ? QStringLiteral("active") : m_group;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 acquired = m_snapshots.latestAcquired(group);
    const qint64 nextRequest = std::max(acquired ? acquired + 7200 : 0, m_retryAfter.value(group));
    if (now < nextRequest) {
        setStatus([nextRequest] {
            return tr("下次可更新：%1").arg(QDateTime::fromSecsSinceEpoch(nextRequest).toString("HH:mm:ss"));
        });
        return;
    }
    QUrl url("https://celestrak.org/NORAD/elements/gp.php");
    QUrlQuery parameters;
    parameters.addQueryItem("GROUP", group);
    parameters.addQueryItem("FORMAT", "json");
    url.setQuery(parameters);
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::UserAgentHeader, "ASTROCHRON/0.1 (satellite observation)");
    auto *reply = m_network.get(request);
    m_downloading = true;
    setStatus([] { return tr("正在获取轨道数据"); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, group, url] {
        reply->deleteLater();
        m_downloading = false;
        const auto finishedAt = QDateTime::currentSecsSinceEpoch();
        m_retryAfter[group] = finishedAt + 60;
        const auto retryHeader = reply->rawHeader("Retry-After");
        if (!retryHeader.isEmpty()) {
            bool seconds = false;
            const auto delay = retryHeader.toLongLong(&seconds);
            const auto retryTime =
                seconds ? finishedAt + std::max<qint64>(delay, 0)
                        : QDateTime::fromString(QString::fromLatin1(retryHeader), Qt::RFC2822Date).toSecsSinceEpoch();
            m_retryAfter[group] = std::max(m_retryAfter.value(group), retryTime);
        }
        if (reply->error() != QNetworkReply::NoError) {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (httpStatus >= 500)
                setStatus([httpStatus] {
                    return tr("CelesTrak 服务暂时异常（HTTP %1），本地轨道资料仍可使用").arg(httpStatus);
                });
            else if (httpStatus > 0)
                setStatus(
                    [httpStatus] { return tr("CelesTrak 请求失败（HTTP %1），本地轨道资料仍可使用").arg(httpStatus); });
            else
                setStatus([error = static_cast<int>(reply->error())] {
                    return tr("轨道数据获取失败：网络连接异常（错误 %1），本地轨道资料仍可使用").arg(error);
                });
            return;
        }
        const auto payload = reply->readAll();
        QString error;
        int skipped = 0;
        auto satellites = Orbit::parse(payload, error, skipped);
        if (satellites.isEmpty()) {
            setStatus([error] { return error.isEmpty() ? tr("数据源当前没有目标") : error; });
            return;
        }
        const auto count = satellites.size();
        const auto snapshot = store(payload, url.toString(), group);
        if (!snapshot)
            return;
        m_retryAfter.remove(group);
        install(std::move(satellites), snapshot->source);
        setStatus([count, skipped, error] {
            return tr("目录已获取 %1 条根数").arg(count) +
                   (skipped ? tr("，%1 条根数未载入：%2").arg(skipped).arg(error) : QString());
        });
    });
}

std::optional<OrbitSnapshot> SatelliteController::store(const QByteArray &payload, const QString &source,
                                                        const QString &group)
{
    if (m_storageBusy) {
        setStatus([] { return tr("正在整理数据库，请稍后保存资料"); });
        return std::nullopt;
    }
    QString error;
    auto snapshot = m_snapshots.save(payload, source, group, error);
    if (!snapshot)
        setStatus([error] { return tr("轨道资料保存失败：") + error; });
    return snapshot;
}

void SatelliteController::importFile(const QUrl &url)
{
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus([error = file.errorString()] { return tr("轨道文件打开失败：") + error; });
        return;
    }
    const auto payload = file.readAll();
    QString error;
    int skipped = 0;
    auto satellites = Orbit::parse(payload, error, skipped);
    if (satellites.isEmpty()) {
        setStatus([error] { return error.isEmpty() ? tr("文件内没有卫星根数") : error; });
        return;
    }
    const auto snapshot = store(payload, QFileInfo(file).fileName(), "local");
    if (!snapshot)
        return;
    m_group = "local";
    m_settings.setValue("catalog/group", m_group);
    install(std::move(satellites), snapshot->source);
    setStatus([skipped, error] {
        return tr("轨道文件已加入目录%1")
            .arg(skipped ? tr("，%1 条根数未载入：%2").arg(skipped).arg(error) : QString());
    });
}

void SatelliteController::exportSelected(const QUrl &url)
{
    const auto *satellite = selected();
    if (!satellite)
        return;
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setStatus([error = file.errorString()] { return tr("文件保存失败：") + error; });
        return;
    }
    QJsonArray elements;
    for (const int member : m_catalog.members(satellite->number))
        elements.append(m_catalog.at(member).elements);
    const auto bytes = QJsonDocument(elements).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        setStatus([error = file.errorString()] { return tr("轨道文件写入失败：") + error; });
        return;
    }
    setStatus([] { return tr("轨道根数已保存"); });
}

void SatelliteController::preparePlan()
{
    const auto *satellite = selected();
    if (!satellite || !m_clock || !m_clock->hasObserver())
        return;
    const auto revision = ++m_planRevision;
    m_plan = {};
    m_plan.satellite = *satellite;
    m_plan.observer = {m_clock->observerLatitude(), m_clock->observerLongitude(), m_clock->ellipsoidHeight() / 1000,
                       m_clock->minimumElevation()};
    m_plan.observerName = m_clock->observerName();
    m_plan.zone = QTimeZone(m_clock->timeZone().toUtf8());
    m_plan.height = m_clock->observerHeight();
    m_plan.heightKnown = m_clock->hasObserverHeight();
    m_plan.source = sourceText();
    m_plan.start = std::floor(m_clock->unixTime());
    m_plan.end = m_plan.start + 43200;
    m_planStatus = {};
    if (!std::isfinite(m_plan.observer.heightKm)) {
        m_planBusy = false;
        m_planStatus = [] { return tr("大地水准面数据读取失败"); };
        emit planChanged();
        return;
    }
    m_planBusy = true;
    emit planChanged();
    m_pool.start(
        [this, revision, plan = m_plan] {
            auto passes = Orbit::predictPasses(plan.satellite, plan.start, plan.end, plan.observer);
            QMetaObject::invokeMethod(
                this,
                [this, revision, passes = std::move(passes)]() mutable {
                    if (revision != m_planRevision)
                        return;
                    m_plan.passes = std::move(passes);
                    m_planBusy = false;
                    if (m_plan.passes.isEmpty())
                        m_planStatus = [] { return tr("12 h 内暂无满足高度角条件的过境"); };
                    emit planChanged();
                },
                Qt::QueuedConnection);
        },
        -1);
}

QVariantMap SatelliteController::planInfo() const
{
    if (!m_plan.satellite.number)
        return {};
    return {{"name", Orbit::displayName(m_plan.satellite)},
            {"observer", m_plan.observerName},
            {"start", formatTime(m_plan.start, m_plan.zone, "yyyy-MM-dd HH:mm:ss")},
            {"end", formatTime(m_plan.end, m_plan.zone, "yyyy-MM-dd HH:mm:ss")},
            {"zone", QString::fromUtf8(m_plan.zone.id())},
            {"minimum", m_plan.observer.minimumElevation},
            {"fileName", QStringLiteral("ASTROCHRON-%1-%2")
                             .arg(m_plan.satellite.number)
                             .arg(formatTime(m_plan.start, QTimeZone::UTC, "yyyyMMdd-HHmmss"))}};
}

QVariantList SatelliteController::planPasses() const
{
    QVariantList result;
    for (const auto &pass : m_plan.passes)
        result.append(QVariantMap{{"start", formatTime(pass.rise.time, m_plan.zone)},
                                  {"end", formatTime(pass.set.time, m_plan.zone)},
                                  {"peak", formatTime(pass.peak.time, m_plan.zone)},
                                  {"maximum", number(pass.peak.elevation, 1) + QStringLiteral("°")},
                                  {"partial", pass.startsBeforeWindow || pass.endsAfterWindow},
                                  {"optical", !pass.visibleIntervals.isEmpty()}});
    return result;
}

bool SatelliteController::exportPlan(const QUrl &url, const QString &format)
{
    if (m_planBusy || m_plan.passes.isEmpty() || !url.isLocalFile() || (format != "csv" && format != "ics"))
        return false;
    const auto output = ObservationPlanExporter::serialize(m_plan, format == "csv" ? PlanFormat::Csv : PlanFormat::Ics,
                                                           QDateTime::currentSecsSinceEpoch());
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly) || file.write(output) != output.size() || !file.commit()) {
        m_planStatus = [error = file.errorString()] { return tr("观测计划保存失败：%1").arg(error); };
        emit planChanged();
        return false;
    }
    m_planStatus = [] { return tr("观测计划已保存"); };
    emit planChanged();
    return true;
}

QVariantList SatelliteController::snapshots() const
{
    QVariantList result;
    if (!m_database.isOpen())
        return result;
    const auto sourceGroup = m_catalog.source(m_selected).group;
    const auto zone = m_clock ? QTimeZone(m_clock->timeZone().toUtf8()) : QTimeZone::systemTimeZone();
    for (const auto &snapshot : m_snapshots.list(sourceGroup.isEmpty() ? m_group : sourceGroup)) {
        result.append(QVariantMap{
            {"id", snapshot.source.snapshot},
            {"label", QDateTime::fromSecsSinceEpoch(snapshot.source.acquired, zone).toString("yyyy-MM-dd HH:mm:ss") +
                          (snapshot.pinned ? tr(" · 已固定") : QString())},
            {"source", snapshot.source.url},
            {"pinned", snapshot.pinned},
            {"imported", snapshot.imported}});
    }
    return result;
}

void SatelliteController::loadSnapshot(qint64 id)
{
    if (m_storageBusy)
        return;
    const auto snapshot = m_snapshots.load(id);
    if (!snapshot)
        return;
    QString error;
    int skipped = 0;
    auto satellites = Orbit::parse(snapshot->payload, error, skipped);
    if (satellites.isEmpty()) {
        setStatus([error] { return error; });
        return;
    }
    install(std::move(satellites), snapshot->source);
    setStatus([skipped, error] {
        return tr("本地轨道数据已载入") + (skipped ? tr("，%1 条根数未载入：%2").arg(skipped).arg(error) : QString());
    });
}

qint64 SatelliteController::snapshotId() const
{
    return m_catalog.source(m_selected).snapshot;
}
QString SatelliteController::sourceText() const
{
    return m_catalog.source(m_selected).url;
}
QString SatelliteController::elementEpoch() const
{
    const auto *satellite = selected();
    return satellite ? satellite->elements.value("EPOCH").toString() : QString();
}

void SatelliteController::install(QVector<Orbit::Satellite> satellites, OrbitSource source)
{
    m_catalog.install(std::move(satellites), source);
    m_watchlist.setElevations({});
    m_selected = m_catalog.find(m_selected) ? m_catalog.ownerOf(m_selected) : 0;
    if (!m_selected) {
        for (const auto &id : m_watchlist.ids()) {
            if (m_catalog.find(id.toLongLong())) {
                m_selected = m_catalog.ownerOf(id.toLongLong());
                break;
            }
        }
    }
    if (selectedWatched())
        m_lastWatchedSelection = m_selected;
    m_watchlist.rebuild();
    invalidate();
    emit selectionChanged();
    emit catalogChanged();
    emit watchlistChanged();
    emit sourceChanged();
    requestWatchPredictions();
    if (m_autoCleanup && !m_storageBusy)
        pruneSnapshots();
    emit storageChanged();
    if (previewActive()) {
        ++m_previewRevision;
        refreshPreview();
    }
}
const Orbit::Satellite *SatelliteController::selected() const
{
    return m_catalog.find(m_selected);
}
void SatelliteController::select(const QString &id)
{
    const auto value = m_catalog.ownerOf(id.toLongLong());
    if (value == m_selected || !m_catalog.find(value))
        return;
    m_selected = value;
    if (selectedWatched())
        m_lastWatchedSelection = value;
    invalidate();
    emit selectionChanged();
    emit watchlistChanged();
    emit sourceChanged();
}

bool SatelliteController::isWatched(const QString &id) const
{
    return m_watchlist.contains(m_catalog.ownerOf(id.toLongLong()));
}

void SatelliteController::setWatched(const QString &id, bool watched)
{
    const qint64 number = m_catalog.ownerOf(id.toLongLong());
    if (!m_watchlist.setWatched(number, watched))
        return;
    requestWatchPredictions();
    ++m_revision;
    if ((!watched && m_selected == number && !previewActive()) || !m_selected) {
        m_selected = 0;
        for (const auto &candidate : m_watchlist.ids()) {
            if (m_catalog.find(candidate.toLongLong())) {
                m_selected = candidate.toLongLong();
                break;
            }
        }
        m_lastWatchedSelection = m_selected;
        invalidate();
        emit selectionChanged();
        emit sourceChanged();
    } else
        requestFrame();
}

QString SatelliteController::previewName() const
{
    return SatelliteCatalog::constellationName(m_previewGroup);
}

void SatelliteController::previewConstellation(const QString &key)
{
    m_previewGroup = key;
    m_previewMode = 0;
    m_previewCandidates.clear();
    m_previewCount = 0;
    m_previewTotal = 0;
    m_previewTime = 0;
    ++m_previewRevision;
    ++m_revision;
    emit previewChanged();
    refreshPreview();
}

void SatelliteController::setPreviewMode(int mode)
{
    mode = qBound(0, mode, 2);
    if (m_previewMode == mode)
        return;
    m_previewMode = mode;
    ++m_previewRevision;
    ++m_revision;
    m_previewTime = 0;
    emit previewChanged();
    refreshPreview();
}

void SatelliteController::closePreview()
{
    m_previewGroup.clear();
    m_previewCandidates.clear();
    m_previewCount = 0;
    ++m_previewRevision;
    ++m_revision;
    emit previewChanged();
    if (!selectedWatched()) {
        m_selected = 0;
        if (m_watchlist.contains(m_lastWatchedSelection))
            m_selected = m_lastWatchedSelection;
        else
            for (const auto &id : m_watchlist.ids())
                if (m_catalog.find(id.toLongLong())) {
                    m_selected = id.toLongLong();
                    break;
                }
        invalidate();
        emit selectionChanged();
        emit watchlistChanged();
        emit sourceChanged();
    } else
        requestFrame();
}

void SatelliteController::refreshPreview()
{
    if (!previewActive() || !m_clock || !m_clock->hasObserver())
        return;
    if (m_previewBusy) {
        m_previewPending = true;
        return;
    }
    QVector<Orbit::Satellite> satellites;
    const auto members = m_catalog.groupMembers(m_previewGroup);
    for (const int index : m_catalog.targets())
        if (m_catalog.constellationKey(m_catalog.at(index)) == m_previewGroup &&
            (!m_catalog.hasGroup(m_previewGroup) || members.contains(m_catalog.at(index).number)))
            satellites.append(m_catalog.at(index));
    m_previewTotal = static_cast<int>(satellites.size());
    if (m_previewMode == 2) {
        m_previewCandidates.clear();
        for (const auto &satellite : satellites)
            m_previewCandidates.insert(satellite.number);
        m_previewTime = m_clock->unixTime();
        ++m_revision;
        emit previewChanged();
        requestFrame();
        return;
    }
    const auto generation = m_previewRevision;
    const int mode = m_previewMode;
    const double time = m_clock->unixTime();
    const Orbit::Observer observer{m_clock->observerLatitude(), m_clock->observerLongitude(),
                                   m_clock->ellipsoidHeight() / 1000.0, m_clock->minimumElevation()};
    if (!std::isfinite(observer.heightKm))
        return;
    m_previewBusy = true;
    m_previewPending = false;
    emit previewChanged();
    m_pool.start([this, satellites, generation, mode, time, observer] {
        auto candidates = Orbit::previewCandidates(satellites, time, observer, mode == 1);
        QMetaObject::invokeMethod(
            this,
            [this, candidates = std::move(candidates), generation, time]() mutable {
                m_previewBusy = false;
                if (generation == m_previewRevision && previewActive()) {
                    m_previewCandidates = std::move(candidates);
                    m_previewTime = time;
                    ++m_revision;
                    requestFrame();
                }
                emit previewChanged();
                if (previewActive() && (m_previewPending || generation != m_previewRevision))
                    refreshPreview();
            },
            Qt::QueuedConnection);
    });
}
void SatelliteController::invalidate()
{
    ++m_revision;
    m_needTrack = true;
    m_trackCache = {};
    m_gnssTime = 0;
    m_currentState.reset();
    m_observation.clear();
    m_trajectory.clear();
    m_markers.clear();
    m_shadowEvents.clear();
    emit frameChanged();
    emit trajectoryChanged();
    requestFrame();
}

void SatelliteController::setReceiveFrequency(double value)
{
    if (!std::isfinite(value) || value < 0 || value > 1000000)
        return;
    if (m_frequency == value)
        return;
    m_frequency = value;
    m_settings.setValue("radio/frequencyMHz", value);
    emit receiveFrequencyChanged();
    if (m_currentState)
        m_observation.insert("doppler", -m_currentState->rangeRate / 299792.458 * m_frequency * 1e6);
    emit frameChanged();
}

void SatelliteController::requestFrame()
{
    if (!m_clock || !m_clock->hasObserver() || m_catalog.isEmpty())
        return;
    const double time = m_clock->unixTime();
    const double calculationInterval = 1.0 / m_clock->calculationFrequency();
    if (m_clock->live() && m_revision == m_calculationRevision && !m_needTrack &&
        std::abs(time - m_calculationTime) < calculationInterval)
        return;
    if (m_busy) {
        m_pending = true;
        return;
    }
    auto request = frameRequest();
    if (!std::isfinite(request.observer.heightKm)) {
        setStatus([] { return tr("大地水准面数据读取失败"); });
        return;
    }
    m_busy = true;
    m_pending = false;
    m_needTrack = false;
    m_calculationTime = time;
    m_calculationRevision = request.revision;
    emit statusChanged();
    m_pool.start([this, request = std::move(request)] {
        auto result = calculateOrbitFrame(request);
        QMetaObject::invokeMethod(
            this, [this, result = std::move(result)]() mutable { applyFrame(std::move(result)); },
            Qt::QueuedConnection);
    });
}

FrameRequest SatelliteController::frameRequest() const
{
    FrameRequest request;
    request.selectedId = m_selected;
    request.revision = m_revision;
    request.time = m_clock->unixTime();
    request.referenceTime = m_clock->referenceTime();
    request.calculationInterval = 1.0 / m_clock->calculationFrequency();
    request.interpolate = m_clock->live() && m_clock->updateFrequency() > m_clock->calculationFrequency();
    request.interpolateNavigation = m_clock->live() && m_clock->updateFrequency() > 1;
    request.calculateNavigation = std::abs(request.time - m_gnssTime) >= 1;
    request.calculateTrack = m_needTrack;
    request.previousTrack = m_needTrack ? m_trackCache : Orbit::Track{};
    request.observer = {m_clock->observerLatitude(), m_clock->observerLongitude(), m_clock->ellipsoidHeight() / 1000.0,
                        m_clock->minimumElevation()};
    request.hasHeight = m_clock->hasObserverHeight();
    request.previewCandidates = m_previewCandidates;
    request.previewMode = m_previewMode;
    for (const auto &id : m_watchlist.ids())
        request.watchedIds.insert(m_catalog.ownerOf(id.toLongLong()));
    auto ids = request.watchedIds;
    ids.unite(m_previewCandidates);
    if (m_selected)
        ids.insert(m_selected);
    for (const auto id : ids) {
        if (const auto *satellite = m_catalog.find(id))
            request.satellites.append(*satellite);
    }
    if (request.calculateNavigation) {
        for (const int index : m_catalog.gnssTargets())
            request.navigationSatellites.append(m_catalog.at(index));
    }
    return request;
}

void SatelliteController::applyFrame(FrameResult result)
{
    m_busy = false;
    if (result.revision == m_revision) {
        const auto markerMap = [](const SatelliteMarker &marker, bool navigation) {
            QVariantMap value = navigation ? QVariantMap{{"time", marker.state.time},
                                                         {"latitude", marker.state.latitude},
                                                         {"longitude", marker.state.longitude}}
                                           : stateMap(marker.state);
            value.insert("id", QString::number(marker.id));
            if (marker.nextPosition) {
                value.insert("nextTime", marker.nextPosition->time);
                value.insert("nextLatitude", marker.nextPosition->latitude);
                value.insert("nextLongitude", marker.nextPosition->longitude);
                value.insert("nextAltitude", marker.nextPosition->altitude);
            }
            return value;
        };
        m_markers.clear();
        for (const auto &marker : result.markers)
            m_markers.append(markerMap(marker, false));
        m_currentState = result.observation;
        m_observation = result.observation ? stateMap(*result.observation) : QVariantMap{};
        if (result.observation) {
            m_observation.insert("id", QString::number(m_selected));
            m_observation.insert("originalName", result.originalName);
            m_observation.insert("epochAge", result.epochAgeDays);
            m_observation.insert("heightEstimated", !result.hasHeight);
            m_observation.insert("doppler", -result.observation->rangeRate / 299792.458 * m_frequency * 1e6);
        }
        m_previewCount = result.previewCount;
        if (result.calculatedNavigation) {
            m_gnssMarkers.clear();
            for (const auto &marker : result.navigationMarkers)
                m_gnssMarkers.append(markerMap(marker, true));
            m_gnssTime = result.time;
            emit gnssChanged();
        }
        updateMagnitude();
        emit frameChanged();
        m_watchlist.setElevations(std::move(result.elevations));
        if (result.calculatedTrack) {
            m_trackCache = std::move(result.track);
            m_shadowEvents = std::move(result.shadowEvents);
            m_trajectory.clear();
            for (const auto &sample : m_trackCache.samples)
                m_trajectory.append(stateMap(sample));
            m_trackReference = result.referenceTime;
            emit trajectoryChanged();
        }
    } else if (result.calculatedTrack)
        m_needTrack = true;
    emit statusChanged();
    if (m_pending || result.revision != m_revision)
        requestFrame();
}

QVariantMap SatelliteController::photometry() const
{
    const auto *entry = m_photometry.entry(selected());
    if (!entry)
        return {};
    const bool hasDefault = m_photometry.hasDefault(selected());
    const auto source = !m_photometry.hasOverride(m_selected) && m_selected == 48274
                            ? tr("SeeSat-L / Jay Respler（2022 年构型）")
                        : entry->manual && entry->source == QStringLiteral("手动填写") ? tr("手动填写")
                                                                                       : entry->source;
    return {
        {"magnitude", entry->magnitude},
        {"phase", entry->phase},
        {"source", source},
        {"manual", entry->manual},
        {"builtin", !m_photometry.hasOverride(m_selected)},
        {"hasOverride", m_photometry.hasOverride(m_selected)},
        {"hasDefault", hasDefault},
        {"sourceDate", entry->sourceDate},
        {"recordedAt",
         entry->recordedAt
             ? QDateTime::fromSecsSinceEpoch(entry->recordedAt, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm:ss 'UTC'")
             : QString()}};
}

bool SatelliteController::setPhotometry(double magnitude, int phase, const QString &source, const QString &sourceDate)
{
    if (!selected())
        return false;
    const bool saved = m_photometry.set(m_selected, magnitude, phase, source, sourceDate);
    if (saved) {
        updateMagnitude();
        emit frameChanged();
    }
    emit photometryChanged();
    return saved;
}

bool SatelliteController::clearPhotometry()
{
    const bool cleared = m_photometry.clear(selected());
    if (cleared) {
        updateMagnitude();
        emit frameChanged();
    }
    emit photometryChanged();
    return cleared;
}

bool SatelliteController::importMagnitudes(const QUrl &url, const QString &sourceDate)
{
    const bool imported = m_photometry.importMagnitudes(url, sourceDate);
    if (imported) {
        updateMagnitude();
        emit frameChanged();
    }
    emit photometryChanged();
    return imported;
}

void SatelliteController::updateMagnitude()
{
    m_observation.remove("magnitude");
    if (!m_currentState)
        return;
    int status = 0;
    const auto *entry = m_photometry.entry(selected());
    if (!entry)
        status = 1;
    else if (m_currentState->elevation <= 0)
        status = 2;
    else if (m_currentState->illumination == 2)
        status = 3;
    else if (m_currentState->illumination == 1)
        status = 4;
    else {
        const auto magnitude = Orbit::apparentMagnitude(*m_currentState, entry->magnitude, entry->phase);
        if (magnitude)
            m_observation.insert("magnitude", *magnitude);
        else
            status = 5;
    }
    m_observation.insert("magnitudeStatusCode", status);
}

QVariantMap SatelliteController::observation() const
{
    auto result = m_observation;
    if (result.isEmpty())
        return result;
    if (const auto *satellite = selected())
        result.insert("name", Orbit::displayName(*satellite));
    const int illumination = result.value("illumination").toInt();
    result.insert("lighting", Orbit::illuminationName(illumination));
    result.insert("direction", Orbit::directionName(result.value("azimuth").toDouble()));
    result.insert("motion", result.value("rangeRate").toDouble() < 0 ? tr("正在接近") : tr("正在远离"));
    result.insert("visibility", result.value("elevation").toDouble() < (m_clock ? m_clock->minimumElevation() : 10)
                                    ? tr("低于最低高度角")
                                : illumination != 0                            ? tr("位于地影")
                                : result.value("sunElevation").toDouble() > -6 ? tr("天空较亮")
                                                                               : tr("具备光照观测条件"));
    const char *magnitudeStates[] = {"",
                                     QT_TR_NOOP("暂无参考星等"),
                                     QT_TR_NOOP("地平线下"),
                                     QT_TR_NOOP("本影内"),
                                     QT_TR_NOOP("半影内"),
                                     QT_TR_NOOP("相位接近 180°")};
    result.insert("magnitudeStatus", tr(magnitudeStates[qBound(0, result.value("magnitudeStatusCode").toInt(), 5)]));
    return result;
}

QVariantList SatelliteController::passes() const
{
    QVariantList result;
    const auto zone = m_clock ? QTimeZone(m_clock->timeZone().toUtf8()) : QTimeZone::systemTimeZone();
    for (const auto &pass : m_trackCache.passes) {
        QStringList visible;
        for (const auto &interval : pass.visibleIntervals)
            visible.append(formatTime(interval[0], zone, "HH:mm:ss") + " - " +
                           formatTime(interval[1], zone, "HH:mm:ss"));
        result.append(QVariantMap{
            {"start", pass.rise.time},
            {"peak", pass.peak.time},
            {"end", pass.set.time},
            {"startClipped", pass.startsBeforeWindow},
            {"endClipped", pass.endsAfterWindow},
            {"startText", (pass.startsBeforeWindow ? tr("早于 ") : QString()) + formatTime(pass.rise.time, zone)},
            {"peakText", formatTime(pass.peak.time, zone)},
            {"endText", (pass.endsAfterWindow ? tr("晚于 ") : QString()) + formatTime(pass.set.time, zone)},
            {"startAzimuth", pass.rise.azimuth},
            {"endAzimuth", pass.set.azimuth},
            {"startAz",
             Orbit::directionName(pass.rise.azimuth) + " " + number(pass.rise.azimuth, 1) + QStringLiteral("°")},
            {"endAz", Orbit::directionName(pass.set.azimuth) + " " + number(pass.set.azimuth, 1) + QStringLiteral("°")},
            {"maximum", number(pass.peak.elevation, 1) + QStringLiteral("°")},
            {"duration", number((pass.set.time - pass.rise.time) / 60, 1) + QStringLiteral(" min")},
            {"range", number(pass.peak.range, 0) + QStringLiteral(" km")},
            {"hasOptical", !visible.isEmpty()},
            {"optical", visible.isEmpty() ? tr("光照条件欠佳") : visible.join(" / ")}});
    }
    return result;
}

QVariantList SatelliteController::shadowEvents() const
{
    QVariantList result;
    const auto zone = m_clock ? QTimeZone(m_clock->timeZone().toUtf8()) : QTimeZone::systemTimeZone();
    for (const auto &event : m_shadowEvents) {
        result.append(QVariantMap{
            {"time", event.time},
            {"timeText", formatTime(event.time, zone)},
            {"entering", event.entering},
            {"umbra", event.umbra},
            {"name", (event.entering ? tr("进入") : tr("离开")) + (event.umbra ? tr("本影") : tr("半影"))}});
    }
    return result;
}

QVariantList SatelliteController::relatedObjects() const
{
    QVariantList result;
    for (const int index : m_catalog.members(m_selected)) {
        const auto &satellite = m_catalog.at(index);
        result.append(QVariantMap{{"id", QString::number(satellite.number)},
                                  {"name", Orbit::displayName(satellite)},
                                  {"originalName", satellite.name},
                                  {"internationalId", satellite.internationalId}});
    }
    return result;
}

QVariantList SatelliteController::orbitFields() const
{
    const auto *satellite = selected();
    if (!satellite)
        return {};
    const auto &e = satellite->elements;
    const auto field = [&e](const char *key, int precision, const QString &unit = {}) {
        return e.contains(key) ? QString::number(e.value(key).toDouble(), 'f', precision) + unit : tr("暂无数据");
    };
    const auto scientific = [&e](const char *key) {
        return e.contains(key) ? QString::number(e.value(key).toDouble(), 'e', 7) : tr("暂无数据");
    };
    QVariantList result;
    const auto add = [&result](const QString &label, const QString &value, bool epoch = false) {
        result.append(QVariantMap{{"label", label}, {"value", value}, {"epoch", epoch}});
    };
    add(tr("卫星编号"), QString::number(satellite->number));
    add(tr("国际编号"), satellite->internationalId.isEmpty() ? tr("暂无数据") : satellite->internationalId);
    add(tr("名称"), satellite->name);
    const auto constellation = m_catalog.constellationKey(*satellite);
    if (SatelliteCatalog::isNavigationGroup(constellation)) {
        add(QStringLiteral("PRN"), m_catalog.prn(satellite->number, tr("暂无数据")));
        add(tr("轨道面"), m_catalog.plane(satellite->number, tr("暂无数据")));
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
void SatelliteController::copyDetails()
{
    QStringList lines;
    for (const auto &entry : orbitFields()) {
        const auto field = entry.toMap();
        lines.append(field.value("label").toString() + ": " + field.value("value").toString());
    }
    QGuiApplication::clipboard()->setText(lines.join('\n'));
    setStatus([] { return tr("轨道参数已复制"); });
}
