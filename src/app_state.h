#pragma once

#include <QDateTime>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QTimeZone>
#include <QVariantList>
#include <QVector3D>
#include <QtQml/qqmlregistration.h>

class AppState : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString timeText READ timeText NOTIFY timeChanged)
    Q_PROPERTY(QString startTimeText READ startTimeText NOTIFY timeChanged)
    Q_PROPERTY(QString endTimeText READ endTimeText NOTIFY timeChanged)
    Q_PROPERTY(QString offsetText READ offsetText NOTIFY timeChanged)
    Q_PROPERTY(int minuteOffset READ minuteOffset WRITE setMinuteOffset NOTIFY timeChanged)
    Q_PROPERTY(bool live READ live NOTIFY timeChanged)
    Q_PROPERTY(QVector3D sunDirection READ sunDirection NOTIFY timeChanged)
    Q_PROPERTY(QString observerName READ observerName NOTIFY observerChanged)
    Q_PROPERTY(double observerLatitude READ observerLatitude NOTIFY observerChanged)
    Q_PROPERTY(double observerLongitude READ observerLongitude NOTIFY observerChanged)
    Q_PROPERTY(double observerHeight READ observerHeight NOTIFY observerChanged)
    Q_PROPERTY(bool hasObserverHeight READ hasObserverHeight NOTIFY observerChanged)
    Q_PROPERTY(QString timeZone READ timeZone NOTIFY observerChanged)
    Q_PROPERTY(QString timeZoneName READ timeZoneName NOTIFY timeChanged)
    Q_PROPERTY(QStringList timeZones READ timeZones CONSTANT)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY themeChanged)

public:
    explicit AppState(QObject *parent = nullptr);
    QString timeText() const;
    QString startTimeText() const;
    QString endTimeText() const;
    QString offsetText() const;
    int minuteOffset() const { return m_offset; }
    bool live() const { return m_live; }
    QVector3D sunDirection() const { return m_sun; }
    QString observerName() const { return m_observerName; }
    double observerLatitude() const { return m_latitude; }
    double observerLongitude() const { return m_longitude; }
    double observerHeight() const { return m_height; }
    bool hasObserverHeight() const;
    QString timeZone() const { return QString::fromUtf8(m_timeZone.id()); }
    QString timeZoneName() const;
    QStringList timeZones() const;
    bool darkTheme() const { return m_dark; }
    void setMinuteOffset(int minutes);
    void setDarkTheme(bool dark);
    Q_INVOKABLE void resumeLive();
    Q_INVOKABLE bool setObserver(const QString &name, double latitude, double longitude, double height, const QString &timeZone);
    Q_INVOKABLE QVariantList findCities(const QString &query) const;

signals:
    void timeChanged();
    void observerChanged();
    void themeChanged();

private:
    void updateSun();
    QDateTime selectedTime() const;
    QSettings m_settings;
    QDateTime m_reference;
    QTimer m_timer;
    QTimeZone m_timeZone;
    QVector3D m_sun;
    QString m_observerName;
    double m_latitude;
    double m_longitude;
    double m_height;
    int m_offset = 0;
    bool m_live = true;
    bool m_dark;
};
