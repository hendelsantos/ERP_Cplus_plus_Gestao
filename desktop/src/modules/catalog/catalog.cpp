#include "../../core/auth/auth.h"
#include "catalog.h"
#include "../../core/audit/audit.h"
#include <QSqlDatabase>

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <cmath>

namespace MHStore {
namespace {
class Transaction {
public:
    bool active=false;
    bool begin() { QSqlQuery q; active=q.exec("BEGIN IMMEDIATE"); return active; }
    bool commit() { if (!QSqlDatabase::database().commit()) return false; active=false; return true; }
    ~Transaction() { if (active) QSqlDatabase::database().rollback(); }
};
QString fieldLabels(const QStringList &fields) {
    static const QMap<QString,QString> labels={
        {"name","Nome"},{"code","Código"},{"barcode","Código de barras"},
        {"category_id","Categoria"},{"supplier_id","Fornecedor"},
        {"cost_price","Preço de custo"},{"sale_price","Preço de venda"},
        {"minimum_stock","Estoque mínimo"},{"maximum_stock","Estoque máximo"},
        {"brand","Marca"},{"unit","Unidade"},{"size","Tamanho"},{"color","Cor"},{"location","Localização"},
        {"document","Documento"},{"phone","Telefone"},{"email","E-mail"},
        {"address","Endereço"},{"birth_date","Nascimento"},{"notes","Observações"}
    };
    QStringList result;
    for (const auto &field : fields) {
        // Legacy REAL prices and integer cents describe the same business field.
        if (field=="cost_price_cents") {
            if (!fields.contains("cost_price")) result.append("Preço de custo");
        } else if (field=="sale_price_cents") {
            if (!fields.contains("sale_price")) result.append("Preço de venda");
        } else result.append(labels.value(field,field));
    }
    return result.join(", ");
}
QString tableFor(const QString &section)
{
    if (section == QStringLiteral("Produtos")) return QStringLiteral("products");
    if (section == QStringLiteral("Clientes")) return QStringLiteral("customers");
    if (section == QStringLiteral("Categorias")) return QStringLiteral("categories");
    if (section == QStringLiteral("Fornecedores")) return QStringLiteral("suppliers");
    return {};
}
QVariantList records(QSqlQuery &query)
{
    QVariantList result;
    while (query.next()) {
        QVariantMap row;
        for (int i = 0; i < query.record().count(); ++i)
            row.insert(query.record().fieldName(i), query.value(i));
        result.append(row);
    }
    return result;
}
}

Catalog::Catalog(QObject *parent) : QObject(parent) { refresh(); }

bool Catalog::fail(const QString &message)
{
    m_error = message;
    emit changed();
    return false;
}

void Catalog::search(const QString &section, const QString &text, bool includeInactive)
{
    if (tableFor(section).isEmpty()) { fail(QStringLiteral("Cadastro inválido.")); return; }
    m_section = section;
    m_text = text.trimmed();
    m_includeInactive = includeInactive;
    refresh();
}

void Catalog::refresh()
{
    if (!Auth::allowed("read")) { m_rows.clear(); m_categories.clear(); m_suppliers.clear(); emit changed(); return; }
    m_error.clear();
    QSqlQuery categories;
    if (!categories.exec(QStringLiteral("SELECT id, name, active FROM categories ORDER BY name COLLATE NOCASE"))) {
        fail(categories.lastError().text()); return;
    }
    m_categories = records(categories);
    QSqlQuery suppliers;
    if (!suppliers.exec(QStringLiteral("SELECT id, name, active FROM suppliers ORDER BY name COLLATE NOCASE"))) {
        fail(suppliers.lastError().text()); return;
    }
    m_suppliers = records(suppliers);
    QString filter = QStringLiteral("name LIKE :term ESCAPE '\\' OR CAST(id AS TEXT) = :exact");
    if (m_section == QStringLiteral("Produtos"))
        filter += QStringLiteral(" OR code LIKE :term ESCAPE '\\' OR barcode LIKE :term ESCAPE '\\'");
    if (m_section == QStringLiteral("Clientes") || m_section == QStringLiteral("Fornecedores"))
        filter += QStringLiteral(" OR document LIKE :term ESCAPE '\\' OR phone LIKE :term ESCAPE '\\'");
    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT * FROM %1 WHERE (%2) %3 ORDER BY name COLLATE NOCASE")
        .arg(tableFor(m_section), filter, m_includeInactive ? QString() : QStringLiteral("AND active = 1")));
    QString escaped = m_text;
    escaped.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_");
    query.bindValue(QStringLiteral(":term"), QStringLiteral("%") + escaped + QStringLiteral("%"));
    query.bindValue(QStringLiteral(":exact"), m_text);
    if (!query.exec()) { m_rows.clear(); fail(query.lastError().text()); return; }
    m_rows = records(query);
    emit changed();
}

