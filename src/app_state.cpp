#include "app_state.h"
#include "map_data.h"
#include "astronomy.h"

#include <QGuiApplication>
#include <QStyleHints>
#include <QLocale>
#include <cmath>
#include <numbers>
#include <limits>

AppState::AppState(QObject *parent) : QObject(parent), m_reference(QDateTime::currentDateTimeUtc())
{
    m_observerName = m_settings.value("observer/name", QStringLiteral("台北")).toString();
    m_latitude = m_settings.value("observer/latitude", 25.0330).toDouble();
    m_longitude = m_settings.value("observer/longitude", 121.5654).toDouble();
    m_height = m_settings.contains("observer/height") ? m_settings.value("observer/height").toDouble() : std::numeric_limits<double>::quiet_NaN();
    m_timeZone = QTimeZone(m_settings.value("observer/timeZone", QStringLiteral("Asia/Taipei")).toByteArray());
    m_dark = m_settings.value("appearance/dark", QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark).toBool();
    updateSun();
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_live) {
            m_reference = QDateTime::currentDateTimeUtc();
            updateSun();
            emit timeChanged();
        }
    });
    m_timer.start(1000);
}

QDateTime AppState::selectedTime() const { return m_reference.addSecs(m_offset * 60); }
QString AppState::timeText() const { return selectedTime().toTimeZone(m_timeZone).toString("yyyy-MM-dd HH:mm:ss"); }
QString AppState::startTimeText() const { return m_reference.addSecs(-43200).toTimeZone(m_timeZone).toString("MM-dd HH:mm"); }
QString AppState::endTimeText() const { return m_reference.addSecs(43200).toTimeZone(m_timeZone).toString("MM-dd HH:mm"); }
bool AppState::hasObserverHeight() const { return std::isfinite(m_height); }
QString AppState::timeZoneName() const { return m_timeZone.displayName(selectedTime(), QTimeZone::LongName, QLocale(QLocale::Chinese)); }
QStringList AppState::timeZones() const
{
    QStringList result;
    for (const auto &id : QTimeZone::availableTimeZoneIds()) result.append(QString::fromUtf8(id));
    return result;
}
QString AppState::offsetText() const
{
    if (m_live) return QStringLiteral("当前时刻");
    if (m_offset == 0) return QStringLiteral("暂停");
    return QStringLiteral("%1 %2 小时 %3 分").arg(m_offset < 0 ? QStringLiteral("过去") : QStringLiteral("未来"))
        .arg(std::abs(m_offset) / 60).arg(std::abs(m_offset) % 60);
}

void AppState::setMinuteOffset(int minutes)
{
    m_live = false;
    m_offset = qBound(-720, minutes, 720);
    updateSun();
    emit timeChanged();
}

void AppState::resumeLive()
{
    m_live = true;
    m_offset = 0;
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

bool AppState::setObserver(const QString &name, double latitude, double longitude, double height, const QString &timeZone)
{
    const QTimeZone zone(timeZone.toUtf8());
    if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) ||
        std::isinf(height) || !zone.isValid() || latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180)
        return false;
    m_observerName = name.trimmed();
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
    emit observerChanged();
    emit timeChanged();
    return true;
}

QVariantList AppState::findCities(const QString &query) const
{
    QVariantList result;
    const auto search = query.trimmed();
    for (const auto &city : worldMapData().cities) {
        if (!search.isEmpty() && !city.name.contains(search, Qt::CaseInsensitive) &&
            !city.originalName.contains(search, Qt::CaseInsensitive)) continue;
        result.append(QVariantMap{{"name", city.name}, {"latitude", city.latitude}, {"longitude", city.longitude},
                                  {"timeZone", QTimeZone(city.timeZone.toUtf8()).isValid() ? city.timeZone : timeZone()}});
        if (result.size() == 60) break;
    }
    return result;
}
