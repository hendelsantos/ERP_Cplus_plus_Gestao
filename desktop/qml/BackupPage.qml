import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var backup
    spacing: 12
    Label { text: "Backup local"; font.bold: true; font.pixelSize: 22 }
    Label { text: "Escolha uma pasta para guardar cópias do banco. Para proteção contra falha do disco, use também outra unidade."; wrapMode: Text.Wrap; Layout.fillWidth: true }
    RowLayout {
        Layout.fillWidth: true
        TextField { id: folder; objectName: "backupFolder"; text: page.backup.folder; placeholderText: "Caminho completo da pasta"; Layout.fillWidth: true }
        Button { text: "Criar backup"; onClicked: page.backup.create(folder.text) }
        Button { text: "Listar arquivos"; onClicked: page.backup.list(folder.text) }
    }
    Label { text: page.backup.message; wrapMode: Text.Wrap; Layout.fillWidth: true }
    Label { text: "Arquivos disponíveis na pasta • Nome, data de modificação e tamanho"; color: "#6d7781"; Layout.fillWidth: true; wrapMode: Text.Wrap }
    ListView {
        Layout.fillHeight: true
        Layout.fillWidth: true
        clip: true
        spacing: 8
        model: page.backup.files
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width
            height: 82
            color: "white"
            radius: 6
            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: modelData.name; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    Label { text: modelData.date + " • " + Math.ceil(modelData.size/1024) + " KB"; color: "#6d7781" }
                }
                Button { text: "Selecionar"; onClicked: restoreFile.text = modelData.path }
            }
        }
        Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Nenhum backup .mhb nesta pasta." }
    }
    Label { text: "Restaurar um backup"; font.bold: true }
    RowLayout {
        Layout.fillWidth: true
        TextField { id: restoreFile; objectName: "restoreFile"; placeholderText: "Caminho completo do arquivo .mhb"; Layout.fillWidth: true }
        Button { text: "Restaurar backup"; enabled: restoreFile.text.length > 0; onClicked: { confirmation.file = restoreFile.text; confirmation.open() } }
    }
    Label { text: "Restauração substitui os dados atuais. Feche o caixa e limpe o carrinho. Backups devem ser compatíveis com esta versão."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#6d7781" }
    Dialog {
        id: confirmation
        property string file: ""
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(560,parent.width-32)
        modal: true
        title: "Confirmar restauração"
        contentItem: Label {
            wrapMode: Text.Wrap
            text: "Restaurar este arquivo?\n\n" + confirmation.file + "\n\nOs dados atuais serão substituídos. Antes, será guardada uma cópia na pasta backups do aplicativo. Após restaurar, o aplicativo fechará; abra-o novamente."
        }
        footer: DialogButtonBox {
            Button { text: "Cancelar restauração"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Confirmar e restaurar"; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: { confirmation.close(); page.backup.restore(confirmation.file) } }
            onRejected: confirmation.close()
        }
    }
}
