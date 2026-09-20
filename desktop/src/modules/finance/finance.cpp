#include "../../core/diagnostics/diagnostics.h"
#include "finance.h"
#include "../../core/auth/auth.h"
#include "../../core/settings/settings.h"
#include "../../core/audit/audit.h"
#include <QDate>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <cmath>

namespace MHStore {
namespace {
QVariantList records(QSqlQuery &query) {
    QVariantList rows;
    while (query.next()) {
        QVariantMap row;
        for (int i = 0; i < query.record().count(); ++i) row.insert(query.record().fieldName(i), query.value(i));
        rows.append(row);
    }
    return rows;
}
bool money(QString value, qint64 &cents) {
    value = value.trimmed(); value.replace(',', '.');
    static const QRegularExpression pattern(QStringLiteral("^[0-9]{1,10}(\\.[0-9]{1,2})?$"));
    if (!pattern.match(value).hasMatch()) return false;
    const auto parts = value.split('.'); cents = parts.first().toLongLong() * 100;
    if (parts.size() == 2) cents += parts.last().leftJustified(2, '0').toLongLong();
    return cents > 0 && cents <= 100000000000LL;
}
}
Finance::Finance(QObject *parent) : QObject(parent) { refresh(); }
bool Finance::fail(const QString &message) { m_error = message; emit changed(); return false; }
void Finance::refresh(bool includePaid) {
    if (!Auth::allowed("read")) { m_expenses.clear(); m_receivables.clear(); m_serviceOrders.clear(); emit changed(); return; }
    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT id, description, amount_cents, due_date, status, paid_at, cash_session_id, operator_name, datetime(created_at,'localtime') AS local_created_at FROM expenses %1 ORDER BY CASE status WHEN 'open' THEN 0 ELSE 1 END, due_date, id DESC")
        .arg(includePaid ? QString() : QStringLiteral("WHERE status='open'")));
    if (!query.exec()) { m_expenses.clear(); fail(query.lastError().text()); return; }
    m_expenses = records(query); m_error.clear(); emit changed();
    query.prepare(QStringLiteral("SELECT r.id, r.description, r.customer_id, c.name AS customer_name, r.amount_cents, r.due_date, r.status, r.received_at, r.cash_session_id, datetime(r.created_at,'localtime') AS local_created_at FROM receivables r LEFT JOIN customers c ON c.id=r.customer_id %1 ORDER BY CASE r.status WHEN 'open' THEN 0 ELSE 1 END, r.due_date, r.id DESC")
        .arg(includePaid ? QString() : QStringLiteral("WHERE r.status='open'")));
    if (!query.exec()) { m_receivables.clear(); fail(query.lastError().text()); return; }
    m_receivables = records(query); emit changed();
    if (!query.exec("SELECT o.id, o.customer_id, c.name AS customer_name, o.service_id, p.name AS service_name, o.description, o.notes, o.status, o.amount_cents, datetime(o.created_at,'localtime') AS local_created_at FROM service_orders o JOIN customers c ON c.id=o.customer_id JOIN products p ON p.id=o.service_id ORDER BY o.id DESC")) { m_serviceOrders.clear(); fail(query.lastError().text()); return; }
    m_serviceOrders = records(query); emit changed();
}

bool Finance::createServiceOrder(int customerId, int serviceId, const QString &description, const QString &notes) {
    if (!Auth::allowed("finance")) return fail("Acesso negado. Apenas administradores podem criar ordens de serviço.");
    if (customerId <= 0 || serviceId <= 0 || description.trimmed().isEmpty() || description.trimmed().size() > 200 || notes.size() > 500)
        return fail("Informe cliente, serviço, descrição e observações válidos.");
    QSqlQuery query;
    query.prepare("SELECT id, sale_price_cents FROM products WHERE id=? AND active=1 AND product_type='service'"); query.addBindValue(serviceId);
    if (!query.exec() || !query.next()) return fail("Serviço inexistente ou inativo.");
    const auto amount = query.value(1).toLongLong();
    query.prepare("SELECT id FROM customers WHERE id=? AND active=1"); query.addBindValue(customerId);
    if (!query.exec() || !query.next()) return fail("Cliente inexistente ou inativo.");
    query.prepare("INSERT INTO service_orders(customer_id,service_id,description,notes,amount_cents,user_id) VALUES(?,?,?,?,?,?)");
    for (const auto &value : QVariantList{customerId, serviceId, description.trimmed(), notes.trimmed(), amount, Auth::userId()}) query.addBindValue(value);
    if (!query.exec()) return fail(query.lastError().text());
    refresh(); return true;
}

