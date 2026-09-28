#include "satellite_catalog.h"
#include "orbital_elements_parser.h"
#include <QRegularExpression>

namespace {
qint64 stationFor(const Orbit::Satellite &satellite)
{
    switch (satellite.number) {
    case 25544:
    case 36086:
    case 49044:
        return 25544;
    case 48274:
    case 53239:
    case 54216:
        return 48274;
    default:
        return 0;
    }
}

qint64 visitingStation(const QString &name)
{
    for (const auto *prefix : {"TIANZHOU-", "SHENZHOU-"})
        if (name.startsWith(QLatin1String(prefix), Qt::CaseInsensitive))
            return 48274;
    for (const auto *prefix : {"CREW DRAGON ", "DRAGON ", "CYGNUS ", "PROGRESS-MS ", "SOYUZ-MS "})
        if (name.startsWith(QLatin1String(prefix), Qt::CaseInsensitive))
            return 25544;
    return 0;
}
} // namespace

QString SatelliteCatalog::constellation(const Orbit::Satellite &satellite)
{
    const auto name = satellite.name.toUpper();
    if (satellite.number == 25544 || satellite.number == 48274)
        return "stations";
    if (name.startsWith("STARLINK-"))
        return "starlink";
    if (name.startsWith("GPS ") || name.startsWith("NAVSTAR "))
        return "gps-ops";
    if (name.contains("GALILEO"))
        return "galileo";
    if (name.contains("GLONASS"))
        return "glo-ops";
    if (name.startsWith("BEIDOU"))
        return "beidou";
    if (name.startsWith("ONEWEB"))
        return "oneweb";
    if (name.startsWith("QIANFAN"))
        return "qianfan";
    if (name.startsWith("HULIANWANG"))
        return "hulianwang";
    if (name.startsWith("KUIPER"))
        return "kuiper";
    if (name.startsWith("IRIDIUM"))
        return "iridium-NEXT";
    return "other";
}

QString SatelliteCatalog::constellationName(const QString &key)
{
    static const QHash<QString, const char *> names{{"stations", QT_TR_NOOP("空间站")},
                                                    {"starlink", "Starlink"},
                                                    {"gps-ops", "GPS"},
                                                    {"galileo", QT_TR_NOOP("伽利略")},
                                                    {"glo-ops", "GLONASS"},
                                                    {"beidou", QT_TR_NOOP("北斗")},
                                                    {"oneweb", "OneWeb"},
                                                    {"qianfan", QT_TR_NOOP("千帆")},
                                                    {"hulianwang", QT_TR_NOOP("互联网低轨")},
                                                    {"kuiper", QT_TR_NOOP("柯伊伯")},
                                                    {"iridium-NEXT", QT_TR_NOOP("铱星")},
                                                    {"other", QT_TR_NOOP("其他目标")}};
    const auto name = names.value(key, nullptr);
    return name ? tr(name) : key;
}

QString SatelliteCatalog::constellationKey(const Orbit::Satellite &satellite) const
{
    const auto key = constellation(satellite);
    if (key != "other")
        return key;
    for (const auto *group : {"gps-ops", "glo-ops", "galileo", "beidou"})
        if (m_groupMembers.value(QLatin1String(group)).contains(satellite.number))
            return QLatin1String(group);
    return key;
}

void SatelliteCatalog::install(QVector<Orbit::Satellite> satellites, const OrbitSource &source)
{
    m_groupSources.insert(source.group, source);
    QSet<qint64> sourceMembers;
    static const QRegularExpression prnPattern(QStringLiteral("PRN\\s*([A-Z]?\\d+)"),
                                               QRegularExpression::CaseInsensitiveOption);
    for (auto &satellite : satellites) {
        const auto id = satellite.number;
        const auto prn = prnPattern.match(satellite.name);
        if (satellite.elements.contains("PRN"))
            m_prns[id] = satellite.elements.value("PRN").toVariant().toString();
        else if (prn.hasMatch())
            m_prns[id] = prn.captured(1);
        if (satellite.elements.contains("ORBITAL_PLANE"))
            m_planes[id] = satellite.elements.value("ORBITAL_PLANE").toVariant().toString();
        sourceMembers.insert(id);
        m_sources.insert(id, source);
        if (m_index.contains(id))
            m_satellites[m_index.value(id)] = std::move(satellite);
        else {
            m_index.insert(id, static_cast<int>(m_satellites.size()));
            m_satellites.append(std::move(satellite));
        }
    }
    m_groupMembers[source.group] = sourceMembers;
    m_targets.clear();
    m_owners.clear();
    m_members.clear();
    QHash<qint64, int> indices;
    for (int i = 0; i < m_satellites.size(); ++i)
        indices.insert(m_satellites[i].number, i);
    QVector<int> stationRecords;
    for (int i = 0; i < m_satellites.size(); ++i) {
        const auto &satellite = m_satellites[i];
        const qint64 station = stationFor(satellite);
        m_owners.insert(satellite.number, station && indices.contains(station) ? station : satellite.number);
        if (station && indices.contains(station))
            stationRecords.append(i);
    }
    // Visiting spacecraft reappear separately when the source supplies an independent orbit.
    for (int i = 0; i < m_satellites.size(); ++i) {
        const auto &satellite = m_satellites[i];
        if (!stationFor(satellite)) {
            const auto station = visitingStation(satellite.name);
            if (station && indices.contains(station))
                for (const int candidate : stationRecords) {
                    if (stationFor(m_satellites[candidate]) == station &&
                        Orbit::sameElements(satellite, m_satellites[candidate])) {
                        m_owners[satellite.number] = station;
                        break;
                    }
                }
        }
        m_members[m_owners.value(satellite.number)].append(i);
    }
    for (int i = 0; i < m_satellites.size(); ++i)
        if (m_owners.value(m_satellites[i].number) == m_satellites[i].number)
            m_targets.append(i);
    m_gnssTargets.clear();
    for (const int index : m_targets) {
        const auto key = constellationKey(m_satellites[index]);
        if (!isNavigationGroup(key))
            continue;
        if (!m_groupMembers.contains(key) || m_groupMembers.value(key).contains(m_satellites[index].number))
            m_gnssTargets.append(index);
    }
}

const Orbit::Satellite *SatelliteCatalog::find(qint64 id) const
{
    const auto index = indexOf(id);
    return index >= 0 ? &m_satellites[index] : nullptr;
}

QSet<qint64> SatelliteCatalog::usedSnapshots() const
{
    QSet<qint64> used;
    for (const auto &source : m_sources)
        used.insert(source.snapshot);
    for (const auto &source : m_groupSources)
        used.insert(source.snapshot);
    return used;
}

bool SatelliteCatalog::isNavigationGroup(const QString &key)
{
    return key == "gps-ops" || key == "glo-ops" || key == "galileo" || key == "beidou";
}
