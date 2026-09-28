#pragma once

#include <QSet>
#include <QSqlDatabase>
#include <QVector>
#include <optional>

struct OrbitSource {
    QString group;
    QString url;
    qint64 snapshot = 0;
    qint64 acquired = 0;
};

struct OrbitSnapshot {
    OrbitSource source;
    QByteArray payload;
    bool pinned = false;
    bool imported = false;
    qint64 bytes = 0;
};

struct SnapshotStorageInfo {
    qint64 count = 0;
    qint64 pinned = 0;
    qint64 imported = 0;
    qint64 bytes = 0;
    qint64 freeBytes = 0;
};

class SnapshotRepository {
  public:
    explicit SnapshotRepository(QSqlDatabase database) : m_database(database) {}

    std::optional<OrbitSnapshot> save(const QByteArray &payload, const QString &source, const QString &group,
                                      QString &error);
    std::optional<OrbitSnapshot> load(qint64 id) const;
    QVector<OrbitSnapshot> list(const QString &group) const;
    QVector<qint64> latestPerGroup() const;
    qint64 latestId(const QString &group) const;
    qint64 latestAcquired(const QString &group) const;
    bool isPinned(qint64 id) const;
    bool setPinned(qint64 id, bool pinned, QString &error);
    SnapshotStorageInfo storageInfo() const;
    QVector<OrbitSnapshot> cleanupCandidates(int retention, const QSet<qint64> &used) const;
    bool remove(const QVector<OrbitSnapshot> &snapshots, QString &error);
    bool needsCompaction() const;
    static QString compact(const QString &path, bool full);

  private:
    QSqlDatabase m_database;
};
