#include "auth_fixture.h"
#include "core/database/database.h"
#include "modules/finance/finance.h"
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

class FinanceTest : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QString path;
    QVariant scalar(const QString &sql) { QSqlQuery q(sql); return q.next() ? q.value(0) : QVariant(); }
private slots:
    void init() {
        path = directory.filePath(QUuid::createUuid().toString() + ".sqlite");
        MHStore::Database::DatabaseManager db;
        QVERIFY(db.initialize(nullptr, path));
        QVERIFY(authenticateTestAdmin());
    }
    void cleanup() { QSqlDatabase::database().close(); QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection); }
    void expenseLifecycle() {
        MHStore::Finance finance;
        QVERIFY(finance.createExpense("Aluguel", "100,00", "2026-09-30"));
        QCOMPARE(finance.expenses().size(), 1);
        QVERIFY(!finance.createExpense("", "1", "2026-09-30"));
        QVERIFY(!finance.createExpense("Taxa", "1", "30/09/2026"));
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO cash_sessions(opening_cents,opening_balance,status,operator_name,user_id) VALUES(50000,500,'open','Ana',1)"));
        const int session = scalar("SELECT id FROM cash_sessions").toInt();
        QVERIFY(finance.payExpense(1, session));
        QCOMPARE(scalar("SELECT status FROM expenses WHERE id=1").toString(), QString("paid"));
        QCOMPARE(scalar("SELECT amount_cents FROM cash_movements WHERE cash_session_id=1").toInt(), 10000);
        QCOMPARE(scalar("SELECT balance_cents FROM cash_movements WHERE cash_session_id=1").toInt(), 40000);
        QVERIFY(!finance.payExpense(1, session));
    }
    void rollbackOnMovementFailure() {
        MHStore::Finance finance;
        QVERIFY(finance.createExpense("Energia", "50", "2026-09-30"));
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO cash_sessions(opening_cents,opening_balance,status,operator_name,user_id) VALUES(10000,100,'open','Ana',1)"));
        QVERIFY(q.exec("CREATE TRIGGER reject_finance_movement BEFORE INSERT ON cash_movements BEGIN SELECT RAISE(ABORT,'Falha simulada'); END"));
        QVERIFY(!finance.payExpense(1, 1));
        QCOMPARE(scalar("SELECT status FROM expenses WHERE id=1").toString(), QString("open"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM cash_movements").toInt(), 0);
    }
    void receivableLifecycle() {
        MHStore::Finance finance;
        QVERIFY(finance.createReceivable("Venda fiada", "75,50", "2026-10-05"));
        QCOMPARE(finance.receivables().size(), 1);
        QVERIFY(!finance.createReceivable("Cliente inválido", "10", "2026-10-05", 999));
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO cash_sessions(opening_cents,opening_balance,status,operator_name,user_id) VALUES(1000,10,'open','Ana',1)"));
        QVERIFY(finance.receiveReceivable(1, 1));
        QCOMPARE(scalar("SELECT status FROM receivables WHERE id=1").toString(), QString("received"));
        QCOMPARE(scalar("SELECT type FROM cash_movements WHERE cash_session_id=1").toString(), QString("supply"));
        QCOMPARE(scalar("SELECT amount_cents FROM cash_movements WHERE cash_session_id=1").toInt(), 7550);
        QVERIFY(!finance.receiveReceivable(1, 1));
    }
    void serviceOrderLifecycle() {
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO customers(name) VALUES('Cliente OS')"));
        QVERIFY(q.exec("INSERT INTO products(code,name,sale_price_cents,product_type,active) VALUES('SRV-1','Conserto',12500,'service',1)"));
        MHStore::Finance finance;
        QVERIFY(finance.createServiceOrder(1, 1, "Troca de tela", "Cliente aguarda orçamento"));
        QCOMPARE(finance.serviceOrders().size(), 1);
        QCOMPARE(finance.serviceOrders().first().toMap().value("status").toString(), QString("open"));
        QVERIFY(finance.updateServiceOrder(1, "in_progress"));
        QCOMPARE(scalar("SELECT status FROM service_orders WHERE id=1").toString(), QString("in_progress"));
        QVERIFY(!finance.updateServiceOrder(1, "invalid"));
        QVERIFY(!finance.createServiceOrder(1, 999, "Falha", ""));
    }
};
QTEST_GUILESS_MAIN(FinanceTest)
#include "finance_test.moc"
