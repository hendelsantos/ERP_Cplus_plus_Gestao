import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var audit
    spacing: 8
    Label { text: "Alterações administrativas registradas no momento em que ocorreram: usuários, senhas, códigos de recuperação e configurações. Senhas e códigos nunca são gravados. Operações de venda, estoque e caixa já identificam o responsável em seus próprios registros."; Layout.fillWidth: true; wrapMode: Text.Wrap }
    RowLayout {
        Button { text: "Atualizar"; onClicked: page.audit.refresh() }
        Item { Layout.fillWidth: true }
    }
    ListView {
        Layout.fillHeight: true; Layout.fillWidth: true; clip: true
        model: page.audit.entries
        delegate: ColumnLayout {
            required property var modelData
            width: ListView.view.width
            spacing: 2
            Label { text: modelData.created_at + " • " + modelData.action_label; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Label { text: "Responsável: " + modelData.user_name + " • Alvo: " + modelData.target; color: "#6d7781"; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Label { text: modelData.details; color: "#6d7781"; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Rectangle { height: 1; Layout.fillWidth: true; color: "#e3e7ea" }
        }
        ScrollBar.vertical: ScrollBar {}
        Label {
            anchors.centerIn: parent
            visible: page.audit.entries.length === 0
            text: page.audit.message.length > 0 ? page.audit.message : "Nenhuma alteração administrativa registrada."
            color: "#6d7781"
        }
    }
    Button { visible: page.audit.moreAvailable; text: "Carregar mais"; onClicked: page.audit.loadMore() }
}
