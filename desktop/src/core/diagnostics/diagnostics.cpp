#include "diagnostics.h"
#include <QDateTime>
#include <iterator>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QSettings>
#include <QCoreApplication>

namespace MHStore {
namespace {
QMutex mutex;
QString folder, problem;
qint64 limit=1024*1024;
int threshold=0;
bool unclean=false, active=false;
const char *events[]={"startup","shutdown","unclean_shutdown","database_open_failed",
    "integrity_failed","migration_started","migration_completed","migration_failed",
    "transaction_rollback","backup_started","backup_completed","backup_failed","restore_completed","transaction_start_failed"};
const char *components[]={"application","database","auth","catalog","inventory","pos","finance","settings","backup"};
const char *levels[]={"info","warning","error"};
}
bool Diagnostics::start(const QString &dataFolder,qint64 maxBytes) {
    {
        QMutexLocker lock(&mutex);
        active=false; folder=dataFolder; problem.clear(); limit=qMax<qint64>(256,maxBytes);
        unclean=QFile::exists(folder+"/session.active");
        QSettings config(folder+"/diagnostics.ini",QSettings::IniFormat);
        threshold=config.value("level",0).toInt(); if(threshold<0 || threshold>2) threshold=0;
        if(!QDir().mkpath(folder+"/logs")) { problem="Não foi possível preparar os logs. Verifique as permissões da pasta de dados."; return false; }
        QSaveFile marker(folder+"/session.active");
        if(!marker.open(QIODevice::WriteOnly) || marker.write("active\n")!=7 || !marker.commit()) {
            problem="Não foi possível registrar a sessão. Verifique espaço e permissões da pasta de dados."; return false;
        }
        active=true;
    }
    record(Level::Info,Event::Startup,Component::Application);
    if(unclean) record(Level::Warning,Event::UncleanShutdown,Component::Application);
    return report().value("error").toString().isEmpty();
}
void Diagnostics::finish() {
    record(Level::Info,Event::Shutdown,Component::Application);
    QMutexLocker lock(&mutex);
    if(active && !QFile::remove(folder+"/session.active")) problem="Não foi possível registrar o encerramento normal.";
    active=false;
}
void Diagnostics::record(Level level,Event event,Component component) {
    QMutexLocker lock(&mutex);
    const int l=int(level), e=int(event), c=int(component);
    if(!active || l<0 || l>2 || e<0 || e>=int(std::size(events)) || c<0 || c>=int(std::size(components)) || l<threshold) return;
    const QByteArray line=(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)+" "+levels[l]+" "+components[c]+" "+events[e]+"\n").toUtf8();
    const auto path=folder+"/logs/operations.log";
    QFile file(path);
    if(file.size()+line.size()>limit) {
        // Three archives plus the current file. Stop on failure; never truncate a log.
        const auto oldest=path+".3";
        if(QFile::exists(oldest) && !QFile::remove(oldest)) { problem="Falha na rotação dos logs. Verifique as permissões."; return; }
        for(int i=2;i>=0;--i) {
            const auto source=i?path+"."+QString::number(i):path;
            if(QFile::exists(source) && !QFile::rename(source,path+"."+QString::number(i+1))) { problem="Falha na rotação dos logs. Verifique as permissões."; return; }
        }
    }
    if(!file.open(QIODevice::WriteOnly|QIODevice::Append) || file.write(line)!=line.size() || !file.flush()) {
        problem="Não foi possível gravar logs. Verifique espaço e permissões da pasta de dados."; return;
    }
    file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    problem.clear();
}
bool Diagnostics::setLevel(int level) {
    QMutexLocker lock(&mutex);
    if(!active || level<0 || level>2) return false;
    QSettings config(folder+"/diagnostics.ini",QSettings::IniFormat);
    config.setValue("level",level); config.sync();
    if(config.status()!=QSettings::NoError) { problem="Não foi possível salvar o nível de log."; return false; }
    threshold=level; return true;
}
QVariantMap Diagnostics::report() {
    QMutexLocker lock(&mutex);
    return {{"application",QCoreApplication::applicationVersion()}, {"qt",QString::fromLatin1(qVersion())},
        {"logs",folder+"/logs"}, {"level",threshold}, {"unclean_shutdown",unclean}, {"error",problem}};
}
}
