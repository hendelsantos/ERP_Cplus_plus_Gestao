#include "../../core/auth/auth.h"
#include "backup.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QTemporaryDir>
#include <QUuid>

namespace MHStore {
namespace {
QString identifier(QString name) { return '"' + name.replace('"', "\"\"") + '"'; }
bool schema(QSqlDatabase db, QMap<QString,QString> &result, QString &error)
{
    QSqlQuery q(db);
    if (!q.exec("SELECT type || ':' || name, sql FROM sqlite_master WHERE name NOT LIKE 'sqlite_%' ORDER BY name")) {
        error = q.lastError().text(); return false;
    }
    while (q.next()) result.insert(q.value(0).toString(),q.value(1).toString());
    return true;
}
bool validate(QSqlDatabase db, QString &error)
{
    QSqlQuery q(db);
    if (!q.exec("PRAGMA integrity_check") || !q.next() || q.value(0).toString() != "ok") {
        error = "Arquivo SQLite inválido ou corrompido."; return false;
    }
    q.finish();
    if (!q.exec("PRAGMA foreign_key_check") || q.next()) {
        error = "O backup contém vínculos inválidos entre registros."; return false;
    }
    return true;
}
// Own the named inspection connection so every error path closes it before removal.
class Inspection {
public:
    QString name = QUuid::createUuid().toString();
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE",name);
    explicit Inspection(const QString &file) { db.setConnectOptions("QSQLITE_OPEN_READONLY"); db.setDatabaseName(file); }
    ~Inspection() { db.close(); db = QSqlDatabase(); QSqlDatabase::removeDatabase(name); }
};
}
Backup::Backup(QObject *parent) : QObject(parent)
{
    m_folder = QFileInfo(QSqlDatabase::database().databaseName()).absolutePath() + "/backups";
    list(m_folder);
}
bool Backup::fail(const QString &message) { m_message=message; emit changed(); return false; }
void Backup::list(const QString &folder)
{
    if (!Auth::allowed("backup")) { m_files.clear(); emit changed(); return; }
    m_folder = folder;
    m_files.clear();
    if (QDir::isAbsolutePath(folder)) {
        const auto files = QDir(folder).entryInfoList({"*.mhb"},QDir::Files,QDir::Time);
        for (const auto &file : files) m_files.append(QVariantMap{{"path",file.absoluteFilePath()},
            {"name",file.fileName()},{"size",file.size()},{"date",file.lastModified().toString("dd/MM/yyyy HH:mm:ss")}});
    }
    emit changed();
}
bool Backup::snapshot(const QString &folder, const QString &prefix, QString &path)
{
    if (!QDir::isAbsolutePath(folder) || !QDir().mkpath(folder)) return fail("Informe uma pasta absoluta e gravável para o backup.");
    path=QDir(folder).filePath(prefix + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_") + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".mhb");
    QSqlQuery q;
    q.prepare("VACUUM main INTO ?"); q.addBindValue(path);
    if (!q.exec()) { QFile::remove(path); return fail("Falha ao criar backup: " + q.lastError().text()); }
    bool valid = false;
    QString error;
    {
        Inspection inspection(path);
        valid = inspection.db.open() && validate(inspection.db,error);
    }
    if (!valid) { QFile::remove(path); return fail("Falha na verificação do backup: " + error); }
    return true;
}
bool Backup::create(const QString &folder)
{
    if (!Auth::allowed("backup")) return fail("Acesso negado. Entre com um usuário autorizado.");
    QString path;
    if (!snapshot(folder,"mhstore_",path)) return false;
    m_message="Backup criado e verificado: " + path;
    list(folder);
    return true;
}
bool Backup::restore(const QString &file)
{
    if (!Auth::allowed("backup")) return fail("Acesso negado. Entre com um usuário autorizado.");
    if (hasPendingCart && hasPendingCart()) return fail("Finalize ou limpe o carrinho antes de restaurar.");
    const auto live = QSqlDatabase::database();
    if (!QFileInfo(file).isFile() || QFileInfo(file).canonicalFilePath() == QFileInfo(live.databaseName()).canonicalFilePath())
        return fail("Selecione um arquivo de backup diferente do banco em uso.");
    QTemporaryDir staging;
    if (!staging.isValid()) return fail("Não foi possível preparar a restauração.");
    const auto staged = staging.filePath("restore.sqlite");
    QString error;
    QMap<QString,QString> currentSchema, sourceSchema;
    if (!schema(live,currentSchema,error)) return fail(error);
    {
        Inspection source(file);
        if (!source.db.open()) return fail("Não foi possível abrir o backup: " + source.db.lastError().text());
        if (!validate(source.db,error) || !schema(source.db,sourceSchema,error)) return fail(error);
        if (sourceSchema != currentSchema) return fail("Backup incompatível com a estrutura desta versão do sistema.");
        QSqlQuery version(source.db);
        if (!version.exec("SELECT MAX(version) FROM schema_migrations") || !version.next() || version.value(0).toInt()!=6)
            return fail("Versão de backup não suportada. Esta versão restaura bancos na versão 6.");
        version.finish();
        QSqlQuery copy(source.db);
        copy.prepare("VACUUM main INTO ?"); copy.addBindValue(staged);
        if (!copy.exec()) return fail("Não foi possível preparar uma cópia do backup: " + copy.lastError().text());
    }
    QSqlQuery q;
    if (!q.exec("SELECT id FROM cash_sessions WHERE status='open'")) return fail(q.lastError().text());
    if (q.next()) return fail("Feche o caixa atual antes de restaurar.");
    q.finish();
    QString safety;
    const auto safetyFolder = QFileInfo(live.databaseName()).absolutePath()+"/backups";
    if (!snapshot(safetyFolder,"antes_restauracao_",safety)) return false;
    q.prepare("ATTACH DATABASE ? AS recovery"); q.addBindValue(staged);
    if (!q.exec()) return fail(q.lastError().text());
    bool transaction=false;
    auto abort = [&](const QString &message) {
        q.finish();
        if (transaction) QSqlDatabase::database().rollback();
        QSqlQuery detach; detach.exec("DETACH DATABASE recovery");
        return fail(message + " Cópia de segurança: " + safety);
    };
    if (!q.exec("BEGIN IMMEDIATE")) return abort(q.lastError().text());
    transaction=true;
    if (!q.exec("SELECT id FROM main.cash_sessions WHERE status='open'")) return abort(q.lastError().text());
    if (q.next()) return abort("O caixa foi aberto durante a preparação. Restauração cancelada.");
    q.finish();
    if (!q.exec("PRAGMA defer_foreign_keys=ON")) return abort(q.lastError().text());
    QStringList tables;
    for (auto it=currentSchema.cbegin();it!=currentSchema.cend();++it)
        if (it.key().startsWith("table:")) tables.append(it.key().mid(6));
    for (const auto &table : tables) {
        if (!q.exec("DELETE FROM main."+identifier(table))) return abort(q.lastError().text());
    }
    for (const auto &table : tables) {
        if (!q.exec("INSERT INTO main."+identifier(table)+" SELECT * FROM recovery."+identifier(table))) return abort(q.lastError().text());
    }
    if (!q.exec("DELETE FROM main.sqlite_sequence") || !q.exec("INSERT INTO main.sqlite_sequence SELECT * FROM recovery.sqlite_sequence"))
        return abort(q.lastError().text());
    if (!q.exec("PRAGMA main.foreign_key_check")) return abort(q.lastError().text());
    if (q.next()) return abort("Falha de integridade durante a restauração.");
    q.finish();
    if (!QSqlDatabase::database().commit()) return abort(QSqlDatabase::database().lastError().text());
    transaction=false;
    q.exec("DETACH DATABASE recovery");
    m_message="Restauração concluída. Abra novamente o aplicativo. Cópia anterior: " + safety;
    list(m_folder);
    Auth::resetSession();
    emit restored();
    return true;
}
}
