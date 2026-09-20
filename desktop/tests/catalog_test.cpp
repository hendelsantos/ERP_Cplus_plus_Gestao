#include "auth_fixture.h"
#include "core/database/database.h"
#include "modules/catalog/catalog.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class CatalogTest : public QObject
{
    Q_OBJECT
private slots:
    void workflow()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("test.sqlite");
        MHStore::Database::DatabaseManager database;
        QString error;
        QVERIFY2(database.initialize(&error, path), qPrintable(error));
        QVERIFY(authenticateTestAdmin());
        {
            MHStore::Catalog catalog;
            QVERIFY(catalog.save("Categorias", 0, {{"name", "Roupas"}}));
            const int category = catalog.categories().first().toMap().value("id").toInt();
            QVERIFY(!catalog.save("Categorias", 0, {{"name", "Roupas"}}));
            QVariantMap product{{"name", "Camiseta 100%"}, {"code", "CAM-01"},
                {"barcode", "001234"}, {"cost_price", "12,50"}, {"sale_price", "24,90"},
                {"minimum_stock", "2"}, {"category_id", category}};
            QVERIFY(catalog.save("Produtos", 0, product));
            QVERIFY(!catalog.save("Produtos", 0, product));
            catalog.search("Produtos", "001234");
            QCOMPARE(catalog.rows().size(), 1);
            const int id = catalog.rows().first().toMap().value("id").toInt();
            QCOMPARE(catalog.rows().first().toMap().value("sale_price").toDouble(), 24.9);
            QCOMPARE(catalog.rows().first().toMap().value("stock_quantity").toDouble(), 0.0);
            product["sale_price"] = "-1";
            QVERIFY(!catalog.save("Produtos", id, product));
            product["sale_price"] = "abc";
            QVERIFY(!catalog.save("Produtos", id, product));
            product["sale_price"] = "25,50";
            product["category_id"] = 99999;
            QVERIFY(!catalog.save("Produtos", id, product));
            product["category_id"] = category;
            QVERIFY(catalog.save("Produtos", id, product));
            catalog.search("Produtos", "%");
            QCOMPARE(catalog.rows().size(), 1);
            catalog.search("Produtos", "' OR 1=1 --");
            QCOMPARE(catalog.rows().size(), 0);
            catalog.search("Produtos");
            QVERIFY(catalog.setActive("Produtos", id, false));
            QCOMPARE(catalog.rows().size(), 0);
            catalog.search("Produtos", "", true);
            QCOMPARE(catalog.rows().size(), 1);
            QVERIFY(catalog.setActive("Produtos", id, true));
            QVERIFY(!catalog.setActive("Produtos", 99999, false));
            QVERIFY(!catalog.save("Clientes", 0, {{"name", "  "}}));
            QVERIFY(catalog.save("Clientes", 0, {{"name", "Ana d'Ávila"}, {"phone", "11999990000"}}));
            catalog.search("Clientes", "9999");
            QCOMPARE(catalog.rows().size(), 1);
            QVERIFY(!catalog.save("sales", 0, {{"name", "Inválido"}}));
        }
        QSqlDatabase::database().close();
        QVERIFY2(database.initialize(&error, path), qPrintable(error));
        QVERIFY(authenticateTestAdmin());
        {
            MHStore::Catalog catalog;
            catalog.search("Produtos");
            QCOMPARE(catalog.rows().size(), 1);
            QCOMPARE(catalog.rows().first().toMap().value("sale_price").toDouble(), 25.5);
            QSqlQuery query("SELECT COUNT(*) FROM schema_migrations WHERE version = 1");
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 1);
        }
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
};

QTEST_GUILESS_MAIN(CatalogTest)
#include "catalog_test.moc"
