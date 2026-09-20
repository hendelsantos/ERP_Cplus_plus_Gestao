#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <functional>
namespace MHStore {
class Auth : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool needsSetup READ needsSetup NOTIFY changed)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY changed)
    Q_PROPERTY(QVariantMap user READ user NOTIFY changed)
    Q_PROPERTY(QVariantList users READ users NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString recoveryCode READ recoveryCode NOTIFY changed)
public:
    explicit Auth(QObject *parent=nullptr) : QObject(parent) {}
    bool needsSetup() const;
    bool authenticated() const { return !current().isEmpty(); }
    QVariantMap user() const { return current(); }
    QVariantList users() const;
    QString message() const { return m_message; }
    QString recoveryCode() const { return m_recovery; }
    Q_INVOKABLE bool setup(const QString &name,const QString &login,const QString &password);
    Q_INVOKABLE bool login(const QString &login,const QString &password);
    Q_INVOKABLE bool logout();
    Q_INVOKABLE bool changePassword(const QString &currentPassword,const QString &newPassword,const QString &confirmation);
    Q_INVOKABLE bool saveUser(int id,const QString &name,const QString &login,const QString &password,const QString &role,bool active);
    Q_INVOKABLE bool recover(const QString &login,const QString &code,const QString &password);
    Q_INVOKABLE bool issueRecoveryCode(int id);
    Q_INVOKABLE void dismissRecovery() { m_recovery.clear(); emit changed(); }
    Q_INVOKABLE bool can(const QString &permission) const { return allowed(permission); }
    static bool allowed(const QString &permission);
    static QVariantMap current();
    static int userId() { return current().value("id").toInt(); }
    static QString operatorName() { return current().value("name").toString(); }
    static void resetSession();
    std::function<bool()> hasPendingCart;
signals:
    void changed();
private:
    bool fail(const QString &message);
    QString m_message, m_recovery;
};
}
