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
    if (!Auth::allowed("read")) { m_expenses.clear(); emit changed(); return; }
    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT id, description, amount_cents, due_date, status, paid_at, cash_session_id, operator_name, datetime(created_at,'localtime') AS local_created_at FROM expenses %1 ORDER BY CASE status WHEN 'open' THEN 0 ELSE 1 END, due_date, id DESC")
        .arg(includePaid ? QString() : QStringLiteral("WHERE status='open'")));
    if (!query.exec()) { m_expenses.clear(); fail(query.lastError().text()); return; }
    m_expenses = records(query); m_error.clear(); emit changed();
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
    if (!db.transaction()) return fail(db.lastError().text());
    auto rollback = [&](const QString &message) { db.rollback(); return fail(message); };
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