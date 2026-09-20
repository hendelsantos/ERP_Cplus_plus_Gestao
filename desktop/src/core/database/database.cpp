#include "database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace MHStore::Database {

bool DatabaseManager::initialize(QString *errorMessage, const QString &databasePath, const QString &connectionName)
{
    const auto dataDirectory = databasePath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : QFileInfo(databasePath).absolutePath();
    if (!QDir().mkpath(dataDirectory)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Não foi possível criar o diretório de dados: %1").arg(dataDirectory);
        }
        return false;
    }

    auto database = QSqlDatabase::contains(connectionName) ? QSqlDatabase::database(connectionName)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),connectionName);
    database.setDatabaseName(databasePath.isEmpty()
        ? dataDirectory + QStringLiteral("/mhstore.sqlite") : databasePath);
    if (!database.open()) {
        if (errorMessage) {
            *errorMessage = database.lastError().text();
        }
        return false;
    }

    return applyMigrations(errorMessage,database);
}

bool DatabaseManager::applyMigrations(QString *errorMessage, QSqlDatabase database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON")) ||
        !query.exec(QStringLiteral("PRAGMA busy_timeout = 5000"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    if (!database.transaction()) {
        if (errorMessage) *errorMessage = database.lastError().text();
        return false;
    }
    auto fail = [&](const QString &message) {
        if (errorMessage) *errorMessage = message;
        database.rollback();
        return false;
    };
    if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS schema_migrations (version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)")))
        return fail(query.lastError().text());
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_migrations")) || !query.next())
        return fail(query.lastError().text());
    const int version = query.value(0).toInt();
    query.finish();
    if (version > 20) return fail(QStringLiteral("Banco criado por uma versão mais recente do MH Store."));
    const QList<QStringList> migrations = {{
        QStringLiteral("CREATE TABLE IF NOT EXISTS schema_migrations (version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS categories (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, active INTEGER NOT NULL DEFAULT 1)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS products (id INTEGER PRIMARY KEY AUTOINCREMENT, code TEXT NOT NULL UNIQUE, barcode TEXT, name TEXT NOT NULL, category_id INTEGER REFERENCES categories(id), cost_price REAL NOT NULL DEFAULT 0, sale_price REAL NOT NULL DEFAULT 0, stock_quantity REAL NOT NULL DEFAULT 0, minimum_stock REAL NOT NULL DEFAULT 0, active INTEGER NOT NULL DEFAULT 1, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS customers (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, document TEXT, phone TEXT, email TEXT, active INTEGER NOT NULL DEFAULT 1, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS cash_sessions (id INTEGER PRIMARY KEY AUTOINCREMENT, opened_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, opening_balance REAL NOT NULL DEFAULT 0, closed_at TEXT, closing_balance REAL, status TEXT NOT NULL DEFAULT 'open')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS sales (id INTEGER PRIMARY KEY AUTOINCREMENT, customer_id INTEGER REFERENCES customers(id), cash_session_id INTEGER REFERENCES cash_sessions(id), total_amount REAL NOT NULL DEFAULT 0, status TEXT NOT NULL DEFAULT 'completed', created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (1)")
    }, {
        QStringLiteral("CREATE TABLE inventory_movements (id INTEGER PRIMARY KEY AUTOINCREMENT, product_id INTEGER NOT NULL REFERENCES products(id), type TEXT NOT NULL CHECK(type IN ('entry','exit','adjustment')), quantity REAL NOT NULL CHECK(quantity != 0), previous_balance REAL NOT NULL CHECK(previous_balance >= 0), balance REAL NOT NULL CHECK(balance >= 0), reason TEXT NOT NULL CHECK(length(trim(reason)) > 0), operator_name TEXT NOT NULL CHECK(length(trim(operator_name)) > 0), created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE INDEX inventory_movements_product ON inventory_movements(product_id, id DESC)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (2)")
    }, {
        QStringLiteral("ALTER TABLE products ADD COLUMN sale_price_cents INTEGER NOT NULL DEFAULT 0 CHECK(sale_price_cents >= 0)"),
        QStringLiteral("ALTER TABLE products ADD COLUMN cost_price_cents INTEGER NOT NULL DEFAULT 0 CHECK(cost_price_cents >= 0)"),
        QStringLiteral("UPDATE products SET sale_price_cents = CAST(ROUND(sale_price * 100) AS INTEGER), cost_price_cents = CAST(ROUND(cost_price * 100) AS INTEGER)"),
        QStringLiteral("ALTER TABLE cash_sessions ADD COLUMN opening_cents INTEGER NOT NULL DEFAULT 0"),
        QStringLiteral("ALTER TABLE cash_sessions ADD COLUMN expected_cents INTEGER"),
        QStringLiteral("ALTER TABLE cash_sessions ADD COLUMN counted_cents INTEGER"),
        QStringLiteral("ALTER TABLE cash_sessions ADD COLUMN operator_name TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE cash_sessions ADD COLUMN closed_by TEXT"),
        QStringLiteral("UPDATE cash_sessions SET opening_cents = CAST(ROUND(opening_balance * 100) AS INTEGER), counted_cents = CAST(ROUND(closing_balance * 100) AS INTEGER)"),
        QStringLiteral("CREATE UNIQUE INDEX one_open_cash_session ON cash_sessions(status) WHERE status = 'open'"),
        QStringLiteral("ALTER TABLE sales ADD COLUMN total_cents INTEGER NOT NULL DEFAULT 0"),
        QStringLiteral("ALTER TABLE sales ADD COLUMN operator_name TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("UPDATE sales SET total_cents = CAST(ROUND(total_amount * 100) AS INTEGER)"),
        QStringLiteral("CREATE TABLE sale_items (id INTEGER PRIMARY KEY, sale_id INTEGER NOT NULL REFERENCES sales(id), product_id INTEGER NOT NULL REFERENCES products(id), product_code TEXT NOT NULL, product_name TEXT NOT NULL, quantity INTEGER NOT NULL CHECK(quantity > 0), unit_price_cents INTEGER NOT NULL CHECK(unit_price_cents >= 0), total_cents INTEGER NOT NULL CHECK(total_cents >= 0))"),
        QStringLiteral("CREATE TABLE payments (id INTEGER PRIMARY KEY, sale_id INTEGER NOT NULL UNIQUE REFERENCES sales(id), method TEXT NOT NULL CHECK(method IN ('cash','pix','credit','debit','other')), amount_cents INTEGER NOT NULL CHECK(amount_cents > 0), tendered_cents INTEGER NOT NULL, change_cents INTEGER NOT NULL CHECK(change_cents >= 0))"),
        QStringLiteral("CREATE INDEX sale_items_sale ON sale_items(sale_id)"),
        QStringLiteral("CREATE INDEX sales_cash_session ON sales(cash_session_id)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (3)")
    }, {
        QStringLiteral("CREATE TABLE cash_movements (id INTEGER PRIMARY KEY, cash_session_id INTEGER NOT NULL REFERENCES cash_sessions(id), type TEXT NOT NULL CHECK(type IN ('supply','withdrawal')), amount_cents INTEGER NOT NULL CHECK(amount_cents > 0), previous_cents INTEGER NOT NULL CHECK(previous_cents >= 0), balance_cents INTEGER NOT NULL CHECK(balance_cents >= 0), reason TEXT NOT NULL CHECK(length(trim(reason)) > 0), operator_name TEXT NOT NULL CHECK(length(trim(operator_name)) > 0), created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, CHECK(balance_cents = previous_cents + CASE WHEN type = 'supply' THEN amount_cents ELSE -amount_cents END))"),
        QStringLiteral("CREATE INDEX cash_movements_session ON cash_movements(cash_session_id, id DESC)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (4)")
    }, {
        QStringLiteral("CREATE TABLE business_settings (id INTEGER PRIMARY KEY CHECK(id=1), company TEXT NOT NULL, profile TEXT NOT NULL CHECK(profile IN ('general','fashion','market','services')), inventory INTEGER NOT NULL CHECK(inventory IN (0,1)), cash INTEGER NOT NULL CHECK(cash IN (0,1)), pos INTEGER NOT NULL CHECK(pos IN (0,1)), CHECK(pos=0 OR (inventory=1 AND cash=1)))"),
        QStringLiteral("INSERT INTO business_settings VALUES(1,'Minha empresa','general',1,1,1)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (5)")
    }, {
        QStringLiteral("CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, login TEXT NOT NULL UNIQUE COLLATE NOCASE, password_hash BLOB NOT NULL, salt BLOB NOT NULL, iterations INTEGER NOT NULL, role TEXT NOT NULL CHECK(role IN ('admin','operator')), active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)), session_version INTEGER NOT NULL DEFAULT 1, failed_attempts INTEGER NOT NULL DEFAULT 0, locked_until INTEGER NOT NULL DEFAULT 0, recovery_hash BLOB)"),
        QStringLiteral("ALTER TABLE sales ADD COLUMN user_id INTEGER REFERENCES users(id)"),
        QStringLiteral("ALTER TABLE inventory_movements ADD COLUMN user_id INTEGER REFERENCES users(id)"),
        QStringLiteral("ALTER TABLE cash_movements ADD COLUMN user_id INTEGER REFERENCES users(id)"),
        QStringLiteral("ALTER TABLE cash_sessions ADD COLUMN user_id INTEGER REFERENCES users(id)"),
        QStringLiteral("ALTER TABLE cash_sessions ADD COLUMN closed_user_id INTEGER REFERENCES users(id)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (6)")
    }, {
        QStringLiteral("CREATE TABLE audit_log (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL REFERENCES users(id), user_name TEXT NOT NULL CHECK(length(user_name) > 0), action TEXT NOT NULL CHECK(length(action) > 0), target TEXT NOT NULL CHECK(length(target) > 0), details TEXT NOT NULL CHECK(length(details) > 0), created_at TEXT NOT NULL CHECK(length(created_at) > 0))"),
        QStringLiteral("CREATE INDEX audit_log_recent ON audit_log(id DESC)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (7)")
    }, {
        QStringLiteral("CREATE TABLE suppliers (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL CHECK(length(trim(name)) > 0), document TEXT UNIQUE, phone TEXT, email TEXT, active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)), created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("ALTER TABLE products ADD COLUMN supplier_id INTEGER REFERENCES suppliers(id)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (8)")
    }, {
        QStringLiteral("ALTER TABLE business_settings ADD COLUMN document TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE business_settings ADD COLUMN phone TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE business_settings ADD COLUMN address TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE customers ADD COLUMN address TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE customers ADD COLUMN birth_date TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE customers ADD COLUMN notes TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE products ADD COLUMN brand TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE products ADD COLUMN unit TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE products ADD COLUMN maximum_stock REAL NOT NULL DEFAULT 0"),
        QStringLiteral("ALTER TABLE products ADD COLUMN location TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE products ADD COLUMN notes TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (9)")
    }, {
        QStringLiteral("ALTER TABLE sales ADD COLUMN subtotal_cents INTEGER NOT NULL DEFAULT 0"),
        QStringLiteral("ALTER TABLE sales ADD COLUMN discount_cents INTEGER NOT NULL DEFAULT 0 CHECK(discount_cents>=0)"),
        QStringLiteral("ALTER TABLE sales ADD COLUMN surcharge_cents INTEGER NOT NULL DEFAULT 0 CHECK(surcharge_cents>=0)"),
        QStringLiteral("ALTER TABLE sales ADD COLUMN adjustment_reason TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("UPDATE sales SET subtotal_cents=total_cents"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (10)")
    }, {
        QStringLiteral("CREATE TABLE payment_items (id INTEGER PRIMARY KEY AUTOINCREMENT, sale_id INTEGER NOT NULL REFERENCES sales(id), method TEXT NOT NULL CHECK(method IN ('cash','pix','credit','debit','other')), amount_cents INTEGER NOT NULL CHECK(amount_cents > 0), tendered_cents INTEGER NOT NULL DEFAULT 0 CHECK(tendered_cents >= 0), change_cents INTEGER NOT NULL DEFAULT 0 CHECK(change_cents >= 0), created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE INDEX payment_items_sale ON payment_items(sale_id, id DESC)"),
        QStringLiteral("CREATE TABLE payments_new (id INTEGER PRIMARY KEY, sale_id INTEGER NOT NULL UNIQUE REFERENCES sales(id), method TEXT NOT NULL CHECK(method IN ('cash','pix','credit','debit','other','split')), amount_cents INTEGER NOT NULL CHECK(amount_cents > 0), tendered_cents INTEGER NOT NULL, change_cents INTEGER NOT NULL CHECK(change_cents >= 0))"),
        QStringLiteral("INSERT INTO payments_new(id, sale_id, method, amount_cents, tendered_cents, change_cents) SELECT id, sale_id, method, amount_cents, tendered_cents, change_cents FROM payments"),
        QStringLiteral("DROP TABLE payments"),
        QStringLiteral("ALTER TABLE payments_new RENAME TO payments"),
        QStringLiteral("INSERT INTO payment_items(sale_id, method, amount_cents, tendered_cents, change_cents) SELECT sale_id, method, amount_cents, tendered_cents, change_cents FROM payments"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (11)")
    }, {
        QStringLiteral("ALTER TABLE sales ADD COLUMN cancel_reason TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (12)")
    }, {
        QStringLiteral("CREATE TABLE expenses (id INTEGER PRIMARY KEY AUTOINCREMENT, description TEXT NOT NULL CHECK(length(trim(description)) > 0), amount_cents INTEGER NOT NULL CHECK(amount_cents > 0), due_date TEXT NOT NULL, status TEXT NOT NULL DEFAULT 'open' CHECK(status IN ('open','paid','cancelled')), paid_at TEXT, cash_session_id INTEGER REFERENCES cash_sessions(id), operator_name TEXT NOT NULL DEFAULT '', user_id INTEGER REFERENCES users(id), created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE INDEX expenses_status_due ON expenses(status, due_date, id DESC)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (13)")
    }, {
        QStringLiteral("CREATE TABLE receivables (id INTEGER PRIMARY KEY AUTOINCREMENT, description TEXT NOT NULL CHECK(length(trim(description)) > 0), customer_id INTEGER REFERENCES customers(id), amount_cents INTEGER NOT NULL CHECK(amount_cents > 0), due_date TEXT NOT NULL, status TEXT NOT NULL DEFAULT 'open' CHECK(status IN ('open','received','cancelled')), received_at TEXT, cash_session_id INTEGER REFERENCES cash_sessions(id), operator_name TEXT NOT NULL DEFAULT '', user_id INTEGER REFERENCES users(id), created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE INDEX receivables_status_due ON receivables(status, due_date, id DESC)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (14)")
    }, {
        QStringLiteral("CREATE TABLE sale_items_new (id INTEGER PRIMARY KEY, sale_id INTEGER NOT NULL REFERENCES sales(id), product_id INTEGER NOT NULL REFERENCES products(id), product_code TEXT NOT NULL, product_name TEXT NOT NULL, quantity REAL NOT NULL CHECK(quantity > 0), unit_price_cents INTEGER NOT NULL CHECK(unit_price_cents >= 0), total_cents INTEGER NOT NULL CHECK(total_cents >= 0))"),
        QStringLiteral("INSERT INTO sale_items_new SELECT id,sale_id,product_id,product_code,product_name,quantity,unit_price_cents,total_cents FROM sale_items"),
        QStringLiteral("DROP TABLE sale_items"),
        QStringLiteral("ALTER TABLE sale_items_new RENAME TO sale_items"),
        QStringLiteral("CREATE INDEX sale_items_sale ON sale_items(sale_id)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (15)")
    }, {
        QStringLiteral("ALTER TABLE products ADD COLUMN size TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("ALTER TABLE products ADD COLUMN color TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (16)")
    }, {
        QStringLiteral("ALTER TABLE products ADD COLUMN product_type TEXT NOT NULL DEFAULT 'product' CHECK(product_type IN ('product','service'))"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (17)")
    }, {
        QStringLiteral("CREATE TABLE service_orders (id INTEGER PRIMARY KEY AUTOINCREMENT, customer_id INTEGER NOT NULL REFERENCES customers(id), service_id INTEGER NOT NULL REFERENCES products(id), description TEXT NOT NULL CHECK(length(trim(description)) > 0), notes TEXT NOT NULL DEFAULT '', status TEXT NOT NULL DEFAULT 'open' CHECK(status IN ('open','in_progress','completed','cancelled')), amount_cents INTEGER NOT NULL CHECK(amount_cents > 0), created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, user_id INTEGER REFERENCES users(id))"),
        QStringLiteral("CREATE INDEX service_orders_status ON service_orders(status, id DESC)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (18)")
    }, {
        QStringLiteral("CREATE TABLE service_order_materials (order_id INTEGER NOT NULL REFERENCES service_orders(id), product_id INTEGER NOT NULL REFERENCES products(id), quantity REAL NOT NULL CHECK(quantity > 0), consumed INTEGER NOT NULL DEFAULT 0 CHECK(consumed IN (0,1)), PRIMARY KEY(order_id, product_id))"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (19)")
    }, {
        QStringLiteral("ALTER TABLE products ADD COLUMN variant_group TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("CREATE INDEX products_variant_group ON products(variant_group, size, color, id)"),
        QStringLiteral("INSERT INTO schema_migrations(version) VALUES (20)")
    }};

    for (int migration = version; migration < migrations.size(); ++migration) {
        for (const auto &statement : migrations[migration]) {
            if (!query.exec(statement)) return fail(query.lastError().text());
        }
    }

    if (!database.commit()) {
        if (errorMessage) *errorMessage = database.lastError().text();
        database.rollback();
        return false;
    }
    return true;
}

} // namespace MHStore::Database