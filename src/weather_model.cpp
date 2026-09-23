#include "weather_model.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QUrlQuery>
#include <cmath>

namespace {
constexpr const char *variables[] = {"cloud_cover", "cloud_cover_low", "cloud_cover_mid", "cloud_cover_high",
    "visibility", "precipitation_probability", "relative_humidity_2m"};
}

WeatherModel::WeatherModel(QObject *parent) : QObject(parent)
{
    m_enabled = m_settings.value("weather/enabled", true).toBool();
    m_status = [this] { return m_enabled ? tr("天气待获取") : tr("天气查询已关闭"); };
    m_refreshTimer.setInterval(3600000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &WeatherModel::refresh);
    m_locationTimer.setSingleShot(true);
    m_locationTimer.setInterval(400);
    connect(&m_locationTimer, &QTimer::timeout, this, &WeatherModel::refresh);
}

void WeatherModel::setClock(AppState *clock)
{
    if (m_clock == clock) return;
    if (m_clock) disconnect(m_clock, nullptr, this, nullptr);
    m_clock = clock;
    if (clock) {
        connect(clock, &AppState::localizedChanged, this, &WeatherModel::statusChanged);
        connect(clock, &AppState::observerChanged, this, &WeatherModel::updateLocation);
        connect(clock, &AppState::timeChanged, this, [this] {
            const auto hour = static_cast<qint64>(std::floor(m_clock->unixTime() / 3600.0));
            if (hour != m_hour) { m_hour = hour; emit forecastChanged(); }
        });
        updateLocation();
        if (m_enabled) m_refreshTimer.start();
    }
    emit clockChanged();
}

void WeatherModel::updateLocation()
{
    if (!m_clock || !m_clock->hasObserver()) return;
    const bool changed = m_latitude != m_clock->observerLatitude() || m_longitude != m_clock->observerLongitude();
    if (changed) {
        ++m_request; m_busy = false;
        m_latitude = m_clock->observerLatitude(); m_longitude = m_clock->observerLongitude();
        m_hourly = {}; m_fetchedAt = {};
        m_status = [this] { return m_enabled ? tr("天气待获取") : tr("天气查询已关闭"); };
        if (m_enabled) m_locationTimer.start();
    }
    emit statusChanged();
    emit forecastChanged();
}

void WeatherModel::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    m_settings.setValue("weather/enabled", enabled);
    if (enabled) { m_refreshTimer.start(); refresh(); }
    else {
        ++m_request; m_busy = false;
        m_refreshTimer.stop(); m_locationTimer.stop();
        m_hourly = {}; m_fetchedAt = {};
        m_status = [] { return tr("天气查询已关闭"); };
    }
    emit statusChanged(); emit forecastChanged();
}

QString WeatherModel::fetchedAt() const
{
    if (!m_fetchedAt.isValid() || !m_clock) return {};
    return m_fetchedAt.toTimeZone(QTimeZone(m_clock->timeZone().toUtf8())).toString("MM-dd HH:mm");
}

QVariantMap WeatherModel::forecast() const
{
    if (!m_clock || !m_enabled) return {};
    const auto times = m_hourly.value("time").toArray();
    const double selected = m_clock->unixTime();
    if (times.isEmpty() || selected < times.first().toDouble() || selected >= times.last().toDouble() + 3600) return {};
    qsizetype index = 0;
    while (index + 1 < times.size() && times[index + 1].toDouble() <= selected) ++index;
    QVariantMap result{{"time", QDateTime::fromSecsSinceEpoch(times[index].toInteger(), QTimeZone(m_clock->timeZone().toUtf8())).toString("MM-dd HH:mm")}};
    for (const auto *key : variables) {
        const auto values = m_hourly.value(QLatin1String(key)).toArray();
        if (index < values.size() && values[index].isDouble() && std::isfinite(values[index].toDouble()))
            result.insert(QLatin1String(key), values[index].toDouble());
    }
    return result;
}

void WeatherModel::refresh()
{
    if (!m_enabled || m_busy || !m_clock || !m_clock->hasObserver()) return;
    m_locationTimer.stop();
    const auto request = ++m_request;
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery parameters;
    parameters.addQueryItem("latitude", QString::number(m_latitude, 'f', 6));
    parameters.addQueryItem("longitude", QString::number(m_longitude, 'f', 6));
    QStringList names; for (const auto *name : variables) names.append(QLatin1String(name));
    parameters.addQueryItem("hourly", names.join(','));
    parameters.addQueryItem("past_days", "1"); parameters.addQueryItem("forecast_days", "2");
    parameters.addQueryItem("timezone", "GMT"); parameters.addQueryItem("timeformat", "unixtime");
    url.setQuery(parameters);
    QNetworkRequest networkRequest(url); networkRequest.setTransferTimeout(25000);
    auto *reply = m_network.get(networkRequest);
    m_busy = true; m_status = [] { return tr("正在获取天气预报"); }; emit statusChanged();
    connect(reply, &QNetworkReply::finished, this, [this, reply, request] {
        reply->deleteLater();
        if (request != m_request) return;
        m_busy = false;
        const auto hourly = QJsonDocument::fromJson(reply->readAll()).object().value("hourly").toObject();
        const auto times = hourly.value("time").toArray();
        bool valid = !times.isEmpty();
        double previous = -1;
        for (const auto value : times) {
            if (!value.isDouble() || !std::isfinite(value.toDouble()) || value.toDouble() <= previous) { valid = false; break; }
            previous = value.toDouble();
        }
        if (reply->error() == QNetworkReply::NoError && valid) {
            m_hourly = hourly; m_fetchedAt = QDateTime::currentDateTimeUtc();
            m_status = [] { return tr("逐小时预报"); };
            emit forecastChanged();
        } else m_status = [valid = reply->error() == QNetworkReply::NoError, error = reply->errorString()] { return valid ? tr("天气预报暂缺") : tr("天气获取失败：") + error; };
        emit statusChanged();
    });
}
