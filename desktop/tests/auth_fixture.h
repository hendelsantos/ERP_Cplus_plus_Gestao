#pragma once
#include "core/auth/auth.h"
#include <QSqlQuery>
inline bool authenticateTestAdmin() {
    MHStore::Auth::resetSession();
    MHStore::Auth auth;
    if (auth.needsSetup() && !auth.setup("Ana","admin","SenhaTeste123!")) return false;
    return auth.login("admin","SenhaTeste123!");
}
inline bool removeAdjustmentMigration() {
    QSqlQuery q;
    for (const auto &column : {"subtotal_cents","discount_cents","surcharge_cents","adjustment_reason"})
        if (!q.exec(QString("ALTER TABLE sales DROP COLUMN %1").arg(column))) return false;
    if (!q.exec("ALTER TABLE sales DROP COLUMN cancel_reason")) return false;
    if (!q.exec("DROP TABLE payment_items")) return false;
    if (!q.exec("CREATE TABLE payments_old (id INTEGER PRIMARY KEY, sale_id INTEGER NOT NULL UNIQUE REFERENCES sales(id), method TEXT NOT NULL CHECK(method IN ('cash','pix','credit','debit','other')), amount_cents INTEGER NOT NULL CHECK(amount_cents > 0), tendered_cents INTEGER NOT NULL, change_cents INTEGER NOT NULL CHECK(change_cents >= 0))")) return false;
    if (!q.exec("INSERT INTO payments_old SELECT id,sale_id,method,amount_cents,tendered_cents,change_cents FROM payments WHERE method <> 'split'")) return false;
    if (!q.exec("DROP TABLE payments")) return false;
    if (!q.exec("ALTER TABLE payments_old RENAME TO payments")) return false;
    if (!q.exec("DROP TABLE IF EXISTS expenses")) return false;
    if (!q.exec("DROP TABLE IF EXISTS receivables")) return false;
    return q.exec("DELETE FROM schema_migrations WHERE version IN (10,11,12,13,14,15)");
}
inline bool removeComplementaryMigration() {
    if (!removeAdjustmentMigration()) return false;
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
