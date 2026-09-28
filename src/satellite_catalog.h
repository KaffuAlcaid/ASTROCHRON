#pragma once

#include "orbit_types.h"
#include "snapshot_repository.h"
#include <QCoreApplication>
#include <QHash>
#include <QSet>

class SatelliteCatalog {
    Q_DECLARE_TR_FUNCTIONS(SatelliteCatalog)
  public:
    void install(QVector<Orbit::Satellite> satellites, const OrbitSource &source);
    const Orbit::Satellite *find(qint64 id) const;
    const Orbit::Satellite &at(int index) const { return m_satellites[index]; }
    bool isEmpty() const { return m_satellites.isEmpty(); }
    int indexOf(qint64 id) const { return m_index.value(id, -1); }
    qint64 ownerOf(qint64 id) const { return m_owners.value(id, id); }
    QVector<int> members(qint64 id) const { return m_members.value(id); }
    const QVector<int> &targets() const { return m_targets; }
    const QVector<int> &gnssTargets() const { return m_gnssTargets; }
    OrbitSource source(qint64 id) const { return m_sources.value(id); }
    OrbitSource groupSource(const QString &group) const { return m_groupSources.value(group); }
    bool hasGroup(const QString &group) const { return m_groupMembers.contains(group); }
    QSet<qint64> groupMembers(const QString &group) const { return m_groupMembers.value(group); }
    QSet<qint64> usedSnapshots() const;
    QString prn(qint64 id, const QString &fallback = {}) const { return m_prns.value(id, fallback); }
    QString plane(qint64 id, const QString &fallback = {}) const { return m_planes.value(id, fallback); }
    QString constellationKey(const Orbit::Satellite &satellite) const;
    static QString constellation(const Orbit::Satellite &satellite);
    static QString constellationName(const QString &key);
    static bool isNavigationGroup(const QString &key);

  private:
    QVector<Orbit::Satellite> m_satellites;
    QHash<qint64, int> m_index;
    QHash<qint64, OrbitSource> m_sources;
    QHash<QString, OrbitSource> m_groupSources;
    QHash<qint64, QString> m_prns;
    QHash<qint64, QString> m_planes;
    QHash<QString, QSet<qint64>> m_groupMembers;
    QVector<int> m_targets;
    QVector<int> m_gnssTargets;
    QHash<qint64, qint64> m_owners;
    QHash<qint64, QVector<int>> m_members;
};
