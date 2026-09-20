#pragma once

#include <QObject>
#include <QVariantList>

namespace MHStore {
class Finance : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList expenses READ expenses NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit Finance(QObject *parent = nullptr);
    QVariantList expenses() const { return m_expenses; }
    QString error() const { return m_error; }
    Q_INVOKABLE void refresh(bool includePaid = true);
    Q_INVOKABLE bool createExpense(const QString &description, const QString &amount, const QString &dueDate);
    Q_INVOKABLE bool payExpense(int expenseId, int cashSessionId);
signals:
    void changed();
private:
    bool fail(const QString &message);
    QVariantList m_expenses;
    QString m_error;
};
}