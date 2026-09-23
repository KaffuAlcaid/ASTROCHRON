#pragma once

#include "app_state.h"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <functional>

class WeatherModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(AppState *clock READ clock WRITE setClock NOTIFY clockChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString fetchedAt READ fetchedAt NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap forecast READ forecast NOTIFY forecastChanged)
public:
    explicit WeatherModel(QObject *parent = nullptr);
    AppState *clock() const { return m_clock; }
    void setClock(AppState *clock);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    bool busy() const { return m_busy; }
    QString status() const { return m_status ? m_status() : QString(); }
    QString fetchedAt() const;
    QVariantMap forecast() const;
    Q_INVOKABLE void refresh();
signals:
    void clockChanged();
    void statusChanged();
    void forecastChanged();
private:
    void updateLocation();
    AppState *m_clock = nullptr;
    QNetworkAccessManager m_network;
    QSettings m_settings;
    QTimer m_refreshTimer;
    QTimer m_locationTimer;
    QJsonObject m_hourly;
    QDateTime m_fetchedAt;
    std::function<QString()> m_status;
    double m_latitude = 1000;
    double m_longitude = 1000;
    qint64 m_hour = -1;
    quint64 m_request = 0;
    bool m_enabled = true;
    bool m_busy = false;
};
