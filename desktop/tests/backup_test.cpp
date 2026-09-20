#include "auth_fixture.h"
#include "infrastructure/backup/backup.h"
#include "core/database/database.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QFile>
#include <QCryptographicHash>

class BackupTest : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QString path;
    QByteArray digest(const QString &file) { QFile f(file); if(!f.open(QIODevice::ReadOnly)) return {}; return QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256); }
    QVariant scalar(const QString &sql) { QSqlQuery q(sql); return q.next()?q.value(0):QVariant(); }
private slots:
    void init() {
        path=directory.filePath(QUuid::createUuid().toString()+".sqlite");
        MHStore::Database::DatabaseManager db; QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO categories(name) VALUES('Roupas')"));
        QVERIFY(q.exec("INSERT INTO products(code,name,category_id,stock_quantity) VALUES('P1','Produto',1,10)"));
    }
    void cleanup() { QSqlDatabase::database().close(); QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection); }
    void snapshotAndRestore() {
        MHStore::Backup backup;
        const auto folder=directory.filePath(QUuid::createUuid().toString());
        QSqlQuery q;
        QVERIFY(q.exec("PRAGMA journal_mode=WAL")); q.finish();
        QVERIFY2(backup.create(folder),qPrintable(backup.message()));
        const auto saved=backup.files().first().toMap().value("path").toString();
        QVERIFY(q.exec("UPDATE business_settings SET company='Alterado',pos=0"));
        QVERIFY(q.exec("UPDATE products SET stock_quantity=2, name='Alterado'"));
        QVERIFY(q.exec("INSERT INTO customers(name) VALUES('Posterior')"));
        QSignalSpy restored(&backup,&MHStore::Backup::restored);
        QVERIFY2(backup.restore(saved),qPrintable(backup.message()));
        QVERIFY(authenticateTestAdmin());
        QCOMPARE(restored.size(),1);
        QCOMPARE(scalar("SELECT company FROM business_settings").toString(),QString("Minha empresa"));
        QCOMPARE(scalar("SELECT stock_quantity FROM products").toInt(),10);
        QCOMPARE(scalar("SELECT name FROM products").toString(),QString("Produto"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM customers").toInt(),0);
        QVERIFY(q.exec("INSERT INTO products(code,name) VALUES('P2','Segundo')"));
        QCOMPARE(scalar("SELECT id FROM products WHERE code='P2'").toInt(),2);
        const auto safety=QDir(QFileInfo(path).absolutePath()+"/backups").entryInfoList({"antes_restauracao_*.mhb"},QDir::Files,QDir::Time);
        QVERIFY(!safety.isEmpty());
        QVERIFY2(backup.restore(safety.first().absoluteFilePath()),qPrintable(backup.message()));
        QVERIFY(authenticateTestAdmin());
        QCOMPARE(scalar("SELECT stock_quantity FROM products WHERE id=1").toInt(),2);
        QCOMPARE(scalar("SELECT COUNT(*) FROM customers").toInt(),1);
        QSqlDatabase::database().close();
        MHStore::Database::DatabaseManager db; QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        QCOMPARE(scalar("SELECT name FROM products WHERE id=1").toString(),QString("Alterado"));
    }
    void rejectedRestores() {
        MHStore::Backup backup;
        const auto folder=directory.filePath(QUuid::createUuid().toString());
        QVERIFY(backup.create(folder));
        const auto saved=backup.files().first().toMap().value("path").toString();
        QVERIFY(!backup.restore(path));
        QVERIFY(!backup.restore(directory.filePath("missing")));
        backup.hasPendingCart=[] { return true; };
        QVERIFY(!backup.restore(saved));
        backup.hasPendingCart={};
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO cash_sessions(opening_cents) VALUES(100)"));
        QVERIFY(!backup.restore(saved));
        QVERIFY(q.exec("UPDATE cash_sessions SET status='closed'"));
        QFile bad(directory.filePath("invalid.mhb"));
        QVERIFY(bad.open(QIODevice::WriteOnly)); bad.write("not sqlite"); bad.close();
        QVERIFY(!backup.restore(bad.fileName()));
        QCOMPARE(scalar("SELECT stock_quantity FROM products").toInt(),10);
        QVERIFY(!backup.create("relative/path"));
        QVERIFY(!backup.create(path));
        QCOMPARE(scalar("SELECT COUNT(*) FROM cash_sessions").toInt(),1);
    }
    void restoreRollbackAndValidation() {
        MHStore::Backup backup;
        const auto folder=directory.filePath(QUuid::createUuid().toString());
        QVERIFY(backup.create(folder));
        const auto saved=backup.files().first().toMap().value("path").toString();
        QSqlQuery q;
        QVERIFY(q.exec("UPDATE products SET name='Atual', stock_quantity=3"));
        QVERIFY(q.exec("INSERT INTO customers(name) VALUES('Preservar')"));
        QVERIFY(q.exec("CREATE TEMP TRIGGER fail_restore BEFORE INSERT ON main.products BEGIN SELECT RAISE(ABORT,'Falha simulada'); END"));
        QVERIFY(!backup.restore(saved));
        QCOMPARE(scalar("SELECT name FROM products").toString(),QString("Atual"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM customers").toInt(),1);
        QVERIFY(q.exec("DROP TRIGGER fail_restore"));
        QVERIFY2(backup.restore(saved),qPrintable(backup.message()));
        QVERIFY(authenticateTestAdmin());
        // Tamper with a separate backup without touching the live database.
        {
            auto damaged=QSqlDatabase::addDatabase("QSQLITE","damaged");
            damaged.setDatabaseName(saved); QVERIFY(damaged.open());
            QSqlQuery edit(damaged);
            QVERIFY(edit.exec("UPDATE products SET category_id=999"));
            damaged.close();
        }
        QSqlDatabase::removeDatabase("damaged");
        QVERIFY(!backup.restore(saved));
        QCOMPARE(scalar("SELECT category_id FROM products").toInt(),1);
        QVERIFY(QSqlDatabase::database().transaction());
        QVERIFY(!backup.create(folder));
        QVERIFY(QSqlDatabase::database().rollback());
    }
    void versionFourBackupAndMigration() {
        QSqlQuery q;
        QVERIFY(removeAuthMigration());
        QVERIFY(q.exec("DROP TABLE business_settings"));
        QVERIFY(q.exec("DELETE FROM schema_migrations WHERE version=5"));
        MHStore::Backup backup;
        const auto folder=directory.filePath(QUuid::createUuid().toString());
        const auto saved=directory.filePath("legacy.mhb");
        q.prepare("VACUUM INTO ?"); q.addBindValue(saved); QVERIFY(q.exec());
        MHStore::Database::DatabaseManager db;
        QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        QCOMPARE(scalar("SELECT company FROM business_settings").toString(),QString("Minha empresa"));
        const auto original=digest(saved);
        QVERIFY2(backup.restore(saved),qPrintable(backup.message()));
        QCOMPARE(digest(saved),original);
        QCOMPARE(scalar("SELECT stock_quantity FROM products").toInt(),10);
        QCOMPARE(scalar("SELECT MAX(version) FROM schema_migrations").toInt(),20);
    }
    void complementaryBackupCompatibility() {
        QSqlQuery q;
        QVERIFY(q.exec("UPDATE products SET brand='Marca',unit='UN',maximum_stock=20,location='A1',notes='Nota'"));
        QVERIFY(q.exec("UPDATE business_settings SET document='123',phone='456',address='Rua'"));
        MHStore::Backup backup;
        const auto folder=directory.filePath(QUuid::createUuid().toString());
        QVERIFY(backup.create(folder));
        const auto saved=backup.files().first().toMap().value("path").toString();
        QVERIFY(q.exec("UPDATE products SET brand='Alterada'"));
        QVERIFY(q.exec("UPDATE business_settings SET address='Outra'"));
        QVERIFY(backup.restore(saved)); QVERIFY(authenticateTestAdmin());
        QCOMPARE(scalar("SELECT brand FROM products").toString(),QString("Marca"));
        QCOMPARE(scalar("SELECT address FROM business_settings").toString(),QString("Rua"));
        QVERIFY(removeComplementaryMigration());
        const auto old=directory.filePath("version8.mhb");
        q.prepare("VACUUM INTO ?"); q.addBindValue(old); QVERIFY(q.exec());
        MHStore::Database::DatabaseManager db; QVERIFY(db.initialize(nullptr,path));
        const auto original=digest(old);
        QVERIFY2(backup.restore(old),qPrintable(backup.message()));
        QCOMPARE(digest(old),original);
        QCOMPARE(scalar("SELECT stock_quantity FROM products").toInt(),10);
    }
    void adjustmentBackupCompatibility() {
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO sales(total_amount,total_cents,subtotal_cents,discount_cents,surcharge_cents,adjustment_reason) VALUES(9.5,950,1000,100,50,'Teste')"));
        MHStore::Backup backup;
        const auto folder=directory.filePath(QUuid::createUuid().toString());
        QVERIFY(backup.create(folder));
        const auto saved=backup.files().first().toMap().value("path").toString();
        QVERIFY(q.exec("DELETE FROM sales"));
        QVERIFY(backup.restore(saved)); QVERIFY(authenticateTestAdmin());
        QCOMPARE(scalar("SELECT discount_cents FROM sales").toInt(),100);
        QCOMPARE(scalar("SELECT adjustment_reason FROM sales").toString(),QString("Teste"));
        QVERIFY(removeAdjustmentMigration());
        const auto old=directory.filePath("version9.mhb");
        q.prepare("VACUUM INTO ?"); q.addBindValue(old); QVERIFY(q.exec());
        MHStore::Database::DatabaseManager db; QVERIFY(db.initialize(nullptr,path));
        const auto original=digest(old);
        QVERIFY2(backup.restore(old),qPrintable(backup.message()));
        QCOMPARE(digest(old),original);
        QCOMPARE(scalar("SELECT total_cents FROM sales").toInt(),950);
    }
    void automaticScheduleRetentionAndFailure() {
        MHStore::Backup backup;
        const auto folder=directory.filePath(QUuid::createUuid().toString());
        QVERIFY(!backup.configureAutomatic(true,"relative",60,2));
        QVERIFY(!backup.configureAutomatic(true,folder,0,2));
        QVERIFY(!backup.configureAutomatic(true,folder,60,0));
        QVERIFY(backup.configureAutomatic(true,folder,60,2));
        QVERIFY(backup.create(folder));
        const auto manual=backup.files().first().toMap().value("path").toString();
        const auto safety=QDir(folder).filePath("antes_restauracao_preservar.mhb");
        QVERIFY(QFile::copy(manual,safety));
        const auto unrelated=QDir(folder).filePath("auto_outra_instalacao.mhb");
        QVERIFY(QFile::copy(manual,unrelated));
        MHStore::Auth::resetSession();
        QVERIFY(!backup.configureAutomatic(false,folder,60,2));
        backup.startScheduler();
        QTRY_VERIFY_WITH_TIMEOUT(backup.automatic().value("last_success").toLongLong()>0,10000);
        QVERIFY(!backup.busy());
        backup.runAutomatic(); QVERIFY(!backup.busy());
        for(int i=0;i<3;++i) {
            backup.runAutomatic(true); QVERIFY(backup.busy());
            QTRY_VERIFY_WITH_TIMEOUT(!backup.busy(),10000);
            QVERIFY2(backup.automaticMessage().startsWith("Backup automático criado"),qPrintable(backup.automaticMessage()));
        }
        QCOMPARE(QDir(folder).entryList({"auto_*_v20_*.mhb"},QDir::Files).size(),2);
        QVERIFY(QFile::exists(manual)); QVERIFY(QFile::exists(safety)); QVERIFY(QFile::exists(unrelated));
        QVERIFY(authenticateTestAdmin());
        backup.list(folder);
        for(const auto &row:backup.files()) {
            QVERIFY(row.toMap().value("valid").toBool());
            QCOMPARE(row.toMap().value("version").toInt(),20);
        }
        MHStore::Backup reloaded;
        QCOMPARE(reloaded.automatic(),backup.automatic());
        // A file cannot be used as a destination directory. Failed jobs preserve old copies.
        QVERIFY(backup.configureAutomatic(true,manual,60,2));
        backup.runAutomatic(true);
        QTRY_VERIFY_WITH_TIMEOUT(!backup.busy(),10000);
        QVERIFY(backup.automaticMessage().contains("Não foi possível"));
        QCOMPARE(backup.automatic().value("last_success").toLongLong(),0);
        QCOMPARE(QDir(folder).entryList({"auto_*_v20_*.mhb"},QDir::Files).size(),2);
        QVERIFY(QFile::exists(manual));
        QFile corrupt(QDir(folder).filePath("corrupt.mhb"));
        QVERIFY(corrupt.open(QIODevice::WriteOnly)); corrupt.write("invalid sqlite"); corrupt.close();
        backup.list(folder);
        bool found=false;
        for(const auto &row:backup.files()) if(row.toMap().value("path")==corrupt.fileName()) {
            found=true; QVERIFY(!row.toMap().value("valid").toBool());
        }
        QVERIFY(found);
        QVERIFY(!backup.restore(corrupt.fileName()));
        QCOMPARE(scalar("SELECT stock_quantity FROM products").toInt(),10);
    }
    void automaticSnapshotRestoresDataAndKeepsPolicy() {
        MHStore::Backup backup;
        const auto folder=directory.filePath(QUuid::createUuid().toString());
        QVERIFY(backup.configureAutomatic(true,folder,30,3));
        backup.runAutomatic(true);
        // A cash-close request while copying must cause another snapshot afterwards.
        backup.runAutomatic(true);
        QTRY_VERIFY_WITH_TIMEOUT(!backup.busy(),10000);
        QCOMPARE(QDir(folder).entryList({"auto_*.mhb"},QDir::Files).size(),2);
        backup.list(folder);
        const auto saved=backup.files().first().toMap().value("path").toString();
        QSqlQuery q; QVERIFY(q.exec("UPDATE products SET stock_quantity=1"));
        const auto policy=backup.automatic();
        QVERIFY2(backup.restore(saved),qPrintable(backup.message()));
        QCOMPARE(scalar("SELECT stock_quantity FROM products").toInt(),10);
        QCOMPARE(backup.automatic(),policy);
        backup.runAutomatic(true); QVERIFY(!backup.busy());
    }
    void unsupportedAndUnmigratableBackupsLeaveLiveDataUntouched() {
        MHStore::Backup backup;
        const auto folder=directory.filePath(QUuid::createUuid().toString());
        QVERIFY(backup.create(folder));
        const auto saved=backup.files().first().toMap().value("path").toString();
        for(bool future : {true,false}) {
            {
                auto source=QSqlDatabase::addDatabase("QSQLITE","invalid_migration");
                source.setDatabaseName(saved); QVERIFY(source.open());
                QSqlQuery q(source);
                // A future version must be rejected. A falsified old version must
                // fail migration without modifying the original or the live DB.
                QVERIFY(q.exec("DELETE FROM schema_migrations"));
                QVERIFY(q.exec(future ? "INSERT INTO schema_migrations(version) VALUES(999)" : "INSERT INTO schema_migrations(version) VALUES(1)"));
            }
            QSqlDatabase::removeDatabase("invalid_migration");
            const auto original=digest(saved);
            QVERIFY(!backup.restore(saved));
            QCOMPARE(digest(saved),original);
            QCOMPARE(scalar("SELECT MAX(version) FROM schema_migrations").toInt(),20);
            QCOMPARE(scalar("SELECT name FROM products").toString(),QString("Produto"));
        }
    }
    void incompatibleSchema() {
        MHStore::Backup backup;
        const auto folder=directory.filePath(QUuid::createUuid().toString());
        QVERIFY(backup.create(folder));
        const auto saved=backup.files().first().toMap().value("path").toString();
        QSqlQuery q; QVERIFY(q.exec("CREATE TABLE extra(id INTEGER)"));
        QVERIFY(!backup.restore(saved));
        QVERIFY(q.exec("SELECT * FROM extra"));
    }
};
QTEST_GUILESS_MAIN(BackupTest)
#include "backup_test.moc"
