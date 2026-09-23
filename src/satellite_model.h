#pragma once

#include "app_state.h"
#include "orbit_engine.h"
#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QSqlDatabase>
#include <QThreadPool>
#include <QUrl>
#include <QSet>
#include <functional>
#include <atomic>
#include <memory>

class CatalogModel;

class SatelliteModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(AppState *clock READ clock WRITE setClock NOTIFY clockChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY watchlistChanged)
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY catalogChanged)
    Q_PROPERTY(QVariantMap groupInfo READ groupInfo NOTIFY localizedChanged)
    Q_PROPERTY(QVariantList groups READ groups NOTIFY localizedChanged)
    Q_PROPERTY(QString selectedId READ selectedId WRITE select NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap observation READ observation NOTIFY frameChanged)
    Q_PROPERTY(QVariantList orbitFields READ orbitFields NOTIFY detailsChanged)
    Q_PROPERTY(QVariantList relatedObjects READ relatedObjects NOTIFY detailsChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY frameChanged)
    Q_PROPERTY(QVariantList gnssMarkers READ gnssMarkers NOTIFY gnssChanged)
    Q_PROPERTY(QVariantList trajectory READ trajectory NOTIFY trajectoryChanged)
    Q_PROPERTY(QVariantList passes READ passes NOTIFY trajectoryChanged)
    Q_PROPERTY(QVariantList shadowEvents READ shadowEvents NOTIFY trajectoryChanged)
    Q_PROPERTY(QVariantList snapshots READ snapshots NOTIFY sourceChanged)
    Q_PROPERTY(qint64 snapshotId READ snapshotId NOTIFY sourceChanged)
    Q_PROPERTY(QString sourceText READ sourceText NOTIFY sourceChanged)
    Q_PROPERTY(QString elementEpoch READ elementEpoch NOTIFY selectionChanged)
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
    Q_PROPERTY(QVariantMap photometry READ photometry NOTIFY photometryChanged)
    Q_PROPERTY(QString photometryStatus READ photometryStatus NOTIFY photometryChanged)
    Q_PROPERTY(QVariantMap planInfo READ planInfo NOTIFY planChanged)
    Q_PROPERTY(QVariantList planPasses READ planPasses NOTIFY planChanged)
    Q_PROPERTY(bool planBusy READ planBusy NOTIFY planChanged)
    Q_PROPERTY(QString planStatus READ planStatus NOTIFY planChanged)
    Q_PROPERTY(QVariantMap storageInfo READ storageInfo NOTIFY storageChanged)
    Q_PROPERTY(QVariantList cleanupCandidates READ cleanupCandidates NOTIFY storageChanged)
    Q_PROPERTY(int snapshotRetention READ snapshotRetention WRITE setSnapshotRetention NOTIFY storageChanged)
    Q_PROPERTY(bool autoCleanup READ autoCleanup NOTIFY storageChanged)
    Q_PROPERTY(bool storageBusy READ storageBusy NOTIFY storageChanged)
    Q_PROPERTY(QString storageStatus READ storageStatus NOTIFY storageChanged)
    Q_PROPERTY(bool snapshotPinned READ snapshotPinned NOTIFY sourceChanged)
    Q_PROPERTY(bool snapshotImported READ snapshotImported NOTIFY sourceChanged)
    Q_PROPERTY(int watchOrder READ watchOrder WRITE setWatchOrder NOTIFY watchlistChanged)

