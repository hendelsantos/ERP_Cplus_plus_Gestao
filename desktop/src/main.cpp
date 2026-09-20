#include "core/auth/auth.h"
#include "core/settings/settings.h"
#include "core/audit/audit.h"
#include "core/database/database.h"
#include "modules/catalog/catalog.h"
#include "modules/inventory/inventory.h"
#include "modules/pos/pos.h"
#include <QQmlContext>
#include "infrastructure/backup/backup.h"
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>
#include <QtQml>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("MH Store"));
    QGuiApplication::setOrganizationName(QStringLiteral("MHSoftware"));

    const auto dataFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataFolder)) return 1;
    QLockFile instanceLock(dataFolder + "/mhstore.lock");
    if (!instanceLock.tryLock(0)) {
        qCritical() << "Outra instância está aberta ou o diretório de dados está indisponível.";
        return 1;
    }
    MHStore::Database::DatabaseManager database;
    QString databaseError;
    if (!database.initialize(&databaseError)) {
        qCritical().noquote() << "Falha ao inicializar o banco local:" << databaseError;
        return 1;
    }

    MHStore::Auth auth;
    MHStore::Audit audit;
    MHStore::Catalog catalog;
    MHStore::Inventory inventory;
    MHStore::Pos pos;
    MHStore::Backup backup;
    MHStore::Settings settings;
    auth.hasPendingCart = [&pos] { return !pos.cart().isEmpty(); };
    settings.hasPendingCart = [&pos] { return !pos.cart().isEmpty(); };
    backup.hasPendingCart = [&pos] { return !pos.cart().isEmpty(); };
    QObject::connect(&backup, &MHStore::Backup::restored, &application, &QCoreApplication::quit, Qt::QueuedConnection);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("authStore", &auth);
    engine.rootContext()->setContextProperty("auditStore", &audit);
    engine.rootContext()->setContextProperty("settingsStore", &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("catalogStore"), &catalog);
    engine.rootContext()->setContextProperty(QStringLiteral("inventoryStore"), &inventory);
    engine.rootContext()->setContextProperty(QStringLiteral("posStore"), &pos);
    engine.rootContext()->setContextProperty(QStringLiteral("backupStore"), &backup);
    engine.load(QUrl(QStringLiteral("qrc:/MHStore/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return application.exec();
}