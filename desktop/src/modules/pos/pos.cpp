#include "../../core/auth/auth.h"
#include "../../core/settings/settings.h"
#include "pos.h"
#include "../../core/audit/audit.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QRegularExpression>
#include <QLocale>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QStringConverter>
#include <QPainter>
#include <QPdfWriter>
#include <QPageSize>
#include <QTemporaryFile>

namespace MHStore {
namespace {
constexpr qint64 limit = 100000000000LL;
bool money(QString input, qint64 &cents)
{
    input = input.trimmed();
    static const QRegularExpression pattern(QStringLiteral("^[0-9]{1,10}([.,][0-9]{1,2})?$"));
    if (!pattern.match(input).hasMatch()) return false;
    input.replace(',', '.');
    const auto parts = input.split('.');
    cents = parts[0].toLongLong() * 100;
    if (parts.size() == 2) cents += parts[1].leftJustified(2, '0').toLongLong();
    return cents <= limit;
}
// Shared by the display, withdrawal validation and closing snapshot. Alias c is a cash session.
QString cashBalanceExpression()
{
    return QStringLiteral("c.opening_cents + COALESCE((SELECT SUM(pi.amount_cents) "
        "FROM payment_items pi JOIN sales s ON s.id=pi.sale_id WHERE s.cash_session_id=c.id "
        "AND s.status='completed' AND pi.method='cash'),0) + COALESCE((SELECT SUM("
        "CASE WHEN m.type='supply' THEN m.amount_cents ELSE -m.amount_cents END) "
        "FROM cash_movements m WHERE m.cash_session_id=c.id),0)");
}
QString currency(qint64 cents) { return QLocale("pt_BR").toCurrencyString(cents / 100.0); }
QString csvCell(const QVariant &value)
{
    auto text = value.toString();
    text.replace('"', "\"\"");
    return '"' + text + '"';
}
QVariantList records(QSqlQuery &query)
{
    QVariantList result;
    while (query.next()) {
        QVariantMap row;
        for (int i = 0; i < query.record().count(); ++i) row.insert(query.record().fieldName(i), query.value(i));
        result.append(row);
    }
    return result;
}
// A transaction acquires the SQLite writer lock before any balance/session checks.
class Transaction {
public:
    QSqlDatabase db = QSqlDatabase::database();
    bool active = false;
    bool begin() { QSqlQuery q(db); active = q.exec("BEGIN IMMEDIATE"); return active; }
    bool commit() { if (!db.commit()) return false; active = false; return true; }
    ~Transaction() { if (active) db.rollback(); }
};
}
Pos::Pos(QObject *parent) : QObject(parent) { refresh(); }
bool Pos::fail(const QString &message) { m_error = message; emit changed(); return false; }
qint64 Pos::subtotal() const
{
    qint64 result = 0;
    for (const auto &item : m_cart) result += item.toMap().value("total_cents").toLongLong();
    return result;
}
bool Pos::setAdjustments(const QString &discount,const QString &surcharge,const QString &reason) {
    if (!Auth::allowed("pos.adjust") || !Settings::enabled("pos")) return fail("Sem permissão para ajustar a venda.");
    qint64 decrease=0,increase=0;
    if (!money(discount,decrease) || !money(surcharge,increase)) return fail("Informe valores não negativos com até duas casas decimais.");
    if (m_cart.isEmpty() || decrease>=subtotal() || subtotal()-decrease+increase>limit)
        return fail("O desconto deve ser menor que o subtotal e o total deve respeitar o limite da venda.");
    const auto why=reason.trimmed();
    if ((decrease || increase) && (why.isEmpty() || why.size()>200)) return fail("Informe uma justificativa com até 200 caracteres.");
    m_discount=decrease; m_surcharge=increase;
    m_adjustmentReason=(decrease || increase)?why:QString();
    m_error.clear(); emit changed(); return true;
}
void Pos::refresh(const QString &search)
{
    if (!Auth::allowed("read")) { m_products.clear(); m_sessions.clear(); m_cash.clear(); m_cashMovements.clear(); emit changed(); return; }
    m_error.clear();
    m_search = search;
    QString term = search.trimmed();
    term.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_");
    QSqlQuery q;
    q.prepare("SELECT id, code, barcode, name, sale_price_cents, stock_quantity FROM products WHERE active = 1 AND "
              "(name LIKE :term ESCAPE '\\' OR code LIKE :term ESCAPE '\\' OR barcode LIKE :term ESCAPE '\\') ORDER BY name COLLATE NOCASE");
    q.bindValue(":term", "%" + term + "%");
    if (!q.exec()) { fail(q.lastError().text()); return; }
    m_products = records(q);
    if (!q.exec(QStringLiteral("SELECT c.*, datetime(c.opened_at, 'localtime') AS opened_local, "
                "datetime(c.closed_at, 'localtime') AS closed_local, %1 AS cash_expected, "
                "COALESCE((SELECT SUM(m.amount_cents) FROM cash_movements m WHERE m.cash_session_id=c.id AND m.type='supply'),0) AS supply_cents, "
                "COALESCE((SELECT SUM(m.amount_cents) FROM cash_movements m WHERE m.cash_session_id=c.id AND m.type='withdrawal'),0) AS withdrawal_cents, "
                "COALESCE((SELECT SUM(s.total_cents) FROM sales s WHERE s.cash_session_id = c.id AND s.status = 'completed'),0) AS sales_cents "
                "FROM cash_sessions c ORDER BY c.id DESC LIMIT 100").arg(cashBalanceExpression()))) {
        fail(q.lastError().text()); return;
    }
    m_sessions = records(q);
    m_cash.clear();
    for (const auto &session : m_sessions) if (session.toMap().value("status") == "open") m_cash = session.toMap();
    selectCashHistory(m_historySession);
}
void Pos::refreshCustomers()
{
    if (!Auth::allowed("read")) { m_customers.clear(); emit customersChanged(); emit changed(); return; }
    m_customers = {QVariantMap{{"id",0},{"label",QStringLiteral("Consumidor não identificado")}}};
    QSqlQuery q;
    if (!q.exec("SELECT id, name FROM customers WHERE active=1 ORDER BY name COLLATE NOCASE")) {
        fail(q.lastError().text()); emit customersChanged(); return;
    }
    while (q.next()) m_customers.append(QVariantMap{{"id",q.value(0)},
        {"label",QString("%1 (#%2)").arg(q.value(1).toString(),q.value(0).toString())}});
    emit customersChanged();
}

void Pos::refreshDashboard()
{
    if (!Auth::allowed("read")) { m_dashboard.clear(); m_dashboardError="Entre para consultar o painel."; emit dashboardChanged(); emit changed(); return; }
    m_dashboard.clear();
    m_dashboardError.clear();
    QSqlQuery q;
    // One statement provides a consistent SQLite snapshot for all indicators.
    const QString statement = QStringLiteral(
        "SELECT (SELECT COALESCE(SUM(total_cents),0) FROM sales WHERE status='completed' "
        "AND date(created_at,'localtime')=date('now','localtime')) AS today_cents, "
        "(SELECT COUNT(*) FROM sales WHERE status='completed' AND date(created_at,'localtime')=date('now','localtime')) AS today_count, "
        "(SELECT COALESCE(SUM(total_cents),0) FROM sales WHERE status='completed' "
        "AND strftime('%Y-%m',created_at,'localtime')=strftime('%Y-%m','now','localtime')) AS month_cents, "
        "(SELECT COUNT(*) FROM sales WHERE status='cancelled' "
        "AND strftime('%Y-%m',created_at,'localtime')=strftime('%Y-%m','now','localtime')) AS month_cancelled_count, "
        "(SELECT COALESCE(SUM(pi.amount_cents),0) FROM payment_items pi JOIN sales s ON s.id=pi.sale_id "
        "WHERE s.status='completed' AND pi.method='cash' AND date(s.created_at,'localtime')=date('now','localtime')) AS today_cash_cents, "
        "(SELECT COALESCE(SUM(pi.amount_cents),0) FROM payment_items pi JOIN sales s ON s.id=pi.sale_id "
        "WHERE s.status='completed' AND pi.method<>'cash' AND date(s.created_at,'localtime')=date('now','localtime')) AS today_other_payment_cents, "
        "(SELECT COUNT(*) FROM products WHERE active=1 AND stock_quantity<=minimum_stock) AS low_stock, "
        "(SELECT COUNT(*) FROM products WHERE active=1 AND stock_quantity<=0) AS no_stock, "
        "(SELECT COUNT(*) FROM cash_sessions WHERE status='open') AS cash_open, "
        "COALESCE((SELECT %1 FROM cash_sessions c WHERE status='open'),0) AS cash_expected, "
        "datetime('now','localtime') AS updated_at").arg(cashBalanceExpression());
    if (!q.exec(statement)) m_dashboardError = q.lastError().text();
    else {
        const auto rows = records(q);
        if (!rows.isEmpty()) m_dashboard = rows.first().toMap();
    }
    emit dashboardChanged();
}

void Pos::searchSales(const QString &number, int page, int customerId, const QString &fromDate, const QString &toDate)
{
    if (!Auth::allowed("read")) { m_sales.clear(); m_customerSummary.clear(); m_moreSales=false; m_salesError="Entre para consultar vendas."; emit salesChanged(); emit changed(); return; }
    m_sales.clear();
    m_customerSummary.clear();
    m_moreSales = false;
    m_salesError.clear();
    const auto input = number.trimmed();
    bool validNumber = false;
    const qint64 id = input.toLongLong(&validNumber);
    static const QRegularExpression digits(QStringLiteral("^[0-9]+$"));
    const auto startDate = fromDate.trimmed();
    const auto endDate = toDate.trimmed();
    const QRegularExpression isoDate(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    if (customerId < 0 || page < 0 || page > 1000000 ||
        (!input.isEmpty() && (!validNumber || id <= 0 || !digits.match(input).hasMatch())) ||
        (!startDate.isEmpty() && !isoDate.match(startDate).hasMatch()) ||
        (!endDate.isEmpty() && !isoDate.match(endDate).hasMatch()) ||
        (!startDate.isEmpty() && !endDate.isEmpty() && startDate > endDate)) {
        m_salesError = QStringLiteral("Informe um número de venda válido.");
        emit salesChanged();
        return;
    }
    QSqlQuery q;
    if (customerId > 0) {
        auto statement = QStringLiteral("SELECT c.id, c.name, c.active, COUNT(s.id) AS purchase_count, "
                  "COALESCE(SUM(s.total_cents),0) AS spent_cents, "
                  "datetime(MAX(s.created_at),'localtime') AS last_purchase "
                  "FROM customers c LEFT JOIN sales s ON s.customer_id=c.id AND s.status='completed' "
                  );
        if (!startDate.isEmpty()) statement += " AND date(s.created_at,'localtime')>=date(?)";
        if (!endDate.isEmpty()) statement += " AND date(s.created_at,'localtime')<=date(?)";
        statement += " WHERE c.id=? GROUP BY c.id";
        q.prepare(statement);
        if (!startDate.isEmpty()) q.addBindValue(startDate);
        if (!endDate.isEmpty()) q.addBindValue(endDate);
        q.addBindValue(customerId);
        if (!q.exec()) m_salesError = q.lastError().text();
        else {
            const auto rows = records(q);
            if (rows.isEmpty()) m_salesError = QStringLiteral("Cliente não encontrado.");
            else m_customerSummary = rows.first().toMap();
        }
        if (!m_salesError.isEmpty()) { emit salesChanged(); return; }
    }
    auto statement = QStringLiteral("SELECT s.id, s.cash_session_id, s.total_cents, s.status, s.operator_name, "
              "datetime(s.created_at,'localtime') AS local_created_at, p.method "
              "FROM sales s LEFT JOIN payments p ON p.sale_id=s.id "
              "WHERE 1=1");
    if (!input.isEmpty()) statement += " AND s.id=?";
    if (customerId > 0) statement += " AND s.customer_id=? AND s.status='completed'";
    if (!startDate.isEmpty()) statement += " AND date(s.created_at,'localtime')>=date(?)";
    if (!endDate.isEmpty()) statement += " AND date(s.created_at,'localtime')<=date(?)";
    statement += " ORDER BY s.id DESC LIMIT 51 OFFSET ?";
    q.prepare(statement);
    if (!input.isEmpty()) q.addBindValue(id);
    if (customerId > 0) q.addBindValue(customerId);
    if (!startDate.isEmpty()) q.addBindValue(startDate);
    if (!endDate.isEmpty()) q.addBindValue(endDate);
    q.addBindValue(page * 50);
    if (!q.exec()) { m_salesError = q.lastError().text(); m_customerSummary.clear(); }
    else {
        m_sales = records(q);
        m_moreSales = m_sales.size() > 50;
        if (m_moreSales) m_sales.removeLast();
    }
    emit salesChanged();
}

bool Pos::loadSale(int saleId)
{
    if (!Auth::allowed("read")) { m_selectedSale.clear(); m_saleItems.clear(); m_salesError="Acesso negado."; emit salesChanged(); return false; }
    m_selectedSale.clear();
    m_saleItems.clear();
    m_salesError.clear();
    QSqlQuery q;
    q.prepare("SELECT s.*, datetime(s.created_at,'localtime') AS local_created_at, p.method, "
              "p.amount_cents AS paid_cents, p.tendered_cents, p.change_cents, c.name AS customer_name "
              "FROM sales s LEFT JOIN payments p ON p.sale_id=s.id LEFT JOIN customers c ON c.id=s.customer_id WHERE s.id=?");
    q.addBindValue(saleId);
    if (!q.exec()) m_salesError = q.lastError().text();
    else {
        const auto rows = records(q);
        if (rows.isEmpty()) m_salesError = QStringLiteral("Venda não encontrada.");
        else {
            const auto sale = rows.first().toMap();
            q.prepare("SELECT product_code, product_name, quantity, unit_price_cents, total_cents "
                      "FROM sale_items WHERE sale_id=? ORDER BY id");
            q.addBindValue(saleId);
            if (!q.exec()) m_salesError = q.lastError().text();
            else {
                m_saleItems = records(q);
                m_selectedSale = sale;
            }
        }
    }
    emit salesChanged();
    return m_salesError.isEmpty();
}

bool Pos::add(int productId)
{
    if (!Auth::allowed("pos")) return fail("Acesso negado. Entre com um usuário autorizado.");
    if (!Settings::enabled("pos")) return fail("Módulo desabilitado nas configurações da empresa.");
    for (const auto &entry : m_cart) {
        const auto row = entry.toMap();
        if (row.value("id").toInt() == productId) return setQuantity(productId, row.value("quantity").toInt()+1);
    }
    QSqlQuery q;
    q.prepare("SELECT id, code, name, sale_price_cents, stock_quantity FROM products WHERE id = ? AND active = 1");
    q.addBindValue(productId);
    if (!q.exec()) return fail(q.lastError().text());
    if (!q.next()) return fail("Produto não encontrado ou inativo.");
    const qint64 price = q.value(3).toLongLong();
    if (price <= 0 || price > limit) return fail("Informe um preço de venda maior que zero no cadastro.");
    if (q.value(4).toDouble() < 1) return fail("Estoque insuficiente.");
    if (subtotal() + price > limit) return fail("Valor da venda acima do limite.");
    resetAdjustments();
    m_cart.append(QVariantMap{{"id",productId},{"code",q.value(1)},{"name",q.value(2)},
        {"unit_price_cents",price},{"quantity",1},{"total_cents",price}});
    m_error.clear(); emit changed(); return true;
}
bool Pos::setQuantity(int productId, int quantity)
{
    if (!Auth::allowed("pos")) return fail("Acesso negado. Entre com um usuário autorizado.");
    if (!Settings::enabled("pos")) return fail("Módulo desabilitado nas configurações da empresa.");
    if (quantity < 0 || quantity > 10000) return fail("Quantidade deve estar entre 0 e 10.000 unidades.");
    for (qsizetype i = 0; i < m_cart.size(); ++i) {
        auto row = m_cart[i].toMap();
        if (row.value("id").toInt() != productId) continue;
        if (quantity == 0) { resetAdjustments(); m_cart.removeAt(i); m_error.clear(); emit changed(); return true; }
        QSqlQuery q;
        q.prepare("SELECT stock_quantity FROM products WHERE id = ? AND active = 1");
        q.addBindValue(productId);
        if (!q.exec()) return fail(q.lastError().text());
        if (!q.next() || q.value(0).toDouble() < quantity) return fail("Produto inativo ou estoque insuficiente.");
        const auto line = row.value("unit_price_cents").toLongLong() * quantity;
        if (subtotal() - row.value("total_cents").toLongLong() + line > limit) return fail("Valor da venda acima do limite.");
        if (quantity!=row.value("quantity").toInt()) resetAdjustments();
        row["quantity"] = quantity; row["total_cents"] = line; m_cart[i] = row;
        m_error.clear(); emit changed(); return true;
    }
    return fail("Item não encontrado no carrinho.");
}
void Pos::clearCart() { resetAdjustments(); m_cart.clear(); m_error.clear(); emit changed(); }
bool Pos::openCash(const QString &amount, const QString & /*operatorName*/)
{
    const QString operatorName = Auth::operatorName();
    if (!Auth::allowed("cash")) return fail("Acesso negado. Entre com um usuário autorizado.");
    if (!Settings::enabled("cash")) return fail("Módulo desabilitado nas configurações da empresa.");
    qint64 cents;
    if (!money(amount,cents) || operatorName.trimmed().isEmpty()) return fail("Informe o responsável e um valor de abertura válido, com até duas casas decimais.");
    Transaction tx;
    if (!tx.begin()) return fail("Banco ocupado. Tente novamente.");
    QSqlQuery q;
    if (!q.exec("SELECT id FROM cash_sessions WHERE status = 'open'")) return fail(q.lastError().text());
    if (q.next()) return fail("Já existe um caixa aberto.");
    q.prepare("INSERT INTO cash_sessions(opening_balance, opening_cents, operator_name,user_id) VALUES(?,?,?,?)");
    q.addBindValue(cents / 100.0); q.addBindValue(cents); q.addBindValue(operatorName.trimmed()); q.addBindValue(Auth::userId());
    if (!q.exec()) return fail(q.lastError().text());
    if (!tx.commit()) return fail(tx.db.lastError().text());
    refresh(m_search); return true;
}
bool Pos::closeCash(int sessionId, const QString &counted, const QString & /*operatorName*/)
{
    const QString operatorName = Auth::operatorName();
    if (!Auth::allowed("cash")) return fail("Acesso negado. Entre com um usuário autorizado.");
    if (!Settings::enabled("cash")) return fail("Módulo desabilitado nas configurações da empresa.");
    qint64 cents;
    if (!money(counted,cents) || operatorName.trimmed().isEmpty()) return fail("Informe o responsável e o dinheiro contado, com até duas casas decimais.");
    Transaction tx;
    if (!tx.begin()) return fail("Banco ocupado. Tente novamente.");
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT %1 FROM cash_sessions c WHERE id=? AND status='open'").arg(cashBalanceExpression()));
    q.addBindValue(sessionId);
    if (!q.exec()) return fail(q.lastError().text());
    if (!q.next()) return fail("Este caixa já foi fechado ou não existe.");
    const auto expected = q.value(0).toLongLong(); q.finish();
    q.prepare("UPDATE cash_sessions SET status='closed', closed_at=CURRENT_TIMESTAMP, expected_cents=?, counted_cents=?, closing_balance=?, closed_by=?,closed_user_id=? WHERE id=?");
    q.addBindValue(expected); q.addBindValue(cents); q.addBindValue(cents/100.0); q.addBindValue(operatorName.trimmed()); q.addBindValue(Auth::userId()); q.addBindValue(sessionId);
    if (!q.exec()) return fail(q.lastError().text());
    if (!tx.commit()) return fail(tx.db.lastError().text());
    refresh(m_search); return true;
}
void Pos::selectCashHistory(int sessionId)
{
    if (!Auth::allowed("read")) { m_cashMovements.clear(); emit changed(); return; }
    m_historySession = sessionId;
    QSqlQuery q;
    q.prepare("SELECT *, datetime(created_at, 'localtime') AS local_created_at FROM cash_movements "
              "WHERE cash_session_id=? ORDER BY id DESC LIMIT 200");
    q.addBindValue(sessionId);
    if (!q.exec()) { m_cashMovements.clear(); fail(q.lastError().text()); return; }
    m_cashMovements = records(q);
    emit changed();
}

bool Pos::moveCash(int sessionId, const QString &type, const QString &amount,
                   const QString &reason, const QString & /*operatorName*/)
{
    const QString operatorName = Auth::operatorName();
    if (!Auth::allowed("cash")) return fail("Acesso negado. Entre com um usuário autorizado.");
    if (!Settings::enabled("cash")) return fail("Módulo desabilitado nas configurações da empresa.");
    if (type != "supply" && type != "withdrawal") return fail("Tipo de movimentação de caixa inválido.");
    qint64 cents;
    if (!money(amount, cents) || cents == 0)
        return fail("Informe um valor maior que zero, com até duas casas decimais.");
    if (reason.trimmed().isEmpty() || operatorName.trimmed().isEmpty())
        return fail("Informe o motivo e o responsável pela movimentação.");
    Transaction tx;
    if (!tx.begin()) return fail("Banco ocupado. Tente novamente.");
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT %1 FROM cash_sessions c WHERE id=? AND status='open'").arg(cashBalanceExpression()));
    q.addBindValue(sessionId);
    if (!q.exec()) return fail(q.lastError().text());
    if (!q.next()) return fail("Este caixa já foi fechado ou não existe.");
    const qint64 previous = q.value(0).toLongLong();
    q.finish();
    if (type == "withdrawal" && cents > previous) return fail("Dinheiro insuficiente no caixa para esta sangria.");
    const qint64 balance = previous + (type == "supply" ? cents : -cents);
    if (balance > limit) return fail("Saldo do caixa acima do limite permitido.");
    q.prepare("INSERT INTO cash_movements(cash_session_id,type,amount_cents,previous_cents,balance_cents,reason,operator_name,user_id) "
              "VALUES(?,?,?,?,?,?,?,?)");
    for (const auto &value : QVariantList{sessionId,type,cents,previous,balance,reason.trimmed(),operatorName.trimmed(),Auth::userId()})
        q.addBindValue(value);
    if (!q.exec()) return fail(q.lastError().text());
    if (!tx.commit()) return fail(tx.db.lastError().text());
    refresh(m_search);
    return true;
}

bool Pos::checkout(int sessionId, const QString &method, const QString &tendered, const QString & /*operatorName*/, int customerId)
{
    const QString operatorName = Auth::operatorName();
    if (!Auth::allowed("pos")) return fail("Acesso negado. Entre com um usuário autorizado.");
    if (!Settings::enabled("pos")) return fail("Módulo desabilitado nas configurações da empresa.");
    if (m_cart.isEmpty()) return fail("Adicione produtos ao carrinho.");
    if (operatorName.trimmed().isEmpty()) return fail("Informe o responsável pela venda.");
    if ((m_discount || m_surcharge) && !Auth::allowed("pos.adjust")) return fail("Sem permissão para finalizar uma venda com ajustes.");
    const qint64 amount = total();
    if (amount<=0 || amount>limit || m_discount>=subtotal()) return fail("Revise os ajustes da venda.");

    QStringList methods;
    QList<qint64> parts;
    const QString normalizedMethod = method.trimmed();
    if (normalizedMethod.contains('|')) {
        methods = normalizedMethod.split('|', Qt::SkipEmptyParts);
        const auto rawParts = tendered.split('|', Qt::SkipEmptyParts);
        if (methods.size() != rawParts.size()) return fail("Informe um valor para cada forma de pagamento em um pagamento dividido.");
        for (int i = 0; i < methods.size(); ++i) {
            const auto parsed = methods[i].trimmed();
            if (!QStringList{"cash","pix","credit","debit","other"}.contains(parsed)) return fail("Forma de pagamento inválida.");
            qint64 cents = 0;
            if (!money(rawParts[i], cents) || cents <= 0) return fail("Informe valores válidos para cada parcela do pagamento.");
            parts.append(cents);
        }
        qint64 sum = 0; for (const auto &part : parts) sum += part;
        if (sum != amount) return fail("A soma das parcelas deve bater com o total da venda.");
    } else {
        methods = { normalizedMethod };
        if (!QStringList{"cash","pix","credit","debit","other"}.contains(normalizedMethod)) return fail("Forma de pagamento inválida.");
        qint64 received = amount;
        if (normalizedMethod == "cash" && (!money(tendered,received) || received < amount)) return fail("Dinheiro recebido insuficiente ou inválido.");
        parts = { received };
    }

    Transaction tx;
    if (!tx.begin()) return fail("Banco ocupado. Tente novamente.");
    QSqlQuery q;
    q.prepare("SELECT id FROM cash_sessions WHERE id=? AND status='open'"); q.addBindValue(sessionId);
    if (!q.exec()) return fail(q.lastError().text());
    if (!q.next()) return fail("Abra um caixa antes de finalizar a venda.");
    q.finish();
    if (customerId < 0) return fail("Cliente inválido.");
    QString customerName;
    if (customerId > 0) {
        q.prepare("SELECT name FROM customers WHERE id=? AND active=1");
        q.addBindValue(customerId);
        if (!q.exec()) return fail(q.lastError().text());
        if (!q.next()) return fail("Cliente inexistente ou inativo. Selecione outro cliente ou consumidor não identificado.");
        customerName = q.value(0).toString();
        q.finish();
    }
    q.prepare("INSERT INTO sales(cash_session_id,total_amount,total_cents,operator_name,customer_id,user_id,subtotal_cents,discount_cents,surcharge_cents,adjustment_reason) VALUES(?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(sessionId); q.addBindValue(amount/100.0); q.addBindValue(amount); q.addBindValue(operatorName.trimmed());
    q.addBindValue(customerId > 0 ? QVariant(customerId) : QVariant());
    q.addBindValue(Auth::userId());
    q.addBindValue(subtotal()); q.addBindValue(m_discount); q.addBindValue(m_surcharge);
    q.addBindValue(m_adjustmentReason.isEmpty()?QStringLiteral(""):m_adjustmentReason);
    if (!q.exec()) return fail(q.lastError().text());
    const auto saleId = q.lastInsertId().toLongLong();
    QString receipt = QString("Venda #%1\nCliente: %2\n").arg(saleId).arg(customerId > 0 ? customerName : QStringLiteral("Consumidor não identificado"));
    for (const auto &entry : m_cart) {
        const auto row = entry.toMap();
        const int id = row.value("id").toInt(), quantity = row.value("quantity").toInt();
        q.prepare("SELECT stock_quantity, sale_price_cents, code, name FROM products WHERE id=? AND active=1"); q.addBindValue(id);
        if (!q.exec()) return fail(q.lastError().text());
        if (!q.next() || q.value(0).toDouble() < quantity) return fail("Produto inativo ou saldo insuficiente. Revise o carrinho.");
        if (q.value(1).toLongLong() != row.value("unit_price_cents").toLongLong()) return fail("O preço de um produto mudou. Remova e adicione o item novamente.");
        const double previous = q.value(0).toDouble();
        const auto code = q.value(2), name = q.value(3); q.finish();
        q.prepare("UPDATE products SET stock_quantity=stock_quantity-? WHERE id=?"); q.addBindValue(quantity); q.addBindValue(id);
        if (!q.exec()) return fail(q.lastError().text());
        q.prepare("INSERT INTO sale_items(sale_id,product_id,product_code,product_name,quantity,unit_price_cents,total_cents) VALUES(?,?,?,?,?,?,?)");
        for (const auto &value : QVariantList{saleId,id,code,name,quantity,row.value("unit_price_cents"),row.value("total_cents")}) q.addBindValue(value);
        if (!q.exec()) return fail(q.lastError().text());
        q.prepare("INSERT INTO inventory_movements(product_id,type,quantity,previous_balance,balance,reason,operator_name,user_id) VALUES(?,'exit',?,?,?,?,?,?)");
        for (const auto &value : QVariantList{id,-quantity,previous,previous-quantity,QString("Venda #%1").arg(saleId),operatorName.trimmed(),Auth::userId()}) q.addBindValue(value);
        if (!q.exec()) return fail(q.lastError().text());
        receipt += QString("%1 × %2: %3\n").arg(quantity).arg(name.toString(),currency(row.value("total_cents").toLongLong()));
    }
    q.prepare("INSERT INTO payments(sale_id,method,amount_cents,tendered_cents,change_cents) VALUES(?,?,?,?,?)");
    const auto paymentMethod = methods.size() > 1 ? QStringLiteral("split") : methods.first();
    const auto tenderedValue = methods.size() > 1 ? amount : parts.first();
    const auto change = methods.size() > 1 ? 0 : std::max<qint64>(0, tenderedValue - amount);
    for (const auto &value : QVariantList{saleId,paymentMethod,amount,tenderedValue,change}) q.addBindValue(value);
    if (!q.exec()) return fail(q.lastError().text());
    for (int i = 0; i < methods.size(); ++i) {
        q.prepare("INSERT INTO payment_items(sale_id,method,amount_cents,tendered_cents,change_cents) VALUES(?,?,?,?,?)");
        const auto methodName = methods[i];
        const auto itemAmount = methodName == "cash" && methods.size() == 1 ? amount : parts[i];
        const auto itemTendered = methodName == "cash" ? (methods.size() == 1 ? parts[i] : itemAmount) : itemAmount;
        const auto itemChange = methodName == "cash" && methods.size() == 1 ? std::max<qint64>(0, itemTendered - amount) : 0;
        for (const auto &value : QVariantList{saleId,methodName,itemAmount,itemTendered,itemChange}) q.addBindValue(value);
        if (!q.exec()) return fail(q.lastError().text());
    }
    if ((m_discount || m_surcharge) && !Audit::record("sale.adjust", "Venda #"+QString::number(saleId),
        QString("Subtotal: %1; desconto: %2; acréscimo: %3; total: %4; motivo: %5")
            .arg(currency(subtotal()),currency(m_discount),currency(m_surcharge),currency(amount),m_adjustmentReason)))
        return fail("Falha ao registrar a auditoria dos ajustes.");
    if (!tx.commit()) return fail(tx.db.lastError().text());
    if (m_discount || m_surcharge) receipt += QString("Subtotal: %1\nDesconto: %2\nAcréscimo: %3\nMotivo: %4\n")
        .arg(currency(subtotal()),currency(m_discount),currency(m_surcharge),m_adjustmentReason);
    const auto changeAmount = methods.size() > 1 ? 0LL : std::max<qint64>(0, parts.first() - amount);
    m_receipt = receipt + QString("Total: %1\nTroco: %2").arg(currency(amount),currency(changeAmount));
    resetAdjustments(); m_cart.clear(); refresh(m_search); return true;
}

bool Pos::cancelSale(int saleId, const QString &reason)
{
    if (!Auth::allowed("pos")) return fail("Acesso negado. Entre com um usuário autorizado.");
    if (!Settings::enabled("pos")) return fail("Módulo desabilitado nas configurações da empresa.");

    const auto cleanReason = reason.trimmed();
    if (cleanReason.isEmpty() || cleanReason.size() > 200) return fail("Informe uma justificativa com até 200 caracteres.");

    Transaction tx;
    if (!tx.begin()) return fail("Banco ocupado. Tente novamente.");

    QSqlQuery q;
    q.prepare("SELECT id, cash_session_id, status, total_cents, operator_name, user_id FROM sales WHERE id = ?");
    q.addBindValue(saleId);
    if (!q.exec()) return fail(q.lastError().text());
    if (!q.next()) return fail("Venda não encontrada.");
    if (q.value("status").toString() != "completed") return fail("A venda já não está mais em aberto para cancelamento.");

    const auto saleSessionId = q.value("cash_session_id").toInt();
    const auto saleOperator = q.value("operator_name").toString();
    const auto saleUserId = q.value("user_id").toInt();
    const auto saleTotalCents = q.value("total_cents").toLongLong();

    q.prepare("SELECT product_id, quantity FROM sale_items WHERE sale_id = ? ORDER BY id");
    q.addBindValue(saleId);
    if (!q.exec()) return fail(q.lastError().text());
    while (q.next()) {
        const auto productId = q.value(0).toInt();
        const auto quantity = q.value(1).toInt();
        q.prepare("SELECT stock_quantity FROM products WHERE id = ? AND active = 1");
        q.addBindValue(productId);
        if (!q.exec()) return fail(q.lastError().text());
        if (!q.next()) return fail("Produto da venda não encontrado ou inativo.");
        const auto currentStock = q.value(0).toDouble();

        q.prepare("UPDATE products SET stock_quantity = ? WHERE id = ?");
        q.addBindValue(currentStock + quantity);
        q.addBindValue(productId);
        if (!q.exec()) return fail(q.lastError().text());

        q.prepare("INSERT INTO inventory_movements(product_id,type,quantity,previous_balance,balance,reason,operator_name,user_id) VALUES(?,?,?,?,?,?,?,?)");
        for (const auto &value : QVariantList{productId, QStringLiteral("entry"), quantity, currentStock, currentStock + quantity,
                QString("Cancelamento da venda #%1").arg(saleId), saleOperator.isEmpty() ? QStringLiteral("Sistema") : saleOperator, saleUserId})
            q.addBindValue(value);
        if (!q.exec()) return fail(q.lastError().text());
    }

    q.prepare("UPDATE sales SET status = 'cancelled', cancel_reason = ? WHERE id = ?");
    q.addBindValue(cleanReason);
    q.addBindValue(saleId);
    if (!q.exec()) return fail(q.lastError().text());

    if (!Audit::record("sale.cancel", "Venda #" + QString::number(saleId),
        QString("Motivo: %1; total: %2").arg(cleanReason, currency(saleTotalCents))))
        return fail("Falha ao registrar a auditoria do cancelamento.");

    if (!tx.commit()) return fail(tx.db.lastError().text());
    refresh(m_search);
    return true;
}

bool Pos::exportSalesCsv(const QString &filePath, const QString &fromDate, const QString &toDate)
{
    if (!Auth::allowed("read")) return fail("Acesso negado. Entre com um usuário autorizado.");
    const auto path = filePath.trimmed();
    const auto startDate = fromDate.trimmed();
    const auto endDate = toDate.trimmed();
    const QRegularExpression isoDate(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    if (path.isEmpty() || !QFileInfo(path).isAbsolute()) return fail("Informe um caminho absoluto para o arquivo CSV.");
    if ((!startDate.isEmpty() && !isoDate.match(startDate).hasMatch()) ||
        (!endDate.isEmpty() && !isoDate.match(endDate).hasMatch()) ||
        (!startDate.isEmpty() && !endDate.isEmpty() && startDate > endDate))
        return fail("Informe um período válido no formato AAAA-MM-DD.");

    QSqlQuery query;
    query.prepare("SELECT s.id, datetime(s.created_at,'localtime'), s.status, s.total_cents, "
                  "s.operator_name, COALESCE(p.method,''), COALESCE(p.amount_cents,0) "
                  "FROM sales s LEFT JOIN payments p ON p.sale_id=s.id "
                  "WHERE (?='' OR date(s.created_at,'localtime')>=date(?)) "
                  "AND (?='' OR date(s.created_at,'localtime')<=date(?)) ORDER BY s.id");
    query.addBindValue(startDate); query.addBindValue(startDate);
    query.addBindValue(endDate); query.addBindValue(endDate);
    if (!query.exec()) return fail(query.lastError().text());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return fail(file.errorString());
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "Venda;Data;Status;Total (centavos);Operador;Pagamento;Pago (centavos)\n";
    while (query.next()) {
        for (int column = 0; column < query.record().count(); ++column) {
            if (column) stream << ';';
            stream << csvCell(query.value(column));
        }
        stream << '\n';
    }
    if (stream.status() != QTextStream::Ok || !file.commit()) return fail(file.errorString());
    m_error.clear(); emit changed();
    return true;
}

bool Pos::exportSalesPdf(const QString &filePath, const QString &fromDate, const QString &toDate)
{
    if (!Auth::allowed("read")) return fail("Acesso negado. Entre com um usuário autorizado.");
    const auto path = filePath.trimmed();
    const auto startDate = fromDate.trimmed();
    const auto endDate = toDate.trimmed();
    const QRegularExpression isoDate(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    if (path.isEmpty() || !QFileInfo(path).isAbsolute()) return fail("Informe um caminho absoluto para o arquivo PDF.");
    if ((!startDate.isEmpty() && !isoDate.match(startDate).hasMatch()) ||
        (!endDate.isEmpty() && !isoDate.match(endDate).hasMatch()) ||
        (!startDate.isEmpty() && !endDate.isEmpty() && startDate > endDate))
        return fail("Informe um período válido no formato AAAA-MM-DD.");

    QSqlQuery query;
    query.prepare("SELECT s.id, datetime(s.created_at,'localtime'), s.status, s.total_cents, "
                  "s.operator_name, COALESCE(p.method,''), COALESCE(p.amount_cents,0) "
                  "FROM sales s LEFT JOIN payments p ON p.sale_id=s.id "
                  "WHERE (?='' OR date(s.created_at,'localtime')>=date(?)) "
                  "AND (?='' OR date(s.created_at,'localtime')<=date(?)) ORDER BY s.id");
    query.addBindValue(startDate); query.addBindValue(startDate);
    query.addBindValue(endDate); query.addBindValue(endDate);
    if (!query.exec()) return fail(query.lastError().text());

    QTemporaryFile temporary(QFileInfo(path).absolutePath() + QStringLiteral("/.mhstore-pdf-XXXXXX"));
    if (!temporary.open()) return fail(temporary.errorString());
    const auto temporaryPath = temporary.fileName();
    temporary.close();
    QPdfWriter writer(temporaryPath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(96);
    QPainter painter(&writer);
    if (!painter.isActive()) {
        QFile::remove(temporaryPath);
        return fail("Não foi possível criar o PDF.");
    }
    const auto pageWidth = writer.width();
    const auto pageHeight = writer.height();
    const auto left = 80;
    const auto right = pageWidth - 80;
    const auto lineHeight = 34;
    int y = 90;
    painter.setFont(QFont(QStringLiteral("sans"), 16, QFont::Bold));
    painter.drawText(left, y, QStringLiteral("Relatório de vendas"));
    y += 35;
    painter.setFont(QFont(QStringLiteral("sans"), 9));
    painter.drawText(left, y, QStringLiteral("Período: %1 a %2").arg(startDate.isEmpty() ? QStringLiteral("início") : startDate,
        endDate.isEmpty() ? QStringLiteral("fim") : endDate));
    y += 40;
    painter.setFont(QFont(QStringLiteral("sans"), 8, QFont::Bold));
    painter.drawText(left, y, QStringLiteral("Venda     Data                 Status       Total       Operador        Pagamento"));
    y += lineHeight;
    painter.setFont(QFont(QStringLiteral("sans"), 8));
    while (query.next()) {
        if (y > pageHeight - 70) { writer.newPage(); y = 90; }
        const auto line = QStringLiteral("#%1     %2     %3     %4     %5     %6")
            .arg(query.value(0).toString(), query.value(1).toString(), query.value(2).toString(),
                 query.value(3).toString(), query.value(4).toString(), query.value(5).toString());
        painter.drawText(left, y, painter.fontMetrics().elidedText(line, Qt::ElideRight, right - left));
        y += lineHeight;
    }
    painter.end();
    if (QFile::exists(path) && !QFile::remove(path)) {
        QFile::remove(temporaryPath);
        return fail("Não foi possível substituir o PDF existente.");
    }
    if (!QFile::rename(temporaryPath, path)) {
        QFile::remove(temporaryPath);
        return fail("Não foi possível finalizar o arquivo PDF.");
    }
    m_error.clear(); emit changed();
    return true;
}
}
