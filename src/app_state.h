#pragma once

#include <QDateTime>
#include <QObject>
#include <QNetworkAccessManager>
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
    Q_PROPERTY(int updateFrequency READ updateFrequency WRITE setUpdateFrequency NOTIFY updateFrequencyChanged)
    Q_PROPERTY(double unixTime READ unixTime NOTIFY timeChanged)
    Q_PROPERTY(double referenceTime READ referenceTime NOTIFY timeChanged)
    Q_PROPERTY(double nowTime READ nowTime NOTIFY nowChanged)
    Q_PROPERTY(double ellipsoidHeight READ ellipsoidHeight NOTIFY observerChanged)
    Q_PROPERTY(QString elevationStatus READ elevationStatus NOTIFY elevationChanged)
    Q_PROPERTY(bool elevationBusy READ elevationBusy NOTIFY elevationChanged)
    Q_PROPERTY(bool automaticElevation READ automaticElevation WRITE setAutomaticElevation NOTIFY elevationChanged)
    Q_PROPERTY(double minimumElevation READ minimumElevation WRITE setMinimumElevation NOTIFY observerChanged)
    Q_PROPERTY(QVariantList savedObservers READ savedObservers NOTIFY observerChanged)

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
    int updateFrequency() const { return m_updateFrequency; }
    void setUpdateFrequency(int frequency);
    double unixTime() const { return selectedTime().toMSecsSinceEpoch() / 1000.0; }
    double referenceTime() const { return m_reference.toMSecsSinceEpoch() / 1000.0; }
    double nowTime() const { return m_now.toMSecsSinceEpoch() / 1000.0; }
    Q_INVOKABLE QString formatTime(double unixSeconds, const QString &format = QStringLiteral("HH:mm:ss")) const;
    double ellipsoidHeight() const;
    QString elevationStatus() const { return m_elevationStatus; }
    bool elevationBusy() const { return m_elevationBusy; }
    bool automaticElevation() const { return m_automaticElevation; }
    double minimumElevation() const { return m_minimumElevation; }
    QVariantList savedObservers() const;
    void setAutomaticElevation(bool enabled);
    void setMinimumElevation(double value);
    Q_INVOKABLE void lookupElevation();
    Q_INVOKABLE void saveObserver();
    Q_INVOKABLE void loadObserver(int index);
    Q_INVOKABLE void removeObserver(int index);
    Q_INVOKABLE void seek(double unixSeconds);
    void setMinuteOffset(int minutes);
    void setDarkTheme(bool dark);
    Q_INVOKABLE void resumeLive();
    Q_INVOKABLE bool setObserver(const QString &name, double latitude, double longitude, double height, const QString &timeZone);
    Q_INVOKABLE QVariantList findCities(const QString &query) const;

signals:
    void timeChanged();
    void observerChanged();
    void themeChanged();
    void elevationChanged();
    void updateFrequencyChanged();
    void nowChanged();

private:
    void updateSun();
    QDateTime selectedTime() const;
    QSettings m_settings;
    QDateTime m_reference;
    QDateTime m_now;
    QTimer m_timer;
    QTimeZone m_timeZone;
    QVector3D m_sun;
    QString m_observerName;
    double m_latitude;
    double m_longitude;
    double m_height;
    int m_offset = 0;
    int m_secondOffset = 0;
    int m_millisecondOffset = 0;
    bool m_live = true;
    bool m_dark;
    int m_updateFrequency = 1;
    QNetworkAccessManager m_network;
    QString m_elevationStatus;
    bool m_elevationBusy = false;
    bool m_automaticElevation = true;
    double m_minimumElevation = 10;
    quint64 m_elevationRequest = 0;
};