bool Catalog::save(const QString &section, int id, const QVariantMap &values)
{
    if (!Auth::allowed("catalog")) return fail("Acesso negado. Entre com um usuário autorizado.");
    const QString table = tableFor(section);
    if (table.isEmpty() || id < 0) return fail(QStringLiteral("Cadastro inválido."));
    QVariantMap data;
    const auto name = values.value("name").toString().trimmed();
    if (name.isEmpty()) return fail(QStringLiteral("Informe o nome."));
    data.insert("name", name);
    if (table == "products") {
        const auto code = values.value("code").toString().trimmed();
        if (code.isEmpty()) return fail(QStringLiteral("Informe o código interno do produto."));
        data.insert("code", code);
        data.insert("barcode", values.value("barcode").toString().trimmed());
        for (const auto &field : {"cost_price", "sale_price", "minimum_stock", "maximum_stock"}) {
            bool ok = false;
            QString input = values.value(field, QString(field)=="maximum_stock" ? QVariant("0") : QVariant()).toString().trimmed();
            if (QString(field)=="maximum_stock" && input.isEmpty()) input="0";
            input.replace(',', '.');
            const double number = input.toDouble(&ok);
            if (!ok || !std::isfinite(number) || number < 0 || number > 1e9)
                return fail(QStringLiteral("Preços e estoques devem ser números entre 0 e 1 bilhão, sem separador de milhar."));
            data.insert(field, QString(field) == "minimum_stock" || QString(field) == "maximum_stock"
                ? number : std::round(number * 100.0) / 100.0);
        }
        if (data.value("maximum_stock").toDouble() > 0
            && data.value("maximum_stock").toDouble() < data.value("minimum_stock").toDouble())
            return fail(QStringLiteral("Estoque máximo deve ser maior ou igual ao estoque mínimo (0 ignora o máximo)."));
        data.insert("cost_price_cents", qRound64(data.value("cost_price").toDouble() * 100));
        data.insert("sale_price_cents", qRound64(data.value("sale_price").toDouble() * 100));
        const auto brand = values.value("brand", QStringLiteral("")).toString().trimmed();
        const auto unit = values.value("unit", QStringLiteral("")).toString().trimmed();
        const auto size = values.value("size", QStringLiteral("")).toString().trimmed();
        const auto color = values.value("color", QStringLiteral("")).toString().trimmed();
        const auto variantGroup = values.value("variant_group", QStringLiteral("")).toString().trimmed();
        const auto location = values.value("location", QStringLiteral("")).toString().trimmed();
        const auto notes = values.value("notes", QStringLiteral("")).toString().trimmed();
        if (brand.size() > 60 || unit.size() > 10 || size.size() > 20 || color.size() > 40 || variantGroup.size() > 60 || location.size() > 60 || notes.size() > 500)
            return fail(QStringLiteral("Marca e localização aceitam até 60 caracteres; unidade, 10; tamanho, 20; cor, 40; observações, 500."));
        data.insert("brand", brand);
        data.insert("unit", unit);
        data.insert("size", size);
        data.insert("color", color);
        data.insert("variant_group", variantGroup);
        data.insert("location", location);
        data.insert("notes", notes);
        const int category = values.value("category_id").toInt();
        data.insert("category_id", category > 0 ? QVariant(category) : QVariant());
        const int supplier = values.value("supplier_id").toInt();
        data.insert("supplier_id", supplier > 0 ? QVariant(supplier) : QVariant());
        const auto productType = values.value("product_type", QStringLiteral("product")).toString();
        if (productType != "product" && productType != "service") return fail(QStringLiteral("Tipo de cadastro inválido."));
        data.insert("product_type", productType);
    } else if (table == "customers") {
        const auto address = values.value("address", QStringLiteral("")).toString().trimmed();
        const auto notes = values.value("notes", QStringLiteral("")).toString().trimmed();
        if (address.size() > 160 || notes.size() > 500)
            return fail(QStringLiteral("Endereço aceita até 160 caracteres; observações, até 500."));
        data.insert("address", address);
        data.insert("notes", notes);
        const auto birth = values.value("birth_date", QStringLiteral("")).toString().trimmed();
        if (!birth.isEmpty()) {
            auto date = QDate::fromString(birth, "dd/MM/yyyy");
            if (!date.isValid()) date = QDate::fromString(birth, "yyyy-MM-dd");
            if (!date.isValid() || date > QDate::currentDate())
                return fail(QStringLiteral("Informe a data de nascimento como DD/MM/AAAA, sem data futura."));
            data.insert("birth_date", date.toString("yyyy-MM-dd"));
        } else data.insert("birth_date", QStringLiteral(""));
        for (const auto &field : {"document", "phone", "email"})
            data.insert(field, values.value(field).toString().trimmed());
    } else if (table == "suppliers") {
        // Unique document: empty input becomes NULL so it never collides.
        const auto document = values.value("document").toString().trimmed();
        data.insert("document", document.isEmpty() ? QVariant() : QVariant(document));
        for (const auto &field : {"phone", "email"})
            data.insert(field, values.value(field).toString().trimmed());
    }
    QStringList fields, placeholders, assignments;
    for (auto it = data.cbegin(); it != data.cend(); ++it) {
        fields << it.key();
        placeholders << ":" + it.key();
        assignments << it.key() + " = :" + it.key();
    }
    Transaction tx;
    if (!tx.begin()) return fail("Banco ocupado. Tente novamente.");
    QSqlQuery query;
    QStringList changedFields;
    if (id!=0) {
        query.prepare(QStringLiteral("SELECT * FROM %1 WHERE id=?").arg(table));
        query.addBindValue(id);
        if (!query.exec()) return fail(query.lastError().text());
        if (!query.next()) return fail("Registro não encontrado.");
        for (auto it=data.cbegin();it!=data.cend();++it) {
            const auto previous=query.value(it.key());
            if (previous.isNull() && it.value().isNull()) continue;
            if (previous!=it.value()) changedFields.append(it.key());
        }
        query.finish();
    } else changedFields=fields;
    // A save without changed values is not an alteration event.
    if (id!=0 && changedFields.isEmpty()) {
        if (!tx.commit()) return fail("Não foi possível concluir a consulta.");
        refresh(); return true;
    }
    query.prepare(id == 0
        ? QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)").arg(table, fields.join(','), placeholders.join(','))
        : QStringLiteral("UPDATE %1 SET %2 WHERE id = :id").arg(table, assignments.join(',')));
    for (auto it = data.cbegin(); it != data.cend(); ++it) query.bindValue(":" + it.key(), it.value());
    if (id != 0) query.bindValue(":id", id);
    if (!query.exec()) {
        if (query.lastError().nativeErrorCode() == "19" || query.lastError().text().contains("UNIQUE"))
            return fail(QStringLiteral("Código, categoria ou documento já cadastrado, ou vínculo inválido. Verifique os dados."));
        return fail(query.lastError().text());
    }
    if (query.numRowsAffected() != 1) return fail(QStringLiteral("Registro não encontrado."));
    const auto recordId=id==0 ? query.lastInsertId().toLongLong() : id;
    if (!Audit::record(id==0 ? "catalog.create" : "catalog.update",
        section + " #" + QString::number(recordId), "Campos: " + fieldLabels(changedFields)))
        return fail("Falha ao registrar auditoria. Cadastro não salvo.");
    if (!tx.commit()) return fail("Não foi possível salvar o cadastro.");
    refresh();
    return true;
}

