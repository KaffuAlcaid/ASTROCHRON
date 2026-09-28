#include "catalog_model.h"
#include "satellite_labels.h"

int CatalogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}
QHash<int, QByteArray> CatalogModel::roleNames() const
{
    return {{KeyRole, "entryKey"},         {NameRole, "entryName"},    {CountRole, "memberCount"},
            {GroupRole, "isGroup"},        {ExpandedRole, "expanded"}, {WatchedRole, "watched"},
            {NumberRole, "catalogNumber"}, {PrnRole, "prn"},           {PlaneRole, "plane"},
            {NodeRole, "nodeLongitude"}};
}

QVariant CatalogModel::data(const QModelIndex &index, int role) const
{
    if (!m_source || !index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const auto &row = m_rows[index.row()];
    const bool group = row.satellite < 0;
    const auto *satellite = group ? nullptr : &m_source->catalog().at(row.satellite);
    switch (role) {
    case KeyRole:
        return group ? row.group : QString::number(satellite->number);
    case NameRole:
        return group ? SatelliteCatalog::constellationName(row.group) : Orbit::displayName(*satellite);
    case CountRole:
        return row.count;
    case GroupRole:
        return group;
    case ExpandedRole:
        return group && (m_expanded.contains(row.group) || !m_search.trimmed().isEmpty());
    case WatchedRole:
        return !group && m_source->isWatched(QString::number(satellite->number));
    case NumberRole:
        return group ? QString() : QString::number(satellite->number);
    case PrnRole:
        return group ? QString() : m_source->catalog().prn(satellite->number, tr("暂无"));
    case PlaneRole:
        return group ? QString() : m_source->catalog().plane(satellite->number, tr("暂无"));
    case NodeRole:
        return group ? QString()
                     : QString::number(satellite->elements.value("RA_OF_ASC_NODE").toDouble(), 'f', 2) +
                           QStringLiteral("°");
    default:
        return {};
    }
}

void CatalogModel::setSource(SatelliteController *source)
{
    if (m_source == source)
        return;
    if (m_source)
        disconnect(m_source, nullptr, this, nullptr);
    m_source = source;
    if (source) {
        connect(source, &SatelliteController::localizedChanged, this, [this] {
            if (!m_rows.isEmpty())
                emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1));
            emit groupsChanged();
        });
        connect(source, &SatelliteController::catalogChanged, this, &CatalogModel::rebuild);
        connect(source, &SatelliteController::watchlistChanged, this, [this] {
            if (!m_rows.isEmpty())
                emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {WatchedRole});
        });
    }
    rebuild();
    emit sourceChanged();
}

void CatalogModel::setSearch(const QString &text)
{
    if (m_search == text)
        return;
    m_search = text;
    rebuild();
    emit searchChanged();
}
void CatalogModel::toggleGroup(const QString &key)
{
    if (m_expanded.contains(key))
        m_expanded.remove(key);
    else
        m_expanded.insert(key);
    rebuild();
}
void CatalogModel::revealGroup(const QString &key)
{
    m_expanded.insert(key);
    m_search.clear();
    if (m_source)
        m_source->setGroup(key);
    rebuild();
    emit searchChanged();
}
QVariantList CatalogModel::navigationGroups() const
{
    QVariantList result;
    for (const auto *key : {"gps-ops", "glo-ops", "galileo", "beidou"}) {
        const auto group = QString::fromLatin1(key);
        const auto source = m_source ? m_source->catalog().groupSource(group) : OrbitSource{};
        result.append(QVariantMap{
            {"key", group},
            {"name", SatelliteCatalog::constellationName(group)},
            {"catalogCount", m_counts.value(group)},
            {"groupCount", m_source ? static_cast<int>(m_source->catalog().groupMembers(group).size()) : 0},
            {"loaded", m_source && m_source->catalog().hasGroup(group)},
            {"scope", group.endsWith("-ops") ? tr("运行组") : tr("来源组")},
            {"acquired",
             source.acquired
                 ? QDateTime::fromSecsSinceEpoch(source.acquired, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm:ss 'UTC'")
                 : QString()},
            {"source", source.url}});
    }
    return result;
}
void CatalogModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    m_counts.clear();
    m_resultCount = 0;
    const auto search = m_search.trimmed();
    if (m_source) {
        QHash<QString, QVector<int>> groups;
        const bool localMembers = m_source->usingLocalConstellation();
        for (const int index : m_source->catalog().targets()) {
            const auto &satellite = m_source->catalog().at(index);
            const auto key = m_source->catalog().constellationKey(satellite);
            ++m_counts[key];
            if (m_source->group() != "catalog") {
                bool inSource = localMembers && key == m_source->group();
                const auto members = m_source->catalog().groupMembers(m_source->group());
                for (const int member : m_source->catalog().members(satellite.number))
                    if (members.contains(m_source->catalog().at(member).number))
                        inSource = true;
                if (!inSource)
                    continue;
            }
            bool matches =
                search.isEmpty() || SatelliteCatalog::constellationName(key).contains(search, Qt::CaseInsensitive);
            if (search.startsWith("PRN", Qt::CaseInsensitive)) {
                const auto prn = m_source->catalog().prn(satellite.number);
                const auto requested = search.mid(3).trimmed();
                matches = matches ||
                          (!prn.isEmpty() && (requested.isEmpty() || prn.compare(requested, Qt::CaseInsensitive) == 0));
            }
            for (const int member : m_source->catalog().members(satellite.number)) {
                const auto &entry = m_source->catalog().at(member);
                matches = matches || entry.name.contains(search, Qt::CaseInsensitive) ||
                          Orbit::displayName(entry).contains(search) ||
                          QString::number(entry.number).contains(search) ||
                          entry.internationalId.contains(search, Qt::CaseInsensitive);
            }
            if (matches) {
                groups[key].append(index);
                ++m_resultCount;
            }
        }
        for (const auto *key : {"stations", "gps-ops", "glo-ops", "galileo", "beidou", "starlink", "oneweb", "qianfan",
                                "hulianwang", "kuiper", "iridium-NEXT", "other"}) {
            const auto entries = groups.value(QLatin1String(key));
            if (entries.isEmpty())
                continue;
            m_rows.append({QLatin1String(key), -1, static_cast<int>(entries.size())});
            if (m_expanded.contains(QLatin1String(key)) || !search.isEmpty())
                for (const int index : entries)
                    m_rows.append({QLatin1String(key), index, 0});
        }
    }
    endResetModel();
    emit groupsChanged();
}
