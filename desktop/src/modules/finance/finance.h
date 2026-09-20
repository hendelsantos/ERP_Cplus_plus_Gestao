#pragma once

#include <QObject>
#include <QVariantList>

namespace MHStore {
class Finance : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList expenses READ expenses NOTIFY changed)
    Q_PROPERTY(QVariantList receivables READ receivables NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit Finance(QObject *parent = nullptr);
    QVariantList expenses() const { return m_expenses; }
    QVariantList receivables() const { return m_receivables; }
    QString error() const { return m_error; }
    Q_INVOKABLE void refresh(bool includePaid = true);
    Q_INVOKABLE bool createExpense(const QString &description, const QString &amount, const QString &dueDate);
    Q_INVOKABLE bool payExpense(int expenseId, int cashSessionId);
    Q_INVOKABLE bool createReceivable(const QString &description, const QString &amount, const QString &dueDate, int customerId = 0);
    Q_INVOKABLE bool receiveReceivable(int receivableId, int cashSessionId);
signals:
    void changed();
private:
    bool fail(const QString &message);
    QVariantList m_expenses;
    QVariantList m_receivables;
    QString m_error;
};
}