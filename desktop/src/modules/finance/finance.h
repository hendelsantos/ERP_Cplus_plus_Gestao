#pragma once

#include <QObject>
#include <QVariantList>

namespace MHStore {
class Finance : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList expenses READ expenses NOTIFY changed)
    Q_PROPERTY(QVariantList receivables READ receivables NOTIFY changed)
    Q_PROPERTY(QVariantList serviceOrders READ serviceOrders NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit Finance(QObject *parent = nullptr);
    QVariantList expenses() const { return m_expenses; }
    QVariantList receivables() const { return m_receivables; }
    QVariantList serviceOrders() const { return m_serviceOrders; }
    QString error() const { return m_error; }
    Q_INVOKABLE void refresh(bool includePaid = true);
    Q_INVOKABLE bool createExpense(const QString &description, const QString &amount, const QString &dueDate);
    Q_INVOKABLE bool payExpense(int expenseId, int cashSessionId);
    Q_INVOKABLE bool createReceivable(const QString &description, const QString &amount, const QString &dueDate, int customerId = 0);
    Q_INVOKABLE bool receiveReceivable(int receivableId, int cashSessionId);
    Q_INVOKABLE bool createServiceOrder(int customerId, int serviceId, const QString &description, const QString &notes);
    Q_INVOKABLE bool updateServiceOrder(int orderId, const QString &status);
    Q_INVOKABLE bool addServiceMaterial(int orderId, int productId, const QString &quantity);
signals:
    void changed();
private:
    bool fail(const QString &message);
    QVariantList m_expenses;
    QVariantList m_receivables;
    QVariantList m_serviceOrders;
    QString m_error;
};
}