#include "../auth/auth.h"
#include "../audit/audit.h"
#include "settings.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

namespace MHStore {
namespace {
struct Module {
    QString id;
    QString label;
    bool configurable;
    QStringList dependencies;
};
// Order defines navigation. Only implemented modules belong in this registry.
const QList<Module> &registry() {
    static const QList<Module> entries={
        {"dashboard","Dashboard",false,{}},
        {"pos","PDV",true,{"inventory","cash"}},
        {"sales","Vendas",false,{}},
        {"cash","Caixa",true,{}},
        {"products","Produtos",false,{}},
        {"categories","Categorias",false,{}},
        {"inventory","Estoque",true,{}},
        {"customers","Clientes",false,{}},
        {"suppliers","Fornecedores",false,{}}
    };
    return entries;
}
QString dependencyError(const QVariantMap &values) {
    for (const auto &module : registry()) {
        if (!module.configurable || !values.value(module.id).toBool()) continue;
        for (const auto &dependency : module.dependencies) {
            if (values.value(dependency).toBool()) continue;
            for (const auto &entry : registry())
                if (entry.id==dependency) return QString("%1 depende de %2. Habilite a dependência ou desabilite %1.").arg(module.label,entry.label);
        }
    }
    return {};
}
}
QVariantList Settings::modules() const {
    QVariantList result;
    for (const auto &module : registry()) {
        QStringList labels;
        for (const auto &dependency : module.dependencies)
            for (const auto &entry : registry()) if (entry.id==dependency) labels.append(entry.label);
        result.append(QVariantMap{{"id",module.id},{"label",module.label},
            {"configurable",module.configurable},{"dependencies",module.dependencies},
            {"description", labels.isEmpty() ? QString() : "Depende de " + labels.join(" e ")}});
    }
    return result;
}
QStringList Settings::navigation() const {
    QStringList result;
    for (const auto &module : registry()) {
        bool available=!module.configurable || m_values.value(module.id).toBool();
        for (const auto &dependency : module.dependencies) available=available && m_values.value(dependency).toBool();
        if (available) result.append(module.label);
    }
    return result;
}

Settings::Settings(QObject *parent) : QObject(parent) {
    QSqlQuery q("SELECT company, profile, inventory, cash, pos FROM business_settings WHERE id=1");
    if (q.next()) {
        int i=0;
        for (const auto &key : {"company", "profile", "inventory", "cash", "pos"}) m_values[key]=q.value(i++);
    } else m_message="Não foi possível carregar as configurações.";
}
bool Settings::fail(const QString &message) { m_message=message; emit changed(); return false; }
bool Settings::enabled(const QString &module) {
    const Module *definition=nullptr;
    for (const auto &entry : registry()) if (entry.id==module) definition=&entry;
    if (!definition) return false;
    if (!definition->configurable) return true;
    // Read all flags in one snapshot and fail closed if the configuration is unavailable.
    QSqlQuery q("SELECT inventory,cash,pos FROM business_settings WHERE id=1");
    if (!q.next()) return false;
    const QVariantMap values={{"inventory",q.value(0)},{"cash",q.value(1)},{"pos",q.value(2)}};
    if (!values.value(module).toBool()) return false;
    for (const auto &dependency : definition->dependencies) if (!values.value(dependency).toBool()) return false;
    return true;
}
bool Settings::saveModules(const QString &company, const QString &profile, const QVariantMap &modules) {
    QStringList expected;
    for (const auto &entry : registry()) if (entry.configurable) expected.append(entry.id);
    if (modules.size()!=expected.size()) return fail("Informe todos os módulos configuráveis, sem módulos adicionais.");
    for (const auto &key : expected)
        if (!modules.contains(key) || modules.value(key).metaType().id()!=QMetaType::Bool)
            return fail("Configuração de módulos inválida.");
    return save(company,profile,modules.value("inventory").toBool(),modules.value("cash").toBool(),modules.value("pos").toBool());
}
bool Settings::save(const QString &company, const QString &profile, bool inventory, bool cash, bool pos) {
    if (!Auth::allowed("settings")) return fail("Acesso negado. Entre com um usuário autorizado.");
    const auto name=company.trimmed();
    if (name.isEmpty() || name.size()>120) return fail("Informe o nome da empresa com até 120 caracteres.");
    if (!QStringList{"general","fashion","market","services"}.contains(profile)) return fail("Perfil de negócio inválido.");
    const auto dependency=dependencyError({{"inventory",inventory},{"cash",cash},{"pos",pos}});
    if (!dependency.isEmpty()) return fail(dependency);
    QSqlQuery q;
    if (!q.exec("BEGIN IMMEDIATE")) return fail("Banco ocupado. Tente novamente.");
    auto abort=[&](const QString &message) { QSqlDatabase::database().rollback(); return fail(message); };
    if (!q.exec("SELECT inventory,cash,pos FROM business_settings WHERE id=1") || !q.next()) return abort("Falha ao consultar configurações.");
    const bool changing=q.value(0).toBool()!=inventory || q.value(1).toBool()!=cash || q.value(2).toBool()!=pos;
    q.finish();
    if (changing) {
        if (hasPendingCart && hasPendingCart()) return abort("Finalize ou limpe o carrinho antes de alterar os módulos.");
        if (!q.exec("SELECT id FROM cash_sessions WHERE status='open'")) return abort(q.lastError().text());
        if (q.next()) return abort("Feche o caixa antes de alterar os módulos.");
        q.finish();
    }
    q.prepare("UPDATE business_settings SET company=?,profile=?,inventory=?,cash=?,pos=? WHERE id=1");
    q.addBindValue(name); q.addBindValue(profile); q.addBindValue(inventory); q.addBindValue(cash); q.addBindValue(pos);
    if (!q.exec()) return abort(q.lastError().text());
    if (!Audit::record("settings.update","Empresa e módulos",
        QString("empresa=%1; perfil=%2; estoque=%3; caixa=%4; pdv=%5").arg(name,profile).arg(inventory).arg(cash).arg(pos)))
        return abort("Falha ao registrar a auditoria da alteração.");
    if (!QSqlDatabase::database().commit()) return abort("Não foi possível salvar as configurações.");
    m_values={{"company",name},{"profile",profile},{"inventory",inventory},{"cash",cash},{"pos",pos}};
    m_message="Configurações salvas."; emit changed(); return true;
}
}
