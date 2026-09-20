import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var inventory
    property bool ready: false
    property int selectedId: 0
    property string selectedName: ""
    spacing: 12
    function reload() {
        if (ready) inventory.refresh(searchField.text, critical.checked)
    }
    Component.onCompleted: { ready = true; reload() }
    onVisibleChanged: { if (visible) reload() }
    RowLayout {
        Layout.fillWidth: true
        TextField { id: searchField; Layout.fillWidth: true; placeholderText: "Buscar produto, código ou código de barras"; onTextChanged: page.reload() }
        CheckBox { id: critical; text: "Estoque crítico"; onToggled: page.reload() }
    }
    Label { text: "Selecione um produto para consultar seu histórico ou registrar uma movimentação."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#6d7781" }
    Label { visible: page.inventory.error.length > 0 && !movement.opened; text: page.inventory.error; color: "#b42318"; wrapMode: Text.Wrap; Layout.fillWidth: true }
    ListView {
        id: productsList
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 120
        clip: true
        spacing: 6
        model: page.inventory.products
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width
            height: 78
            radius: 6
            color: modelData.id === page.selectedId ? "#dff5f7" : "white"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: modelData.code + " — " + modelData.name + (modelData.active ? "" : " • Inativo"); font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label {
                        text: "Saldo: " + Number(modelData.stock_quantity).toLocaleString(Qt.locale("pt_BR"), 'f', 3)
                            + "  |  Mínimo: " + modelData.minimum_stock
                            + (modelData.stock_quantity <= modelData.minimum_stock ? "  • Estoque crítico" : "")
                        color: modelData.stock_quantity <= modelData.minimum_stock ? "#b54708" : "#6d7781"
                    }
                }
                Button { text: "Histórico"; onClicked: { page.selectedId = modelData.id; page.selectedName = modelData.name; page.inventory.selectProduct(modelData.id) } }
                Button { text: "Movimentar"; enabled: !!modelData.active; onClicked: movement.openProduct(modelData) }
            }
        }
        Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Nenhum produto encontrado." }
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: "Histórico — " + (page.selectedId ? page.selectedName : "Todos os produtos"); font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
        Button { text: "Ver todos"; onClicked: { page.selectedId = 0; page.inventory.selectProduct(0) } }
    }
    Label { text: "Últimas 200 movimentações • Horário local • Responsável informado manualmente"; color: "#6d7781"; wrapMode: Text.Wrap; Layout.fillWidth: true }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 100
        clip: true
        spacing: 6
        model: page.inventory.history
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width
            height: historyText.implicitHeight + 20
            color: "white"
            radius: 6
            Label {
                id: historyText
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                wrapMode: Text.Wrap
                text: modelData.local_created_at + "  |  " + modelData.product_code + " — " + modelData.product_name
                    + "\n" + ({entry: "Entrada", exit: "Saída", adjustment: "Ajuste"})[modelData.type]
                    + ": " + (modelData.quantity > 0 ? "+" : "") + modelData.quantity
                    + "  |  Saldo: " + modelData.previous_balance + " → " + modelData.balance
                    + "\n" + modelData.operator_name + " — " + modelData.reason
            }
        }
        Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Nenhuma movimentação registrada." }
    }
    Dialog {
        id: movement
        property int productId: 0
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(540, parent.width - 32)
        height: Math.min(570, parent.height - 32)
        modal: true
        title: "Movimentar estoque"
        function openProduct(product) {
            productId = product.id
            productLabel.text = product.code + " — " + product.name + "\nSaldo atual: " + product.stock_quantity
            typeField.currentIndex = 0
            quantityField.text = ""
            reasonField.text = ""
            errorLabel.text = ""
            open()
            quantityField.forceActiveFocus()
        }
        contentItem: ScrollView {
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: parent.width
                Label { id: productLabel; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true }
                Label { text: "Tipo de movimentação" }
                ComboBox { id: typeField; Layout.fillWidth: true; model: ["Entrada", "Saída", "Ajuste por contagem"] }
                Label { text: typeField.currentIndex === 2 ? "Novo saldo contado (pode ser zero)" : "Quantidade a movimentar" }
                TextField { id: quantityField; objectName: "movementQuantity"; Layout.fillWidth: true; placeholderText: "Ex.: 2 ou 1,250"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                Label { text: "Motivo *" }
                TextField { id: reasonField; objectName: "movementReason"; Layout.fillWidth: true; placeholderText: "Ex.: recebimento de mercadoria" }
                Label { text: "Responsável *" }
                TextField { id: operatorField; objectName: "movementOperator"; Layout.fillWidth: true; placeholderText: "Nome de quem realizou a movimentação" }
                Label { text: "Identificação manual; login e permissões ainda não disponíveis."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#6d7781" }
                Label { id: errorLabel; color: "#b42318"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            }
        }
        footer: DialogButtonBox {
            Button { text: "Cancelar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button {
                text: "Registrar"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    if (page.inventory.move(movement.productId, ["entry", "exit", "adjustment"][typeField.currentIndex], quantityField.text, reasonField.text, operatorField.text))
                        movement.close()
                    else errorLabel.text = page.inventory.error
                }
            }
            onRejected: movement.close()
        }
    }
}
