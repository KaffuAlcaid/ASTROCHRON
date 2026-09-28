#pragma once

#include "app_state.h"
#include "orbit_types.h"
#include "pass_predictor.h"
#include "observation_database.h"
#include "snapshot_repository.h"
#include "watchlist_model.h"
#include "observation_plan.h"
#include "orbit_frame_calculator.h"
#include "photometry_catalog.h"
#include <QNetworkAccessManager>
#include <QThreadPool>
#include <QUrl>
#include <QSet>
#include <functional>
#include <atomic>
#include <memory>

class SatelliteController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(WatchlistModel *watchlistModel READ watchlistModel CONSTANT)
    Q_PROPERTY(double maximumTrackGap READ maximumTrackGap CONSTANT)
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
    explicit SatelliteController(QObject *parent = nullptr);
    ~SatelliteController() override;
    WatchlistModel *watchlistModel() { return &m_watchlist; }
    double maximumTrackGap() const { return Orbit::maximumSampleGapSeconds; }
    const SatelliteCatalog &catalog() const { return m_catalog; }
    bool usingLocalConstellation() const;
    AppState *clock() const { return m_clock; }
    void setClock(AppState *clock);
    QString search() const { return m_watchlist.search(); }
    void setSearch(const QString &search) { m_watchlist.setSearch(search); }
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
    int total() const { return static_cast<int>(m_watchlist.ids().size()); }
    int visibleCount() const { return m_watchlist.rowCount(); }
    int catalogCount() const { return static_cast<int>(m_catalog.targets().size()); }
    bool selectedWatched() const { return m_watchlist.contains(m_selected); }
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
    QString photometryStatus() const { return m_photometry.status(); }
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
    int watchOrder() const { return m_watchlist.order(); }
    void setWatchOrder(int order) { m_watchlist.setOrder(order); }
    Q_INVOKABLE int watchRow(const QString &id) const { return m_watchlist.rowFor(id.toLongLong()); }

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
    FrameRequest frameRequest() const;
    void applyFrame(FrameResult result);
    void updateMagnitude();
    void requestFrame();
    void invalidate();
    void install(QVector<Orbit::Satellite> satellites, OrbitSource source);
    void refreshPreview();
    std::optional<OrbitSnapshot> store(const QByteArray &payload, const QString &source, const QString &group);
    bool pruneSnapshots();
    void requestWatchPredictions();
    void calculateWatchPredictions();
    void updateWatchRows();
    void setStatus(std::function<QString()> status);
    const Orbit::Satellite *selected() const;
    AppState *m_clock = nullptr;
    QNetworkAccessManager m_network;
    ObservationDatabase m_database;
    SnapshotRepository m_snapshots;
    SatelliteCatalog m_catalog;
    WatchlistModel m_watchlist;
    QThreadPool m_pool;
    QSettings m_settings;
    PhotometryCatalog m_photometry;
    QHash<QString, qint64> m_retryAfter;
    qint64 m_lastWatchedSelection = 0;
    QString m_group = QStringLiteral("catalog");
    std::function<QString()> m_status;
    qint64 m_selected = 0;
    quint64 m_revision = 0;
    double m_trackReference = 0;
    Orbit::Track m_trackCache;
    double m_lastTime = 0;
    double m_calculationTime = 0;
    quint64 m_calculationRevision = 0;
    double m_frequency = 145.8;
    bool m_downloading = false, m_busy = false, m_needTrack = true, m_pending = false;
    std::optional<Orbit::State> m_currentState;
    QVariantMap m_observation;
    QVariantList m_markers, m_trajectory;
    QVector<ShadowEvent> m_shadowEvents;
    QVariantList m_gnssMarkers;
    double m_gnssTime = 0;
    QString m_previewGroup;
    QSet<qint64> m_previewCandidates;
    int m_previewMode = 0, m_previewCount = 0, m_previewTotal = 0;
    bool m_previewBusy = false, m_previewPending = false;
    quint64 m_previewRevision = 0;
    double m_previewTime = 0;
    ObservationPlan m_plan;
    bool m_planBusy = false;
    quint64 m_planRevision = 0;
    std::function<QString()> m_planStatus;
    int m_snapshotRetention = 10;
    bool m_autoCleanup = false, m_storageBusy = false;
    std::function<QString()> m_storageStatus;
    struct WatchPrediction {
        Orbit::Satellite satellite;
        QVector<Orbit::Pass> passes;
    };
    QHash<qint64, WatchPrediction> m_watchPredictions;
    Orbit::Observer m_watchObserver;
    QTimer m_watchTimer;
    std::shared_ptr<std::atomic_bool> m_watchCancel;
    quint64 m_watchGeneration = 0;
    qint64 m_watchDay = -1, m_watchRequestedDay = -1;
    double m_watchClockTime = 0, m_watchNextBoundary = 0;
    bool m_watchBusy = false;
};
