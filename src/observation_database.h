#pragma once

#include <QSqlDatabase>
#include <QCoreApplication>
#include <functional>

class ObservationDatabase {
    Q_DECLARE_TR_FUNCTIONS(ObservationDatabase)
  public:
    explicit ObservationDatabase(const QString &path);
    ~ObservationDatabase();
    ObservationDatabase(const ObservationDatabase &) = delete;
    ObservationDatabase &operator=(const ObservationDatabase &) = delete;

    bool open(std::function<QString()> &error);
    QSqlDatabase connection() const { return m_database; }
    bool isOpen() const { return m_database.isOpen(); }
    QString path() const { return m_database.databaseName(); }

  private:
    bool migrate(std::function<QString()> &error);
    QSqlDatabase m_database;
};
