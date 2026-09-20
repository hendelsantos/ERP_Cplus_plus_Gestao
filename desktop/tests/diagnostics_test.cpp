#include "core/diagnostics/diagnostics.h"
#include "core/database/database.h"
#include "core/settings/settings.h"
#include "auth_fixture.h"
#include "modules/catalog/catalog.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSqlQuery>
#include <QThread>
#include <QProcess>
#include <cstdlib>

using MHStore::Diagnostics;
class DiagnosticsTest : public QObject {
    Q_OBJECT
    QByteArray read(const QString &path) { QFile f(path); if(!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
private slots:
    void cleanup() {
        Diagnostics::finish();
        if(QSqlDatabase::contains()) { QSqlDatabase::database().close(); QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection); }
    }
    void rotationLevelsAndSessionRecovery() {
        QTemporaryDir dir;
        QVERIFY(Diagnostics::start(dir.path(),256));
        QVERIFY(!Diagnostics::report().value("unclean_shutdown").toBool());
        for(int i=0;i<40;++i) Diagnostics::record(Diagnostics::Level::Warning,Diagnostics::Event::TransactionRollback,Diagnostics::Component::Pos);
        const auto files=QDir(dir.filePath("logs")).entryInfoList(QDir::Files);
        QCOMPARE(files.size(),4);
        for(const auto &file:files) QVERIFY(file.size()<=256);
        QVERIFY(Diagnostics::setLevel(2));
        QVERIFY(!Diagnostics::setLevel(3));
        const auto before=read(dir.filePath("logs/operations.log"));
        Diagnostics::record(Diagnostics::Level::Info,Diagnostics::Event::Startup,Diagnostics::Component::Application);
        QCOMPARE(read(dir.filePath("logs/operations.log")),before);
        Diagnostics::finish();
        QVERIFY(!QFile::exists(dir.filePath("session.active")));
        QVERIFY(Diagnostics::start(dir.path(),256));
        QCOMPARE(Diagnostics::report().value("level").toInt(),2);
        QVERIFY(Diagnostics::setLevel(0));
        Diagnostics::finish();
        QFile interrupted(dir.filePath("session.active")); QVERIFY(interrupted.open(QIODevice::WriteOnly)); interrupted.close();
        QVERIFY(Diagnostics::start(dir.path(),256));
        QVERIFY(Diagnostics::report().value("unclean_shutdown").toBool());
        QVERIFY(read(dir.filePath("logs/operations.log")).contains("unclean_shutdown"));
    }
    void unavailableLogsRecoverWithoutTouchingData() {
        QTemporaryDir dir;
        QFile blocked(dir.filePath("logs")); QVERIFY(blocked.open(QIODevice::WriteOnly)); blocked.write("preserve"); blocked.close();
        QVERIFY(!Diagnostics::start(dir.path()));
        QVERIFY(!Diagnostics::report().value("error").toString().isEmpty());
        QCOMPARE(read(blocked.fileName()),QByteArray("preserve"));
        QVERIFY(blocked.remove());
        QVERIFY(Diagnostics::start(dir.path()));
        // A directory occupying the log filename simulates an unwritable log target.
        QVERIFY(QFile::remove(dir.filePath("logs/operations.log")));
        QVERIFY(QDir().mkdir(dir.filePath("logs/operations.log")));
        Diagnostics::record(Diagnostics::Level::Error,Diagnostics::Event::BackupFailed,Diagnostics::Component::Backup);
        QVERIFY(!Diagnostics::report().value("error").toString().isEmpty());
        QVERIFY(QDir().rmdir(dir.filePath("logs/operations.log")));
        Diagnostics::record(Diagnostics::Level::Error,Diagnostics::Event::BackupFailed,Diagnostics::Component::Backup);
        QVERIFY(Diagnostics::report().value("error").toString().isEmpty());
    }
    void corruptDatabaseAndMigrationFailure() {
        QTemporaryDir dir; QVERIFY(Diagnostics::start(dir.path()));
        const auto path=dir.filePath("broken.sqlite");
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("not a database"); f.close();
        MHStore::Database::DatabaseManager db; QString error;
        QVERIFY(!db.initialize(&error,path));
        QVERIFY(error.contains("integridade"));
        QCOMPARE(read(path),QByteArray("not a database"));
        QSqlDatabase::database().close(); QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        QVERIFY(db.initialize(&error,dir.filePath("valid.sqlite")));
        {
            QSqlQuery q; QVERIFY(q.exec("DELETE FROM schema_migrations WHERE version>1"));
        }
        QVERIFY(!db.initialize(&error,dir.filePath("valid.sqlite")));
        QSqlQuery q("SELECT MAX(version) FROM schema_migrations"); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(),1);
        const auto log=read(dir.filePath("logs/operations.log"));
        QVERIFY(log.contains("integrity_failed")); QVERIFY(log.contains("migration_failed"));
        QVERIFY(!log.contains("CREATE TABLE"));
    }
    void invalidReferencesAndDiagnosticPermissions() {
        QTemporaryDir dir; QVERIFY(Diagnostics::start(dir.path()));
        MHStore::Database::DatabaseManager db; const auto path=dir.filePath("test.sqlite");
        QVERIFY(db.initialize(nullptr,path));
        MHStore::Auth::resetSession();
        MHStore::Settings settings;
        QVERIFY(settings.diagnostics().isEmpty()); QVERIFY(!settings.configureLogging(2));
        QVERIFY(authenticateTestAdmin());
        QCOMPARE(settings.diagnostics().value("schema").toInt(),20);
        QCOMPARE(settings.diagnostics().value("database").toString(),path);
        QVERIFY(settings.configureLogging(0));
        {
            QSqlQuery q; QVERIFY(q.exec("PRAGMA foreign_keys=OFF"));
            QVERIFY(q.exec("INSERT INTO products(code,name,category_id) VALUES('private-code','private-name',999)"));
        }
        QString error; QVERIFY(!db.initialize(&error,path)); QVERIFY(error.contains("Vínculos inválidos"));
        QVERIFY(!read(dir.filePath("logs/operations.log")).contains("private-name"));
    }
    void rollbackLogsExcludeSensitiveErrorDetails() {
        QTemporaryDir dir; QVERIFY(Diagnostics::start(dir.path()));
        MHStore::Database::DatabaseManager db; QVERIFY(db.initialize(nullptr,dir.filePath("test.sqlite")));
        QVERIFY(authenticateTestAdmin());
        QSqlQuery q;
        QVERIFY(q.exec("CREATE TEMP TRIGGER reject_customer BEFORE INSERT ON customers BEGIN SELECT RAISE(ABORT,'senha-token-segredo'); END"));
        MHStore::Catalog catalog;
        QVERIFY(!catalog.save("Clientes",0,{{"name","cliente-confidencial"}}));
        QVERIFY(q.exec("SELECT COUNT(*) FROM customers")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(),0);
        const auto log=read(dir.filePath("logs/operations.log"));
        QVERIFY(log.contains("catalog transaction_rollback"));
        QVERIFY(!log.contains("senha-token-segredo")); QVERIFY(!log.contains("cliente-confidencial"));
        QVERIFY(!log.contains("TestAdmin"));
    }
    void interruptedWriterRecoversCommittedData() {
        QTemporaryDir dir;
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(),{"--simulate-crash",dir.path()});
        QVERIFY(child.waitForFinished(10000)); QCOMPARE(child.exitCode(),0);
        QVERIFY(QFile::exists(dir.filePath("session.active")));
        QVERIFY(Diagnostics::start(dir.path()));
        QVERIFY(Diagnostics::report().value("unclean_shutdown").toBool());
        MHStore::Database::DatabaseManager db;
        QString error; QVERIFY2(db.initialize(&error,dir.filePath("crash.sqlite")),qPrintable(error));
        QSqlQuery q("SELECT name FROM categories ORDER BY id");
        QVERIFY(q.next()); QCOMPARE(q.value(0).toString(),QString("Committed")); QVERIFY(!q.next());
        Diagnostics::finish(); QVERIFY(!QFile::exists(dir.filePath("session.active")));
    }
    void concurrentEventsAreWholeLines() {
        QTemporaryDir dir; QVERIFY(Diagnostics::start(dir.path()));
        auto work=[] { for(int i=0;i<100;++i) Diagnostics::record(Diagnostics::Level::Warning,Diagnostics::Event::BackupFailed,Diagnostics::Component::Backup); };
        QThread *a=QThread::create(work), *b=QThread::create(work);
        a->start(); b->start(); QVERIFY(a->wait(10000)); QVERIFY(b->wait(10000)); delete a; delete b;
        const auto log=read(dir.filePath("logs/operations.log"));
        QCOMPARE(log.count("warning backup backup_failed\n"),200);
        QCOMPARE(log.count('\n'),201);
    }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().size()==3 && app.arguments().at(1)=="--simulate-crash") {
        const auto folder=app.arguments().at(2);
        if(!Diagnostics::start(folder)) return 2;
        MHStore::Database::DatabaseManager db;
        if(!db.initialize(nullptr,folder+"/crash.sqlite")) return 3;
        QSqlQuery q;
        if(!q.exec("INSERT INTO categories(name) VALUES('Committed')") || !q.exec("BEGIN IMMEDIATE") ||
           !q.exec("INSERT INTO categories(name) VALUES('Uncommitted')")) return 4;
        std::_Exit(0); // Deliberately bypass destructors, rollback and session cleanup.
    }
    DiagnosticsTest test;
    return QTest::qExec(&test,argc,argv);
}
#include "diagnostics_test.moc"
