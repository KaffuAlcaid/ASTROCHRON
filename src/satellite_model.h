#pragma once

#include "app_state.h"
#include "orbit_engine.h"
#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QSqlDatabase>
#include <QThreadPool>
#include <QUrl>

class SatelliteModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(AppState *clock READ clock WRITE setClock NOTIFY clockChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY catalogChanged)
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList groups READ groups CONSTANT)
    Q_PROPERTY(QString selectedId READ selectedId WRITE select NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap observation READ observation NOTIFY frameChanged)
    Q_PROPERTY(QVariantList orbitFields READ orbitFields NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList relatedObjects READ relatedObjects NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY frameChanged)
    Q_PROPERTY(QVariantList trajectory READ trajectory NOTIFY trajectoryChanged)
    Q_PROPERTY(QVariantList passes READ passes NOTIFY trajectoryChanged)
    Q_PROPERTY(QVariantList shadowEvents READ shadowEvents NOTIFY trajectoryChanged)
    Q_PROPERTY(QVariantList snapshots READ snapshots NOTIFY catalogChanged)
    Q_PROPERTY(qint64 snapshotId READ snapshotId NOTIFY catalogChanged)
    Q_PROPERTY(QString sourceText READ sourceText NOTIFY catalogChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY statusChanged)
    Q_PROPERTY(bool calculating READ calculating NOTIFY statusChanged)
    Q_PROPERTY(int total READ total NOTIFY catalogChanged)
    Q_PROPERTY(double receiveFrequency READ receiveFrequency WRITE setReceiveFrequency NOTIFY frameChanged)

public:
    explicit SatelliteModel(QObject *parent = nullptr);
    ~SatelliteModel() override;
    enum Role { IdRole = Qt::UserRole + 1, NameRole, OriginalRole, ElevationRole };
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    AppState *clock() const { return m_clock; }
    void setClock(AppState *clock);
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    QString group() const { return m_group; }
    void setGroup(const QString &group);
    QVariantList groups() const;
    QString selectedId() const { return QString::number(m_selected); }
    Q_INVOKABLE void select(const QString &id);
    QVariantMap observation() const { return m_observation; }
    QVariantList orbitFields() const;
    QVariantList relatedObjects() const;
    QVariantList markers() const { return m_markers; }
    QVariantList trajectory() const { return m_trajectory; }
    QVariantList passes() const { return m_passes; }
    QVariantList shadowEvents() const { return m_shadowEvents; }
    QVariantList snapshots() const;
    qint64 snapshotId() const { return m_snapshot; }
    QString sourceText() const { return m_source; }
    QString status() const { return m_status; }
    bool downloading() const { return m_downloading; }
    bool calculating() const { return m_busy; }
    int total() const { return static_cast<int>(m_targets.size()); }
    double receiveFrequency() const { return m_frequency; }
    void setReceiveFrequency(double value);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void importFile(const QUrl &url);
    Q_INVOKABLE void exportSelected(const QUrl &url);
    Q_INVOKABLE void loadSnapshot(qint64 id);
    Q_INVOKABLE void copyDetails();

signals:
    void clockChanged();
    void catalogChanged();
    void selectionChanged();
    void frameChanged();
    void trajectoryChanged();
    void statusChanged();

private:
    void filter();
    void requestFrame();
    void invalidate();
    void install(QVector<Orbit::Satellite> satellites);
    bool store(const QByteArray &payload, const QString &source, const QString &group);
    void setStatus(const QString &status);
    const Orbit::Satellite *selected() const;
    AppState *m_clock = nullptr;
    QNetworkAccessManager m_network;
    QSqlDatabase m_database;
    QThreadPool m_pool;
    QSettings m_settings;
    QVector<Orbit::Satellite> m_satellites;
    QVector<int> m_targets;
    QHash<qint64, qint64> m_owners;
    QHash<qint64, QVector<int>> m_members;
    QVector<int> m_rows;
    QHash<qint64, double> m_elevations;
    QString m_search, m_group = QStringLiteral("active"), m_source, m_status;
    qint64 m_selected = 0, m_snapshot = 0;
    quint64 m_revision = 0;
    double m_trackReference = 0;
    double m_lastTime = 0;
    double m_frequency = 145.8;
    bool m_downloading = false, m_busy = false, m_needTrack = true, m_pending = false;
    QVariantMap m_observation;
    QVariantList m_markers, m_trajectory, m_passes, m_shadowEvents;
};