bool Catalog::setActive(const QString &section, int id, bool active)
{
    if (!Auth::allowed("catalog")) return fail("Acesso negado. Entre com um usuário autorizado.");
    const auto table = tableFor(section);
    if (table.isEmpty() || id <= 0) return fail(QStringLiteral("Registro inválido."));
    Transaction tx;
    if (!tx.begin()) return fail("Banco ocupado. Tente novamente.");
    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT active FROM %1 WHERE id=?").arg(table));
    query.addBindValue(id);
    if (!query.exec()) return fail(query.lastError().text());
    if (!query.next()) return fail("Registro não encontrado.");
    const bool unchanged=query.value(0).toBool()==active;
    query.finish();
    if (unchanged) {
        if (!tx.commit()) return fail("Não foi possível concluir a consulta.");
        refresh(); return true;
    }
    query.prepare(QStringLiteral("UPDATE %1 SET active = :active WHERE id = :id").arg(table));
    query.bindValue(":active", active ? 1 : 0);
    query.bindValue(":id", id);
    if (!query.exec()) return fail(query.lastError().text());
    if (query.numRowsAffected() != 1) return fail(QStringLiteral("Registro não encontrado."));
    if (!Audit::record(active ? "catalog.reactivate" : "catalog.deactivate",
        section + " #" + QString::number(id), active ? "Status: inativo → ativo" : "Status: ativo → inativo"))
        return fail("Falha ao registrar auditoria. Status não alterado.");
    if (!tx.commit()) return fail("Não foi possível alterar o status.");
    refresh();
    return true;
}
}
