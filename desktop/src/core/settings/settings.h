#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <functional>

namespace MHStore {
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(QVariantList modules READ modules CONSTANT)
    Q_PROPERTY(QStringList navigation READ navigation NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    explicit Settings(QObject *parent = nullptr);
    QVariantMap values() const { return m_values; }
    QString message() const { return m_message; }
    bool save(const QString &company, const QString &profile, bool inventory, bool cash, bool pos,
        const QString &document = QStringLiteral(""), const QString &phone = QStringLiteral(""), const QString &address = QStringLiteral(""));
    QVariantList modules() const;
    QStringList navigation() const;
    Q_INVOKABLE bool saveModules(const QString &company, const QString &profile, const QVariantMap &modules,
        const QString &document = QStringLiteral(""), const QString &phone = QStringLiteral(""), const QString &address = QStringLiteral(""));
    static bool enabled(const QString &module);
    std::function<bool()> hasPendingCart;
signals:
    void changed();
private:
    bool fail(const QString &message);
    QVariantMap m_values;
    QString m_message;
};
}
