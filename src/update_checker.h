#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class UpdateChecker : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(bool includePrereleases READ includePrereleases WRITE setIncludePrereleases NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY changed)
    Q_PROPERTY(QUrl releaseUrl READ releaseUrl NOTIFY changed)
public:
    explicit UpdateChecker(QObject *parent = nullptr);
    QString currentVersion() const;
    bool includePrereleases() const { return m_includePrereleases; }
    void setIncludePrereleases(bool enabled);
    bool busy() const { return m_state == Checking; }
    bool available() const { return m_state == Available; }
    QString status() const;
    QString releaseNotes() const { return m_notes; }
    QUrl releaseUrl() const { return m_url; }
    Q_INVOKABLE void check();
    Q_INVOKABLE void retranslate() { emit changed(); }
signals:
    void changed();
private:
    enum State { Idle, Checking, Current, Available, Empty, Failed };
    void clearResult();
    QNetworkAccessManager m_network;
    QSettings m_settings;
    State m_state = Idle;
    bool m_includePrereleases = false;
    quint64 m_request = 0;
    int m_httpStatus = 0;
    QString m_error, m_version, m_notes;
    QUrl m_url;
};
