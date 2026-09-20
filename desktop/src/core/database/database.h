#pragma once

#include <QString>

namespace MHStore::Database {

class DatabaseManager
{
public:
    bool initialize(QString *errorMessage = nullptr, const QString &databasePath = {});

private:
    bool applyMigrations(QString *errorMessage);
};

} // namespace MHStore::Database