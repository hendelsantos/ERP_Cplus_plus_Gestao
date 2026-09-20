#include "core/auth/auth.h"
#include "core/diagnostics/diagnostics.h"
#include "core/settings/settings.h"
#include "core/audit/audit.h"
#include "core/database/database.h"
#include "modules/catalog/catalog.h"
#include "modules/inventory/inventory.h"
#include "modules/pos/pos.h"
#include "modules/finance/finance.h"
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
    QGuiApplication::setApplicationVersion(QStringLiteral(MHSTORE_VERSION));
    QGuiApplication::setOrganizationName(QStringLiteral("MHSoftware"));

    auto showFailure=[&application](const QString &message) {
        QQmlApplicationEngine failure;
        failure.rootContext()->setContextProperty("startupFailure",message);
        failure.loadData(R"(import QtQuick
import QtQuick.Controls
ApplicationWindow {
    width: 660; height: 400; visible: true; title: "MH Store — recuperação necessária"
    ScrollView { anchors.fill: parent; anchors.margins: 24
        TextArea { text: startupFailure; readOnly: true; wrapMode: TextEdit.Wrap }
    }
})");
        if(!failure.rootObjects().isEmpty()) application.exec();
        return 1;
    };

    const auto dataFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataFolder)) return showFailure("Não foi possível preparar a pasta de dados. Verifique espaço e permissões: "+dataFolder);
    QLockFile instanceLock(dataFolder + "/mhstore.lock");
    if (!instanceLock.tryLock(0)) {
        qCritical() << "Outra instância está aberta ou o diretório de dados está indisponível.";
        return showFailure("Outra instância está aberta ou a pasta de dados está indisponível. Feche a outra janela e tente novamente. Se persistir, verifique as permissões: "+dataFolder);
    }
    MHStore::Diagnostics::start(dataFolder);
    struct SessionGuard { ~SessionGuard() { MHStore::Diagnostics::finish(); } } sessionGuard;
    MHStore::Database::DatabaseManager database;
    QString databaseError;
    if (!database.initialize(&databaseError)) {
        qCritical() << "Falha ao inicializar o banco. Consulte o diagnóstico local.";
        return showFailure(QStringLiteral("Não foi possível abrir o banco com segurança.\n\n")+databaseError+
            "\n\nNão apague o banco. Verifique espaço, permissões e procure suporte para recuperar um backup.\nPasta de dados: "+dataFolder);
    }

    MHStore::Auth auth;
    MHStore::Audit audit;
    MHStore::Catalog catalog;
    MHStore::Inventory inventory;
    MHStore::Pos pos;
    MHStore::Finance finance;
    MHStore::Backup backup;
    MHStore::Settings settings;
    auth.hasPendingCart = [&pos] { return !pos.cart().isEmpty(); };
    settings.hasPendingCart = [&pos] { return !pos.cart().isEmpty(); };
    backup.hasPendingCart = [&pos] { return !pos.cart().isEmpty(); };
    QObject::connect(&pos,&MHStore::Pos::cashClosed,&backup,[&backup] { backup.runAutomatic(true); });
    QObject::connect(&backup, &MHStore::Backup::restored, &application, &QCoreApplication::quit, Qt::QueuedConnection);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("authStore", &auth);
    engine.rootContext()->setContextProperty("auditStore", &audit);
    engine.rootContext()->setContextProperty("settingsStore", &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("catalogStore"), &catalog);
    engine.rootContext()->setContextProperty(QStringLiteral("inventoryStore"), &inventory);
    engine.rootContext()->setContextProperty(QStringLiteral("posStore"), &pos);
    engine.rootContext()->setContextProperty(QStringLiteral("financeStore"), &finance);
    engine.rootContext()->setContextProperty(QStringLiteral("backupStore"), &backup);
    engine.load(QUrl(QStringLiteral("qrc:/MHStore/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    backup.startScheduler();
    return application.exec();
}
