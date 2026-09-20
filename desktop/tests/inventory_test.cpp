#include "auth_fixture.h"
#include "core/database/database.h"
#include "modules/inventory/inventory.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class InventoryTest : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;
    QString path;
    bool initialize()
    {
        MHStore::Database::DatabaseManager database;
        return database.initialize(nullptr, path);
    }
    double balance()
    {
        QSqlQuery query("SELECT stock_quantity FROM products WHERE id = 1");
        return query.next() ? query.value(0).toDouble() : -1;
    }
private slots:
    void init()
    {
        path = directory.filePath(QUuid::createUuid().toString() + ".sqlite");
        QVERIFY(initialize());
        QVERIFY(authenticateTestAdmin());
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO products (code, name, minimum_stock) VALUES ('P1','Produto',2)"));
    }
    void disabledInventory() {
        QSqlQuery q;
        QVERIFY(q.exec("UPDATE business_settings SET pos=0,inventory=0"));
        MHStore::Inventory inventory;
        QVERIFY(!inventory.move(1,"entry","5","Teste","Ana"));
        QCOMPARE(balance(),0.0);
        QVERIFY(q.exec("UPDATE business_settings SET inventory=1"));
        QVERIFY(inventory.move(1,"entry","5","Teste","Ana"));
    }
    void cleanup()
    {
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
    void movementsAndPersistence()
    {
        MHStore::Inventory inventory;
        QVERIFY(inventory.move(1,"entry","10,250","Recebimento","Ana"));
        QVERIFY(inventory.move(1,"exit","2,125","Avaria","Ana"));
        QCOMPARE(balance(), 8.125);
        QVERIFY(inventory.move(1,"adjustment","3","Contagem","João"));
        QCOMPARE(balance(), 3.0);
        QCOMPARE(inventory.history().size(), 3);
        const auto latest = inventory.history().first().toMap();
        QCOMPARE(latest.value("quantity").toDouble(), -5.125);
        QCOMPARE(latest.value("previous_balance").toDouble(), 8.125);
        QCOMPARE(latest.value("balance").toDouble(), 3.0);
        QCOMPARE(latest.value("operator_name").toString(), QString("Ana"));
        QVERIFY(inventory.move(1,"adjustment","0","Contagem zerada","Ana"));
        QCOMPARE(balance(), 0.0);
        inventory.refresh("P1",true);
        QCOMPARE(inventory.products().size(), 1);
        QSqlDatabase::database().close();
        QVERIFY(initialize());
        QVERIFY(authenticateTestAdmin());
        inventory.refresh();
        QCOMPARE(inventory.history().size(), 4);
        QCOMPARE(balance(), 0.0);
    }
    void invalidMovements()
    {
        MHStore::Inventory inventory;
        QVERIFY(inventory.move(1,"entry","2","Recebimento","Ana"));
        QVERIFY(!inventory.move(1,"exit","3","Saída","Ana"));
        QVERIFY(!inventory.move(1,"entry","-1","Entrada","Ana"));
        QVERIFY(!inventory.move(1,"entry","0","Entrada","Ana"));
        QVERIFY(!inventory.move(1,"entry","0,0001","Entrada","Ana"));
        QVERIFY(!inventory.move(1,"entry","nan","Entrada","Ana"));
        QVERIFY(!inventory.move(1,"entry","1000000001","Entrada","Ana"));
        QVERIFY(!inventory.move(1,"entry","1"," ","Ana"));
        QVERIFY(!inventory.move(999,"entry","1","Entrada","Ana"));
        QVERIFY(!inventory.move(1,"invalid","1","Entrada","Ana"));
        QVERIFY(!inventory.move(1,"adjustment","2","Sem mudança","Ana"));
        QSqlQuery query;
        QVERIFY(query.exec("UPDATE products SET active = 0 WHERE id = 1"));
        QVERIFY(!inventory.move(1,"entry","1","Entrada","Ana"));
        QCOMPARE(balance(), 2.0);
        inventory.selectProduct(1);
        QCOMPARE(inventory.history().size(), 1);
    }
    void rollbackOnHistoryFailure()
    {
        MHStore::Inventory inventory;
        QSqlQuery query;
        QVERIFY(query.exec("CREATE TRIGGER reject_movement BEFORE INSERT ON inventory_movements BEGIN SELECT RAISE(ABORT, 'Falha simulada'); END"));
        QVERIFY(!inventory.move(1,"entry","5","Teste","Ana"));
        QCOMPARE(balance(), 0.0);
        inventory.selectProduct(1);
        QCOMPARE(inventory.history().size(), 0);
        QVERIFY(query.exec("DROP TRIGGER reject_movement"));
        QVERIFY(inventory.move(1,"entry","1","Teste","Ana"));
    }
    void upgradeFromVersionOne()
    {
        QSqlQuery query;
        QVERIFY(removeAuthMigration());
        QVERIFY(query.exec("DROP TABLE cash_movements"));
        QVERIFY(query.exec("DROP TABLE business_settings"));
        QVERIFY(query.exec("DELETE FROM schema_migrations WHERE version=5"));
        QVERIFY(query.exec("DELETE FROM schema_migrations WHERE version = 4"));
        QVERIFY(query.exec("DROP TABLE payments"));
        QVERIFY(query.exec("DROP TABLE sale_items"));
        QVERIFY(query.exec("DROP INDEX one_open_cash_session"));
        QVERIFY(query.exec("DROP INDEX sales_cash_session"));
        for (const auto &column : {"sale_price_cents", "cost_price_cents"})
            QVERIFY(query.exec(QString("ALTER TABLE products DROP COLUMN %1").arg(column)));
        for (const auto &column : {"opening_cents", "expected_cents", "counted_cents", "operator_name", "closed_by"})
            QVERIFY(query.exec(QString("ALTER TABLE cash_sessions DROP COLUMN %1").arg(column)));
        for (const auto &column : {"total_cents", "operator_name"})
            QVERIFY(query.exec(QString("ALTER TABLE sales DROP COLUMN %1").arg(column)));
        QVERIFY(query.exec("DELETE FROM schema_migrations WHERE version = 3"));
        QVERIFY(query.exec("DROP TABLE inventory_movements"));
        QVERIFY(query.exec("DELETE FROM schema_migrations WHERE version = 2"));
        QVERIFY(query.exec("UPDATE products SET stock_quantity = 7, sale_price = 19.99, cost_price = 10.01 WHERE id = 1"));
        QSqlDatabase::database().close();
        QVERIFY(initialize());
        QVERIFY(authenticateTestAdmin());
        QVERIFY(initialize());
        QVERIFY(authenticateTestAdmin());
        QCOMPARE(balance(), 7.0);
        QVERIFY(query.exec("SELECT sale_price_cents, cost_price_cents FROM products WHERE id=1"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(),1999);
        QCOMPARE(query.value(1).toInt(),1001);
        query.finish();
        QVERIFY(query.exec("SELECT COUNT(*) FROM schema_migrations"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 12);
        query.finish();
        MHStore::Inventory inventory;
        QVERIFY(inventory.move(1,"exit","2","Após migração","Ana"));
        QCOMPARE(balance(), 5.0);
    }
    void futureVersionRejected()
    {
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO schema_migrations(version) VALUES (99)"));
        QSqlDatabase::database().close();
        QVERIFY(!initialize());
        QCOMPARE(balance(), 0.0);
    }
};
QTEST_GUILESS_MAIN(InventoryTest)
#include "inventory_test.moc"
