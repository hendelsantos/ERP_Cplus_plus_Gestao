#pragma once
#include <QString>
#include <QVariantMap>

namespace MHStore {
// Only fixed events enter the log: no SQL, credentials or business data.
class Diagnostics {
public:
    enum class Level { Info, Warning, Error };
    enum class Event { Startup, Shutdown, UncleanShutdown, DatabaseOpenFailed,
        IntegrityFailed, MigrationStarted, MigrationCompleted, MigrationFailed,
        TransactionRollback, BackupStarted, BackupCompleted, BackupFailed, RestoreCompleted, TransactionStartFailed };
    enum class Component { Application, Database, Auth, Catalog, Inventory, Pos, Finance, Settings, Backup };
    static bool start(const QString &dataFolder, qint64 maxBytes=1024*1024);
    static void finish();
    static void record(Level level, Event event, Component component);
    static bool setLevel(int level);
    static QVariantMap report();
};
}
