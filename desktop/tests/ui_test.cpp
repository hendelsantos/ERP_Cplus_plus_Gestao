#include "auth_fixture.h"
#include "core/settings/settings.h"
#include "core/audit/audit.h"
#include "core/database/database.h"
#include "modules/catalog/catalog.h"
#include "modules/inventory/inventory.h"
#include "modules/pos/pos.h"

#include "infrastructure/backup/backup.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>

class UiTest : public QObject
{
    Q_OBJECT
    QQuickWindow *window = nullptr;
    QQuickItem *findItem(QQuickItem *parent, const std::function<bool(QQuickItem *)> &matches)
    {
        if (parent->isVisible() && matches(parent)) return parent;
        for (auto *child : parent->childItems())
            if (auto *found = findItem(child, matches)) return found;
        return nullptr;
    }
    bool click(const QString &text)
    {
        auto *item = findItem(window->contentItem(), [&](QQuickItem *candidate) {
            return candidate->property("text").toString().startsWith(text) && candidate->metaObject()->indexOfSignal("clicked()") >= 0;
        });
        if (!item) return false;
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
        QTest::qWait(50);
        return true;
    }
    bool fill(const QString &name, const QString &text)
    {
        auto *item = findItem(window->contentItem(), [&](QQuickItem *candidate) { return candidate->objectName() == name; });
        return item && item->setProperty("text", text);
    }
private slots:
    void offlineLoginWorkflow() {
        QTemporaryDir directory;
        MHStore::Database::DatabaseManager database;
        QVERIFY(database.initialize(nullptr,directory.filePath("auth-ui.sqlite")));
        MHStore::Auth::resetSession();
        {
            MHStore::Auth auth;
            MHStore::Audit audit;
            MHStore::Catalog catalog;
            MHStore::Inventory inventory;
            MHStore::Pos pos;
            MHStore::Backup backup;
            MHStore::Settings settings;
            QQmlApplicationEngine engine;
            QStringList warnings;
            connect(&engine,&QQmlEngine::warnings,this,[&](const QList<QQmlError> &errors) { for(const auto &e:errors) warnings << e.toString(); });
            engine.rootContext()->setContextProperty("authStore",&auth);
            engine.rootContext()->setContextProperty("auditStore",&audit);
            engine.rootContext()->setContextProperty("catalogStore",&catalog);
            engine.rootContext()->setContextProperty("inventoryStore",&inventory);
            engine.rootContext()->setContextProperty("posStore",&pos);
            engine.rootContext()->setContextProperty("backupStore",&backup);
            engine.rootContext()->setContextProperty("settingsStore",&settings);
            engine.load(QUrl::fromLocalFile(QStringLiteral(MHSTORE_QML_DIR "/Main.qml")));
            QVERIFY(!engine.rootObjects().isEmpty());
            window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            QVERIFY(window); window->resize(960,640);
            QVERIFY(QTest::qWaitForWindowExposed(window));
            QVERIFY(!catalog.save("Clientes",0,{{"name","Bloqueado"}}));
            QVERIFY(!inventory.move(1,"entry","1","Teste","Falso"));
            QVERIFY(!backup.create(directory.filePath("backup")));
            QVERIFY(fill("authName","Administrador"));
            QVERIFY(fill("authLogin","admin"));
            QVERIFY(fill("authPassword","SenhaTeste123!"));
            QVERIFY(fill("authConfirm","SenhaTeste123!"));
            QVERIFY(click("Criar administrador"));
            QVERIFY(!auth.needsSetup()); QVERIFY(!auth.authenticated());
            QVERIFY(click("Guardei o código"));
            QVERIFY(fill("authPassword","incorreta")); QVERIFY(click("Entrar"));
            QVERIFY(!auth.authenticated());
            QVERIFY(fill("authPassword","SenhaTeste123!")); QVERIFY(click("Entrar"));
            QVERIFY(auth.authenticated());
            QVERIFY(click("Usuários"));
            const auto folder=qEnvironmentVariable("MHSTORE_TEST_SCREENSHOTS");
            if(!folder.isEmpty()) window->grabWindow().save(folder+"/users.png");
            QVERIFY(auth.saveUser(0,"Caixa","caixa","SenhaCaixa123!","operator",true));
            QVERIFY(click("Minha senha"));
            QVERIFY(fill("currentPassword","incorreta"));
            QVERIFY(fill("newPassword","NovaSenha12345!"));
            QVERIFY(fill("confirmPassword","NovaSenha12345!"));
            QVERIFY(click("Alterar senha"));
            QVERIFY(auth.authenticated());
            if(!folder.isEmpty()) window->grabWindow().save(folder+"/password.png");
            QVERIFY(fill("currentPassword","SenhaTeste123!"));
            QVERIFY(fill("newPassword","NovaSenha12345!"));
            QVERIFY(fill("confirmPassword","NovaSenha12345!"));
            QVERIFY(click("Alterar senha"));
            QVERIFY(auth.authenticated());
            QVERIFY(click("Usuários"));
            QVERIFY(auth.saveUser(0,"Backup","backup","SenhaBackup123!","admin",true));
            QVERIFY(click("Backup • backup"));
            QVERIFY(click("Código de recuperação"));
            QCOMPARE(auth.recoveryCode().size(),64);
            const QString issuedCode=auth.recoveryCode();
            if(!folder.isEmpty()) window->grabWindow().save(folder+"/recovery-code.png");
            QVERIFY(click("Guardei o código"));
            QVERIFY(auth.recoveryCode().isEmpty());
            QVERIFY(click("Auditoria"));
            QVERIFY(audit.refresh());
            QCOMPARE(audit.entries().size(),4);
            QCOMPARE(audit.entries().at(0).toMap().value("action").toString(),QString("user.recovery_code"));
            QCOMPARE(audit.entries().at(1).toMap().value("action").toString(),QString("user.create"));
            QCOMPARE(audit.entries().at(2).toMap().value("action").toString(),QString("user.password"));
            QCOMPARE(audit.entries().at(3).toMap().value("action").toString(),QString("user.create"));
            QVERIFY(!audit.moreAvailable());
            if(!folder.isEmpty()) window->grabWindow().save(folder+"/audit.png");
            QVERIFY(click("Sair"));
            QVERIFY(!auth.authenticated());
            if(!folder.isEmpty()) window->grabWindow().save(folder+"/login.png");
            QVERIFY(fill("authLogin","caixa")); QVERIFY(fill("authPassword","SenhaCaixa123!"));
            QVERIFY(click("Entrar"));
            QVERIFY(auth.authenticated());
            QVERIFY(!click("Usuários"));
            QVERIFY(!click("Configurações"));
            QVERIFY(!catalog.save("Clientes",0,{{"name","Bloqueado"}}));
            QVERIFY(!catalog.setActive("Clientes",1,false));
            QVERIFY(!inventory.move(1,"entry","1","Teste","Falso"));
            QVERIFY(!backup.create(directory.filePath("backup")));
            QVERIFY(!backup.restore(directory.filePath("missing")));
            QVERIFY(click("Sair"));
            QVERIFY(!auth.authenticated());
            QVERIFY(fill("authLogin","admin"));
            QVERIFY(fill("authPassword","SenhaTeste123!"));
            QVERIFY(click("Entrar"));
            QVERIFY(!auth.authenticated());
            QVERIFY(fill("authPassword","NovaSenha12345!"));
            QVERIFY(click("Entrar"));
            QVERIFY(auth.authenticated());
            QVERIFY(click("Sair"));
            QVERIFY(!auth.authenticated());
            QVERIFY(click("Esqueci minha senha"));
            QVERIFY(fill("authLogin","backup"));
            QVERIFY(fill("authCode",issuedCode));
            QVERIFY(fill("authPassword","SenhaBackupNova1!"));
            QVERIFY(fill("authConfirm","SenhaBackupNova1!"));
            QVERIFY(click("Redefinir senha"));
            QCOMPARE(auth.recoveryCode().size(),64);
            QVERIFY(auth.recoveryCode()!=issuedCode);
            if(!folder.isEmpty()) window->grabWindow().save(folder+"/recovery-rotated.png");
            QVERIFY(click("Guardei o código"));
            QVERIFY(fill("authLogin","backup"));
            QVERIFY(fill("authPassword","SenhaBackupNova1!"));
            QVERIFY(click("Entrar"));
            QVERIFY(auth.authenticated());
            QVERIFY(click("Sair"));
            QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
            window=nullptr;
        }
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
    void catalogAndInventoryWorkflow()
    {
        QTemporaryDir directory;
        MHStore::Database::DatabaseManager database;
        QString error;
        QVERIFY2(database.initialize(&error, directory.filePath("ui.sqlite")), qPrintable(error));
        QVERIFY(authenticateTestAdmin());
        {
            MHStore::Auth auth;
            MHStore::Audit audit;
            MHStore::Catalog catalog;
            QVERIFY(catalog.save("Clientes",0,{{"name","Cliente de teste"}}));
            MHStore::Inventory inventory;
            MHStore::Pos pos;
            MHStore::Backup backup;
            MHStore::Settings settings;
            settings.hasPendingCart = [&pos] { return !pos.cart().isEmpty(); };
            QQmlApplicationEngine engine;
            engine.rootContext()->setContextProperty("authStore", &auth);
            engine.rootContext()->setContextProperty("auditStore", &audit);
            engine.rootContext()->setContextProperty("settingsStore", &settings);
            QStringList warnings;
            connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError> &errors) {
                for (const auto &entry : errors) warnings << entry.toString();
            });
            engine.rootContext()->setContextProperty("catalogStore", &catalog);
            engine.rootContext()->setContextProperty("inventoryStore", &inventory);
            engine.rootContext()->setContextProperty("posStore", &pos);
            engine.rootContext()->setContextProperty("backupStore", &backup);
            engine.load(QUrl::fromLocalFile(QStringLiteral(MHSTORE_QML_DIR "/Main.qml")));
            QVERIFY(!engine.rootObjects().isEmpty());
            window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            QVERIFY(window);
            QVERIFY(QTest::qWaitForWindowExposed(window));
            QVERIFY(click("Categorias"));
            QVERIFY(click("Novo cadastro"));
            QVERIFY(fill("catalogName", "Roupas"));
            QVERIFY(click("Salvar"));
            QCOMPARE(catalog.categories().size(), 1);
            QVERIFY(click("Fornecedores"));
            QVERIFY(click("Novo cadastro"));
            QVERIFY(fill("catalogName", "Confecção Sul"));
            QVERIFY(fill("documentField", "12345678000199"));
            QVERIFY(fill("phoneField", "5433221100"));
            QVERIFY(click("Salvar"));
            QCOMPARE(catalog.suppliers().size(), 1);
            const auto suppliersFolder = qEnvironmentVariable("MHSTORE_TEST_SCREENSHOTS");
            if (!suppliersFolder.isEmpty()) window->grabWindow().save(suppliersFolder + "/suppliers.png");
            QVERIFY(click("Produtos"));
            QVERIFY(click("Novo cadastro"));
            QVERIFY(fill("catalogName", "Camiseta"));
            QVERIFY(fill("catalogCode", "CAM-01"));
            QVERIFY(fill("catalogPrice", "19,90"));
            auto *supplierChoice = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName()=="supplierField"; });
            QVERIFY(supplierChoice);
            QVERIFY(supplierChoice->setProperty("currentIndex", 1));
            QVERIFY(click("Salvar"));
            QCOMPARE(catalog.rows().size(), 1);
            QCOMPARE(catalog.rows().first().toMap().value("supplier_id").toInt(), 1);
            QVERIFY(click("Estoque"));
            QCOMPARE(inventory.products().size(), 1);
            QVERIFY(click("Movimentar"));
            QVERIFY(fill("movementQuantity", "5,250"));
            QVERIFY(fill("movementReason", "Recebimento"));
            QVERIFY(fill("movementOperator", "Ana"));
            QVERIFY(click("Registrar"));
            QCOMPARE(inventory.history().size(), 1);
            QCOMPARE(inventory.products().first().toMap().value("stock_quantity").toDouble(), 5.25);
            QVERIFY(click("Produtos"));
            QCOMPARE(catalog.rows().first().toMap().value("stock_quantity").toDouble(), 5.25);
            window->resize(960,640);
            QTest::qWait(100);
            QVERIFY(click("Editar"));
            QTest::qWait(100);
            const auto screenshot = window->grabWindow();
            const auto screenshotDirectory = qEnvironmentVariable("MHSTORE_TEST_SCREENSHOTS");
            if (!screenshotDirectory.isEmpty() && !screenshot.isNull())
                screenshot.save(screenshotDirectory + "/catalog.png");
            QVERIFY(click("Cancelar"));
            QVERIFY(click("Estoque"));
            QVERIFY(click("Histórico"));
            QCOMPARE(inventory.history().size(), 1);
            if (!screenshotDirectory.isEmpty())
                window->grabWindow().save(screenshotDirectory + "/inventory.png");
            QVERIFY(click("Caixa"));
            QVERIFY(fill("posOperator", "Ana"));
            QVERIFY(fill("cashAmount", "100,00"));
            QVERIFY(click("Abrir caixa"));
            QVERIFY(!pos.cash().isEmpty());
            QVERIFY(click("PDV"));
            auto *customerChoice = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName() == "posCustomer"; });
            QVERIFY(customerChoice);
            customerChoice->forceActiveFocus();
            QTest::keyClick(window, Qt::Key_Down);
            QTest::qWait(50);
            QCOMPARE(customerChoice->property("currentValue").toInt(),1);
            QVERIFY(click("CAM-01 — Camiseta"));
            QCOMPARE(pos.cart().size(),1);
            QVERIFY(fill("posReceived", "20,00"));
            if (!screenshotDirectory.isEmpty()) window->grabWindow().save(screenshotDirectory + "/pos.png");
            QVERIFY(click("Finalizar venda"));
            QCOMPARE(pos.cart().size(),0);
            QCOMPARE(customerChoice->property("currentValue").toInt(),0);
            QCOMPARE(pos.cash().value("cash_expected").toInt(),11990);
            QTest::keyClick(window, Qt::Key_Escape);
            QTest::qWait(100);
            QVERIFY(click("Caixa"));
            QVERIFY(click("Suprimento"));
            QVERIFY(fill("cashMovementAmount", "50,00"));
            QVERIFY(fill("cashMovementReason", "Reforço de troco"));
            QVERIFY(click("Registrar movimentação"));
            QCOMPARE(pos.cash().value("cash_expected").toInt(),16990);
            QVERIFY(click("Sangria"));
            QVERIFY(fill("cashMovementAmount", "70,00"));
            QVERIFY(fill("cashMovementReason", "Retirada para depósito"));
            QVERIFY(click("Registrar movimentação"));
            QCOMPARE(pos.cash().value("cash_expected").toInt(),9990);
            QVERIFY(fill("cashAmount", "99,90"));
            QVERIFY(click("Fechar caixa"));
            QVERIFY(pos.cash().isEmpty());
            if (!screenshotDirectory.isEmpty()) window->grabWindow().save(screenshotDirectory + "/cash.png");
            QVERIFY(click("Movimentações"));
            QCOMPARE(pos.cashMovements().size(),2);
            if (!screenshotDirectory.isEmpty()) window->grabWindow().save(screenshotDirectory + "/cash-history.png");
            QTest::keyClick(window, Qt::Key_Escape);
            QTest::qWait(100);
            QVERIFY(click("Vendas"));
            QCOMPARE(pos.sales().size(),1);
            QVERIFY(fill("salesNumber", "999"));
            QCOMPARE(pos.sales().size(),0);
            QVERIFY(fill("salesNumber", "1"));
            QCOMPARE(pos.sales().size(),1);
            QVERIFY(click("Ver venda"));
            QCOMPARE(pos.saleItems().size(),1);
            QCOMPARE(pos.selectedSale().value("customer_id").toInt(),1);
            QCOMPARE(pos.selectedSale().value("total_cents").toInt(),1990);
            if (!screenshotDirectory.isEmpty()) window->grabWindow().save(screenshotDirectory + "/sale-details.png");
            QVERIFY(click("Fechar detalhes"));
            QVERIFY(click("Clientes"));
            QVERIFY(click("Compras"));
            QCOMPARE(pos.customerSummary().value("purchase_count").toInt(),1);
            QCOMPARE(pos.customerSummary().value("spent_cents").toInt(),1990);
            QCOMPARE(pos.sales().size(),1);
            if (!screenshotDirectory.isEmpty()) window->grabWindow().save(screenshotDirectory + "/customer-history.png");
            QVERIFY(click("Ver venda"));
            QCOMPARE(pos.selectedSale().value("customer_id").toInt(),1);
            QVERIFY(click("Fechar detalhes"));
            QVERIFY(click("Voltar para clientes"));
            QVERIFY(click("Inativar"));
            QVERIFY(click("Mostrar inativos"));
            QVERIFY(click("Compras"));
            QCOMPARE(pos.customerSummary().value("active").toInt(),0);
            QCOMPARE(pos.sales().size(),1);
            QVERIFY(click("Todas as vendas"));
            QVERIFY(pos.customerSummary().isEmpty());
            QVERIFY(click("Dashboard"));
            QCOMPARE(pos.dashboard().value("today_cents").toInt(),1990);
            QCOMPARE(pos.dashboard().value("today_count").toInt(),1);
            QCOMPARE(pos.dashboard().value("cash_open").toInt(),0);
            QVERIFY(click("Atualizar painel"));
            if (!screenshotDirectory.isEmpty()) window->grabWindow().save(screenshotDirectory + "/dashboard.png");
            QVERIFY(click("Configurações"));
            QVERIFY(click("Empresa e módulos"));
            QVERIFY(fill("companyName", "Loja modular"));
            QVERIFY(click("Salvar configurações"));
            QCOMPARE(settings.values().value("company").toString(),QString("Loja modular"));
            if (!screenshotDirectory.isEmpty()) window->grabWindow().save(screenshotDirectory + "/settings.png");
            auto *posModule = findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName()=="modulePos"; });
            QVERIFY(posModule);
            QVERIFY(posModule->setProperty("checked",false));
            QVERIFY(click("Salvar configurações"));
            QVERIFY(!settings.values().value("pos").toBool());
            QVERIFY(!findItem(window->contentItem(), [](QQuickItem *item) { return item->objectName()=="navigation_PDV"; }));
            QVERIFY(posModule->setProperty("checked",true));
            QVERIFY(click("Salvar configurações"));
            QVERIFY(click("Auditoria"));
            QVERIFY(audit.refresh());
            QCOMPARE(audit.entries().size(),3);
            QCOMPARE(audit.entries().first().toMap().value("action").toString(),QString("settings.update"));
            QVERIFY(audit.entries().first().toMap().value("details").toString().contains("empresa=Loja modular"));
            QVERIFY(click("Configurações"));
            QVERIFY(click("Backup local"));
            QVERIFY(fill("backupFolder", directory.filePath("copies")));
            QVERIFY(click("Criar backup"));
            QCOMPARE(backup.files().size(),1);
            QVERIFY(click("Selecionar"));
            QVERIFY(click("Restaurar backup"));
            QVERIFY(click("Cancelar restauração"));
            QCOMPARE(pos.dashboard().value("today_cents").toInt(),1990);
            if (!screenshotDirectory.isEmpty()) window->grabWindow().save(screenshotDirectory + "/backup.png");
            QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
            window = nullptr;
        }
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
};
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    UiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "ui_test.moc"
