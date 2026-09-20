#include "auth.h"
#include "../audit/audit.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QRegularExpression>
#include <QDateTime>
#include <QCryptographicHash>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
namespace MHStore {
namespace {
int sessionId=0, sessionVersion=0;
QString sessionDatabase;
constexpr int iterations=600000;
QByteArray randomBytes(int size) {
    QByteArray result(size,0);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(result.data()),size)!=1) return {};
    return result;
}
QByteArray derive(const QString &password,const QByteArray &salt,int rounds) {
    if (rounds<600000 || rounds>2000000 || salt.size()!=16) return {};
    const auto input=password.toUtf8();
    QByteArray result(32,0);
    if (PKCS5_PBKDF2_HMAC(input.constData(),input.size(),reinterpret_cast<const unsigned char *>(salt.constData()),salt.size(),rounds,EVP_sha256(),result.size(),reinterpret_cast<unsigned char *>(result.data()))!=1) return {};
    return result;
}
bool equal(const QByteArray &a,const QByteArray &b) { return a.size()==32 && b.size()==32 && CRYPTO_memcmp(a.constData(),b.constData(),32)==0; }
bool validPassword(const QString &p) { return p.size()>=12 && p.size()<=128 && !p.trimmed().isEmpty(); }
bool validIdentity(const QString &name,const QString &login) {
    return !name.trimmed().isEmpty() && name.trimmed().size()<=120 && QRegularExpression("^[a-z0-9._-]{3,40}$").match(login).hasMatch();
}
class Transaction {
public:
    bool active=false;
    bool begin() { QSqlQuery q; active=q.exec("BEGIN IMMEDIATE"); return active; }
    bool commit() { if (!QSqlDatabase::database().commit()) return false; active=false; return true; }
    ~Transaction() { if(active) QSqlDatabase::database().rollback(); }
};
}
void Auth::resetSession() { sessionId=0; sessionVersion=0; sessionDatabase.clear(); }
QVariantMap Auth::current() {
    if (!sessionId || !QSqlDatabase::database().isOpen() || QSqlDatabase::database().databaseName()!=sessionDatabase) return {};
    QSqlQuery q; q.prepare("SELECT id,name,login,role FROM users WHERE id=? AND active=1 AND session_version=?");
    q.addBindValue(sessionId); q.addBindValue(sessionVersion);
    if (!q.exec() || !q.next()) return {};
    return {{"id",q.value(0)},{"name",q.value(1)},{"login",q.value(2)},{"role",q.value(3)}};
}
bool Auth::allowed(const QString &permission) {
    const auto u=current();
    if (u.isEmpty()) return false;
    const bool admin=u.value("role")=="admin", op=u.value("role")=="operator";
    for (const auto &p : permissions())
        if (p.id==permission) return admin ? p.admin : (op && p.operatorRole);
    return false;
}
const QList<Auth::PermissionInfo> &Auth::permissions() {
    static const QList<PermissionInfo> matrix={
        {"read","Consultar cadastros, estoque, vendas, clientes, caixa e painel",true,true},
        {"catalog","Criar, editar e ativar/inativar produtos, categorias e clientes",true,false},
        {"inventory","Movimentar estoque manualmente: entrada, saída e ajuste",true,false},
        {"cash","Abrir e fechar caixa, registrar suprimento e sangria",true,true},
        {"pos","Operar o PDV e finalizar vendas",true,true},
        {"settings","Configurar empresa e habilitar módulos",true,false},
        {"backup","Criar e restaurar backups locais",true,false},
        {"users","Gerenciar usuários e emitir código de recuperação",true,false},
        {"audit","Consultar o registro de auditoria de alterações administrativas",true,false},
    };
    return matrix;
}
bool Auth::needsSetup() const { QSqlQuery q("SELECT COUNT(*) FROM users"); return q.next() && q.value(0).toInt()==0; }
bool Auth::fail(const QString &message) { m_message=message; emit changed(); return false; }
bool Auth::setup(const QString &name,const QString &login,const QString &password) {
    const auto normalized=login.trimmed().toLower();
    if (!validIdentity(name,normalized) || !validPassword(password)) return fail("Informe nome, login de 3 a 40 caracteres (letras, números, ponto, _ ou -) e senha de 12 a 128 caracteres.");
    const auto salt=randomBytes(16), token=randomBytes(32);
    const auto hash=derive(password,salt,iterations);
    if (hash.isEmpty() || token.isEmpty()) return fail("Falha ao gerar credenciais seguras.");
    const auto code=QString::fromLatin1(token.toHex());
    Transaction tx; if (!tx.begin()) return fail("Banco ocupado.");
    if (!needsSetup()) return fail("O administrador inicial já foi criado.");
    QSqlQuery q; q.prepare("INSERT INTO users(name,login,password_hash,salt,iterations,role,recovery_hash) VALUES(?,?,?,?,?,'admin',?)");
    for (const auto &v: QVariantList{name.trimmed(),normalized,hash,salt,iterations,QCryptographicHash::hash(code.toUtf8(),QCryptographicHash::Sha256)}) q.addBindValue(v);
    if (!q.exec() || !tx.commit()) return fail("Não foi possível criar o administrador.");
    m_recovery=code; m_message="Administrador criado. Guarde o código de recuperação em local seguro e faça login."; emit changed(); return true;
}
bool Auth::login(const QString &login,const QString &password) {
    if (authenticated()) return fail("Saia da sessão atual antes de entrar com outro usuário.");
    if (password.size()>128) return fail("Login ou senha inválidos.");
    Transaction tx; if (!tx.begin()) return fail("Banco ocupado.");
    QSqlQuery q; q.prepare("SELECT id,password_hash,salt,iterations,active,session_version,locked_until,failed_attempts FROM users WHERE login=?");
    q.addBindValue(login.trimmed().toLower());
    if (!q.exec()) return fail("Não foi possível consultar os usuários.");
    const bool found=q.next();
    const auto now=QDateTime::currentSecsSinceEpoch();
    const int id=found?q.value(0).toInt():0, version=found?q.value(5).toInt():0;
    if (found && q.value(6).toLongLong()>now) return fail("Acesso temporariamente bloqueado. Aguarde alguns minutos.");
    const auto hash=derive(password,found?q.value(2).toByteArray():QByteArray(16,'x'),found?q.value(3).toInt():iterations);
    const bool valid=found && q.value(4).toBool() && equal(hash,q.value(1).toByteArray());
    const int attempts=found?q.value(7).toInt()+1:0;
    q.finish();
    if (!valid) {
        if (found) {
            q.prepare("UPDATE users SET failed_attempts=?,locked_until=? WHERE id=?");
            q.addBindValue(attempts>=5?0:attempts); q.addBindValue(attempts>=5?now+300:0); q.addBindValue(id);
            if (!q.exec()) return fail("Não foi possível registrar a tentativa.");
        }
        if (!tx.commit()) return fail("Não foi possível validar o acesso.");
        return fail("Login ou senha inválidos.");
    }
    q.prepare("UPDATE users SET failed_attempts=0,locked_until=0 WHERE id=?"); q.addBindValue(id);
    if (!q.exec() || !tx.commit()) return fail("Não foi possível iniciar a sessão.");
    sessionId=id; sessionVersion=version; sessionDatabase=QSqlDatabase::database().databaseName();
    m_message.clear(); m_recovery.clear(); emit changed(); return true;
}
bool Auth::changePassword(const QString &currentPassword,const QString &newPassword,const QString &confirmation) {
    if (!authenticated()) return fail("Entre para alterar sua senha.");
    if (newPassword!=confirmation) return fail("As senhas não coincidem.");
    if (!validPassword(newPassword)) return fail("A nova senha deve ter de 12 a 128 caracteres.");
    if (currentPassword.size()>128) return fail("Senha atual inválida.");
    Transaction tx; if (!tx.begin()) return fail("Banco ocupado.");
    QSqlQuery q;
    q.prepare("SELECT password_hash,salt,iterations,failed_attempts,locked_until FROM users WHERE id=? AND active=1 AND session_version=?");
    q.addBindValue(sessionId); q.addBindValue(sessionVersion);
    if (!q.exec() || !q.next()) return fail("Sessão inválida. Entre novamente.");
    const auto now=QDateTime::currentSecsSinceEpoch();
    if (q.value(4).toLongLong()>now) return fail("Alteração temporariamente bloqueada. Aguarde alguns minutos.");
    const bool valid=equal(derive(currentPassword,q.value(1).toByteArray(),q.value(2).toInt()),q.value(0).toByteArray());
    const int attempts=q.value(3).toInt()+1;
    q.finish();
    if (!valid) {
        q.prepare("UPDATE users SET failed_attempts=?,locked_until=? WHERE id=?");
        q.addBindValue(attempts>=5?0:attempts); q.addBindValue(attempts>=5?now+300:0); q.addBindValue(sessionId);
        if (!q.exec() || !tx.commit()) return fail("Não foi possível registrar a tentativa.");
        return fail("Senha atual inválida.");
    }
    if (currentPassword==newPassword) return fail("Escolha uma senha diferente da atual.");
    const auto salt=randomBytes(16), hash=derive(newPassword,salt,iterations);
    if (hash.isEmpty()) return fail("Falha ao gerar senha segura.");
    // Record while the session still matches the stored version; the update below
    // bumps it and would invalidate the in-memory session for the audit lookup.
    if (!Audit::record("user.password",current().value("login").toString(),"senha alterada pelo próprio usuário na sessão"))
        return fail("Falha ao registrar a auditoria da alteração.");
    // Preserve recovery credentials; only this process adopts the new session version.
    q.prepare("UPDATE users SET password_hash=?,salt=?,iterations=?,session_version=session_version+1,failed_attempts=0,locked_until=0 WHERE id=? AND session_version=? AND active=1");
    for (const auto &v : QVariantList{hash,salt,iterations,sessionId,sessionVersion}) q.addBindValue(v);
    if (!q.exec() || q.numRowsAffected()!=1 || !tx.commit()) return fail("Não foi possível alterar a senha.");
    ++sessionVersion;
    m_message="Senha alterada. Sua sessão foi mantida; sessões anteriores foram invalidadas. O código de recuperação existente permanece válido.";
    emit changed(); return true;
}
bool Auth::logout() {
    if (hasPendingCart && hasPendingCart()) return fail("Finalize ou limpe o carrinho antes de sair.");
    resetSession(); m_recovery.clear(); m_message.clear(); emit changed(); return true;
}
QVariantList Auth::users() const {
    QVariantList result; if (!allowed("users")) return result;
    QSqlQuery q("SELECT id,name,login,role,active FROM users ORDER BY name COLLATE NOCASE");
    while(q.next()) result.append(QVariantMap{{"id",q.value(0)},{"name",q.value(1)},{"login",q.value(2)},{"role",q.value(3)},{"active",q.value(4)}});
    return result;
}
bool Auth::saveUser(int id,const QString &name,const QString &login,const QString &password,const QString &role,bool active) {
    if (!allowed("users")) return fail("Sem permissão para gerenciar usuários.");
    const auto normalized=login.trimmed().toLower();
    if (id<0 || !validIdentity(name,normalized) || !QStringList{"admin","operator"}.contains(role) || ((id==0 || !password.isEmpty()) && !validPassword(password)))
        return fail("Confira nome, login, perfil e senha (12 a 128 caracteres para nova senha).");
    if (id==userId()) return fail("Use outro administrador para alterar seu cadastro.");
    QByteArray salt,hash;
    if (!password.isEmpty()) { salt=randomBytes(16); hash=derive(password,salt,iterations); if (hash.isEmpty()) return fail("Falha ao gerar senha segura."); }
    Transaction tx; if(!tx.begin()) return fail("Banco ocupado.");
    QSqlQuery q;
    if(id>0) {
        q.prepare("SELECT role,active FROM users WHERE id=?"); q.addBindValue(id);
        if(!q.exec() || !q.next()) return fail("Usuário não encontrado.");
        if(q.value(0)=="admin" && q.value(1).toBool() && (role!="admin" || !active)) {
            q.finish();
            if(!q.exec("SELECT COUNT(*) FROM users WHERE role='admin' AND active=1") || !q.next() || q.value(0).toInt()<=1) return fail("Mantenha pelo menos um administrador ativo.");
        }
    }
    q.finish();
    if (id==0) q.prepare("INSERT INTO users(name,login,role,active,password_hash,salt,iterations) VALUES(?,?,?,?,?,?,?)");
    else q.prepare("UPDATE users SET name=?,login=?,role=?,active=?,session_version=session_version+1" + QString(password.isEmpty()?"":",password_hash=?,salt=?,iterations=?,failed_attempts=0,locked_until=0") + ",recovery_hash=NULL WHERE id=?");
    for (const auto &v: QVariantList{name.trimmed(),normalized,role,active}) q.addBindValue(v);
    if (id==0 || !password.isEmpty()) { q.addBindValue(hash); q.addBindValue(salt); q.addBindValue(iterations); }
    if(id>0) q.addBindValue(id);
    if(!q.exec()) return fail("Não foi possível salvar. Verifique se o login já existe.");
    const QString details=QString("nome=%1; login=%2; perfil=%3; ativo=%4%5").arg(name.trimmed(),normalized,role,active?"sim":"não",
        (id==0 || !password.isEmpty())?"; senha definida":QString());
    if (!Audit::record(id==0?"user.create":"user.update",normalized,details))
        return fail("Falha ao registrar a auditoria da alteração.");
    if(!tx.commit()) return fail("Não foi possível salvar. Verifique se o login já existe.");
    m_message="Usuário salvo."; emit changed(); return true;
}
bool Auth::recover(const QString &login,const QString &code,const QString &password) {
    if(authenticated()) return fail("Saia da sessão antes de recuperar o acesso.");
    if(!validPassword(password) || !QRegularExpression("^[0-9a-f]{64}$").match(code.trimmed()).hasMatch()) return fail("Código ou nova senha inválidos.");
    const auto salt=randomBytes(16), token=randomBytes(32), hash=derive(password,salt,iterations);
    if(hash.isEmpty() || token.isEmpty()) return fail("Falha ao gerar credenciais seguras.");
    Transaction tx; if(!tx.begin()) return fail("Banco ocupado.");
    QSqlQuery q; q.prepare("SELECT id,recovery_hash FROM users WHERE login=? AND role='admin' AND active=1"); q.addBindValue(login.trimmed().toLower());
    if(!q.exec() || !q.next() || !equal(q.value(1).toByteArray(),QCryptographicHash::hash(code.trimmed().toUtf8(),QCryptographicHash::Sha256))) return fail("Código ou login inválidos.");
    const int id=q.value(0).toInt(); q.finish();
    const auto replacement=QString::fromLatin1(token.toHex());
    q.prepare("UPDATE users SET password_hash=?,salt=?,iterations=?,session_version=session_version+1,failed_attempts=0,locked_until=0,recovery_hash=? WHERE id=?");
    for(const auto &v: QVariantList{hash,salt,iterations,QCryptographicHash::hash(replacement.toUtf8(),QCryptographicHash::Sha256),id}) q.addBindValue(v);
    if(!q.exec() || !tx.commit()) return fail("Não foi possível recuperar o acesso.");
    m_recovery=replacement; m_message="Senha redefinida. Guarde o novo código; o anterior foi invalidado."; emit changed(); return true;
}
bool Auth::issueRecoveryCode(int id) {
    if (!allowed("users")) return fail("Sem permissão para gerar código de recuperação.");
    if (id==userId()) return fail("Para sua conta, guarde o código recebido no primeiro acesso ou troque a senha.");
    const auto token=randomBytes(32);
    if (token.isEmpty()) return fail("Falha ao gerar código seguro.");
    const auto code=QString::fromLatin1(token.toHex());
    Transaction tx; if (!tx.begin()) return fail("Banco ocupado.");
    QSqlQuery q;
    q.prepare("SELECT login FROM users WHERE id=? AND role='admin' AND active=1");
    q.addBindValue(id);
    QString target;
    if (q.exec() && q.next()) target=q.value(0).toString();
    q.finish();
    if (target.isEmpty()) return fail("Código não gerado. Confirme um administrador ativo que não seja você.");
    q.prepare("UPDATE users SET recovery_hash=? WHERE id=? AND role='admin' AND active=1");
    q.addBindValue(QCryptographicHash::hash(code.toUtf8(),QCryptographicHash::Sha256)); q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected()!=1) return fail("Código não gerado. Confirme um administrador ativo que não seja você.");
    if (!Audit::record("user.recovery_code",target,"código de recuperação emitido por administrador; o anterior foi invalidado"))
        return fail("Falha ao registrar a auditoria da alteração.");
    if (!tx.commit()) return fail("Código não gerado. Confirme um administrador ativo que não seja você.");
    m_recovery=code; m_message="Código de recuperação gerado. O anterior foi invalidado; entregue-o ao administrador por canal seguro.";
    emit changed(); return true;
}
}
