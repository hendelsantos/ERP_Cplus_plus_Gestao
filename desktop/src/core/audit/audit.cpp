#include "audit.h"
#include "../auth/auth.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QMap>

namespace MHStore {
namespace {
constexpr int pageSize=50;
QString actionLabel(const QString &action) {
    static const QMap<QString,QString> labels={
        {"user.create","Usuário criado"},
        {"user.update","Usuário alterado"},
        {"user.password","Senha alterada pelo próprio usuário"},
        {"user.recovery_code","Código de recuperação emitido"},
        {"sale.adjust","Venda com desconto/acréscimo"},
        {"catalog.create","Cadastro criado"},
        {"catalog.update","Cadastro alterado"},
        {"catalog.deactivate","Cadastro inativado"},
        {"catalog.reactivate","Cadastro reativado"},
        {"settings.update","Empresa e módulos alterados"},
    };
    return labels.value(action,action);
}
}
bool Audit::record(const QString &action,const QString &target,const QString &details) {
    const auto user=Auth::current();
    if (user.isEmpty() || action.isEmpty() || target.isEmpty() || details.isEmpty()) return false;
    QSqlQuery q;
    q.prepare("INSERT INTO audit_log(user_id,user_name,action,target,details,created_at) VALUES(?,?,?,?,?,?)");
    for (const auto &v : QVariantList{user.value("id"),user.value("name"),action,target,details,
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")}) q.addBindValue(v);
    return q.exec();
}
bool Audit::refresh() { return fetch(0); }
bool Audit::loadMore() { return m_more ? fetch(m_entries.size()) : false; }
bool Audit::fetch(int offset) {
    if (!Auth::allowed("audit")) {
        m_entries.clear(); m_more=false; m_message="Sem permissão para consultar a auditoria."; emit changed(); return false;
    }
    QSqlQuery q;
    q.prepare("SELECT id,user_name,action,target,details,created_at FROM audit_log ORDER BY id DESC LIMIT ? OFFSET ?");
    q.addBindValue(pageSize+1); q.addBindValue(offset);
    if (!q.exec()) {
        m_entries.clear(); m_more=false; m_message="Não foi possível consultar a auditoria."; emit changed(); return false;
    }
    if (offset==0) m_entries.clear();
    m_more=false;
    int appended=0;
    while (q.next()) {
        if (appended==pageSize) { m_more=true; break; }
        const auto when=QDateTime::fromString(q.value(5).toString(),"yyyy-MM-dd HH:mm:ss");
        m_entries.append(QVariantMap{{"id",q.value(0)},{"user_name",q.value(1)},{"action",q.value(2)},
            {"action_label",actionLabel(q.value(2).toString())},{"target",q.value(3)},{"details",q.value(4)},
            {"created_at",when.isValid()?when.toString("dd/MM/yyyy HH:mm:ss"):q.value(5)}});
        ++appended;
    }
    m_message.clear(); emit changed(); return true;
}
}
