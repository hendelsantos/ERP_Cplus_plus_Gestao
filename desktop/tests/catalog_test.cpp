#include "auth_fixture.h"
#include "core/audit/audit.h"
#include "core/database/database.h"
#include "modules/catalog/catalog.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class CatalogTest : public QObject
{
    Q_OBJECT
    int productId(const QString &code) {
        QSqlQuery query;
        query.prepare("SELECT id FROM products WHERE code=?");
        query.addBindValue(code);
        return query.exec() && query.next() ? query.value(0).toInt() : 0;
    }
    int supplierOf(const QString &code) {
        QSqlQuery query;
        query.prepare("SELECT supplier_id FROM products WHERE code=?");
        query.addBindValue(code);
        return query.exec() && query.next() ? query.value(0).toInt() : -1;
    }
private slots:
    void catalogAuditAtomicity() {
        QTemporaryDir directory;
        const auto path=directory.filePath("audit.sqlite");
        MHStore::Database::DatabaseManager db;
        QVERIFY(db.initialize(nullptr,path)); QVERIFY(authenticateTestAdmin());
        MHStore::Catalog catalog;
        auto scalar=[](const QString &sql) { QSqlQuery q(sql); return q.next()?q.value(0):QVariant(); };
        QSqlQuery q;
        const QStringList sections={"Categorias","Clientes","Fornecedores","Produtos"};
        const QStringList tables={"categories","customers","suppliers","products"};
        for (int i=0;i<sections.size();++i) {
            QVariantMap values{{"name","Nome privado"},{"code","P"},{"cost_price","1"},{"sale_price","2"},{"minimum_stock","0"},
                {"document","Documento privado"},{"address","Endereço privado"},{"notes","Observação privada"}};
            const auto count=scalar("SELECT COUNT(*) FROM audit_log").toInt();
            QVERIFY(catalog.save(sections[i],0,values));
            QCOMPARE(scalar("SELECT COUNT(*) FROM audit_log").toInt(),count+1);
            QCOMPARE(scalar("SELECT action FROM audit_log ORDER BY id DESC LIMIT 1").toString(),QString("catalog.create"));
            QCOMPARE(scalar("SELECT target FROM audit_log ORDER BY id DESC LIMIT 1").toString(),sections[i]+" #1");
            QCOMPARE(scalar("SELECT user_id FROM audit_log ORDER BY id DESC LIMIT 1").toInt(),MHStore::Auth::userId());
            QVERIFY(catalog.save(sections[i],1,values));
            QCOMPARE(scalar("SELECT COUNT(*) FROM audit_log").toInt(),count+1);
            values["name"]="Nome atualizado";
            QVERIFY(catalog.save(sections[i],1,values));
            QCOMPARE(scalar("SELECT details FROM audit_log ORDER BY id DESC LIMIT 1").toString(),QString("Campos: Nome"));
            QVERIFY(catalog.setActive(sections[i],1,false));
            QVERIFY(catalog.setActive(sections[i],1,false));
            QCOMPARE(scalar("SELECT COUNT(*) FROM audit_log").toInt(),count+3);
            QVERIFY(catalog.setActive(sections[i],1,true));
            QCOMPARE(scalar("SELECT COUNT(*) FROM audit_log").toInt(),count+4);
            QVERIFY(q.exec("CREATE TEMP TRIGGER deny_catalog_audit BEFORE INSERT ON audit_log BEGIN SELECT RAISE(ABORT,'audit failure'); END"));
            values["name"]="Não deve salvar";
            QVERIFY(!catalog.save(sections[i],1,values));
            QCOMPARE(scalar("SELECT name FROM "+tables[i]+" WHERE id=1").toString(),QString("Nome atualizado"));
            QVERIFY(!catalog.setActive(sections[i],1,false));
            QCOMPARE(scalar("SELECT active FROM "+tables[i]+" WHERE id=1").toInt(),1);
            values["code"]="P2"; values["document"]="Outro documento";
            QVERIFY(!catalog.save(sections[i],0,values));
            QCOMPARE(scalar("SELECT COUNT(*) FROM "+tables[i]).toInt(),1);
            QCOMPARE(scalar("SELECT COUNT(*) FROM audit_log").toInt(),count+4);
            QVERIFY(q.exec("DROP TRIGGER deny_catalog_audit"));
            QVERIFY(!catalog.setActive(sections[i],999,false));
            QVERIFY(!catalog.save(sections[i],999,values));
            QCOMPARE(scalar("SELECT COUNT(*) FROM audit_log").toInt(),count+4);
        }
        QVERIFY(q.exec("SELECT details,target FROM audit_log WHERE action LIKE 'catalog.%'"));
        while(q.next()) {
            const auto text=q.value(0).toString()+q.value(1).toString();
            QVERIFY(!text.contains("privad")); QVERIFY(!text.contains("atualizado"));
        }
        q.finish(); QSqlDatabase::database().close();
        QVERIFY(db.initialize(nullptr,path)); QVERIFY(authenticateTestAdmin());
        MHStore::Audit audit; QVERIFY(audit.refresh()); QCOMPARE(audit.entries().size(),16);
        QCOMPARE(audit.entries().first().toMap().value("action_label").toString(),QString("Cadastro reativado"));
        MHStore::Auth auth;
        QVERIFY(auth.saveUser(0,"Caixa","caixa","SenhaCaixa123!","operator",true));
        QVERIFY(auth.logout()); QVERIFY(auth.login("caixa","SenhaCaixa123!"));
        const auto before=scalar("SELECT COUNT(*) FROM audit_log").toInt();
        QVERIFY(!catalog.save("Clientes",0,{{"name","Negado"}}));
        QVERIFY(!catalog.setActive("Clientes",1,false));
        QCOMPARE(scalar("SELECT COUNT(*) FROM audit_log").toInt(),before);
        QVERIFY(!audit.refresh()); QVERIFY(audit.entries().isEmpty());
        QSqlDatabase::database().close(); QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
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
    void suppliersWorkflow()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("fornecedores.sqlite");
        MHStore::Database::DatabaseManager database;
        QString error;
        QVERIFY2(database.initialize(&error, path), qPrintable(error));
        QVERIFY(authenticateTestAdmin());
        {
            MHStore::Catalog catalog;
            QVERIFY(!catalog.save("Fornecedores", 0, {{"name", " "}}));
            QVERIFY(catalog.save("Fornecedores", 0, {{"name", "Confecção Sul"}, {"document", "12345678000199"}, {"phone", "5433221100"}, {"email", "vendas@confeccaosul.example"}}));
            QVERIFY(!catalog.save("Fornecedores", 0, {{"name", "Outra Empresa"}, {"document", "12345678000199"}}));
            QVERIFY(catalog.save("Fornecedores", 0, {{"name", "Distribuidora Norte"}}));
            QVERIFY(catalog.save("Fornecedores", 0, {{"name", "Sem Documento Irmã"}}));
            QCOMPARE(catalog.suppliers().size(), 3);
            catalog.search("Fornecedores", "3322");
            QCOMPARE(catalog.rows().size(), 1);
            QCOMPARE(catalog.rows().first().toMap().value("name").toString(), QString("Confecção Sul"));
            catalog.search("Fornecedores", "78000199");
            QCOMPARE(catalog.rows().size(), 1);
            catalog.search("Fornecedores", "norte");
            QCOMPARE(catalog.rows().size(), 1);
            const int id = catalog.rows().first().toMap().value("id").toInt();
            catalog.search("Fornecedores", "%");
            QCOMPARE(catalog.rows().size(), 0);
            QVERIFY(catalog.save("Fornecedores", id, {{"name", "Distribuidora Norte S.A."}, {"phone", "1155551234"}}));
            catalog.search("Fornecedores", "", true);
            QCOMPARE(catalog.rows().size(), 3);
            QVERIFY(catalog.setActive("Fornecedores", id, false));
            catalog.search("Fornecedores");
            QCOMPARE(catalog.rows().size(), 2);
            QVERIFY(catalog.setActive("Fornecedores", id, true));
            QVERIFY(!catalog.setActive("Fornecedores", 99999, false));
            QVERIFY(!catalog.save("Seção", 0, {{"name", "Inválida"}}));
        }
        QSqlDatabase::database().close();
        QVERIFY2(database.initialize(&error, path), qPrintable(error));
        QVERIFY(authenticateTestAdmin());
        {
            MHStore::Catalog catalog;
            catalog.search("Fornecedores", "1155551234");
            QCOMPARE(catalog.rows().size(), 1);
            QCOMPARE(catalog.rows().first().toMap().value("name").toString(), QString("Distribuidora Norte S.A."));
            catalog.search("Fornecedores");
            QCOMPARE(catalog.rows().size(), 3);
        }
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
    void productSupplierLink()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        MHStore::Database::DatabaseManager database;
        QString error;
        QVERIFY2(database.initialize(&error, directory.filePath("vinculo.sqlite")), qPrintable(error));
        QVERIFY(authenticateTestAdmin());
        MHStore::Catalog catalog;
        QVERIFY(catalog.save("Fornecedores", 0, {{"name", "Confecção Sul"}}));
        QVERIFY(catalog.save("Fornecedores", 0, {{"name", "Tecelagem Leste"}}));
        const int first = catalog.suppliers().first().toMap().value("id").toInt();
        const int second = catalog.suppliers().last().toMap().value("id").toInt();
        QVERIFY(catalog.save("Produtos", 0, {{"name", "Camiseta"}, {"code", "CAM-01"}, {"cost_price", "10"}, {"sale_price", "20"}, {"minimum_stock", "1"}, {"supplier_id", first}}));
        QCOMPARE(supplierOf("CAM-01"), first);
        QVERIFY(catalog.save("Produtos", productId("CAM-01"), {{"name", "Camiseta"}, {"code", "CAM-01"}, {"cost_price", "10"}, {"sale_price", "20"}, {"minimum_stock", "1"}, {"supplier_id", second}}));
        QCOMPARE(supplierOf("CAM-01"), second);
        QVERIFY(catalog.save("Produtos", productId("CAM-01"), {{"name", "Camiseta"}, {"code", "CAM-01"}, {"cost_price", "10"}, {"sale_price", "20"}, {"minimum_stock", "1"}, {"supplier_id", 0}}));
        QCOMPARE(supplierOf("CAM-01"), 0);
        QVERIFY(!catalog.save("Produtos", productId("CAM-01"), {{"name", "Camiseta"}, {"code", "CAM-01"}, {"cost_price", "10"}, {"sale_price", "20"}, {"minimum_stock", "1"}, {"supplier_id", 99999}}));
        QVERIFY(catalog.save("Produtos", productId("CAM-01"), {{"name", "Camiseta"}, {"code", "CAM-01"}, {"cost_price", "10"}, {"sale_price", "20"}, {"minimum_stock", "1"}, {"supplier_id", first}}));
        QVERIFY(catalog.setActive("Fornecedores", first, false));
        QCOMPARE(supplierOf("CAM-01"), first);
        catalog.search("Produtos", "CAM-01");
        QCOMPARE(catalog.rows().first().toMap().value("supplier_id").toInt(), first);
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
    void complementaryFieldsAndMigration() {
        QTemporaryDir directory;
        const auto path=directory.filePath("fields.sqlite");
        MHStore::Database::DatabaseManager db;
        QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO customers(name) VALUES('Anterior')"));
        QVERIFY(removeComplementaryMigration());
        QVERIFY(db.initialize(nullptr,path));
        QVERIFY(db.initialize(nullptr,path));
        QVERIFY(q.exec("SELECT address,birth_date,notes FROM customers WHERE id=1"));
        QVERIFY(q.next()); QCOMPARE(q.value(0).toString(),QString(""));
        q.finish();
        MHStore::Catalog catalog;
        QVariantMap customer{{"name","Cliente"},{"address"," Rua A "},{"birth_date","29/02/2000"},{"notes"," Preferência "}};
        QVERIFY(catalog.save("Clientes",1,customer));
        for (const auto &date : {"31/02/2000","01/01/2999","texto"}) {
            customer["birth_date"]=date; QVERIFY(!catalog.save("Clientes",1,customer));
        }
        customer["birth_date"]="2000-02-29";
        customer["address"]=QString(161,'a'); QVERIFY(!catalog.save("Clientes",1,customer));
        QVariantMap product{{"name","Produto"},{"code","P"},{"cost_price","1"},{"sale_price","2"},{"minimum_stock","3"},
            {"maximum_stock","10,5"},{"brand"," Marca "},{"unit","UN"},{"location","A1"},{"notes","Teste"}};
        QVERIFY(catalog.save("Produtos",0,product));
        for(const auto &maximum : {"-1","nan","abc","2"}) {
            product["maximum_stock"]=maximum; QVERIFY(!catalog.save("Produtos",1,product));
        }
        product["maximum_stock"]="0"; product["unit"]=QString(11,'x');
        QVERIFY(!catalog.save("Produtos",1,product));
        auto optional=product; optional["unit"]=""; optional["maximum_stock"]=""; optional["code"]="OPTIONAL"; optional["name"]="Z opcional";
        QVERIFY(catalog.save("Produtos",0,optional));
        QSqlDatabase::database().close();
        QVERIFY(db.initialize(nullptr,path)); QVERIFY(authenticateTestAdmin());
        catalog.search("Clientes");
        QCOMPARE(catalog.rows().first().toMap().value("birth_date").toString(),QString("2000-02-29"));
        QCOMPARE(catalog.rows().first().toMap().value("address").toString(),QString("Rua A"));
        catalog.search("Produtos");
        QCOMPARE(catalog.rows().first().toMap().value("maximum_stock").toDouble(),10.5);
        QCOMPARE(catalog.rows().first().toMap().value("brand").toString(),QString("Marca"));
        QSqlDatabase::database().close(); QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
    void upgradeFromVersionSeven()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("versao7.sqlite");
        MHStore::Database::DatabaseManager database;
        QString error;
        QVERIFY2(database.initialize(&error, path), qPrintable(error));
        QVERIFY(authenticateTestAdmin());
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO products(code,name,sale_price,sale_price_cents) VALUES('ANTIGO','Produto Antigo',9.99,999)"));
        QVERIFY(removeComplementaryMigration());
        QVERIFY(query.exec("ALTER TABLE products DROP COLUMN supplier_id"));
        QVERIFY(query.exec("DROP TABLE suppliers"));
        QVERIFY(query.exec("DELETE FROM schema_migrations WHERE version=8"));
        QVERIFY2(database.initialize(&error, path), qPrintable(error));
        QVERIFY(query.exec("SELECT COUNT(*) FROM schema_migrations WHERE version=8"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec("SELECT name FROM products WHERE code='ANTIGO'"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("Produto Antigo"));
        QVERIFY(authenticateTestAdmin());
        MHStore::Catalog catalog;
        QVERIFY(catalog.suppliers().isEmpty());
        QVERIFY(catalog.save("Fornecedores", 0, {{"name", "Fornecedor Pós-Migração"}}));
        QVERIFY(catalog.save("Produtos", productId("ANTIGO"), {{"name", "Produto Antigo"}, {"code", "ANTIGO"}, {"cost_price", "5"}, {"sale_price", "9,99"}, {"minimum_stock", "0"}, {"supplier_id", catalog.suppliers().first().toMap().value("id").toInt()}}));
        QCOMPARE(supplierOf("ANTIGO"), catalog.suppliers().first().toMap().value("id").toInt());        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
};
QTEST_GUILESS_MAIN(CatalogTest)
#include "catalog_test.moc"
