#include "auth_fixture.h"
#include "core/settings/settings.h"
#include "core/audit/audit.h"
#include "core/database/database.h"
#include "modules/pos/pos.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class PosTest : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QString path;
    QVariant scalar(const QString &sql) { QSqlQuery q(sql); return q.next() ? q.value(0) : QVariant(); }
private slots:
    void init() {
        path=directory.filePath(QUuid::createUuid().toString()+".sqlite");
        MHStore::Database::DatabaseManager db;
        QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO products(code,name,sale_price,sale_price_cents,stock_quantity) VALUES('P1','Produto',19.90,1990,10),('P2','Outro',0.10,10,5)"));
    }
    void cleanup() { QSqlDatabase::database().close(); QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection); }
    void authenticationAndPermissions() {
        MHStore::Auth auth;
        QVERIFY(!auth.needsSetup());
        QVERIFY(!auth.setup("Outro","outro","SenhaTeste123!"));
        QVERIFY(auth.saveUser(0,"Funcionário","caixa","SenhaCaixa123!","operator",true));
        QVERIFY(!auth.saveUser(0,"Duplicado","CAIXA","SenhaCaixa123!","admin",true));
        QVERIFY(!auth.saveUser(1,"Ana","admin","","operator",false));
        QVERIFY(auth.logout());
        MHStore::Pos pos;
        QVERIFY(!pos.openCash("0","Falso"));
        pos.searchSales(); QVERIFY(pos.sales().isEmpty());
        QVERIFY(!auth.login("admin","errada"));
        QVERIFY(auth.login("CAIXA","SenhaCaixa123!"));
        QVERIFY(!MHStore::Auth::allowed("catalog"));
        QVERIFY(!MHStore::Auth::allowed("inventory"));
        QVERIFY(!MHStore::Auth::allowed("backup"));
        QVERIFY(!MHStore::Auth::allowed("users"));
        QVERIFY(!auth.saveUser(0,"Outro","outro","SenhaTeste123!","admin",true));
        MHStore::Settings settings;
        QVERIFY(!settings.save("Inválido","general",true,true,true));
        QVERIFY(pos.openCash("0","Nome falsificado"));
        const int session=pos.cash().value("id").toInt();
        QVERIFY(pos.add(1));
        auth.hasPendingCart=[&pos] { return !pos.cart().isEmpty(); };
        QVERIFY(!auth.logout());
        QVERIFY(pos.checkout(session,"pix","","Falso"));
        QCOMPARE(scalar("SELECT operator_name FROM sales").toString(),QString("Funcionário"));
        QCOMPARE(scalar("SELECT user_id FROM sales").toInt(),2);
        QCOMPARE(scalar("SELECT user_id FROM inventory_movements").toInt(),2);
        QVERIFY(pos.closeCash(session,"0","Falso"));
        QCOMPARE(scalar("SELECT closed_user_id FROM cash_sessions").toInt(),2);
        QVERIFY(auth.logout());
        QVERIFY(auth.login("admin","SenhaTeste123!"));
        QVERIFY(auth.saveUser(2,"Funcionário","caixa","","operator",false));
        QVERIFY(auth.logout());
        QVERIFY(!auth.login("caixa","SenhaCaixa123!"));
        QVERIFY(auth.login("admin","SenhaTeste123!"));
        QVERIFY(auth.saveUser(2,"Funcionário","caixa","NovaSenha12345!","operator",true));
        QVERIFY(auth.logout());
        QVERIFY(!auth.login("caixa","SenhaCaixa123!"));
        QVERIFY(auth.login("caixa","NovaSenha12345!"));
        QSqlQuery q;
        QVERIFY(q.exec("UPDATE users SET session_version=session_version+1 WHERE id=2"));
        QVERIFY(!MHStore::Auth::allowed("pos"));
        QVERIFY(!pos.openCash("0","Falso"));
    }
    void bootstrapRecoveryAndLockout() {
        MHStore::Auth::resetSession();
        QSqlQuery q; QVERIFY(q.exec("DELETE FROM users"));
        MHStore::Auth auth;
        QVERIFY(auth.needsSetup());
        QVERIFY(!auth.setup("Admin","adm","curta"));
        QVERIFY(auth.setup("Admin","adm","UmaSenhaForte123!"));
        const auto code=auth.recoveryCode();
        QCOMPARE(code.size(),64);
        QVERIFY(scalar("SELECT password_hash FROM users").toByteArray()!=QByteArray("UmaSenhaForte123!"));
        QVERIFY(!auth.recover("adm",QString(64,'0'),"OutraSenha12345!"));
        QVERIFY(auth.recover("adm",code,"OutraSenha12345!"));
        const auto replacement=auth.recoveryCode();
        QVERIFY(replacement!=code);
        QVERIFY(!auth.recover("adm",code,"OutraSenha12345!"));
        for(int i=0;i<5;++i) QVERIFY(!auth.login("adm","errada"));
        QVERIFY(!auth.login("adm","OutraSenha12345!"));
        QCOMPARE(scalar("SELECT failed_attempts FROM users").toInt(),0);
        QVERIFY(auth.recover("adm",replacement,"SenhaFinal12345!"));
        QVERIFY(auth.login("adm","SenhaFinal12345!"));
        QVERIFY(auth.logout());
        QSqlDatabase::database().close();
        QVERIFY(QSqlDatabase::database().open());
        QVERIFY(auth.login("adm","SenhaFinal12345!"));
    }
    void ownPasswordChange() {
        MHStore::Auth::resetSession();
        QSqlQuery q; QVERIFY(q.exec("DELETE FROM users"));
        MHStore::Auth auth;
        QVERIFY(auth.setup("Admin","adm","SenhaTeste123!"));
        const auto code=auth.recoveryCode();
        QVERIFY(!auth.changePassword("SenhaTeste123!","NovaSenha12345!","NovaSenha12345!"));
        QVERIFY(auth.login("adm","SenhaTeste123!"));
        QVERIFY(!auth.changePassword("SenhaTeste123!","curta","curta"));
        QVERIFY(!auth.changePassword("SenhaTeste123!","NovaSenha12345!","Divergente1234!"));
        QVERIFY(!auth.changePassword("SenhaTeste123!","SenhaTeste123!","SenhaTeste123!"));
        QVERIFY(!auth.changePassword("errada","NovaSenha12345!","NovaSenha12345!"));
        QCOMPARE(scalar("SELECT failed_attempts FROM users").toInt(),1);
        const auto version=scalar("SELECT session_version FROM users").toInt();
        const auto hash=scalar("SELECT password_hash FROM users").toByteArray();
        const auto salt=scalar("SELECT salt FROM users").toByteArray();
        QVERIFY(auth.changePassword("SenhaTeste123!","NovaSenha12345!","NovaSenha12345!"));
        QCOMPARE(scalar("SELECT session_version FROM users").toInt(),version+1);
        QVERIFY(scalar("SELECT password_hash FROM users").toByteArray()!=hash);
        QVERIFY(scalar("SELECT salt FROM users").toByteArray()!=salt);
        QCOMPARE(scalar("SELECT failed_attempts FROM users").toInt(),0);
        QVERIFY(auth.authenticated());
        QVERIFY(MHStore::Auth::allowed("settings"));
        for(int i=0;i<5;++i) QVERIFY(!auth.changePassword("errada","OutraSenha12345!","OutraSenha12345!"));
        QVERIFY(!auth.changePassword("NovaSenha12345!","OutraSenha12345!","OutraSenha12345!"));
        QVERIFY(auth.authenticated());
        QVERIFY(auth.logout());
        QVERIFY(!auth.login("adm","SenhaTeste123!"));
        QVERIFY(!auth.login("adm","NovaSenha12345!"));
        QVERIFY(auth.recover("adm",code,"SenhaFinal12345!"));
        QVERIFY(auth.login("adm","SenhaFinal12345!"));
        QVERIFY(auth.logout());
        QSqlDatabase::database().close();
        QVERIFY(QSqlDatabase::database().open());
        QVERIFY(auth.login("adm","SenhaFinal12345!"));
    }
    void recoveryCodeIssuance() {
        MHStore::Auth auth;
        QVERIFY(auth.saveUser(0,"Bia","bia","SenhaBia12345!","admin",true));
        QVERIFY(auth.saveUser(0,"Caixa","caixa","SenhaCaixa123!","operator",true));
        QVERIFY(auth.logout());
        QVERIFY(auth.login("caixa","SenhaCaixa123!"));
        QVERIFY(!auth.issueRecoveryCode(2));
        QVERIFY(auth.logout());
        QVERIFY(auth.login("admin","SenhaTeste123!"));
        QVERIFY(!auth.issueRecoveryCode(0));
        QVERIFY(!auth.issueRecoveryCode(999));
        QVERIFY(!auth.issueRecoveryCode(3));
        QVERIFY(!auth.issueRecoveryCode(1));
        QVERIFY(auth.issueRecoveryCode(2));
        const auto code=auth.recoveryCode();
        QCOMPARE(code.size(),64);
        QVERIFY(scalar("SELECT recovery_hash FROM users WHERE id=2").toByteArray()!=code.toUtf8());
        auth.dismissRecovery();
        QVERIFY(auth.recoveryCode().isEmpty());
        QVERIFY(auth.issueRecoveryCode(2));
        const auto replacement=auth.recoveryCode();
        QVERIFY(replacement!=code);
        auth.dismissRecovery();
        QVERIFY(auth.logout());
        QVERIFY(!auth.recover("bia",code,"NovaSenhaBia123!"));
        QVERIFY(auth.recover("bia",replacement,"NovaSenhaBia123!"));
        const auto rotated=auth.recoveryCode();
        QVERIFY(rotated!=replacement);
        auth.dismissRecovery();
        QVERIFY(!auth.recover("bia",replacement,"OutraSenha123!"));
        QVERIFY(auth.login("bia","NovaSenhaBia123!"));
        QVERIFY(auth.logout());
        QVERIFY(auth.login("admin","SenhaTeste123!"));
        QVERIFY(auth.issueRecoveryCode(2));
        const auto beforeEdit=auth.recoveryCode();
        auth.dismissRecovery();
        QVERIFY(auth.saveUser(2,"Bia Souza","bia","","admin",true));
        QVERIFY(scalar("SELECT recovery_hash FROM users WHERE id=2").toByteArray().isNull());
        QVERIFY(auth.issueRecoveryCode(2));
        const auto forInactive=auth.recoveryCode();
        auth.dismissRecovery();
        QSqlQuery q;
        QVERIFY(q.exec("UPDATE users SET active=0 WHERE id=2"));
        QVERIFY(!auth.issueRecoveryCode(2));
        QVERIFY(auth.logout());
        QVERIFY(!auth.recover("bia",forInactive,"OutraSenha123!"));
        QVERIFY(q.exec("UPDATE users SET active=1 WHERE id=2"));
        QVERIFY(auth.recover("bia",forInactive,"SenhaReativada1!"));
        auth.dismissRecovery();
        QVERIFY(auth.login("admin","SenhaTeste123!"));
        QVERIFY(auth.issueRecoveryCode(2));
        const auto persisted=auth.recoveryCode();
        auth.dismissRecovery();
        QVERIFY(auth.logout());
        QSqlDatabase::database().close();
        QVERIFY(QSqlDatabase::database().open());
        QVERIFY(auth.recover("bia",persisted,"SenhaPersiste123!"));
        QVERIFY(auth.login("bia","SenhaPersiste123!"));
    }
    void cancelSale() {
        MHStore::Pos pos;
        QVERIFY(pos.openCash("100,00","Ana"));
        const int session = pos.cash().value("id").toInt();
        QVERIFY(pos.add(1));
        QVERIFY(pos.checkout(session,"cash","20,00","Ana"));
        QCOMPARE(scalar("SELECT status FROM sales WHERE id=1").toString(),QString("completed"));
        QVERIFY(!pos.cancelSale(1,""));
        QVERIFY(pos.cancelSale(1,"Erro de item"));
        QCOMPARE(scalar("SELECT status FROM sales WHERE id=1").toString(),QString("cancelled"));
        QCOMPARE(scalar("SELECT cancel_reason FROM sales WHERE id=1").toString(),QString("Erro de item"));
        QCOMPARE(scalar("SELECT stock_quantity FROM products WHERE id=1").toInt(),10);
        QCOMPARE(pos.cash().value("cash_expected").toInt(),10000);
        QVERIFY(!pos.cancelSale(1,"Duplicado"));
        QVERIFY(!pos.cancelSale(999,"Qualquer"));
    }
    void saleAdjustments() {
        MHStore::Pos pos;
        QVERIFY(!pos.setAdjustments("1","0","Teste"));
        QVERIFY(pos.openCash("10",""));
        const auto session=pos.cash().value("id").toInt();
        QVERIFY(pos.add(1));
        QVERIFY(!pos.setAdjustments("19,90","0","Integral"));
        QVERIFY(!pos.setAdjustments("-1","0","Inválido"));
        QVERIFY(!pos.setAdjustments("1.001","0","Inválido"));
        QVERIFY(!pos.setAdjustments("0","1000000000","Limite"));
        QVERIFY(!pos.setAdjustments("1","0",""));
        QVERIFY(!pos.setAdjustments("1","0",QString(201,'x')));
        QVERIFY(pos.setAdjustments("2,00","0,50","Negociação"));
        QCOMPARE(pos.subtotal(),1990); QCOMPARE(pos.total(),1840);
        QVERIFY(pos.setQuantity(1,1)); QCOMPARE(pos.discount(),200);
        QSqlQuery q;
        QVERIFY(q.exec("UPDATE users SET role='operator' WHERE id=1"));
        QVERIFY(!pos.setAdjustments("1","0","Negado"));
        QVERIFY(!pos.checkout(session,"pix","",""));
        QCOMPARE(scalar("SELECT COUNT(*) FROM sales").toInt(),0);
        QVERIFY(q.exec("UPDATE users SET role='admin' WHERE id=1"));
        QVERIFY(q.exec("CREATE TEMP TRIGGER reject_adjust_audit BEFORE INSERT ON audit_log WHEN NEW.action='sale.adjust' BEGIN SELECT RAISE(ABORT,'audit failure'); END"));
        QVERIFY(!pos.checkout(session,"cash","20",""));
        QCOMPARE(scalar("SELECT stock_quantity FROM products WHERE id=1").toInt(),10);
        QCOMPARE(scalar("SELECT COUNT(*) FROM sales").toInt(),0);
        QCOMPARE(scalar("SELECT COUNT(*) FROM payments").toInt(),0);
        QCOMPARE(pos.total(),1840);
        QVERIFY(q.exec("DROP TRIGGER reject_adjust_audit"));
        QVERIFY(pos.checkout(session,"cash","20",""));
        QCOMPARE(pos.cash().value("cash_expected").toInt(),2840);
        QCOMPARE(scalar("SELECT amount_cents FROM payments").toInt(),1840);
        QCOMPARE(scalar("SELECT change_cents FROM payments").toInt(),160);
        QCOMPARE(scalar("SELECT SUM(total_cents) FROM sale_items").toInt(),1990);
        QCOMPARE(scalar("SELECT user_id FROM audit_log WHERE action='sale.adjust'").toInt(),1);
        QCOMPARE(pos.discount(),0); QCOMPARE(pos.surcharge(),0);
        QVERIFY(pos.receipt().contains("Negociação"));
        QSqlDatabase::database().close(); QVERIFY(QSqlDatabase::database().open());
        MHStore::Pos reopened; QVERIFY(reopened.loadSale(1));
        QCOMPARE(reopened.selectedSale().value("subtotal_cents").toInt(),1990);
        QCOMPARE(reopened.selectedSale().value("discount_cents").toInt(),200);
        QCOMPARE(reopened.selectedSale().value("surcharge_cents").toInt(),50);
        reopened.refreshDashboard(); QCOMPARE(reopened.dashboard().value("today_cents").toInt(),1840);
        QVERIFY(reopened.add(2));
        QVERIFY(reopened.setAdjustments("0.01","0","Teste"));
        QVERIFY(reopened.add(2)); QCOMPARE(reopened.discount(),0);
        QVERIFY(reopened.setAdjustments("0","0.05","Taxa"));
        reopened.clearCart(); QCOMPARE(reopened.surcharge(),0);
        QVERIFY(reopened.add(2)); QVERIFY(reopened.setAdjustments("0","0.05","Taxa"));
        QVERIFY(reopened.checkout(session,"pix","",""));
        QCOMPARE(reopened.cash().value("cash_expected").toInt(),2840);
        QCOMPARE(scalar("SELECT total_cents FROM sales WHERE id=2").toInt(),15);
    }
    void adjustmentMigration() {
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO sales(total_amount,total_cents) VALUES(12.34,1234)"));
        QVERIFY(removeAdjustmentMigration());
        MHStore::Database::DatabaseManager db;
        QVERIFY(db.initialize(nullptr,path)); QVERIFY(db.initialize(nullptr,path));
        QCOMPARE(scalar("SELECT subtotal_cents FROM sales").toInt(),1234);
        QCOMPARE(scalar("SELECT discount_cents FROM sales").toInt(),0);
        QCOMPARE(scalar("SELECT total_cents FROM sales").toInt(),1234);
    }
    void testSplitPayments() {
        MHStore::Pos pos;
        QVERIFY(pos.openCash("100,00","Ana"));
        const int session = pos.cash().value("id").toInt();
        QVERIFY(pos.add(1));
        QVERIFY(pos.checkout(session, "cash|pix", "10,00|9,90", "Ana"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM payments").toInt(),1);
        QCOMPARE(scalar("SELECT method FROM payments").toString(),QString("split"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM payment_items").toInt(),2);
        QCOMPARE(scalar("SELECT SUM(amount_cents) FROM payment_items WHERE method='cash'").toInt(),1000);
        QCOMPARE(scalar("SELECT SUM(amount_cents) FROM payment_items WHERE method='pix'").toInt(),990);
        QCOMPARE(scalar("SELECT total_cents FROM sales").toInt(),1990);
        QCOMPARE(pos.cash().value("cash_expected").toInt(),11000);
        QVERIFY(!pos.checkout(session, "cash|pix", "10,00|5,00", "Ana"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM sales").toInt(),1);
    }
    void permissionMatrix() {
        const auto matrix=MHStore::Auth::permissions();
        QCOMPARE(matrix.size(),11);
        QStringList ids;
        for (const auto &p : matrix) { QVERIFY(!p.id.isEmpty()); QVERIFY(!p.description.isEmpty()); QVERIFY(p.admin); ids << p.id; }
        for (const auto &id : QStringList{"read","catalog","inventory","cash","pos","settings","backup","users","audit"}) QVERIFY(ids.contains(id));
        for (const auto &p : matrix) QVERIFY(MHStore::Auth::allowed(p.id));
        QVERIFY(!MHStore::Auth::allowed("unknown"));
        QVERIFY(!MHStore::Auth::allowed(""));
        QVERIFY(!MHStore::Auth::allowed("READ"));
        QVERIFY(!MHStore::Auth::allowed("users; DROP TABLE users"));
        MHStore::Auth auth;
        QVERIFY(auth.saveUser(0,"Caixa","caixa","SenhaCaixa123!","operator",true));
        QVERIFY(auth.logout());
        QVERIFY(auth.login("caixa","SenhaCaixa123!"));
        for (const auto &p : matrix) QCOMPARE(MHStore::Auth::allowed(p.id),p.operatorRole);
        QVERIFY(!MHStore::Auth::allowed("unknown"));
        QVERIFY(auth.logout());
        for (const auto &p : matrix) QVERIFY(!MHStore::Auth::allowed(p.id));
    }
    void auditLog() {
        MHStore::Auth auth;
        MHStore::Settings settings;
        MHStore::Audit audit;
        QVERIFY(audit.refresh());
        QCOMPARE(audit.entries().size(),0);
        QVERIFY(!audit.moreAvailable());
        QVERIFY(auth.saveUser(0,"Bia","bia","SenhaBia12345!","admin",true));
        QVERIFY(auth.saveUser(0,"Caixa","caixa","SenhaCaixa123!","operator",true));
        QVERIFY(auth.changePassword("SenhaTeste123!","NovaSenha12345!","NovaSenha12345!"));
        QVERIFY(auth.issueRecoveryCode(2));
        QVERIFY(settings.save("Loja Audit","general",true,true,true));
        QCOMPARE(scalar("SELECT COUNT(*) FROM audit_log").toInt(),5);
        QVERIFY(audit.refresh());
        QCOMPARE(audit.entries().size(),5);
        QCOMPARE(audit.entries().at(0).toMap().value("action").toString(),QString("settings.update"));
        QCOMPARE(audit.entries().at(1).toMap().value("action").toString(),QString("user.recovery_code"));
        QCOMPARE(audit.entries().at(2).toMap().value("action").toString(),QString("user.password"));
        QCOMPARE(audit.entries().at(3).toMap().value("action").toString(),QString("user.create"));
        QCOMPARE(audit.entries().at(4).toMap().value("action").toString(),QString("user.create"));
        QCOMPARE(audit.entries().at(0).toMap().value("target").toString(),QString("Empresa e módulos"));
        QCOMPARE(audit.entries().at(1).toMap().value("target").toString(),QString("bia"));
        QCOMPARE(audit.entries().at(2).toMap().value("target").toString(),QString("admin"));
        QVERIFY(audit.entries().at(3).toMap().value("details").toString().contains("perfil=operator"));
        QCOMPARE(audit.entries().at(0).toMap().value("user_name").toString(),QString("Ana"));
        for (const auto &entry : audit.entries()) {
            const auto details=entry.toMap().value("details").toString()+entry.toMap().value("target").toString();
            QVERIFY(!details.contains("SenhaBia12345!"));
            QVERIFY(!details.contains("SenhaCaixa123!"));
            QVERIFY(!details.contains("NovaSenha12345!"));
            QVERIFY(!entry.toMap().value("created_at").toString().isEmpty());
            QVERIFY(!entry.toMap().value("action_label").toString().isEmpty());
        }
        QVERIFY(!auth.saveUser(0,"Ruim","ruim","curta","admin",true));
        QCOMPARE(scalar("SELECT COUNT(*) FROM audit_log").toInt(),5);
        QVERIFY(auth.saveUser(3,"Caixa Dois","caixa","","operator",true));
        QCOMPARE(scalar("SELECT COUNT(*) FROM audit_log").toInt(),6);
        QVERIFY(audit.refresh());
        QCOMPARE(audit.entries().first().toMap().value("action").toString(),QString("user.update"));
        QVERIFY(audit.entries().first().toMap().value("details").toString().contains("nome=Caixa Dois"));
        QVERIFY(!audit.entries().first().toMap().value("details").toString().contains("senha definida"));
        for (int i=0;i<60;++i) QVERIFY(auth.saveUser(3,"Caixa Dois","caixa","","operator",true));
        QVERIFY(audit.refresh());
        QCOMPARE(audit.entries().size(),50);
        QVERIFY(audit.moreAvailable());
        QVERIFY(audit.loadMore());
        QCOMPARE(audit.entries().size(),66);
        QVERIFY(!audit.moreAvailable());
        QVERIFY(auth.logout());
        QVERIFY(auth.login("caixa","SenhaCaixa123!"));
        QVERIFY(!MHStore::Auth::allowed("audit"));
        QVERIFY(!audit.refresh());
        QVERIFY(audit.entries().isEmpty());
        QVERIFY(audit.message().contains("permissão"));
        QVERIFY(auth.logout());
        QVERIFY(auth.login("admin","NovaSenha12345!"));
        QSqlQuery q;
        QVERIFY(q.exec("DROP TABLE audit_log"));
        QVERIFY(!auth.saveUser(3,"Caixa Três","caixa","","operator",true));
        QCOMPARE(scalar("SELECT name FROM users WHERE id=3").toString(),QString("Caixa Dois"));
        QVERIFY(!settings.save("Outra Loja","general",true,true,true));
        QCOMPARE(scalar("SELECT company FROM business_settings").toString(),QString("Loja Audit"));
    }
    void companyComplementaryFields() {
        MHStore::Settings settings;
        QVERIFY(settings.save("Empresa","general",true,true,true,"12.345","1199999"," Rua da loja "));
        QVERIFY(!settings.save("Empresa","general",true,true,true,QString(21,'1'),"",""));
        QVERIFY(!settings.save("Empresa","general",true,true,true,"",QString(21,'1'),""));
        QVERIFY(!settings.save("Empresa","general",true,true,true,"","",QString(161,'a')));
        QSqlDatabase::database().close(); QVERIFY(QSqlDatabase::database().open());
        MHStore::Settings reopened;
        QCOMPARE(reopened.values().value("address").toString(),QString("Rua da loja"));
        QCOMPARE(reopened.values().value("document").toString(),QString("12.345"));
        QSqlQuery q;
        QVERIFY(q.exec("CREATE TEMP TRIGGER fail_company BEFORE INSERT ON audit_log BEGIN SELECT RAISE(ABORT,'audit failed'); END"));
        QVERIFY(!reopened.save("Alterada","general",true,true,true,"novo","novo","novo"));
        QCOMPARE(scalar("SELECT document FROM business_settings").toString(),QString("12.345"));
        QVERIFY(q.exec("DROP TRIGGER fail_company"));
    }
    void moduleRegistry() {
        MHStore::Settings settings;
        const QVariantMap disabled={{"inventory",false},{"cash",false},{"pos",false}};
        QVERIFY(settings.saveModules("Loja","general",disabled));
        QVERIFY(settings.navigation().contains("Vendas"));
        QVERIFY(settings.navigation().contains("Clientes"));
        QVERIFY(!settings.navigation().contains("PDV"));
        QVERIFY(!settings.navigation().contains("Estoque"));
        QVERIFY(!MHStore::Settings::enabled("unknown"));
        QVERIFY(!MHStore::Settings::enabled("pos; DROP TABLE products"));
        QVERIFY(MHStore::Settings::enabled("sales"));
        auto selection=disabled;
        selection["pos"]=true;
        QVERIFY(!settings.saveModules("Loja","general",selection));
        selection["inventory"]=true;
        QVERIFY(!settings.saveModules("Loja","general",selection));
        QVERIFY(settings.message().contains("Caixa"));
        selection["cash"]=true;
        QVERIFY(settings.saveModules("Loja","general",selection));
        QVERIFY(settings.navigation().contains("PDV"));
        selection["unknown"]=true;
        QVERIFY(!settings.saveModules("Loja","general",selection));
        QVERIFY(!settings.saveModules("Loja","general",{}));
        selection=disabled;
        selection["pos"]=QString("false");
        QVERIFY(!settings.saveModules("Loja","general",selection));
        QCOMPARE(settings.values().value("pos").toBool(),true);
        QSet<QString> ids;
        for (const auto &value : settings.modules()) {
            const auto entry=value.toMap();
            const auto id=entry.value("id").toString();
            QVERIFY(!ids.contains(id));
            ids.insert(id);
            QVERIFY(!entry.value("label").toString().isEmpty());
        }
        for (const auto &value : settings.modules())
            for (const auto &dependency : value.toMap().value("dependencies").toStringList())
                QVERIFY(ids.contains(dependency));
        // Even an externally corrupted dependency must not enable checkout.
        QSqlQuery q;
        QVERIFY(q.exec("PRAGMA ignore_check_constraints=ON"));
        QVERIFY(q.exec("UPDATE business_settings SET cash=0,pos=1"));
        QVERIFY(!MHStore::Settings::enabled("pos"));
        MHStore::Settings reopened;
        QVERIFY(!reopened.navigation().contains("PDV"));
        QVERIFY(q.exec("PRAGMA ignore_check_constraints=OFF"));
        QVERIFY(settings.saveModules("Loja","general",disabled));
    }
    void moduleSettings() {
        MHStore::Settings settings;
        MHStore::Pos pos;
        QVERIFY(!settings.save("", "general",true,true,true));
        QVERIFY(!settings.save("Loja", "invalid",true,true,true));
        QVERIFY(!settings.save("Loja", "general",false,true,true));
        QVERIFY(settings.save("Loja", "fashion",false,false,false));
        QVERIFY(!pos.add(1));
        QVERIFY(!pos.openCash("0","Ana"));
        QVERIFY(!pos.checkout(1,"pix","","Ana"));
        QSqlDatabase::database().close();
        QVERIFY(QSqlDatabase::database().open());
        MHStore::Settings reopened;
        QCOMPARE(reopened.values().value("company").toString(),QString("Loja"));
        QVERIFY(!MHStore::Settings::enabled("pos"));
        QVERIFY(settings.save("Loja", "general",true,true,true));
        QVERIFY(pos.add(1));
        settings.hasPendingCart=[&pos] { return !pos.cart().isEmpty(); };
        QVERIFY(!settings.save("Loja", "general",false,false,false));
        pos.clearCart();
        QVERIFY(pos.openCash("0","Ana"));
        QVERIFY(!settings.save("Loja", "general",false,false,false));
        QVERIFY(settings.save("Outro nome", "market",true,true,true));
        QVERIFY(pos.closeCash(pos.cash().value("id").toInt(),"0","Ana"));
        QVERIFY(settings.save("Loja", "services",false,false,false));
        QCOMPARE(scalar("SELECT COUNT(*) FROM products").toInt(),2);
    }
    void saleAndCash() {
        MHStore::Pos pos;
        QVERIFY(pos.add(1)); QVERIFY(pos.add(1)); QVERIFY(pos.add(2));
        QCOMPARE(pos.total(),3990);
        QVERIFY(!pos.checkout(0,"cash","50","Ana"));
        QVERIFY(!pos.openCash("1.001","Ana"));
        QVERIFY(pos.openCash("100,00","Ana"));
        const int session=pos.cash().value("id").toInt();
        QVERIFY(!pos.openCash("0","Ana"));
        QVERIFY(!pos.checkout(session,"cash","39,89","Ana"));
        QVERIFY(pos.checkout(session,"cash","50","Ana"));
        QCOMPARE(pos.cart().size(),0);
        QCOMPARE(pos.cash().value("cash_expected").toLongLong(),13990);
        QCOMPARE(scalar("SELECT change_cents FROM payments").toInt(),1010);
        QCOMPARE(scalar("SELECT stock_quantity FROM products WHERE id=1").toInt(),8);
        QCOMPARE(scalar("SELECT COUNT(*) FROM inventory_movements").toInt(),2);
        QVERIFY(!pos.checkout(session,"cash","50","Ana"));
        QVERIFY(pos.add(2)); QVERIFY(pos.checkout(session,"pix","","Ana"));
        QCOMPARE(pos.cash().value("cash_expected").toLongLong(),13990);
        QVERIFY(pos.closeCash(session,"139,00","Bia"));
        QVERIFY(pos.cash().isEmpty());
        QCOMPARE(scalar("SELECT expected_cents-counted_cents FROM cash_sessions").toInt(),90);
        QVERIFY(!pos.closeCash(session,"139","Bia"));
        QVERIFY(pos.add(1));
        QVERIFY(!pos.checkout(session,"debit","","Ana"));
        QVERIFY(pos.openCash("0","Ana"));
        QVERIFY(!pos.checkout(session,"debit","","Ana"));
        QSqlDatabase::database().close();
        MHStore::Database::DatabaseManager db; QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        pos.refresh();
        QCOMPARE(pos.sessions().size(),2);
        QCOMPARE(scalar("SELECT COUNT(*) FROM sales").toInt(),2);
    }
    void atomicFailure() {
        MHStore::Pos pos;
        QVERIFY(pos.openCash("0","Ana"));
        const int session=pos.cash().value("id").toInt();
        QVERIFY(pos.add(1)); QVERIFY(pos.add(2));
        QSqlQuery q;
        QVERIFY(q.exec("CREATE TRIGGER reject_payment BEFORE INSERT ON payments BEGIN SELECT RAISE(ABORT,'Falha simulada'); END"));
        QVERIFY(!pos.checkout(session,"credit","","Ana"));
        QCOMPARE(pos.cart().size(),2);
        QCOMPARE(scalar("SELECT COUNT(*) FROM sales").toInt(),0);
        QCOMPARE(scalar("SELECT COUNT(*) FROM sale_items").toInt(),0);
        QCOMPARE(scalar("SELECT COUNT(*) FROM inventory_movements").toInt(),0);
        QCOMPARE(scalar("SELECT stock_quantity FROM products WHERE id=1").toInt(),10);
        QVERIFY(q.exec("DROP TRIGGER reject_payment"));
        QVERIFY(q.exec("UPDATE products SET stock_quantity=0 WHERE id=2"));
        QVERIFY(!pos.checkout(session,"credit","","Ana"));
        QCOMPARE(scalar("SELECT stock_quantity FROM products WHERE id=1").toInt(),10);
        QCOMPARE(scalar("SELECT COUNT(*) FROM sales").toInt(),0);
        QVERIFY(q.exec("UPDATE products SET stock_quantity=5, sale_price_cents=20 WHERE id=2"));
        QVERIFY(!pos.checkout(session,"credit","","Ana"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM sales").toInt(),0);
        QVERIFY(pos.setQuantity(2,0)); QVERIFY(pos.add(2));
        QVERIFY(pos.checkout(session,"credit","","Ana"));
    }
    void cashMovementsAndClosing() {
        MHStore::Pos pos;
        QVERIFY(pos.openCash("100,00","Ana"));
        const int session = pos.cash().value("id").toInt();
        QVERIFY(pos.moveCash(session,"supply","50,25","Troco inicial","Ana"));
        QVERIFY(pos.add(1)); QVERIFY(pos.checkout(session,"cash","20,00","Ana"));
        QVERIFY(pos.add(2)); QVERIFY(pos.checkout(session,"pix","","Ana"));
        QCOMPARE(pos.cash().value("cash_expected").toInt(),17015);
        QVERIFY(pos.moveCash(session,"withdrawal","70,15","Depósito","Bia"));
        QCOMPARE(pos.cash().value("cash_expected").toInt(),10000);
        QCOMPARE(pos.cash().value("supply_cents").toInt(),5025);
        QCOMPARE(pos.cash().value("withdrawal_cents").toInt(),7015);
        pos.selectCashHistory(session);
        QCOMPARE(pos.cashMovements().size(),2);
        auto last = pos.cashMovements().first().toMap();
        QCOMPARE(last.value("previous_cents").toInt(),17015);
        QCOMPARE(last.value("balance_cents").toInt(),10000);
        QCOMPARE(last.value("operator_name").toString(),QString("Ana"));
        QVERIFY(pos.closeCash(session,"99,50","Ana"));
        QCOMPARE(scalar("SELECT expected_cents FROM cash_sessions").toInt(),10000);
        QCOMPARE(scalar("SELECT counted_cents FROM cash_sessions").toInt(),9950);
        QVERIFY(!pos.moveCash(session,"supply","1","Após fechar","Ana"));
        QVERIFY(pos.openCash("0","Ana"));
        QVERIFY(!pos.moveCash(session,"withdrawal","1","Sessão antiga","Ana"));
        QSqlDatabase::database().close();
        MHStore::Database::DatabaseManager db; QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        pos.refresh();
        pos.selectCashHistory(session);
        QCOMPARE(pos.cashMovements().size(),2);
        pos.selectCashHistory(pos.cash().value("id").toInt());
        QCOMPARE(pos.cashMovements().size(),0);
    }
    void cashMovementValidationAndStaleBalance() {
        MHStore::Pos pos;
        QVERIFY(!pos.moveCash(999,"supply","1","Teste","Ana"));
        QVERIFY(pos.openCash("100","Ana"));
        const int session = pos.cash().value("id").toInt();
        MHStore::Pos stale;
        for (const auto &amount : {"0","-1","nan","1,001","1.000,00","1000000001"})
            QVERIFY(!pos.moveCash(session,"supply",amount,"Teste","Ana"));
        QVERIFY(!pos.moveCash(session,"invalid","1","Teste","Ana"));
        QVERIFY(!pos.moveCash(session,"supply","1"," ","Ana"));
        QVERIFY(pos.moveCash(session,"withdrawal","60","Retirada","Ana"));
        QCOMPARE(stale.cash().value("cash_expected").toInt(),10000);
        QVERIFY(!stale.moveCash(session,"withdrawal","60","Outra retirada","Bia"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM cash_movements").toInt(),1);
        QVERIFY(stale.moveCash(session,"withdrawal","40","Zerar gaveta","Bia"));
        QCOMPARE(stale.cash().value("cash_expected").toInt(),0);
        QSqlQuery q;
        QVERIFY(q.exec("CREATE TRIGGER reject_cash_movement BEFORE INSERT ON cash_movements BEGIN SELECT RAISE(ABORT,'Falha simulada'); END"));
        QVERIFY(!pos.moveCash(session,"supply","10","Teste","Ana"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM cash_movements").toInt(),2);
        QVERIFY(q.exec("DROP TRIGGER reject_cash_movement"));
        QVERIFY(pos.moveCash(session,"supply","10","Teste","Ana"));
        QCOMPARE(pos.cash().value("cash_expected").toInt(),1000);
    }
    void upgradeFromVersionThree() {
        MHStore::Pos pos;
        QVERIFY(pos.openCash("50","Ana"));
        const int session = pos.cash().value("id").toInt();
        QVERIFY(pos.add(1)); QVERIFY(pos.checkout(session,"cash","20","Ana"));
        QSqlQuery q;
        QVERIFY(removeAuthMigration());
        QVERIFY(q.exec("DROP TABLE cash_movements"));
        QVERIFY(q.exec("DROP TABLE business_settings"));
        QVERIFY(q.exec("DELETE FROM schema_migrations WHERE version=5"));
        QVERIFY(q.exec("DELETE FROM schema_migrations WHERE version=4"));
        QSqlDatabase::database().close();
        MHStore::Database::DatabaseManager db;
        QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        pos.refresh();
        QCOMPARE(pos.cash().value("cash_expected").toInt(),6990);
        QCOMPARE(scalar("SELECT COUNT(*) FROM sales").toInt(),1);
        QCOMPARE(scalar("SELECT COUNT(*) FROM schema_migrations").toInt(),20);
        QVERIFY(pos.moveCash(session,"withdrawal","9,90","Após migração","Ana"));
        QCOMPARE(pos.cash().value("cash_expected").toInt(),6000);
    }
    void persistedSalesHistory() {
        MHStore::Pos pos;
        QVERIFY(pos.openCash("0","Ana"));
        const int session = pos.cash().value("id").toInt();
        QVERIFY(pos.add(1));
        QVERIFY(pos.checkout(session,"cash","20","Ana"));
        const int saleId = scalar("SELECT id FROM sales").toInt();
        QSqlQuery q;
        QVERIFY(q.exec("UPDATE products SET name='Novo nome', sale_price_cents=5000, active=0 WHERE id=1"));
        QSqlDatabase::database().close();
        MHStore::Database::DatabaseManager db; QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        MHStore::Pos reopened;
        reopened.searchSales(QString::number(saleId));
        QCOMPARE(reopened.sales().size(),1);
        QVERIFY(reopened.loadSale(saleId));
        QCOMPARE(reopened.saleItems().first().toMap().value("product_name").toString(),QString("Produto"));
        QCOMPARE(reopened.saleItems().first().toMap().value("unit_price_cents").toInt(),1990);
        QCOMPARE(reopened.selectedSale().value("change_cents").toInt(),10);
        QCOMPARE(reopened.selectedSale().value("operator_name").toString(),QString("Ana"));
        QVERIFY(!reopened.loadSale(999));
        QVERIFY(reopened.selectedSale().isEmpty());
        QVERIFY(reopened.saleItems().isEmpty());
        reopened.searchSales("' OR 1=1 --");
        QVERIFY(reopened.sales().isEmpty());
        QVERIFY(!reopened.salesError().isEmpty());
        reopened.searchSales("999");
        QVERIFY(reopened.sales().isEmpty());
        QVERIFY(reopened.salesError().isEmpty());
    }
    void paginatedAndLegacySales() {
        QSqlQuery q;
        for (int i=0;i<52;++i) QVERIFY(q.exec("INSERT INTO sales(total_amount,total_cents) VALUES(1.25,125)"));
        MHStore::Pos pos;
        pos.searchSales();
        QVERIFY2(pos.salesError().isEmpty(), qPrintable(pos.salesError()));
        QCOMPARE(pos.sales().size(),50);
        QVERIFY(pos.moreSales());
        QCOMPARE(pos.sales().first().toMap().value("id").toInt(),52);
        pos.searchSales("",1);
        QCOMPARE(pos.sales().size(),2);
        QVERIFY(!pos.moreSales());
        QCOMPARE(pos.sales().first().toMap().value("id").toInt(),2);
        QVERIFY(pos.loadSale(1));
        QCOMPARE(pos.selectedSale().value("total_cents").toInt(),125);
        QVERIFY(pos.selectedSale().value("method").isNull());
        QVERIFY(pos.saleItems().isEmpty());
        pos.searchSales("",-1);
        QVERIFY(!pos.salesError().isEmpty());
    }
    void salesDateFilter() {
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO sales(total_cents,created_at) VALUES(1000,'2026-09-10 10:00:00'),(2000,'2026-09-20 10:00:00'),(3000,'2026-09-30 10:00:00')"));
        MHStore::Pos pos;
        pos.searchSales("",0,0,"2026-09-15","2026-09-25");
        QCOMPARE(pos.sales().size(),1);
        QCOMPARE(pos.sales().first().toMap().value("total_cents").toInt(),2000);
        pos.searchSales("",0,0,"2026-09-25","2026-09-15");
        QVERIFY(pos.sales().isEmpty());
        QVERIFY(!pos.salesError().isEmpty());
        pos.searchSales("",0,0,"2026-09-xx",{});
        QVERIFY(pos.sales().isEmpty());
        QVERIFY(!pos.salesError().isEmpty());
    }
    void fractionalSaleQuantity() {
        QSqlQuery q;
        QVERIFY(q.exec("UPDATE products SET unit='KG', stock_quantity=5, sale_price_cents=1000 WHERE id=1"));
        MHStore::Pos pos;
        QVERIFY(pos.openCash("0","Ana"));
        const int session = pos.cash().value("id").toInt();
        QVERIFY(pos.add(1));
        QVERIFY(pos.setQuantityValue(1,"1,250"));
        QCOMPARE(pos.cart().first().toMap().value("quantity").toDouble(),1.25);
        QCOMPARE(pos.cart().first().toMap().value("total_cents").toInt(),1250);
        QVERIFY(pos.checkout(session,"pix","","Ana"));
        QCOMPARE(scalar("SELECT quantity FROM sale_items WHERE sale_id=1").toDouble(),1.25);
        QCOMPARE(scalar("SELECT stock_quantity FROM products WHERE id=1").toDouble(),3.75);
        QVERIFY(!pos.setQuantityValue(1,"1,2345"));
    }
    void exportReceiptPdf() {
        MHStore::Pos pos;
        QVERIFY(pos.openCash("0","Ana"));
        const int session = pos.cash().value("id").toInt();
        QVERIFY(pos.add(1));
        QVERIFY(pos.checkout(session,"pix","","Ana"));
        const auto pdfPath = directory.filePath("comprovante.pdf");
        QVERIFY(pos.exportReceiptPdf(pdfPath));
        QFile pdf(pdfPath);
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        const auto content = pdf.readAll();
        QVERIFY(content.startsWith("%PDF-"));
        QVERIFY(content.size() > 500);
    }
    void serviceSaleDoesNotMoveStock() {
        QSqlQuery q;
        QVERIFY(q.exec("UPDATE products SET product_type='service', stock_quantity=0, name='Conserto' WHERE id=1"));
        MHStore::Pos pos;
        QVERIFY(pos.openCash("0","Ana"));
        const int session = pos.cash().value("id").toInt();
        QVERIFY(pos.add(1));
        QVERIFY(pos.checkout(session,"pix","","Ana"));
        QCOMPARE(scalar("SELECT stock_quantity FROM products WHERE id=1").toDouble(),0.0);
        QCOMPARE(scalar("SELECT COUNT(*) FROM inventory_movements").toInt(),0);
    }
    void exportSalesCsv() {
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO sales(total_cents,operator_name,created_at) VALUES(1000,'Ana; Caixa','2026-09-10 10:00:00'),(2000,'Bia','2026-09-20 10:00:00')"));
        const auto csvPath = directory.filePath("relatorio.csv");
        MHStore::Pos pos;
        QVERIFY(pos.exportSalesCsv(csvPath,"2026-09-15","2026-09-25"));
        QFile csv(csvPath);
        QVERIFY(csv.open(QIODevice::ReadOnly | QIODevice::Text));
        const auto content = QString::fromUtf8(csv.readAll());
        QVERIFY(content.startsWith("Venda;Data;Status;Total (centavos);Operador;Pagamento;Pago (centavos)\n"));
        QVERIFY(content.contains("\"Bia\""));
        QVERIFY(!content.contains("1000"));
        QVERIFY(!pos.exportSalesCsv(directory.filePath("invalido"),"2026-09-25","2026-09-15"));
        QVERIFY(!pos.exportSalesCsv("relatorio.csv"));
    }
    void exportSalesPdf() {
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO sales(total_cents,operator_name,created_at) VALUES(1000,'Ana','2026-09-10 10:00:00'),(2000,'Bia','2026-09-20 10:00:00')"));
        const auto pdfPath = directory.filePath("relatorio.pdf");
        MHStore::Pos pos;
        QVERIFY(pos.exportSalesPdf(pdfPath,"2026-09-15","2026-09-25"));
        QFile pdf(pdfPath);
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        const auto content = pdf.readAll();
        QVERIFY(content.startsWith("%PDF-"));
        QVERIFY(content.size() > 500);
        QVERIFY(!pos.exportSalesPdf(directory.filePath("invalido"),"2026-09-25","2026-09-15"));
    }
    void dashboardMetrics() {
        MHStore::Pos pos;
        pos.refreshDashboard();
        QVERIFY(pos.dashboardError().isEmpty());
        QCOMPARE(pos.dashboard().value("today_cents").toInt(),0);
        QCOMPARE(pos.dashboard().value("today_count").toInt(),0);
        QCOMPARE(pos.dashboard().value("cash_open").toInt(),0);
        QCOMPARE(pos.dashboard().value("today_cash_cents").toInt(),0);
        QCOMPARE(pos.dashboard().value("today_other_payment_cents").toInt(),0);
        QCOMPARE(pos.dashboard().value("month_cancelled_count").toInt(),0);
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO sales(total_cents) VALUES(1000),(2000)"));
        QVERIFY(q.exec("INSERT INTO sales(total_cents,status) VALUES(9000,'cancelled')"));
        QVERIFY(q.exec("INSERT INTO sales(total_cents,created_at) VALUES(700,datetime('now','localtime','start of month','-1 second','utc'))"));
        QVERIFY(q.exec("UPDATE products SET stock_quantity=0, minimum_stock=2 WHERE id=1"));
        QVERIFY(q.exec("UPDATE products SET stock_quantity=0, active=0 WHERE id=2"));
        QVERIFY(pos.openCash("100","Ana"));
        const int session=pos.cash().value("id").toInt();
        QVERIFY(pos.moveCash(session,"supply","50","Troco","Ana"));
        QVERIFY(pos.moveCash(session,"withdrawal","20","Retirada","Ana"));
        QVERIFY(q.exec("INSERT INTO payment_items(sale_id,method,amount_cents) SELECT id,'cash',total_cents FROM sales WHERE status='completed' LIMIT 1"));
        QVERIFY(q.exec("INSERT INTO payment_items(sale_id,method,amount_cents) SELECT id,'pix',total_cents FROM sales WHERE status='completed' LIMIT 1 OFFSET 1"));
        pos.refreshDashboard();
        QCOMPARE(pos.dashboard().value("today_cents").toInt(),3000);
        QCOMPARE(pos.dashboard().value("today_count").toInt(),2);
        QCOMPARE(pos.dashboard().value("month_cents").toInt(),3000);
        QCOMPARE(pos.dashboard().value("today_cash_cents").toInt(),1000);
        QCOMPARE(pos.dashboard().value("today_other_payment_cents").toInt(),2000);
        QCOMPARE(pos.dashboard().value("month_cancelled_count").toInt(),1);
        QCOMPARE(pos.dashboard().value("low_stock").toInt(),1);
        QCOMPARE(pos.dashboard().value("no_stock").toInt(),1);
        QCOMPARE(pos.dashboard().value("cash_expected").toInt(),13000);
        QVERIFY(pos.closeCash(session,"130","Ana"));
        pos.refreshDashboard();
        QCOMPARE(pos.dashboard().value("cash_open").toInt(),0);
        QCOMPARE(pos.dashboard().value("cash_expected").toInt(),0);
        QVERIFY(q.exec("ALTER TABLE sales RENAME TO unavailable_sales"));
        pos.refreshDashboard();
        QVERIFY(!pos.dashboardError().isEmpty());
        QVERIFY(pos.dashboard().isEmpty());
    }
    void customerPurchaseHistory() {
        {
            QSqlQuery q;
            QVERIFY(q.exec("INSERT INTO customers(name,active) VALUES('Inativo',0),('Sem compras',1)"));
            for (int i=0; i<52; ++i)
                QVERIFY(q.exec("INSERT INTO sales(customer_id,total_cents,created_at) VALUES(1,125,'2026-09-10 12:00:00')"));
            QVERIFY(q.exec("INSERT INTO sales(customer_id,total_cents,status,created_at) VALUES(1,90000,'cancelled','2026-09-11 12:00:00')"));
            QVERIFY(q.exec("INSERT INTO sales(total_cents) VALUES(99999)"));
            QVERIFY(q.exec("INSERT INTO sales(customer_id,total_cents,status) VALUES(2,800,'cancelled')"));
        }
        QSqlDatabase::database().close();
        QVERIFY(QSqlDatabase::database().open());
        MHStore::Pos pos;
        pos.searchSales("",0,1);
        QVERIFY(pos.salesError().isEmpty());
        QCOMPARE(pos.sales().size(),50);
        QVERIFY(pos.moreSales());
        QCOMPARE(pos.customerSummary().value("purchase_count").toInt(),52);
        QCOMPARE(pos.customerSummary().value("spent_cents").toLongLong(),6500);
        QCOMPARE(pos.customerSummary().value("active").toInt(),0);
        QCOMPARE(pos.customerSummary().value("last_purchase"),scalar("SELECT datetime('2026-09-10 12:00:00','localtime')"));
        pos.searchSales("",1,1);
        QCOMPARE(pos.sales().size(),2);
        QVERIFY(!pos.moreSales());
        QCOMPARE(pos.customerSummary().value("spent_cents").toLongLong(),6500);
        pos.searchSales("1",0,1);
        QCOMPARE(pos.sales().size(),1);
        QCOMPARE(pos.customerSummary().value("purchase_count").toInt(),52);
        QVERIFY(pos.loadSale(1));
        QCOMPARE(pos.selectedSale().value("customer_id").toInt(),1);
        pos.searchSales("54",0,1);
        QVERIFY(pos.sales().isEmpty()); // Anonymous sale must not match.
        pos.searchSales("",0,2);
        QVERIFY(pos.sales().isEmpty());
        QCOMPARE(pos.customerSummary().value("spent_cents").toInt(),0);
        QCOMPARE(pos.customerSummary().value("purchase_count").toInt(),0);
        QVERIFY(pos.customerSummary().value("last_purchase").isNull());
        pos.searchSales("",0,999);
        QVERIFY(!pos.salesError().isEmpty());
        QVERIFY(pos.customerSummary().isEmpty());
        pos.searchSales("",0,-1);
        QVERIFY(!pos.salesError().isEmpty());
        pos.searchSales();
        QVERIFY(pos.salesError().isEmpty());
        QVERIFY(pos.customerSummary().isEmpty());
        QCOMPARE(pos.sales().size(),50);
        QSqlQuery q;
        QVERIFY(q.exec("ALTER TABLE sales RENAME TO unavailable_sales"));
        pos.searchSales("",0,1);
        QVERIFY(!pos.salesError().isEmpty());
        QVERIFY(pos.customerSummary().isEmpty());
        QVERIFY(pos.sales().isEmpty());
    }
    void customerOnSale() {
        MHStore::Pos pos;
        QSqlQuery q;
        QVERIFY(q.exec("INSERT INTO customers(name) VALUES('Ana Cliente')"));
        QVERIFY(q.exec("INSERT INTO customers(name,active) VALUES('Inativo',0)"));
        pos.refreshCustomers();
        QCOMPARE(pos.customers().size(),2);
        QVERIFY(pos.openCash("0","Operador"));
        const int session = pos.cash().value("id").toInt();
        QVERIFY(pos.add(1));
        QVERIFY(!pos.checkout(session,"pix","","Operador",999));
        QVERIFY(!pos.checkout(session,"pix","","Operador",2));
        QVERIFY(!pos.checkout(session,"pix","","Operador",-1));
        QCOMPARE(scalar("SELECT COUNT(*) FROM sales").toInt(),0);
        QCOMPARE(pos.cart().size(),1);
        QVERIFY(q.exec("UPDATE customers SET active=0 WHERE id=1"));
        QVERIFY(!pos.checkout(session,"pix","","Operador",1));
        QCOMPARE(scalar("SELECT stock_quantity FROM products WHERE id=1").toInt(),10);
        QVERIFY(q.exec("UPDATE customers SET active=1 WHERE id=1"));
        QVERIFY(pos.checkout(session,"pix","","Operador",1));
        QVERIFY(pos.receipt().contains("Ana Cliente"));
        QCOMPARE(scalar("SELECT customer_id FROM sales WHERE id=1").toInt(),1);
        QVERIFY(pos.add(1));
        QVERIFY(pos.checkout(session,"pix","","Operador"));
        QVERIFY(scalar("SELECT customer_id FROM sales WHERE id=2").isNull());
        QVERIFY(q.exec("UPDATE customers SET name='Nome atualizado',active=0 WHERE id=1"));
        QSqlDatabase::database().close();
        MHStore::Database::DatabaseManager db; QVERIFY(db.initialize(nullptr,path));
        QVERIFY(authenticateTestAdmin());
        QVERIFY(pos.loadSale(1));
        QCOMPARE(pos.selectedSale().value("customer_name").toString(),QString("Nome atualizado"));
        QCOMPARE(pos.selectedSale().value("customer_id").toInt(),1);
        QVERIFY(pos.loadSale(2));
        QVERIFY(pos.selectedSale().value("customer_id").isNull());
    }
    void validation() {
        MHStore::Pos pos;
        QVERIFY(!pos.openCash("-1","Ana"));
        QVERIFY(!pos.add(999)); QVERIFY(pos.add(1));
        QVERIFY(!pos.setQuantity(1,11)); QVERIFY(!pos.setQuantity(1,-1));
        QVERIFY(pos.openCash("0","Ana"));
        const int session=pos.cash().value("id").toInt();
        QVERIFY(!pos.checkout(session,"invalid","0","Ana"));
        QVERIFY(!pos.checkout(session,"cash","nan","Ana"));
        QSqlQuery q; QVERIFY(q.exec("UPDATE products SET active=0 WHERE id=1"));
        QVERIFY(!pos.checkout(session,"pix","","Ana"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM sales").toInt(),0);
    }
};
QTEST_MAIN(PosTest)
#include "pos_test.moc"
