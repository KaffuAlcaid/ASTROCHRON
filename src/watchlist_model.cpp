#include "watchlist_model.h"
#include "satellite_labels.h"
#include <algorithm>
#include <limits>

namespace {
QString number(double value, int decimals)
{
    return QString::number(value, 'f', decimals);
}
} // namespace

WatchlistModel::WatchlistModel(const SatelliteCatalog &catalog, QObject *parent)
    : QAbstractListModel(parent), m_catalog(catalog)
{
    m_ids = m_settings.value("watchlist/ids", QStringList{"25544", "48274"}).toStringList();
    m_ids.removeDuplicates();
    m_order = m_settings.value("watchlist/order", 0).toInt() == 1 ? 1 : 0;
}

int WatchlistModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}
QHash<int, QByteArray> WatchlistModel::roleNames() const
{
    return {{IdRole, "satelliteId"},
            {NameRole, "satelliteName"},
            {OriginalRole, "originalName"},
            {ElevationRole, "elevationText"},
            {PredictionRole, "predictionText"}};
}
QVariant WatchlistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const auto &satellite = m_catalog.at(m_rows[index.row()]);
    switch (role) {
    case IdRole:
        return QString::number(satellite.number);
    case NameRole:
        return Orbit::displayName(satellite);
    case OriginalRole:
        return satellite.name;
    case ElevationRole:
        return m_elevations.contains(satellite.number)
                   ? number(m_elevations.value(satellite.number), 1) + QStringLiteral("°")
                   : tr("待计算");
    case PredictionRole:
        return m_summaries.value(satellite.number, m_hasObserver ? tr("预报计算中") : tr("请选择观测地点"));
    default:
        return {};
    }
}

void WatchlistModel::setSearch(const QString &value)
{
    if (m_search == value)
        return;
    m_search = value;
    rebuild();
    emit changed();
}
void WatchlistModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    const auto search = m_search.trimmed();
    for (const auto &id : m_ids) {
        const int i = m_catalog.indexOf(m_catalog.ownerOf(id.toLongLong()));
        if (i < 0)
            continue;
        for (const int member : m_catalog.members(m_catalog.at(i).number)) {
            const auto &satellite = m_catalog.at(member);
            const bool stationAlias =
                satellite.number == 48274 &&
                QStringLiteral("天宫 中国空间站 天和 TIANHE CSS").contains(search, Qt::CaseInsensitive);
            if (search.isEmpty() || satellite.name.contains(search, Qt::CaseInsensitive) ||
                Orbit::displayName(satellite).contains(search) || stationAlias ||
                QString::number(satellite.number).contains(search) ||
                satellite.internationalId.contains(search, Qt::CaseInsensitive)) {
                m_rows.append(i);
                break;
            }
        }
    }
    endResetModel();
    sortRows();
}

void WatchlistModel::setOrder(int order)
{
    order = order == 1 ? 1 : 0;
    if (m_order == order)
        return;
    m_order = order;
    m_settings.setValue("watchlist/order", order);
    sortRows();
    emit changed();
}

void WatchlistModel::sortRows()
{
    auto sorted = m_rows;
    const auto position = [&](int row) { return m_ids.indexOf(QString::number(m_catalog.at(row).number)); };
    std::stable_sort(sorted.begin(), sorted.end(), [&](int a, int b) {
        if (m_order == 1) {
            const double ta = m_nextTimes.value(m_catalog.at(a).number, std::numeric_limits<double>::infinity());
            const double tb = m_nextTimes.value(m_catalog.at(b).number, std::numeric_limits<double>::infinity());
            if (ta != tb)
                return ta < tb;
        }
        return position(a) < position(b);
    });
    for (qsizetype i = 0; i < sorted.size(); ++i) {
        const auto from = m_rows.indexOf(sorted[i]);
        if (from == i)
            continue;
        beginMoveRows({}, static_cast<int>(from), static_cast<int>(from), {}, static_cast<int>(i));
        m_rows.move(from, i);
        endMoveRows();
    }
}

bool WatchlistModel::setWatched(qint64 id, bool watched)
{
    const QString key = QString::number(id);
    if (watched == m_ids.contains(key) || (watched && !m_catalog.find(id)))
        return false;
    if (watched)
        m_ids.append(key);
    else
        m_ids.removeAll(key);
    m_settings.setValue("watchlist/ids", m_ids);
    rebuild();
    emit changed();
    return true;
}

int WatchlistModel::rowFor(qint64 id) const
{
    return static_cast<int>(m_rows.indexOf(m_catalog.indexOf(m_catalog.ownerOf(id))));
}

void WatchlistModel::setElevations(QHash<qint64, double> elevations)
{
    m_elevations = std::move(elevations);
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(rowCount() - 1), {ElevationRole});
}

void WatchlistModel::setPredictions(QHash<qint64, QString> summaries, QHash<qint64, double> nextTimes)
{
    if (summaries == m_summaries && nextTimes == m_nextTimes)
        return;
    m_summaries = std::move(summaries);
    m_nextTimes = std::move(nextTimes);
    sortRows();
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(rowCount() - 1), {PredictionRole});
}

void WatchlistModel::setHasObserver(bool hasObserver)
{
    m_hasObserver = hasObserver;
    retranslate();
}

void WatchlistModel::retranslate()
{
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(rowCount() - 1));
}
