#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace MHStore {

class Catalog : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY changed)
    Q_PROPERTY(QVariantList suppliers READ suppliers NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit Catalog(QObject *parent = nullptr);
    QVariantList rows() const { return m_rows; }
    QVariantList categories() const { return m_categories; }
    QVariantList suppliers() const { return m_suppliers; }
    QString error() const { return m_error; }
    Q_INVOKABLE void search(const QString &section, const QString &text = {}, bool includeInactive = false);
    Q_INVOKABLE bool save(const QString &section, int id, const QVariantMap &values);
    Q_INVOKABLE bool setActive(const QString &section, int id, bool active);
signals:
    void changed();
private:
    bool fail(const QString &message);
    void refresh();
    QString m_section = QStringLiteral("Produtos");
    QString m_text;
    bool m_includeInactive = false;
    QString m_error;
    QVariantList m_rows;
    QVariantList m_categories;
    QVariantList m_suppliers;
};
}
