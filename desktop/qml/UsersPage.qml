import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var auth
    onVisibleChanged: if (!visible) edit({})
    spacing: 8
    property int editingId: 0
    function edit(row) {
        editingId = row.id || 0
        name.text = row.name || ""
        login.text = row.login || ""
        password.clear()
        role.currentIndex = row.role === "admin" ? 1 : 0
        active.checked = row.id ? !!row.active : true
    }
    Label { text: "Administradores gerenciam o sistema. Operadores consultam dados e operam PDV/Caixa; não alteram cadastros, estoque manual, configurações, backups ou usuários."; Layout.fillWidth: true; wrapMode: Text.Wrap }
    RowLayout {
        TextField { id: name; placeholderText: "Nome"; Layout.fillWidth: true; maximumLength: 120 }
        TextField { id: login; placeholderText: "Login"; Layout.fillWidth: true; maximumLength: 40 }
    }
    TextField { id: password; placeholderText: page.editingId ? "Nova senha (vazio mantém a atual)" : "Senha (12 a 128 caracteres)"; echoMode: TextInput.Password; maximumLength: 128; Layout.fillWidth: true }
    RowLayout {
        ComboBox { id: role; model: ["Operador de caixa","Administrador"] }
        CheckBox { id: active; text: "Ativo"; checked: true }
        Button { text: page.editingId ? "Salvar usuário" : "Criar usuário"; onClicked: { if(page.auth.saveUser(page.editingId,name.text,login.text,password.text,role.currentIndex ? "admin" : "operator",active.checked)) page.edit({}); password.clear() } }
        Button { text: "Limpar"; onClicked: page.edit({}) }
    }
    Label { text: page.auth.message; Layout.fillWidth: true; wrapMode: Text.Wrap }
    ListView {
        Layout.fillHeight: true; Layout.fillWidth: true; clip: true
        model: page.auth.users
        delegate: ItemDelegate {
            required property var modelData
            width: ListView.view.width
            text: modelData.name + " • " + modelData.login + " • " + (modelData.role === "admin" ? "Administrador" : "Operador") + (modelData.active ? "" : " • Inativo")
            enabled: modelData.id !== page.auth.user.id
            onClicked: page.edit(modelData)
        }
        ScrollBar.vertical: ScrollBar {}
    }
    Label { text: "Seu próprio cadastro deve ser alterado por outro administrador. Redefinir/editar um usuário invalida suas sessões e seu código de recuperação."; Layout.fillWidth: true; wrapMode: Text.Wrap }
}
