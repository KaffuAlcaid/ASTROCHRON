#include "app_state.h"
#include "map_data.h"
#include "astronomy.h"

#include <QGuiApplication>
#include <QStyleHints>
#include <QLocale>
#include <QFile>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QProcess>
#include <QUrlQuery>
#include <QtEndian>
#include <cmath>
#include <numbers>
#include <limits>

AppState::AppState(QObject *parent) : QObject(parent), m_reference(QDateTime::currentDateTimeUtc()), m_now(m_reference)
{
    m_language = m_settings.value("appearance/language", "system").toString();
    if (m_language != "system" && m_language != "zh_CN" && m_language != "en") m_language = "system";
    m_startupLanguage = m_language;
    m_observerName = m_settings.value("observer/name", tr("台北")).toString();
    m_hasObserver = m_settings.contains("observer/name") && m_settings.contains("observer/latitude") && m_settings.contains("observer/longitude");
    m_latitude = m_settings.value("observer/latitude", 25.0330).toDouble();
    m_longitude = m_settings.value("observer/longitude", 121.5654).toDouble();
    m_height = m_settings.contains("observer/height") ? m_settings.value("observer/height").toDouble() : std::numeric_limits<double>::quiet_NaN();
    m_automaticElevation = m_settings.value("observer/automaticElevation", true).toBool();
    m_minimumElevation = m_settings.value("observer/minimumElevation", 10).toDouble();
    m_elevationStatus = m_settings.value("observer/heightSource", hasObserverHeight() ? QString::fromUtf8(QT_TR_NOOP("手动填写")) : QString::fromUtf8(QT_TR_NOOP("海拔待查询"))).toString();
    m_timeZone = QTimeZone(m_settings.value("observer/timeZone", QStringLiteral("Asia/Taipei")).toByteArray());
    m_dark = m_settings.value("appearance/dark", QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark).toBool();
    m_updateFrequency = qBound(1, m_settings.value("display/updateFrequency", 1).toInt(), 60);
    updateSun();
    connect(&m_timer, &QTimer::timeout, this, [this] {
        const auto now = QDateTime::currentDateTimeUtc();
        if (now.toSecsSinceEpoch() != m_now.toSecsSinceEpoch()) { m_now = now; emit nowChanged(); }
        if (m_live) {
            m_reference = now;
            updateSun();
            emit timeChanged();
        }
    });
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.start(qRound(1000.0 / m_updateFrequency));
    if (!hasObserverHeight() && m_automaticElevation) QTimer::singleShot(0, this, &AppState::lookupElevation);
}

double AppState::ellipsoidHeight() const
{
    struct Geoid {
        QByteArray pixels;
        int width = 0, height = 0;
        double offset = 0, scale = 1;
        Geoid() {
            QFile file(QStringLiteral(":/geoid/egm2008-5.bin"));
            if (!file.open(QIODevice::ReadOnly)) return;
            const auto data = qUncompress(file.readAll());
            if (!data.startsWith("AGD1") || data.size() < 28) return;
            QDataStream stream(data);
            stream.skipRawData(4);
            qint32 columns = 0, rows = 0;
            stream >> columns >> rows >> offset >> scale;
            const qint64 count = static_cast<qint64>(columns) * rows;
            if (stream.status() != QDataStream::Ok || columns <= 0 || rows < 2 || data.size() != 28 + count * 2) return;
            width = columns; height = rows;
            pixels.resize(count * 2);
            for (int y = 0; y < height; ++y) {
                quint16 previous = 0;
                for (int x = 0; x < width; ++x) {
                    const qsizetype index = static_cast<qsizetype>(y) * width + x;
                    const quint16 delta = (static_cast<quint8>(data[28 + index]) << 8)
                        | static_cast<quint8>(data[28 + count + index]);
                    previous = static_cast<quint16>(previous + delta);
                    qToBigEndian(previous, pixels.data() + index * 2);
                }
            }
        }
        double at(int x, int y) const {
            return offset + scale * qFromBigEndian<quint16>(pixels.constData() + (y * width + x % width) * 2);
        }
    };
    static const Geoid geoid;
    if (geoid.width == 0) return std::numeric_limits<double>::quiet_NaN();
    const double x = std::fmod(m_longitude + 360.0, 360.0) / 360.0 * geoid.width;
    const double y = (90.0 - m_latitude) / 180.0 * (geoid.height - 1);
    const int ix = static_cast<int>(x), iy = std::min(static_cast<int>(y), geoid.height - 2);
    const double dx = x - ix, dy = y - iy;
    const double undulation = (1 - dy) * ((1 - dx) * geoid.at(ix, iy) + dx * geoid.at(ix + 1, iy))
        + dy * ((1 - dx) * geoid.at(ix, iy + 1) + dx * geoid.at(ix + 1, iy + 1));
    return (hasObserverHeight() ? m_height : 0.0) + undulation;
}