bool Finance::updateServiceOrder(int orderId, const QString &status) {
    if (!Auth::allowed("finance")) return fail("Acesso negado. Apenas administradores podem atualizar ordens de serviço.");
    if (!QStringList{"open","in_progress","completed","cancelled"}.contains(status)) return fail("Status de ordem de serviço inválido.");
    auto db = QSqlDatabase::database();
    if (!db.transaction()) { Diagnostics::record(Diagnostics::Level::Error,Diagnostics::Event::TransactionStartFailed,Diagnostics::Component::Finance); return fail(db.lastError().text()); }
    auto rollback = [&](const QString &message) { db.rollback(); Diagnostics::record(Diagnostics::Level::Warning,Diagnostics::Event::TransactionRollback,Diagnostics::Component::Finance); return fail(message); };
    QSqlQuery query(db);
    query.prepare("SELECT status FROM service_orders WHERE id=?"); query.addBindValue(orderId);
    if (!query.exec() || !query.next()) return rollback("Ordem de serviço não encontrada.");
    if (status == "completed") {
        query.prepare("SELECT m.product_id, m.quantity, p.name, p.stock_quantity FROM service_order_materials m JOIN products p ON p.id=m.product_id WHERE m.order_id=? AND m.consumed=0");
        query.addBindValue(orderId);
        if (!query.exec()) return rollback(query.lastError().text());
        while (query.next()) {
            const auto productId = query.value(0).toInt(); const auto quantity = query.value(1).toDouble();
            const auto previous = query.value(3).toDouble();
            if (previous + 0.0001 < quantity) return rollback(QStringLiteral("Estoque insuficiente para o material: %1.").arg(query.value(2).toString()));
            QSqlQuery update(db); update.prepare("UPDATE products SET stock_quantity=stock_quantity-? WHERE id=?"); update.addBindValue(quantity); update.addBindValue(productId);
            if (!update.exec()) return rollback(update.lastError().text());
            update.prepare("INSERT INTO inventory_movements(product_id,type,quantity,previous_balance,balance,reason,operator_name,user_id) VALUES(?,'exit',?,?,?,?,?,?)");
            for (const auto &value : QVariantList{productId, -quantity, previous, previous - quantity, QStringLiteral("Material da OS #%1").arg(orderId), Auth::operatorName(), Auth::userId()}) update.addBindValue(value);
            if (!update.exec()) return rollback(update.lastError().text());
        }
        query.prepare("UPDATE service_order_materials SET consumed=1 WHERE order_id=?"); query.addBindValue(orderId);
        if (!query.exec()) return rollback(query.lastError().text());
    }
    query.prepare("UPDATE service_orders SET status=?, updated_at=CURRENT_TIMESTAMP, user_id=? WHERE id=?");
    query.addBindValue(status); query.addBindValue(Auth::userId()); query.addBindValue(orderId);
    if (!query.exec() || query.numRowsAffected() != 1) return rollback("Não foi possível atualizar a ordem de serviço.");
    if (!db.commit()) return fail(db.lastError().text());
    refresh(); return true;
}

bool Finance::addServiceMaterial(int orderId, int productId, const QString &quantityInput) {
    if (!Auth::allowed("finance")) return fail("Acesso negado. Apenas administradores podem adicionar materiais.");
    auto value = quantityInput.trimmed(); value.replace(',', '.'); bool ok = false; const auto quantity = value.toDouble(&ok);
    if (!ok || !std::isfinite(quantity) || quantity <= 0 || quantity > 1000000 || std::abs(quantity * 1000 - std::round(quantity * 1000)) > 0.0001)
        return fail("Informe uma quantidade positiva com até três casas decimais.");
    QSqlQuery query;
    query.prepare("SELECT status FROM service_orders WHERE id=?"); query.addBindValue(orderId);
    if (!query.exec() || !query.next() || !QStringList{"open","in_progress"}.contains(query.value(0).toString())) return fail("A ordem não está aberta para adicionar materiais.");
    query.prepare("SELECT id FROM products WHERE id=? AND active=1 AND product_type='product'"); query.addBindValue(productId);
    if (!query.exec() || !query.next()) return fail("Material inexistente, inativo ou classificado como serviço.");
    query.prepare("INSERT INTO service_order_materials(order_id,product_id,quantity) VALUES(?,?,?) ON CONFLICT(order_id,product_id) DO UPDATE SET quantity=quantity+excluded.quantity, consumed=0");
    query.addBindValue(orderId); query.addBindValue(productId); query.addBindValue(quantity);
    if (!query.exec()) return fail(query.lastError().text());
    refresh(); return true;
}

