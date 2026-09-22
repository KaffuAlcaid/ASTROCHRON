#pragma once

#include "app_state.h"
#include "orbit_engine.h"
#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QSqlDatabase>
#include <QThreadPool>
#include <QUrl>
#include <QSet>

class CatalogModel;

class SatelliteModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(AppState *clock READ clock WRITE setClock NOTIFY clockChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY watchlistChanged)
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY catalogChanged)
    Q_PROPERTY(QVariantMap groupInfo READ groupInfo NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList groups READ groups CONSTANT)
    Q_PROPERTY(QString selectedId READ selectedId WRITE select NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap observation READ observation NOTIFY frameChanged)
    Q_PROPERTY(QVariantList orbitFields READ orbitFields NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList relatedObjects READ relatedObjects NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY frameChanged)
    Q_PROPERTY(QVariantList gnssMarkers READ gnssMarkers NOTIFY gnssChanged)
    Q_PROPERTY(QVariantList trajectory READ trajectory NOTIFY trajectoryChanged)
    Q_PROPERTY(QVariantList passes READ passes NOTIFY trajectoryChanged)
    Q_PROPERTY(QVariantList shadowEvents READ shadowEvents NOTIFY trajectoryChanged)
    Q_PROPERTY(QVariantList snapshots READ snapshots NOTIFY sourceChanged)
    Q_PROPERTY(qint64 snapshotId READ snapshotId NOTIFY sourceChanged)
    Q_PROPERTY(QString sourceText READ sourceText NOTIFY sourceChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY statusChanged)
    Q_PROPERTY(bool calculating READ calculating NOTIFY statusChanged)
    Q_PROPERTY(int total READ total NOTIFY watchlistChanged)
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY watchlistChanged)
    Q_PROPERTY(int catalogCount READ catalogCount NOTIFY catalogChanged)
    Q_PROPERTY(bool selectedWatched READ selectedWatched NOTIFY watchlistChanged)
    Q_PROPERTY(bool previewActive READ previewActive NOTIFY previewChanged)
    Q_PROPERTY(QString previewName READ previewName NOTIFY previewChanged)
    Q_PROPERTY(int previewMode READ previewMode WRITE setPreviewMode NOTIFY previewChanged)
    Q_PROPERTY(int previewCount READ previewCount NOTIFY frameChanged)
    Q_PROPERTY(int previewTotal READ previewTotal NOTIFY previewChanged)
    Q_PROPERTY(bool previewBusy READ previewBusy NOTIFY previewChanged)
    Q_PROPERTY(double receiveFrequency READ receiveFrequency WRITE setReceiveFrequency NOTIFY receiveFrequencyChanged)

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
    QVariantMap groupInfo() const;
    QString selectedId() const { return QString::number(m_selected); }
    Q_INVOKABLE void select(const QString &id);
    QVariantMap observation() const { return m_observation; }
    QVariantList orbitFields() const;
    QVariantList relatedObjects() const;
    QVariantList markers() const { return m_markers; }
    QVariantList gnssMarkers() const { return m_gnssMarkers; }
    QVariantList trajectory() const { return m_trajectory; }
    QVariantList passes() const { return m_passes; }
    QVariantList shadowEvents() const { return m_shadowEvents; }
    QVariantList snapshots() const;
    qint64 snapshotId() const;
    QString sourceText() const;
    QString status() const { return m_status; }
    bool downloading() const { return m_downloading; }
    bool calculating() const { return m_busy; }
    int total() const { return static_cast<int>(m_watchlist.size()); }
    int visibleCount() const { return static_cast<int>(m_rows.size()); }
    int catalogCount() const { return static_cast<int>(m_targets.size()); }
    bool selectedWatched() const { return m_watchlist.contains(QString::number(m_selected)); }
    Q_INVOKABLE bool isWatched(const QString &id) const;
    Q_INVOKABLE void setWatched(const QString &id, bool watched);
    bool previewActive() const { return !m_previewGroup.isEmpty(); }
    QString previewName() const;
    int previewMode() const { return m_previewMode; }
    void setPreviewMode(int mode);
    int previewCount() const { return m_previewCount; }
    int previewTotal() const { return m_previewTotal; }
    bool previewBusy() const { return m_previewBusy; }
    Q_INVOKABLE void previewConstellation(const QString &key);
    Q_INVOKABLE void closePreview();
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
    void receiveFrequencyChanged();
    void watchlistChanged();
    void previewChanged();
    void sourceChanged();
    void gnssChanged();

private:
    friend class CatalogModel;
    struct Source { QString group; QString url; qint64 snapshot = 0; qint64 acquired = 0; };
    void filter();
    void requestFrame();
    void invalidate();
    void install(QVector<Orbit::Satellite> satellites, Source source);
    void refreshPreview();
    bool store(const QByteArray &payload, const QString &source, const QString &group);
    void setStatus(const QString &status);
    const Orbit::Satellite *selected() const;
    AppState *m_clock = nullptr;
    QNetworkAccessManager m_network;
    QSqlDatabase m_database;
    QThreadPool m_pool;
    QSettings m_settings;
    QVector<Orbit::Satellite> m_satellites;
    QHash<qint64, int> m_index;
    QHash<qint64, Source> m_sources;
    QHash<QString, Source> m_groupSources;
    QHash<qint64, QString> m_prns, m_planes;
    QHash<QString, QSet<qint64>> m_groupMembers;
    QStringList m_watchlist;
    qint64 m_lastWatchedSelection = 0;
    QVector<int> m_targets;
    QVector<int> m_gnssTargets;
    QHash<qint64, qint64> m_owners;
    QHash<qint64, QVector<int>> m_members;
    QVector<int> m_rows;
    QHash<qint64, double> m_elevations;
    QString m_search, m_group = QStringLiteral("catalog"), m_source, m_status;
    qint64 m_selected = 0, m_snapshot = 0;
    quint64 m_revision = 0;
    double m_trackReference = 0;
    double m_lastTime = 0;
    double m_frequency = 145.8;
    bool m_downloading = false, m_busy = false, m_needTrack = true, m_pending = false;
    QVariantMap m_observation;
    QVariantList m_markers, m_trajectory, m_passes, m_shadowEvents;
    QVariantList m_gnssMarkers;
    double m_gnssTime = 0;
    QString m_previewGroup;
    QSet<qint64> m_previewCandidates;
    int m_previewMode = 0, m_previewCount = 0, m_previewTotal = 0;
    bool m_previewBusy = false, m_previewPending = false;
    quint64 m_previewRevision = 0;
    double m_previewTime = 0;
};
