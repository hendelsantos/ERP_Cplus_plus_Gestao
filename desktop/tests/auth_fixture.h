#pragma once
#include "core/auth/auth.h"
#include <QSqlQuery>
inline bool authenticateTestAdmin() {
    MHStore::Auth::resetSession();
    MHStore::Auth auth;
    if (auth.needsSetup() && !auth.setup("Ana","admin","SenhaTeste123!")) return false;
    return auth.login("admin","SenhaTeste123!");
}
inline bool removeComplementaryMigration() {
    QSqlQuery q;
    for (const auto &column : {"brand","unit","maximum_stock","location","notes"})
        if (!q.exec(QString("ALTER TABLE products DROP COLUMN %1").arg(column))) return false;
    for (const auto &column : {"address","birth_date","notes"})
        if (!q.exec(QString("ALTER TABLE customers DROP COLUMN %1").arg(column))) return false;
    for (const auto &column : {"document","phone","address"})
        if (!q.exec(QString("ALTER TABLE business_settings DROP COLUMN %1").arg(column))) return false;
    return q.exec("DELETE FROM schema_migrations WHERE version=9");
}
inline bool removeAuthMigration() {
    MHStore::Auth::resetSession();
    if (!removeComplementaryMigration()) return false;
    QSqlQuery q;
    if (!q.exec("ALTER TABLE products DROP COLUMN supplier_id")) return false;
    for (const auto &table : {"sales","inventory_movements","cash_movements","cash_sessions"})
        if (!q.exec(QString("ALTER TABLE %1 DROP COLUMN user_id").arg(table))) return false;
    return q.exec("ALTER TABLE cash_sessions DROP COLUMN closed_user_id")
        && q.exec("DROP TABLE IF EXISTS audit_log")
        && q.exec("DROP TABLE IF EXISTS suppliers")
        && q.exec("DROP TABLE users")
        && q.exec("DELETE FROM schema_migrations WHERE version IN (6,7,8)");
}