bool Finance::createReceivable(const QString &description, const QString &amount, const QString &dueDate, int customerId) {
    if (!Auth::allowed("finance")) return fail("Acesso negado. Apenas administradores podem lançar recebíveis.");
    qint64 cents = 0;
    if (description.trimmed().isEmpty() || description.trimmed().size() > 200 || !money(amount, cents) || !QDate::fromString(dueDate.trimmed(), Qt::ISODate).isValid() || customerId < 0)
        return fail("Informe descrição, valor positivo, vencimento válido e cliente opcional.");
    QSqlQuery query;
    if (customerId > 0) {
        query.prepare("SELECT id FROM customers WHERE id=? AND active=1"); query.addBindValue(customerId);
        if (!query.exec() || !query.next()) return fail("Cliente inexistente ou inativo.");
    }
    query.prepare("INSERT INTO receivables(description,customer_id,amount_cents,due_date,operator_name,user_id) VALUES(?,?,?,?,?,?)");
    for (const auto &value : QVariantList{description.trimmed(), customerId > 0 ? QVariant(customerId) : QVariant(), cents, dueDate.trimmed(), Auth::operatorName(), Auth::userId()}) query.addBindValue(value);
    if (!query.exec()) return fail(query.lastError().text());
    refresh(); return true;
}

