#include "catalog_model.h"

QString CatalogModel::constellation(const Orbit::Satellite &satellite)
{
    const auto name = satellite.name.toUpper();
    if (satellite.number == 25544 || satellite.number == 48274) return "stations";
    if (name.startsWith("STARLINK-")) return "starlink";
    if (name.startsWith("GPS ") || name.startsWith("NAVSTAR ")) return "gps-ops";
    if (name.contains("GALILEO")) return "galileo";
    if (name.contains("GLONASS")) return "glo-ops";
    if (name.startsWith("BEIDOU")) return "beidou";
    if (name.startsWith("ONEWEB")) return "oneweb";
    if (name.startsWith("QIANFAN")) return "qianfan";
    if (name.startsWith("HULIANWANG")) return "hulianwang";
    if (name.startsWith("KUIPER")) return "kuiper";
    if (name.startsWith("IRIDIUM")) return "iridium-NEXT";
    return "other";
}

QString CatalogModel::constellationName(const QString &key)
{
    static const QHash<QString, QString> names{{"stations", QStringLiteral("空间站")}, {"starlink", "Starlink"},
        {"gps-ops", "GPS"}, {"galileo", "Galileo"}, {"glo-ops", "GLONASS"}, {"beidou", QStringLiteral("北斗")},
        {"oneweb", "OneWeb"}, {"qianfan", QStringLiteral("千帆")}, {"hulianwang", QStringLiteral("互联网低轨")},
        {"kuiper", QStringLiteral("柯伊伯")}, {"iridium-NEXT", QStringLiteral("铱星")}, {"other", QStringLiteral("其他目标")}};
    return names.value(key, key);
}

int CatalogModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : static_cast<int>(m_rows.size()); }
QHash<int, QByteArray> CatalogModel::roleNames() const
{
    return {{KeyRole, "entryKey"}, {NameRole, "entryName"}, {CountRole, "memberCount"}, {GroupRole, "isGroup"},
        {ExpandedRole, "expanded"}, {WatchedRole, "watched"}, {NumberRole, "catalogNumber"},
        {PrnRole, "prn"}, {PlaneRole, "plane"}, {NodeRole, "nodeLongitude"}};
}

QVariant CatalogModel::data(const QModelIndex &index, int role) const
{
    if (!m_source || !index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return {};
    const auto &row = m_rows[index.row()];
    const bool group = row.satellite < 0;
    const auto *satellite = group ? nullptr : &m_source->m_satellites[row.satellite];
    switch (role) {
    case KeyRole: return group ? row.group : QString::number(satellite->number);
    case NameRole: return group ? constellationName(row.group) : Orbit::displayName(*satellite);
    case CountRole: return row.count;
    case GroupRole: return group;
    case ExpandedRole: return group && (m_expanded.contains(row.group) || !m_search.trimmed().isEmpty());
    case WatchedRole: return !group && m_source->isWatched(QString::number(satellite->number));
    case NumberRole: return group ? QString() : QString::number(satellite->number);
    case PrnRole: return group ? QString() : m_source->m_prns.value(satellite->number, QStringLiteral("暂无"));
    case PlaneRole: return group ? QString() : m_source->m_planes.value(satellite->number, QStringLiteral("暂无"));
    case NodeRole: return group ? QString() : QString::number(satellite->elements.value("RA_OF_ASC_NODE").toDouble(), 'f', 2) + QStringLiteral("°");
    default: return {};
    }
}

void CatalogModel::setSource(SatelliteModel *source)
{
    if (m_source == source) return;
    if (m_source) disconnect(m_source, nullptr, this, nullptr);
    m_source = source;
    if (source) {
        connect(source, &SatelliteModel::catalogChanged, this, &CatalogModel::rebuild);
        connect(source, &SatelliteModel::watchlistChanged, this, [this] {
            if (!m_rows.isEmpty()) emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {WatchedRole});
        });
    }
    rebuild(); emit sourceChanged();
}

void CatalogModel::setSearch(const QString &text) { if (m_search == text) return; m_search = text; rebuild(); emit searchChanged(); }
void CatalogModel::toggleGroup(const QString &key)
{
    if (m_expanded.contains(key)) m_expanded.remove(key); else m_expanded.insert(key);
    rebuild();
}
void CatalogModel::revealGroup(const QString &key)
{
    m_expanded.insert(key); m_search.clear();
    if (m_source) m_source->setGroup(key);
    rebuild(); emit searchChanged();
}
QVariantList CatalogModel::navigationGroups() const
{
    QVariantList result;
    for (const auto *key : {"gps-ops", "glo-ops", "galileo", "beidou"}) {
        const auto group = QString::fromLatin1(key);
        const auto source = m_source ? m_source->m_groupSources.value(group) : SatelliteModel::Source{};
        result.append(QVariantMap{{"key", group}, {"name", constellationName(group)},
            {"catalogCount", m_counts.value(group)},
            {"groupCount", m_source ? static_cast<int>(m_source->m_groupMembers.value(group).size()) : 0},
            {"loaded", m_source && m_source->m_groupMembers.contains(group)},
            {"scope", group.endsWith("-ops") ? QStringLiteral("运行组") : QStringLiteral("来源组")},
            {"acquired", source.acquired ? QDateTime::fromSecsSinceEpoch(source.acquired, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm:ss 'UTC'") : QString()},
            {"source", source.url}});
    }
    return result;
}
void CatalogModel::rebuild()
{
    beginResetModel(); m_rows.clear(); m_counts.clear(); m_resultCount = 0;
    const auto search = m_search.trimmed();
    if (m_source) {
        QHash<QString, QVector<int>> groups;
        for (const int index : m_source->m_targets) {
            const auto &satellite = m_source->m_satellites[index];
            const auto key = constellation(satellite);
            ++m_counts[key];
            if (m_source->m_group != "catalog") {
                bool inSource = false;
                const auto members = m_source->m_groupMembers.value(m_source->m_group);
                for (const int member : m_source->m_members.value(satellite.number))
                    if (members.contains(m_source->m_satellites[member].number)) inSource = true;
                if (!inSource) continue;
            }
            bool matches = search.isEmpty() || constellationName(key).contains(search, Qt::CaseInsensitive);
            if (search.startsWith("PRN", Qt::CaseInsensitive)) {
                const auto prn = m_source->m_prns.value(satellite.number);
                const auto requested = search.mid(3).trimmed();
                matches = matches || (!prn.isEmpty() && (requested.isEmpty() || prn.compare(requested, Qt::CaseInsensitive) == 0));
            }
            for (const int member : m_source->m_members.value(satellite.number)) {
                const auto &entry = m_source->m_satellites[member];
                matches = matches || entry.name.contains(search, Qt::CaseInsensitive) || Orbit::displayName(entry).contains(search) ||
                    QString::number(entry.number).contains(search) || entry.internationalId.contains(search, Qt::CaseInsensitive);
            }
            if (matches) { groups[key].append(index); ++m_resultCount; }
        }
        for (const auto *key : {"stations", "gps-ops", "glo-ops", "galileo", "beidou", "starlink", "oneweb", "qianfan", "hulianwang", "kuiper", "iridium-NEXT", "other"}) {
            const auto entries = groups.value(QLatin1String(key));
            if (entries.isEmpty()) continue;
            m_rows.append({QLatin1String(key), -1, static_cast<int>(entries.size())});
            if (m_expanded.contains(QLatin1String(key)) || !search.isEmpty())
                for (const int index : entries) m_rows.append({QLatin1String(key), index, 0});
        }
    }
    endResetModel(); emit groupsChanged();
}
