#pragma once
#include <QObject>
#include <QVariantList>
#include <functional>

namespace MHStore {
class Backup : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QVariantList files READ files NOTIFY changed)
public:
    explicit Backup(QObject *parent = nullptr);
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
    QVariantList m_files;
};
}
