#include "../../core/auth/auth.h"
#include "backup.h"
#include "../../core/database/database.h"
#include <QSettings>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <memory>
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
    result.clear();
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
    m_databasePath=QFileInfo(QSqlDatabase::database().databaseName()).absoluteFilePath();
    m_configPath=m_databasePath+".backup.ini";
    m_owner=QString::fromLatin1(QCryptographicHash::hash(m_databasePath.toUtf8(),QCryptographicHash::Sha256).toHex().left(16));
    QSettings config(m_configPath,QSettings::IniFormat);
    m_automatic={{"enabled",config.value("enabled",false)}, {"folder",config.value("folder",QFileInfo(m_databasePath).absolutePath()+"/backups")},
        {"minutes",config.value("minutes",60)},{"retention",config.value("retention",10)}, {"last_success",config.value("last_success",0)}};
    m_timer.setInterval(60000);
    connect(&m_timer,&QTimer::timeout,this,[this] { runAutomatic(); });
    m_folder = QFileInfo(QSqlDatabase::database().databaseName()).absolutePath() + "/backups";
    list(m_folder);
}
Backup::~Backup() {
    m_timer.stop();
    if(m_worker) { m_worker->wait(); delete m_worker; }
}
bool Backup::configureAutomatic(bool enabled,const QString &folder,int minutes,int retention) {
    if(!Auth::allowed("backup")) return fail("Acesso negado.");
    if(busy()) return fail("Aguarde a cópia em andamento.");
    if(!QDir::isAbsolutePath(folder) || minutes<1 || minutes>10080 || retention<1 || retention>100)
        return fail("Informe pasta absoluta, intervalo de 1 a 10080 minutos e retenção de 1 a 100 arquivos.");
    QSettings config(m_configPath,QSettings::IniFormat);
    const auto last=folder==m_automatic.value("folder").toString()?m_automatic.value("last_success").toLongLong():0;
    config.setValue("enabled",enabled); config.setValue("folder",folder); config.setValue("minutes",minutes);
    config.setValue("retention",retention); config.setValue("last_success",last); config.sync();
    if(config.status()!=QSettings::NoError) return fail("Não foi possível salvar o agendamento. Verifique as permissões da pasta de dados.");
    m_automatic={{"enabled",enabled},{"folder",folder},{"minutes",minutes},{"retention",retention},{"last_success",last}};
    m_message="Agendamento salvo. Cópias automáticas usam uma conexão separada e não exigem login do operador.";
    emit changed(); return true;
}
void Backup::startScheduler() {
    m_timer.start();
    QTimer::singleShot(0,this,[this] { runAutomatic(); });
}
void Backup::runAutomatic(bool force) {
    if(m_restored || !m_automatic.value("enabled").toBool()) return;
    if(busy()) { m_pendingAutomatic |= force; return; }
    const auto folder=m_automatic.value("folder").toString();
    const int minutes=m_automatic.value("minutes").toInt(), keep=m_automatic.value("retention").toInt();
    if(!QDir::isAbsolutePath(folder) || minutes<1 || minutes>10080 || keep<1 || keep>100) {
        m_automaticMessage="Configuração de backup automático inválida. Revise em Configurações."; emit changed(); return;
    }
    const auto now=QDateTime::currentSecsSinceEpoch(), last=m_automatic.value("last_success").toLongLong();
    if(!force && now>=last && now-last<minutes*60) return;
    struct Result { QString error,path; };
    auto result=std::make_shared<Result>();
    const auto databasePath=m_databasePath, owner=m_owner;
    m_worker=QThread::create([databasePath,folder,owner,keep,result] {
        const auto connection=QUuid::createUuid().toString();
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE",connection);
            db.setDatabaseName(databasePath); db.setConnectOptions("QSQLITE_OPEN_READONLY");
            if(!db.open()) result->error="Não foi possível abrir o banco para a cópia automática.";
            else {
                QSqlQuery q(db); q.exec("PRAGMA busy_timeout=5000");
                int version=0;
                if(q.exec("SELECT MAX(version) FROM schema_migrations") && q.next()) version=q.value(0).toInt();
                q.finish();
                if(version<=0 || !QDir().mkpath(folder)) result->error="Não foi possível preparar a pasta ou identificar a versão do banco.";
                else {
                    const auto name="auto_"+owner+"_v"+QString::number(version)+"_"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz_")+QUuid::createUuid().toString(QUuid::WithoutBraces)+".mhb";
                    const auto target=QDir(folder).filePath(name), temporary=target+".partial";
                    q.prepare("VACUUM main INTO ?"); q.addBindValue(temporary);
                    if(!q.exec()) result->error="Falha ao criar backup automático. Verifique espaço e permissões: "+q.lastError().text();
                    else {
                        Inspection inspection(temporary);
                        if(!inspection.db.open() || !validate(inspection.db,result->error)) {
                            if(result->error.isEmpty()) result->error="Falha ao validar a cópia automática.";
                        }
                    }
                    q.finish();
                    if(result->error.isEmpty() && !QFile::rename(temporary,target)) result->error="Não foi possível concluir o arquivo de backup automático.";
                    if(!result->error.isEmpty()) QFile::remove(temporary);
                    else {
                        result->path=target;
                        const QRegularExpression owned("^auto_"+owner+"_v[0-9]+_[0-9]{8}_[0-9]{6}_[0-9]{3}_[0-9a-f-]{36}\\.mhb$");
                        const auto files=QDir(folder).entryInfoList({"auto_"+owner+"_*.mhb"},QDir::Files|QDir::NoSymLinks,QDir::Time);
                        int retained=1;
                        for(const auto &file:files) {
                            if(file.absoluteFilePath()==QFileInfo(target).absoluteFilePath() || !owned.match(file.fileName()).hasMatch()) continue;
                            if(retained++<keep) continue;
                            if(!QFile::remove(file.absoluteFilePath())) result->error="Backup criado, mas a retenção não conseguiu remover um arquivo antigo.";
                        }
                    }
                }
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
    });
    connect(m_worker,&QThread::finished,this,[this,result] {
        auto *finished=m_worker; m_worker=nullptr; finished->deleteLater();
        if(!result->path.isEmpty()) {
            const auto now=QDateTime::currentSecsSinceEpoch();
            QSettings config(m_configPath,QSettings::IniFormat); config.setValue("last_success",now); config.sync();
            m_automatic["last_success"]=now;
            if(config.status()!=QSettings::NoError) result->error="Cópia criada, mas não foi possível salvar o horário do agendamento.";
        }
        m_automaticMessage=result->error.isEmpty()?"Backup automático criado e verificado: "+result->path:result->error;
        emit changed();
        if(m_pendingAutomatic) { m_pendingAutomatic=false; runAutomatic(true); }
    });
    m_automaticMessage="Criando backup automático em segundo plano…"; emit changed(); m_worker->start();
}
bool Backup::fail(const QString &message) { m_message=message; emit changed(); return false; }
void Backup::list(const QString &folder)
{
    if (!Auth::allowed("backup")) { m_files.clear(); emit changed(); return; }
    m_folder = folder;
    m_files.clear();
    if (QDir::isAbsolutePath(folder)) {
        const auto files = QDir(folder).entryInfoList({"*.mhb"},QDir::Files,QDir::Time);
        for (const auto &file : files) {
            QString error;
            int version=0;
            bool valid=false;
            {
                Inspection inspection(file.absoluteFilePath());
                if(inspection.db.open() && validate(inspection.db,error)) {
                    QSqlQuery q(inspection.db);
                    if(q.exec("SELECT MAX(version) FROM schema_migrations") && q.next()) version=q.value(0).toInt();
                    valid=version>=1 && version<=20;
                }
            }
            m_files.append(QVariantMap{{"path",file.absoluteFilePath()}, {"name",file.fileName()},
                {"size",file.size()},{"date",file.lastModified().toString("dd/MM/yyyy HH:mm:ss")},
                {"valid",valid},{"version",version}});
        }
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
    if (busy()) return fail("Aguarde o backup automático em andamento.");
    QString path;
    if (!snapshot(folder,"mhstore_",path)) return false;
    m_message="Backup criado e verificado: " + path;
    list(folder);
    return true;
}
bool Backup::restore(const QString &file)
{
    if (!Auth::allowed("backup")) return fail("Acesso negado. Entre com um usuário autorizado.");
    if (busy()) return fail("Aguarde o backup automático em andamento.");
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
        QSqlQuery version(source.db);
        if (!version.exec("SELECT MAX(version) FROM schema_migrations") || !version.next() || (version.value(0).toInt()<1 || version.value(0).toInt()>20))
            return fail("Versão de backup não suportada. São aceitas versões de 1 a 20, com estrutura validada após migração.");
        version.finish();
        QSqlQuery copy(source.db);
        copy.prepare("VACUUM main INTO ?"); copy.addBindValue(staged);
        if (!copy.exec()) return fail("Não foi possível preparar uma cópia do backup: " + copy.lastError().text());
    }
    {
        Inspection migrated(staged);
        migrated.db.setConnectOptions("");
        Database::DatabaseManager manager;
        if(!manager.initialize(&error,staged,migrated.name)) return fail("Não foi possível migrar a cópia temporária: "+error);
        if(!validate(migrated.db,error) || !schema(migrated.db,sourceSchema,error)) return fail(error);
        if(sourceSchema!=currentSchema) return fail("Backup incompatível com a estrutura desta versão após migração.");
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
    m_restored=true; m_timer.stop();
    Auth::resetSession();
    emit restored();
    return true;
}
}
