#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace MHStore {
class Pos : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList customers READ customers NOTIFY customersChanged)
    Q_PROPERTY(QVariantMap dashboard READ dashboard NOTIFY dashboardChanged)
    Q_PROPERTY(QString dashboardError READ dashboardError NOTIFY dashboardChanged)
    Q_PROPERTY(QVariantMap customerSummary READ customerSummary NOTIFY salesChanged)
    Q_PROPERTY(QVariantList sales READ sales NOTIFY salesChanged)
    Q_PROPERTY(QVariantMap selectedSale READ selectedSale NOTIFY salesChanged)
    Q_PROPERTY(QVariantList saleItems READ saleItems NOTIFY salesChanged)
    Q_PROPERTY(bool moreSales READ moreSales NOTIFY salesChanged)
    Q_PROPERTY(QString salesError READ salesError NOTIFY salesChanged)
    Q_PROPERTY(QVariantList products READ products NOTIFY changed)
    Q_PROPERTY(QVariantList cart READ cart NOTIFY changed)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY changed)
    Q_PROPERTY(QVariantList cashMovements READ cashMovements NOTIFY changed)
    Q_PROPERTY(QVariantMap cash READ cash NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString receipt READ receipt NOTIFY changed)
    Q_PROPERTY(qint64 subtotal READ subtotal NOTIFY changed)
    Q_PROPERTY(qint64 discount READ discount NOTIFY changed)
    Q_PROPERTY(qint64 surcharge READ surcharge NOTIFY changed)
    Q_PROPERTY(QString adjustmentReason READ adjustmentReason NOTIFY changed)
    Q_PROPERTY(qint64 total READ total NOTIFY changed)
public:
    explicit Pos(QObject *parent = nullptr);
    QVariantList customers() const { return m_customers; }
    Q_INVOKABLE void refreshCustomers();
    QVariantMap dashboard() const { return m_dashboard; }
    QString dashboardError() const { return m_dashboardError; }
    Q_INVOKABLE void refreshDashboard();
    QVariantMap customerSummary() const { return m_customerSummary; }
    QVariantList sales() const { return m_sales; }
    QVariantMap selectedSale() const { return m_selectedSale; }
    QVariantList saleItems() const { return m_saleItems; }
    bool moreSales() const { return m_moreSales; }
    QString salesError() const { return m_salesError; }
    Q_INVOKABLE void searchSales(const QString &number = {}, int page = 0, int customerId = 0,
                                 const QString &fromDate = {}, const QString &toDate = {});
    Q_INVOKABLE bool loadSale(int saleId);
    QVariantList products() const { return m_products; }
    QVariantList cart() const { return m_cart; }
    QVariantList sessions() const { return m_sessions; }
    QVariantList cashMovements() const { return m_cashMovements; }
    QVariantMap cash() const { return m_cash; }
    QString error() const { return m_error; }
    QString receipt() const { return m_receipt; }
    qint64 subtotal() const;
    qint64 total() const { return subtotal()-m_discount+m_surcharge; }
    qint64 discount() const { return m_discount; }
    qint64 surcharge() const { return m_surcharge; }
    QString adjustmentReason() const { return m_adjustmentReason; }
    Q_INVOKABLE bool setAdjustments(const QString &discount,const QString &surcharge,const QString &reason);
    Q_INVOKABLE void refresh(const QString &search = {});
    Q_INVOKABLE bool add(int productId);
    Q_INVOKABLE bool setQuantity(int productId, int quantity);
    Q_INVOKABLE void clearCart();
    Q_INVOKABLE bool openCash(const QString &amount, const QString &operatorName);
    Q_INVOKABLE bool closeCash(int sessionId, const QString &counted, const QString &operatorName);
    Q_INVOKABLE void selectCashHistory(int sessionId);
    Q_INVOKABLE bool moveCash(int sessionId, const QString &type, const QString &amount,
                              const QString &reason, const QString &operatorName);
    Q_INVOKABLE bool checkout(int sessionId, const QString &method, const QString &tendered, const QString &operatorName, int customerId = 0);
    Q_INVOKABLE bool cancelSale(int saleId, const QString &reason);
signals:
    void changed();
    void customersChanged();
    void salesChanged();
    void dashboardChanged();
private:
    bool fail(const QString &message);
    void resetAdjustments() { m_discount=0; m_surcharge=0; m_adjustmentReason.clear(); }
    qint64 m_discount=0, m_surcharge=0;
    QString m_adjustmentReason;
    QVariantMap m_dashboard;
    QString m_dashboardError;
    QVariantList m_sales, m_saleItems, m_customers;
    QVariantMap m_selectedSale, m_customerSummary;
    QString m_salesError;
    bool m_moreSales = false;
    QVariantList m_products, m_cart, m_sessions, m_cashMovements;
    int m_historySession = 0;
    QVariantMap m_cash;
    QString m_error, m_receipt, m_search;
};
}
