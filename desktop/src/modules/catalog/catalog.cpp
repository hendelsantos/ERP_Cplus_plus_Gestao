#include "../../core/auth/auth.h"
#include "catalog.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <cmath>

namespace MHStore {
namespace {
QString tableFor(const QString &section)
{
    if (section == QStringLiteral("Produtos")) return QStringLiteral("products");
    if (section == QStringLiteral("Clientes")) return QStringLiteral("customers");
    if (section == QStringLiteral("Categorias")) return QStringLiteral("categories");
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
    if (!Auth::allowed("read")) { m_rows.clear(); m_categories.clear(); emit changed(); return; }
    m_error.clear();
    QSqlQuery categories;
    if (!categories.exec(QStringLiteral("SELECT id, name, active FROM categories ORDER BY name COLLATE NOCASE"))) {
        fail(categories.lastError().text()); return;
    }
    m_categories = records(categories);
    QString filter = QStringLiteral("name LIKE :term ESCAPE '\\' OR CAST(id AS TEXT) = :exact");
    if (m_section == QStringLiteral("Produtos"))
        filter += QStringLiteral(" OR code LIKE :term ESCAPE '\\' OR barcode LIKE :term ESCAPE '\\'");
    if (m_section == QStringLiteral("Clientes"))
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
        for (const auto &field : {"cost_price", "sale_price", "minimum_stock"}) {
            bool ok = false;
            QString input = values.value(field).toString().trimmed();
            input.replace(',', '.');
            const double number = input.toDouble(&ok);
            if (!ok || !std::isfinite(number) || number < 0 || number > 1e9)
                return fail(QStringLiteral("Preços e estoque mínimo devem ser números entre 0 e 1 bilhão, sem separador de milhar."));
            data.insert(field, QString(field) == "minimum_stock" ? number : std::round(number * 100.0) / 100.0);
        }
        data.insert("cost_price_cents", qRound64(data.value("cost_price").toDouble() * 100));
        data.insert("sale_price_cents", qRound64(data.value("sale_price").toDouble() * 100));
        const int category = values.value("category_id").toInt();
        data.insert("category_id", category > 0 ? QVariant(category) : QVariant());
    } else if (table == "customers") {
        for (const auto &field : {"document", "phone", "email"})
            data.insert(field, values.value(field).toString().trimmed());
    }
    QStringList fields, placeholders, assignments;
    for (auto it = data.cbegin(); it != data.cend(); ++it) {
        fields << it.key();
        placeholders << ":" + it.key();
        assignments << it.key() + " = :" + it.key();
    }
    QSqlQuery query;
    query.prepare(id == 0
        ? QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)").arg(table, fields.join(','), placeholders.join(','))
        : QStringLiteral("UPDATE %1 SET %2 WHERE id = :id").arg(table, assignments.join(',')));
    for (auto it = data.cbegin(); it != data.cend(); ++it) query.bindValue(":" + it.key(), it.value());
    if (id != 0) query.bindValue(":id", id);
    if (!query.exec()) {
        if (query.lastError().nativeErrorCode() == "19" || query.lastError().text().contains("UNIQUE"))
            return fail(QStringLiteral("Código ou categoria já cadastrado, ou categoria inválida. Verifique os dados."));
        return fail(query.lastError().text());
    }
    if (query.numRowsAffected() != 1) return fail(QStringLiteral("Registro não encontrado."));
    refresh();
    return true;
}

bool Catalog::setActive(const QString &section, int id, bool active)
{
    if (!Auth::allowed("catalog")) return fail("Acesso negado. Entre com um usuário autorizado.");
    const auto table = tableFor(section);
    if (table.isEmpty() || id <= 0) return fail(QStringLiteral("Registro inválido."));
    QSqlQuery query;
    query.prepare(QStringLiteral("UPDATE %1 SET active = :active WHERE id = :id").arg(table));
    query.bindValue(":active", active ? 1 : 0);
    query.bindValue(":id", id);
    if (!query.exec()) return fail(query.lastError().text());
    if (query.numRowsAffected() != 1) return fail(QStringLiteral("Registro não encontrado."));
    refresh();
    return true;
}
}