void AppState::setAutomaticElevation(bool enabled)
{
    m_automaticElevation = enabled;
    m_settings.setValue("observer/automaticElevation", enabled);
    if (!enabled) {
        ++m_elevationRequest;
        if (m_elevationBusy) m_elevationStatus = m_settings.value("observer/heightSource", QString::fromUtf8(QT_TR_NOOP("海拔待填写"))).toString();
        m_elevationBusy = false;
    }
    emit elevationChanged();
    if (enabled && !hasObserverHeight()) lookupElevation();
}

void AppState::lookupElevation()
{
    if (!m_hasObserver || m_elevationBusy) return;
    const auto requestId = ++m_elevationRequest;
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/elevation"));
    QUrlQuery query;
    query.addQueryItem("latitude", QString::number(m_latitude, 'f', 6));
    query.addQueryItem("longitude", QString::number(m_longitude, 'f', 6));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setTransferTimeout(20000);
    auto *reply = m_network.get(request);
    m_elevationBusy = true;
    m_elevationStatus = QString::fromUtf8(QT_TR_NOOP("正在查询地形海拔"));
    emit elevationChanged();
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId] {
        reply->deleteLater();
        if (requestId != m_elevationRequest) return;
        m_elevationBusy = false;
        const auto values = QJsonDocument::fromJson(reply->readAll()).object().value("elevation").toArray();
        if (reply->error() == QNetworkReply::NoError && !values.isEmpty() && values[0].isDouble() && std::isfinite(values[0].toDouble())) {
            m_height = values[0].toDouble();
            m_elevationStatus = QString::fromUtf8(QT_TR_NOOP("地形海拔 · Copernicus DEM / EGM2008"));
            m_settings.setValue("observer/height", m_height);
            m_settings.setValue("observer/heightSource", m_elevationStatus);
            emit observerChanged();
        } else {
            m_elevationStatus = QString::fromUtf8(QT_TR_NOOP("海拔查询失败，可重新查询或手动填写"));
        }
        emit elevationChanged();
    });
}

void AppState::setMinimumElevation(double value)
{
    if (!std::isfinite(value)) return;
    m_minimumElevation = qBound(0.0, value, 89.0);
    m_settings.setValue("observer/minimumElevation", m_minimumElevation);
    emit observerChanged();
}