bool Finance::receiveReceivable(int receivableId, int cashSessionId) {
    if (!Auth::allowed("finance")) return fail("Acesso negado. Apenas administradores podem receber contas.");
    auto db = QSqlDatabase::database();
    if (!db.transaction()) { Diagnostics::record(Diagnostics::Level::Error,Diagnostics::Event::TransactionStartFailed,Diagnostics::Component::Finance); return fail(db.lastError().text()); }
    auto rollback = [&](const QString &message) { db.rollback(); Diagnostics::record(Diagnostics::Level::Warning,Diagnostics::Event::TransactionRollback,Diagnostics::Component::Finance); return fail(message); };
    QSqlQuery query(db);
    query.prepare("SELECT description, amount_cents, status FROM receivables WHERE id=?"); query.addBindValue(receivableId);
    if (!query.exec() || !query.next()) return rollback("Conta a receber não encontrada.");
    if (query.value(2).toString() != "open") return rollback("Esta conta já foi recebida ou cancelada.");
    const auto description = query.value(0).toString(); const auto cents = query.value(1).toLongLong(); query.finish();
    query.prepare("SELECT opening_cents + COALESCE((SELECT SUM(pi.amount_cents) FROM payment_items pi JOIN sales s ON s.id=pi.sale_id WHERE s.cash_session_id=c.id AND s.status='completed' AND pi.method='cash'),0) + COALESCE((SELECT SUM(CASE WHEN type='supply' THEN amount_cents ELSE -amount_cents END) FROM cash_movements WHERE cash_session_id=c.id),0) FROM cash_sessions c WHERE c.id=? AND c.status='open'");
    query.addBindValue(cashSessionId);
    if (!query.exec() || !query.next()) return rollback("Abra um caixa antes de receber a conta.");
    const auto previous = query.value(0).toLongLong(); query.finish();
    query.prepare("INSERT INTO cash_movements(cash_session_id,type,amount_cents,previous_cents,balance_cents,reason,operator_name,user_id) VALUES(?,?,?,?,?,?,?,?)");
    for (const auto &value : QVariantList{cashSessionId, QStringLiteral("supply"), cents, previous, previous + cents, QStringLiteral("Recebimento #%1: %2").arg(receivableId).arg(description), Auth::operatorName(), Auth::userId()}) query.addBindValue(value);
    if (!query.exec()) return rollback(query.lastError().text());
    query.prepare("UPDATE receivables SET status='received', received_at=CURRENT_TIMESTAMP, cash_session_id=?, operator_name=?, user_id=? WHERE id=? AND status='open'");
    for (const auto &value : QVariantList{cashSessionId, Auth::operatorName(), Auth::userId(), receivableId}) query.addBindValue(value);
    if (!query.exec() || query.numRowsAffected() != 1) return rollback("Não foi possível receber a conta.");
    if (!Audit::record("finance.receive", QStringLiteral("Conta #%1").arg(receivableId), QStringLiteral("Valor: %1; caixa: %2").arg(cents).arg(cashSessionId))) return rollback("Falha ao registrar a auditoria.");
    if (!db.commit()) return fail(db.lastError().text());
    refresh(); return true;
}
bool Finance::createExpense(const QString &description, const QString &amount, const QString &dueDate) {
    if (!Auth::allowed("finance")) return fail("Acesso negado. Apenas administradores podem lançar despesas.");
    qint64 cents = 0;
    if (description.trimmed().isEmpty() || description.trimmed().size() > 200 || !money(amount, cents) || !QDate::fromString(dueDate.trimmed(), Qt::ISODate).isValid())
        return fail("Informe descrição, valor positivo e vencimento no formato AAAA-MM-DD.");
    QSqlQuery query;
    query.prepare("INSERT INTO expenses(description,amount_cents,due_date,operator_name,user_id) VALUES(?,?,?,?,?)");
    for (const auto &value : QVariantList{description.trimmed(), cents, dueDate.trimmed(), Auth::operatorName(), Auth::userId()}) query.addBindValue(value);
    if (!query.exec()) return fail(query.lastError().text());
    refresh(); return true;
}
bool Finance::payExpense(int expenseId, int cashSessionId) {
    if (!Auth::allowed("finance")) return fail("Acesso negado. Apenas administradores podem baixar despesas.");
    auto db = QSqlDatabase::database();
    if (!db.transaction()) { Diagnostics::record(Diagnostics::Level::Error,Diagnostics::Event::TransactionStartFailed,Diagnostics::Component::Finance); return fail(db.lastError().text()); }
    auto rollback = [&](const QString &message) { db.rollback(); Diagnostics::record(Diagnostics::Level::Warning,Diagnostics::Event::TransactionRollback,Diagnostics::Component::Finance); return fail(message); };
    QSqlQuery query(db);
    query.prepare("SELECT description, amount_cents, status FROM expenses WHERE id=?"); query.addBindValue(expenseId);
    if (!query.exec() || !query.next()) return rollback("Despesa não encontrada.");
    if (query.value(2).toString() != "open") return rollback("Esta despesa já foi baixada ou cancelada.");
    const auto description = query.value(0).toString(); const auto cents = query.value(1).toLongLong(); query.finish();
    query.prepare("SELECT opening_cents + COALESCE((SELECT SUM(pi.amount_cents) FROM payment_items pi JOIN sales s ON s.id=pi.sale_id WHERE s.cash_session_id=c.id AND s.status='completed' AND pi.method='cash'),0) + COALESCE((SELECT SUM(CASE WHEN type='supply' THEN amount_cents ELSE -amount_cents END) FROM cash_movements WHERE cash_session_id=c.id),0) FROM cash_sessions c WHERE c.id=? AND c.status='open'");
    query.addBindValue(cashSessionId);
    if (!query.exec() || !query.next()) return rollback("Abra um caixa antes de baixar a despesa.");
    const auto previous = query.value(0).toLongLong(); query.finish();
    if (cents > previous) return rollback("Saldo insuficiente no caixa para esta despesa.");
    query.prepare("INSERT INTO cash_movements(cash_session_id,type,amount_cents,previous_cents,balance_cents,reason,operator_name,user_id) VALUES(?,?,?,?,?,?,?,?)");
    for (const auto &value : QVariantList{cashSessionId, QStringLiteral("withdrawal"), cents, previous, previous - cents, QStringLiteral("Despesa #%1: %2").arg(expenseId).arg(description), Auth::operatorName(), Auth::userId()}) query.addBindValue(value);
    if (!query.exec()) return rollback(query.lastError().text());
    query.prepare("UPDATE expenses SET status='paid', paid_at=CURRENT_TIMESTAMP, cash_session_id=?, operator_name=?, user_id=? WHERE id=? AND status='open'");
    for (const auto &value : QVariantList{cashSessionId, Auth::operatorName(), Auth::userId(), expenseId}) query.addBindValue(value);
    if (!query.exec() || query.numRowsAffected() != 1) return rollback("Não foi possível baixar a despesa.");
    if (!Audit::record("finance.pay", QStringLiteral("Despesa #%1").arg(expenseId), QStringLiteral("Valor: %1; caixa: %2").arg(cents).arg(cashSessionId))) return rollback("Falha ao registrar a auditoria.");
    if (!db.commit()) return fail(db.lastError().text());
    refresh(); return true;
}
}