#include "../../core/diagnostics/diagnostics.h"
#include "../../core/auth/auth.h"
#include "../../core/settings/settings.h"
#include "inventory.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <cmath>

namespace MHStore {
namespace {
QVariantList records(QSqlQuery &query)
{
    QVariantList rows;
    while (query.next()) {
        QVariantMap row;
        for (int i = 0; i < query.record().count(); ++i)
            row.insert(query.record().fieldName(i), query.value(i));
        rows.append(row);
    }
    return rows;
}
}
Inventory::Inventory(QObject *parent) : QObject(parent) { refresh(); }

bool Inventory::fail(const QString &message)
{
    m_error = message;
    emit changed();
    return false;
}

void Inventory::refresh(const QString &search, bool criticalOnly)
{
    if (!Auth::allowed("read")) { m_products.clear(); m_history.clear(); emit changed(); return; }
    m_search = search;
    m_criticalOnly = criticalOnly;
    m_error.clear();
    QString term = search.trimmed();
    term.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_");
    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT id, code, name, active, stock_quantity, minimum_stock FROM products WHERE "
        "(name LIKE :term ESCAPE '\\' OR code LIKE :term ESCAPE '\\' OR barcode LIKE :term ESCAPE '\\') %1 "
        "ORDER BY active DESC, name COLLATE NOCASE")
        .arg(criticalOnly ? QStringLiteral("AND active = 1 AND stock_quantity <= minimum_stock") : QString()));
    query.bindValue(":term", "%" + term + "%");
    if (!query.exec()) { m_products.clear(); fail(query.lastError().text()); return; }
    m_products = records(query);
    selectProduct(m_productId);
}

void Inventory::selectProduct(int productId)
{
    if (!Auth::allowed("read")) { m_history.clear(); emit changed(); return; }
    m_productId = productId;
    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT m.*, p.name AS product_name, p.code AS product_code, "
        "datetime(m.created_at, 'localtime') AS local_created_at FROM inventory_movements m "
        "JOIN products p ON p.id = m.product_id WHERE (:id = 0 OR m.product_id = :id) ORDER BY m.id DESC LIMIT 200"));
    query.bindValue(":id", productId);
    if (!query.exec()) { m_history.clear(); fail(query.lastError().text()); return; }
    m_history = records(query);
    emit changed();
}

bool Inventory::move(int productId, const QString &type, const QString &quantity,
                     const QString &reason, const QString & /*operatorName*/)
{
    const QString operatorName = Auth::operatorName();
    if (!Auth::allowed("inventory")) return fail("Acesso negado. Entre com um usuário autorizado.");
    if (!Settings::enabled("inventory")) return fail("Módulo desabilitado nas configurações da empresa.");
    if (type != "entry" && type != "exit" && type != "adjustment")
        return fail(QStringLiteral("Tipo de movimentação inválido."));
    if (reason.trimmed().isEmpty() || operatorName.trimmed().isEmpty())
        return fail(QStringLiteral("Informe o motivo e o responsável pela movimentação."));
    QString input = quantity.trimmed();
    input.replace(',', '.');
    bool ok = false;
    const double amount = input.toDouble(&ok);
    if (!ok || !std::isfinite(amount) || amount < 0 || amount > 1e9 ||
        (amount == 0 && type != "adjustment") ||
        std::abs(amount * 1000 - std::round(amount * 1000)) > 0.0001)
        return fail(QStringLiteral("Informe uma quantidade válida com até três casas decimais; entradas e saídas devem ser maiores que zero."));

    auto database = QSqlDatabase::database();
    QSqlQuery query(database);
    // Obtain the write lock before reading the balance, so two connections cannot overwrite each other.
    if (!query.exec(QStringLiteral("BEGIN IMMEDIATE"))) { Diagnostics::record(Diagnostics::Level::Error,Diagnostics::Event::TransactionStartFailed,Diagnostics::Component::Inventory); return fail(query.lastError().text()); }
    auto rollback = [&](const QString &message) {
        database.rollback(); Diagnostics::record(Diagnostics::Level::Warning,Diagnostics::Event::TransactionRollback,Diagnostics::Component::Inventory);
        return fail(message);
    };
    query.prepare(QStringLiteral("SELECT stock_quantity, active FROM products WHERE id = :id"));
    query.bindValue(":id", productId);
    if (!query.exec()) return rollback(query.lastError().text());
    if (!query.next()) return rollback(QStringLiteral("Produto não encontrado."));
    if (!query.value(1).toBool()) return rollback(QStringLiteral("Reative o produto antes de movimentar o estoque."));
    const double previous = query.value(0).toDouble();
    query.finish();
    const double balance = std::round((type == "adjustment" ? amount : previous + (type == "entry" ? amount : -amount)) * 1000) / 1000;
    if (balance < 0) return rollback(QStringLiteral("Estoque insuficiente para esta saída."));
    if (balance > 1e9) return rollback(QStringLiteral("Saldo acima do limite permitido."));
    const double delta = std::round((balance - previous) * 1000) / 1000;
    if (delta == 0) return rollback(QStringLiteral("O saldo informado já corresponde ao estoque atual."));
    query.prepare(QStringLiteral("UPDATE products SET stock_quantity = :balance WHERE id = :id"));
    query.bindValue(":balance", balance);
    query.bindValue(":id", productId);
    if (!query.exec()) return rollback(query.lastError().text());
    query.prepare(QStringLiteral("INSERT INTO inventory_movements "
        "(product_id, type, quantity, previous_balance, balance, reason, operator_name,user_id) "
        "VALUES (:id, :type, :quantity, :previous, :balance, :reason, :operator,:user)"));
    query.bindValue(":id", productId);
    query.bindValue(":type", type);
    query.bindValue(":quantity", delta);
    query.bindValue(":previous", previous);
    query.bindValue(":balance", balance);
    query.bindValue(":reason", reason.trimmed());
    query.bindValue(":operator", operatorName.trimmed());
    query.bindValue(":user", Auth::userId());
    if (!query.exec()) return rollback(query.lastError().text());
    if (!database.commit()) return rollback(database.lastError().text());
    refresh(m_search, m_criticalOnly);
    emit stockChanged();
    return true;
}
}