QString AppState::placeName(const QString &name, double latitude, double longitude) const
{
    for (const auto &city : worldMapData().cities)
        if ((name == city.name || name == city.originalName) && std::abs(latitude - city.latitude) < 0.0001
            && std::abs(longitude - city.longitude) < 0.0001) return city.displayName();
    if (name == QStringLiteral("地图选点") || name == QStringLiteral("Map location")) return tr("地图选点");
    return name;
}
QString AppState::observerName() const { return placeName(m_observerName, m_latitude, m_longitude); }
QString AppState::elevationStatus() const { return tr(m_elevationStatus.toUtf8().constData()); }
QVariantList AppState::savedObservers() const
{
    auto places = m_settings.value("observer/places").toList();
    for (auto &entry : places) {
        auto place = entry.toMap();
        place["name"] = placeName(place.value("name").toString(), place.value("latitude").toDouble(), place.value("longitude").toDouble());
        entry = place;
    }
    return places;
}
void AppState::saveObserver()
{
    auto places = m_settings.value("observer/places").toList();
    const QVariantMap place{{"name", m_observerName}, {"latitude", m_latitude}, {"longitude", m_longitude},
        {"height", m_height}, {"timeZone", timeZone()}, {"source", m_elevationStatus}};
    bool replaced = false;
    for (auto &entry : places) {
        const auto saved = entry.toMap();
        if (placeName(saved.value("name").toString(), saved.value("latitude").toDouble(), saved.value("longitude").toDouble()) == observerName()) {
            entry = place; replaced = true; break;
        }
    }
    if (!replaced) places.append(place);
    m_settings.setValue("observer/places", places);
    emit observerChanged();
}
void AppState::loadObserver(int index)
{
    const auto places = m_settings.value("observer/places").toList();
    if (index < 0 || index >= places.size()) return;
    const auto place = places[index].toMap();
    setObserver(place.value("name").toString(), place.value("latitude").toDouble(), place.value("longitude").toDouble(),
                place.value("height").toDouble(), place.value("timeZone").toString());
    if (hasObserverHeight()) {
        m_elevationStatus = place.value("source").toString();
        m_settings.setValue("observer/heightSource", m_elevationStatus);
        emit elevationChanged();
    }
}
void AppState::removeObserver(int index)
{
    auto places = m_settings.value("observer/places").toList();
    if (index < 0 || index >= places.size()) return;
    places.removeAt(index);
    m_settings.setValue("observer/places", places);
    emit observerChanged();
}
void AppState::seek(double seconds)
{
    if (!std::isfinite(seconds)) return;
    const auto milliseconds = qBound<qint64>(-43200000LL, static_cast<qint64>(std::llround((seconds - referenceTime()) * 1000)), 43200000LL);
    const auto offset = static_cast<int>(milliseconds / 1000);
    m_live = false;
    m_offset = offset / 60;
    m_secondOffset = offset % 60;
    m_millisecondOffset = static_cast<int>(milliseconds % 1000);
    updateSun();
    emit timeChanged();
}

QDateTime AppState::selectedTime() const { return m_reference.addMSecs((m_offset * 60 + m_secondOffset) * 1000LL + m_millisecondOffset); }
QString AppState::timeText() const { return selectedTime().toTimeZone(m_timeZone).toString("yyyy-MM-dd HH:mm:ss"); }
QString AppState::startTimeText() const { return m_reference.addSecs(-43200).toTimeZone(m_timeZone).toString("MM-dd HH:mm"); }
QString AppState::endTimeText() const { return m_reference.addSecs(43200).toTimeZone(m_timeZone).toString("MM-dd HH:mm"); }
QString AppState::formatTime(double seconds, const QString &format) const
{
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(seconds * 1000), m_timeZone).toString(format);
}
bool AppState::hasObserverHeight() const { return std::isfinite(m_height); }
QString AppState::timeZoneName() const { return m_timeZone.displayName(selectedTime(), QTimeZone::LongName, QLocale()); }
QStringList AppState::timeZones() const
{
    QStringList result;
    for (const auto &id : QTimeZone::availableTimeZoneIds()) result.append(QString::fromUtf8(id));
    return result;
}
QString AppState::offsetText() const
{
    if (m_live) return tr("当前时刻");
    const int seconds = m_offset * 60 + m_secondOffset;
    if (seconds == 0) return tr("暂停");
    return QStringLiteral("%1 %2 h %3 min%4").arg(seconds < 0 ? tr("过去") : tr("未来"))
        .arg(std::abs(seconds) / 3600).arg(std::abs(seconds) / 60 % 60)
        .arg(m_secondOffset ? QStringLiteral(" %1 s").arg(std::abs(m_secondOffset)) : QString());
}

void AppState::setMinuteOffset(int minutes)
{
    m_live = false;
    m_offset = qBound(-720, minutes, 720);
    m_secondOffset = 0;
    m_millisecondOffset = 0;
    updateSun();
    emit timeChanged();
}

void AppState::resumeLive()
{
    m_live = true;
    m_offset = 0;
    m_secondOffset = 0;
    m_millisecondOffset = 0;
    m_reference = QDateTime::currentDateTimeUtc();
    updateSun();
    emit timeChanged();
}