public:
    explicit SatelliteModel(QObject *parent = nullptr);
    ~SatelliteModel() override;
    enum Role { IdRole = Qt::UserRole + 1, NameRole, OriginalRole, ElevationRole, PredictionRole };
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
    QVariantMap observation() const;
    QVariantList orbitFields() const;
    QVariantList relatedObjects() const;
    QVariantList markers() const { return m_markers; }
    QVariantList gnssMarkers() const { return m_gnssMarkers; }
    QVariantList trajectory() const { return m_trajectory; }
    QVariantList passes() const;
    QVariantList shadowEvents() const;
    QVariantList snapshots() const;
    qint64 snapshotId() const;
    QString sourceText() const;
    QString elementEpoch() const;
    QString status() const { return m_status ? m_status() : QString(); }
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
    QVariantMap photometry() const;
    QString photometryStatus() const { return m_photometryStatus ? m_photometryStatus() : QString(); }
    Q_INVOKABLE bool setPhotometry(double magnitude, int phase, const QString &source, const QString &sourceDate);
    Q_INVOKABLE bool clearPhotometry();
    Q_INVOKABLE bool importMagnitudes(const QUrl &url, const QString &sourceDate);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void importFile(const QUrl &url);
    Q_INVOKABLE void exportSelected(const QUrl &url);
    Q_INVOKABLE void loadSnapshot(qint64 id);
    Q_INVOKABLE void copyDetails();
    Q_INVOKABLE void preparePlan();
    Q_INVOKABLE bool exportPlan(const QUrl &url, const QString &format);
    QVariantMap planInfo() const;
    QVariantList planPasses() const;
    bool planBusy() const { return m_planBusy; }
    QString planStatus() const { return m_planStatus ? m_planStatus() : QString(); }
    QVariantMap storageInfo() const;
    QVariantList cleanupCandidates() const;
    int snapshotRetention() const { return m_snapshotRetention; }
    void setSnapshotRetention(int count);
    bool autoCleanup() const { return m_autoCleanup; }
    bool storageBusy() const { return m_storageBusy; }
    QString storageStatus() const { return m_storageStatus ? m_storageStatus() : QString(); }
    bool snapshotPinned() const;
    bool snapshotImported() const;
    Q_INVOKABLE void pinSnapshot(qint64 id, bool pinned);
    Q_INVOKABLE bool enableCleanup();
    Q_INVOKABLE void disableCleanup();
    Q_INVOKABLE void compactDatabase(bool full = true);
    Q_INVOKABLE void refreshStorage() { emit storageChanged(); }
    int watchOrder() const { return m_watchOrder; }
    void setWatchOrder(int order);

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
    void photometryChanged();
    void localizedChanged();
    void detailsChanged();
    void planChanged();
    void storageChanged();

private:
    friend class CatalogModel;
    struct Source { QString group; QString url; qint64 snapshot = 0; qint64 acquired = 0; };
    struct Photometry { double magnitude = 0; int phase = 90; QString source; bool manual = false; QString sourceDate; qint64 recordedAt = 0; QString internationalId; };
    static QHash<qint64, Photometry> readMagnitudes(QIODevice &input, const Photometry &metadata, int &skipped);
    const Photometry *selectedPhotometry() const;
    void updateMagnitude();
    QString constellationKey(const Orbit::Satellite &satellite) const;
    bool usingLocalConstellation() const;
    void filter();
    void requestFrame();
    void invalidate();
    void install(QVector<Orbit::Satellite> satellites, Source source);
    void refreshPreview();
    bool store(const QByteArray &payload, const QString &source, const QString &group);
    bool initializeDatabase();
    bool pruneSnapshots();
    void requestWatchPredictions();
    void calculateWatchPredictions();
    void updateWatchRows();
    void sortWatchRows();
    void setStatus(std::function<QString()> status);
    const Orbit::Satellite *selected() const;
    AppState *m_clock = nullptr;
    QNetworkAccessManager m_network;
    QSqlDatabase m_database;
    QThreadPool m_pool;
    QSettings m_settings;
    QHash<qint64, Photometry> m_photometry;
    QHash<qint64, Photometry> m_defaultPhotometry;
    std::function<QString()> m_photometryStatus;
    QVector<Orbit::Satellite> m_satellites;
    QHash<qint64, int> m_index;
    QHash<qint64, Source> m_sources;
    QHash<QString, Source> m_groupSources;
    QHash<QString, qint64> m_retryAfter;
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
    QString m_search, m_group = QStringLiteral("catalog"), m_source;
    std::function<QString()> m_status;
    qint64 m_selected = 0, m_snapshot = 0;
    quint64 m_revision = 0;
    double m_trackReference = 0;
    Orbit::Track m_trackCache;
    double m_lastTime = 0;
    double m_calculationTime = 0;
    quint64 m_calculationRevision = 0;
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
    struct Plan {
        Orbit::Satellite satellite;
        Orbit::Observer observer;
        QString observerName, source;
        QTimeZone zone;
        double start = 0, end = 0, height = 0;
        bool heightKnown = false;
        QVector<Orbit::Pass> passes;
    } m_plan;
    bool m_planBusy = false;
    quint64 m_planRevision = 0;
    std::function<QString()> m_planStatus;
    int m_snapshotRetention = 10;
    bool m_autoCleanup = false, m_storageBusy = false;
    std::function<QString()> m_storageStatus;
    struct WatchPrediction { Orbit::Satellite satellite; QVector<Orbit::Pass> passes; };
    QHash<qint64, WatchPrediction> m_watchPredictions;
    QHash<qint64, QString> m_watchSummaries;
    QHash<qint64, double> m_watchNextTimes;
    Orbit::Observer m_watchObserver;
    QTimer m_watchTimer;
    std::shared_ptr<std::atomic_bool> m_watchCancel;
    quint64 m_watchGeneration = 0;
    qint64 m_watchDay = -1, m_watchRequestedDay = -1;
    double m_watchClockTime = 0, m_watchNextBoundary = 0;
    bool m_watchBusy = false;
    int m_watchOrder = 0;
};
