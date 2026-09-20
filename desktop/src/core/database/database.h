#pragma once

#include <QString>
#include <QSqlDatabase>

namespace MHStore::Database {

class DatabaseManager
{
public:
    bool initialize(QString *errorMessage = nullptr, const QString &databasePath = {}, const QString &connectionName = QSqlDatabase::defaultConnection);

private:
    bool applyMigrations(QString *errorMessage, QSqlDatabase database);
};

} // namespace MHStore::Database