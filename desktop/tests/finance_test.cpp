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
};
QTEST_GUILESS_MAIN(FinanceTest)
#include "finance_test.moc"
