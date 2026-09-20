#pragma once

#include <QObject>
#include <QVariantList>

namespace MHStore {
class Inventory : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList products READ products NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit Inventory(QObject *parent = nullptr);
    QVariantList products() const { return m_products; }
    QVariantList history() const { return m_history; }
    QString error() const { return m_error; }
    Q_INVOKABLE void refresh(const QString &search = {}, bool criticalOnly = false);
    Q_INVOKABLE void selectProduct(int productId);
    // adjustment means the counted final balance, not a signed delta.
    Q_INVOKABLE bool move(int productId, const QString &type, const QString &quantity,
                          const QString &reason, const QString &operatorName);
signals:
    void changed();
    void stockChanged();
private:
    bool fail(const QString &message);
    QVariantList m_products;
    QVariantList m_history;
    QString m_error;
    QString m_search;
    bool m_criticalOnly = false;
    int m_productId = 0;
};
}
