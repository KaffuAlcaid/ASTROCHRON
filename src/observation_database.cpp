#include "observation_database.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

ObservationDatabase::ObservationDatabase(const QString &path)
{
    m_database = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
    m_database.setDatabaseName(path);
}

ObservationDatabase::~ObservationDatabase()
{
    const auto name = m_database.connectionName();
    m_database.close();
    m_database = {};
    QSqlDatabase::removeDatabase(name);
}

bool ObservationDatabase::open(std::function<QString()> &error)
{
    if (!m_database.open()) {
        error = [detail = m_database.lastError().text()] { return tr("轨道资料库打开失败：") + detail; };
        return false;
    }
    if (migrate(error))
        return true;
    m_database.close();
    return false;
}

bool ObservationDatabase::migrate(std::function<QString()> &errorText)
{
    const auto setStatus = [&](std::function<QString()> message) { errorText = std::move(message); };
    QSqlQuery query(m_database);
    QString error;
    const auto exec = [&](const QString &sql) {
        if (query.exec(sql))
            return true;
        error = query.lastError().text();
        return false;
    };
    if (!exec("PRAGMA user_version") || !query.next()) {
        setStatus([error] { return tr("数据库初始化失败：%1").arg(error); });
        return false;
    }
    int version = query.value(0).toInt();
    query.finish();
    if (version > 2) {
        setStatus([] { return tr("数据库版本较高，请使用对应版本的 ASTROCHRON"); });
        return false;
    }
    if (m_database.tables().isEmpty() && !exec("PRAGMA auto_vacuum=INCREMENTAL")) {
        setStatus([error] { return tr("数据库初始化失败：%1").arg(error); });
        return false;
    }
    while (version < 2) {
        bool ok = m_database.transaction();
        if (!ok)
            error = m_database.lastError().text();
        if (ok && version == 0) {
            ok = exec("CREATE TABLE IF NOT EXISTS snapshots (id INTEGER PRIMARY KEY, group_key TEXT NOT NULL, acquired "
                      "INTEGER NOT NULL, source TEXT NOT NULL, payload BLOB NOT NULL)") &&
                 exec("CREATE TABLE IF NOT EXISTS photometry (norad INTEGER PRIMARY KEY, magnitude REAL NOT NULL, "
                      "phase INTEGER NOT NULL, source TEXT NOT NULL, manual INTEGER NOT NULL)");
            const auto columns = m_database.record("photometry");
            if (ok && !columns.contains("source_date"))
                ok = exec("ALTER TABLE photometry ADD COLUMN source_date TEXT NOT NULL DEFAULT ''");
            if (ok && !columns.contains("recorded_at"))
                ok = exec("ALTER TABLE photometry ADD COLUMN recorded_at INTEGER NOT NULL DEFAULT 0");
        } else if (ok && version == 1) {
            ok = exec("ALTER TABLE snapshots ADD COLUMN origin TEXT NOT NULL DEFAULT 'online'") &&
                 exec("ALTER TABLE snapshots ADD COLUMN pinned INTEGER NOT NULL DEFAULT 0") &&
                 exec("UPDATE snapshots SET origin='import' WHERE group_key='local'") &&
                 exec("CREATE INDEX snapshots_group_acquired ON snapshots(group_key, acquired DESC, id DESC)");
        }
        if (ok)
            ok = exec(QStringLiteral("PRAGMA user_version=%1").arg(version + 1));
        if (ok && !m_database.commit()) {
            ok = false;
            error = m_database.lastError().text();
        }
        if (!ok) {
            m_database.rollback();
            setStatus([error] { return tr("数据库升级失败：%1").arg(error); });
            return false;
        }
        ++version;
    }
    return true;
}
