#pragma once
#include <QObject>
#include <QVariantList>
#include <functional>
namespace MHStore {
class Audit : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(bool moreAvailable READ moreAvailable NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    explicit Audit(QObject *parent=nullptr) : QObject(parent) {}
    QVariantList entries() const { return m_entries; }
    bool moreAvailable() const { return m_more; }
    QString message() const { return m_message; }
    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool loadMore();
    // Records one administrative action inside the caller's transaction; the
    // caller must treat a false return as failure of the whole operation.
    static bool record(const QString &action,const QString &target,const QString &details);
signals:
    void changed();
private:
    bool fetch(int offset);
    QVariantList m_entries;
    bool m_more=false;
    int m_offset=0;
    QString m_message;
};
}