void AppState::updateSun()
{
    const auto selected = selectedTime();
    const auto date = selected.date();
    const auto clock = selected.time();
    auto time = Astronomy_MakeTime(date.year(), date.month(), date.day(), clock.hour(), clock.minute(),
                                  clock.second() + clock.msec() / 1000.0);
    const auto equator = Astronomy_RotateVector(Astronomy_Rotation_EQJ_EQD(&time),
                                               Astronomy_GeoVector(BODY_SUN, time, ABERRATION));
    const double longitude = std::atan2(equator.y, equator.x) - Astronomy_SiderealTime(&time) * std::numbers::pi / 12.0;
    const double latitude = std::atan2(equator.z, std::hypot(equator.x, equator.y));
    m_sun = QVector3D(static_cast<float>(std::cos(latitude) * std::cos(longitude)),
                     static_cast<float>(std::cos(latitude) * std::sin(longitude)),
                     static_cast<float>(std::sin(latitude)));
}

void AppState::setDarkTheme(bool dark)
{
    if (m_dark == dark) return;
    m_dark = dark;
    m_settings.setValue("appearance/dark", dark);
    emit themeChanged();
}

void AppState::setUpdateFrequency(int frequency)
{
    frequency = qBound(1, frequency, 60);
    if (m_updateFrequency == frequency) return;
    m_updateFrequency = frequency;
    m_timer.setInterval(qRound(1000.0 / frequency));
    m_settings.setValue("display/updateFrequency", frequency);
    emit updateFrequencyChanged();
}

bool AppState::setObserver(const QString &name, double latitude, double longitude, double height, const QString &timeZone)
{
    const QTimeZone zone(timeZone.toUtf8());
    if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) ||
        std::isinf(height) || !zone.isValid() || latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180)
        return false;
    m_observerName = name.trimmed();
    m_hasObserver = true;
    const bool sameHeight = latitude == m_latitude && longitude == m_longitude && height == m_height;
    ++m_elevationRequest;
    m_elevationBusy = false;
    m_latitude = latitude;
    m_longitude = longitude;
    m_height = height;
    m_timeZone = zone;
    m_settings.setValue("observer/name", m_observerName);
    m_settings.setValue("observer/latitude", latitude);
    m_settings.setValue("observer/longitude", longitude);
    if (std::isfinite(height)) m_settings.setValue("observer/height", height);
    else m_settings.remove("observer/height");
    m_settings.setValue("observer/timeZone", timeZone);
    if (!sameHeight) m_elevationStatus = hasObserverHeight() ? QString::fromUtf8(QT_TR_NOOP("手动填写 · EGM2008")) : QString::fromUtf8(QT_TR_NOOP("海拔待查询"));
    m_settings.setValue("observer/heightSource", m_elevationStatus);
    emit observerChanged();
    emit timeChanged();
    emit elevationChanged();
    if (!hasObserverHeight() && m_automaticElevation) lookupElevation();
    return true;
}

QVariantList AppState::findCities(const QString &query) const
{
    QVariantList result;
    const auto search = query.trimmed();
    for (const auto &city : worldMapData().cities) {
        if (!search.isEmpty() && !city.name.contains(search, Qt::CaseInsensitive) &&
            !city.originalName.contains(search, Qt::CaseInsensitive)) continue;
        result.append(QVariantMap{{"name", city.displayName()}, {"latitude", city.latitude}, {"longitude", city.longitude},
                                  {"timeZone", QTimeZone(city.timeZone.toUtf8()).isValid() ? city.timeZone : timeZone()}});
        if (result.size() == 60) break;
    }
    return result;
}

bool AppState::chinese() const { return QLocale().language() == QLocale::Chinese; }

void AppState::setLanguage(const QString &language)
{
    if (language == m_language || (language != "system" && language != "zh_CN" && language != "en")) return;
    m_language = language;
    m_settings.setValue("appearance/language", language);
    emit languageChanged();
}

bool AppState::restart()
{
    m_settings.sync();
    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1))) return false;
    QCoreApplication::quit();
    return true;
}
