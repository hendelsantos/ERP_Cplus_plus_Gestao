import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var auth
    property bool recovering: false
    spacing: 12
    Label { text: page.auth.needsSetup ? "Criar administrador" : page.recovering ? "Recuperar acesso" : "Entrar no MH Store"; font.pixelSize: 24; font.bold: true }
    Label { text: "Acesso local • Funciona sem internet" }
    TextField { id: name; objectName: "authName"; visible: page.auth.needsSetup; placeholderText: "Nome do administrador"; Layout.fillWidth: true; maximumLength: 120 }
    TextField { id: login; objectName: "authLogin"; placeholderText: "Login"; Layout.fillWidth: true; maximumLength: 40 }
    TextField { id: code; objectName: "authCode"; visible: page.recovering; placeholderText: "Código de recuperação"; echoMode: TextInput.Password; Layout.fillWidth: true; maximumLength: 64 }
    TextField { id: password; objectName: "authPassword"; placeholderText: page.auth.needsSetup || page.recovering ? "Nova senha (12 a 128 caracteres)" : "Senha"; echoMode: TextInput.Password; Layout.fillWidth: true; maximumLength: 128 }
    TextField { id: confirmation; objectName: "authConfirm"; visible: page.auth.needsSetup || page.recovering; placeholderText: "Confirme a senha"; echoMode: TextInput.Password; Layout.fillWidth: true; maximumLength: 128 }
    Label { id: localError; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#b42318" }
    Button {
        text: page.auth.needsSetup ? "Criar administrador" : page.recovering ? "Redefinir senha" : "Entrar"
        enabled: !page.auth.recoveryCode.length
        onClicked: {
            localError.text = ""
            if ((page.auth.needsSetup || page.recovering) && password.text !== confirmation.text) { localError.text = "As senhas não coincidem."; return }
            var ok = page.auth.needsSetup ? page.auth.setup(name.text,login.text,password.text)
                : page.recovering ? page.auth.recover(login.text,code.text,password.text) : page.auth.login(login.text,password.text)
            password.clear(); confirmation.clear(); code.clear()
            if (ok) page.recovering = false
        }
    }
    Button { visible: !page.auth.needsSetup && !page.auth.recoveryCode.length; text: page.recovering ? "Voltar ao login" : "Esqueci minha senha"; onClicked: { page.recovering = !page.recovering; password.clear(); confirmation.clear(); code.clear() } }
    Label { text: page.auth.message; Layout.fillWidth: true; wrapMode: Text.Wrap }
    Label { visible: page.auth.recoveryCode.length > 0; text: "Guarde este código fora do computador. Ele permite redefinir a senha do administrador e será exibido somente agora."; Layout.fillWidth: true; wrapMode: Text.Wrap }
    TextArea { visible: page.auth.recoveryCode.length > 0; text: page.auth.recoveryCode; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; Layout.fillWidth: true }
    Button { visible: page.auth.recoveryCode.length > 0; text: "Guardei o código"; onClicked: page.auth.dismissRecovery() }
}
