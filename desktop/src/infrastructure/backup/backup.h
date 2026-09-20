#pragma once
#include <QObject>
#include <QVariantList>
#include <functional>
#include <QTimer>
#include <QThread>

namespace MHStore {
class Backup : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap automatic READ automatic NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString automaticMessage READ automaticMessage NOTIFY changed)
    Q_PROPERTY(QString folder READ folder NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QVariantList files READ files NOTIFY changed)
public:
    explicit Backup(QObject *parent = nullptr);
    ~Backup() override;
    QVariantMap automatic() const { return m_automatic; }
    bool busy() const { return m_worker!=nullptr; }
    QString automaticMessage() const { return m_automaticMessage; }
    Q_INVOKABLE bool configureAutomatic(bool enabled,const QString &folder,int minutes,int retention);
    void startScheduler();
    void runAutomatic(bool force=false);
    QString folder() const { return m_folder; }
    QString message() const { return m_message; }
    QVariantList files() const { return m_files; }
    std::function<bool()> hasPendingCart;
    Q_INVOKABLE void list(const QString &folder);
    Q_INVOKABLE bool create(const QString &folder);
    Q_INVOKABLE bool restore(const QString &file);
signals:
    void changed();
    void restored();
private:
    bool fail(const QString &message);
    bool snapshot(const QString &folder, const QString &prefix, QString &path);
    QString m_folder, m_message;
    QString m_databasePath,m_configPath,m_owner,m_automaticMessage;
    QVariantMap m_automatic;
    QTimer m_timer;
    QThread *m_worker=nullptr;
    bool m_restored=false, m_pendingAutomatic=false;
    QVariantList m_files;
};
}
