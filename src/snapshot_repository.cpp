#include "snapshot_repository.h"
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

std::optional<OrbitSnapshot> SnapshotRepository::save(const QByteArray &payload, const QString &source,
                                                      const QString &group, QString &error)
{
    const auto acquired = QDateTime::currentSecsSinceEpoch();
    QSqlQuery query(m_database);
    query.prepare("INSERT INTO snapshots (group_key, acquired, source, payload, origin) VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(group);
    query.addBindValue(acquired);
    query.addBindValue(source);
    query.addBindValue(payload);
    query.addBindValue(group == "local" ? "import" : "online");
    if (!query.exec()) {
        error = query.lastError().text();
        return std::nullopt;
    }
    return OrbitSnapshot{
        {group, source, query.lastInsertId().toLongLong(), acquired}, payload, false, group == "local", payload.size()};
}

std::optional<OrbitSnapshot> SnapshotRepository::load(qint64 id) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT group_key, source, acquired, payload, pinned, origin FROM snapshots WHERE id=?");
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return std::nullopt;
    const auto payload = query.value(3).toByteArray();
    return OrbitSnapshot{{query.value(0).toString(), query.value(1).toString(), id, query.value(2).toLongLong()},
                         payload,
                         query.value(4).toBool(),
                         query.value(5).toString() == "import",
                         payload.size()};
}

QVector<OrbitSnapshot> SnapshotRepository::list(const QString &group) const
{
    QVector<OrbitSnapshot> result;
    QSqlQuery query(m_database);
    query.prepare(
        "SELECT id, acquired, source, pinned, origin FROM snapshots WHERE group_key=? ORDER BY acquired DESC, id DESC");
    query.addBindValue(group);
    if (query.exec()) {
        while (query.next()) {
            result.append({{group, query.value(2).toString(), query.value(0).toLongLong(), query.value(1).toLongLong()},
                           {},
                           query.value(3).toBool(),
                           query.value(4).toString() == "import"});
        }
    }
    return result;
}

QVector<qint64> SnapshotRepository::latestPerGroup() const
{
    QVector<qint64> ids;
    QSqlQuery query(m_database);
    if (query.exec("SELECT id FROM snapshots WHERE id IN (SELECT MAX(id) FROM snapshots GROUP BY group_key) ORDER BY "
                   "acquired")) {
        while (query.next())
            ids.append(query.value(0).toLongLong());
    }
    return ids;
}

qint64 SnapshotRepository::latestId(const QString &group) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT id FROM snapshots WHERE group_key=? ORDER BY id DESC LIMIT 1");
    query.addBindValue(group);
    return query.exec() && query.next() ? query.value(0).toLongLong() : 0;
}

qint64 SnapshotRepository::latestAcquired(const QString &group) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT MAX(acquired) FROM snapshots WHERE group_key=?");
    query.addBindValue(group);
    return query.exec() && query.next() ? query.value(0).toLongLong() : 0;
}

bool SnapshotRepository::isPinned(qint64 id) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT pinned FROM snapshots WHERE id=?");
    query.addBindValue(id);
    return query.exec() && query.next() && query.value(0).toBool();
}

bool SnapshotRepository::setPinned(qint64 id, bool pinned, QString &error)
{
    QSqlQuery query(m_database);
    query.prepare("UPDATE snapshots SET pinned=? WHERE id=? AND origin='online' AND group_key<>'local'");
    query.addBindValue(pinned ? 1 : 0);
    query.addBindValue(id);
    if (query.exec())
        return true;
    error = query.lastError().text();
    return false;
}

SnapshotStorageInfo SnapshotRepository::storageInfo() const
{
    SnapshotStorageInfo result;
    QSqlQuery query(m_database);
    if (query.exec("SELECT COUNT(*), SUM(pinned), SUM(origin='import' OR group_key='local') FROM snapshots") &&
        query.next()) {
        result.count = query.value(0).toLongLong();
        result.pinned = query.value(1).toLongLong();
        result.imported = query.value(2).toLongLong();
    }
    qint64 pages = 0, freePages = 0, pageSize = 0;
    if (query.exec("PRAGMA page_count") && query.next())
        pages = query.value(0).toLongLong();
    if (query.exec("PRAGMA freelist_count") && query.next())
        freePages = query.value(0).toLongLong();
    if (query.exec("PRAGMA page_size") && query.next())
        pageSize = query.value(0).toLongLong();
    result.bytes = pages * pageSize;
    result.freeBytes = freePages * pageSize;
    return result;
}

QVector<OrbitSnapshot> SnapshotRepository::cleanupCandidates(int retention, const QSet<qint64> &used) const
{
    QVector<OrbitSnapshot> result;
    QHash<QString, int> counts;
    QSqlQuery query(m_database);
    if (!query.exec(
            "SELECT id, group_key, acquired, source, length(payload) FROM snapshots "
            "WHERE origin='online' AND group_key<>'local' AND pinned=0 ORDER BY group_key, acquired DESC, id DESC"))
        return result;
    while (query.next()) {
        const auto group = query.value(1).toString();
        const auto id = query.value(0).toLongLong();
        if (++counts[group] <= retention || used.contains(id))
            continue;
        result.append({{group, query.value(3).toString(), id, query.value(2).toLongLong()},
                       {},
                       false,
                       false,
                       query.value(4).toLongLong()});
    }
    return result;
}

bool SnapshotRepository::remove(const QVector<OrbitSnapshot> &snapshots, QString &error)
{
    bool ok = m_database.transaction();
    if (!ok)
        error = m_database.lastError().text();
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM snapshots WHERE id=? AND pinned=0 AND origin='online' AND group_key<>'local'");
    for (const auto &snapshot : snapshots) {
        if (!ok)
            break;
        query.bindValue(0, snapshot.source.snapshot);
        if (!query.exec()) {
            ok = false;
            error = query.lastError().text();
        }
    }
    query.finish();
    if (ok && !m_database.commit()) {
        ok = false;
        error = m_database.lastError().text();
    }
    if (!ok)
        m_database.rollback();
    return ok;
}

bool SnapshotRepository::needsCompaction() const
{
    const auto info = storageInfo();
    QSqlQuery mode(m_database);
    return mode.exec("PRAGMA auto_vacuum") && mode.next() && mode.value(0).toInt() == 2 &&
           info.freeBytes >= 16 * 1024 * 1024 && info.freeBytes >= info.bytes * 0.2;
}

QString SnapshotRepository::compact(const QString &path, bool full)
{
    const auto connection = QUuid::createUuid().toString();
    QString error;
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", connection);
        database.setDatabaseName(path);
        database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=3000");
        if (!database.open())
            error = database.lastError().text();
        else {
            QSqlQuery query(database);
            if (full) {
                if (!query.exec("PRAGMA auto_vacuum=INCREMENTAL") || !query.exec("VACUUM"))
                    error = query.lastError().text();
            } else {
                while (error.isEmpty()) {
                    if (!query.exec("PRAGMA freelist_count") || !query.next()) {
                        error = query.lastError().text();
                        break;
                    }
                    const auto remaining = query.value(0).toLongLong();
                    query.finish();
                    if (remaining == 0)
                        break;
                    if (!query.exec("PRAGMA incremental_vacuum(256)"))
                        error = query.lastError().text();
                    while (query.next()) {
                    }
                }
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return error;
}
