import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var finance
    function money(cents) { return "R$ " + (Number(cents || 0) / 100).toLocaleString(Qt.locale("pt_BR"), 'f', 2) }
    spacing: 12
    RowLayout {
        Layout.fillWidth: true
        TextField { id: description; placeholderText: "Descrição da despesa"; Layout.fillWidth: true }
        TextField { id: amount; placeholderText: "Valor"; Layout.preferredWidth: 120 }
        TextField { id: dueDate; placeholderText: "Vencimento AAAA-MM-DD"; Layout.preferredWidth: 170 }
        Button { text: "Lançar"; onClicked: { if (page.finance.createExpense(description.text, amount.text, dueDate.text)) { description.clear(); amount.clear(); dueDate.clear() } } }
    }
    RowLayout {
        Layout.fillWidth: true
        TextField { id: receivableDescription; placeholderText: "Descrição do recebível"; Layout.fillWidth: true }
        TextField { id: receivableAmount; placeholderText: "Valor"; Layout.preferredWidth: 120 }
        TextField { id: receivableDueDate; placeholderText: "Vencimento AAAA-MM-DD"; Layout.preferredWidth: 170 }
        Button { text: "Lançar recebível"; onClicked: { if (page.finance.createReceivable(receivableDescription.text, receivableAmount.text, receivableDueDate.text)) { receivableDescription.clear(); receivableAmount.clear(); receivableDueDate.clear() } } }
    }
    RowLayout {
        Layout.fillWidth: true
        TextField { id: orderCustomer; placeholderText: "ID do cliente"; inputMethodHints: Qt.ImhDigitsOnly; Layout.preferredWidth: 100 }
        TextField { id: orderService; placeholderText: "ID do serviço"; inputMethodHints: Qt.ImhDigitsOnly; Layout.preferredWidth: 100 }
        TextField { id: orderDescription; placeholderText: "Descrição da ordem"; Layout.fillWidth: true }
        TextField { id: orderNotes; placeholderText: "Observações"; Layout.fillWidth: true }
        Button { text: "Abrir OS"; onClicked: if (page.finance.createServiceOrder(Number(orderCustomer.text), Number(orderService.text), orderDescription.text, orderNotes.text)) { orderCustomer.clear(); orderService.clear(); orderDescription.clear(); orderNotes.clear() } }
    }
    RowLayout {
        Layout.fillWidth: true
        TextField { id: materialOrder; placeholderText: "ID da OS"; inputMethodHints: Qt.ImhDigitsOnly; Layout.preferredWidth: 90 }
        TextField { id: materialProduct; placeholderText: "ID do material"; inputMethodHints: Qt.ImhDigitsOnly; Layout.preferredWidth: 110 }
        TextField { id: materialQuantity; placeholderText: "Quantidade"; Layout.preferredWidth: 110 }
        Button { text: "Adicionar material"; onClicked: if (page.finance.addServiceMaterial(Number(materialOrder.text), Number(materialProduct.text), materialQuantity.text)) { materialOrder.clear(); materialProduct.clear(); materialQuantity.clear() } }
    }
    Label { text: page.finance.error; visible: text.length > 0; color: "#b42318"; wrapMode: Text.Wrap; Layout.fillWidth: true }
    RowLayout {
        Layout.fillWidth: true
        Label { text: "Despesas e contas a pagar"; font.bold: true; font.pixelSize: 20; Layout.fillWidth: true }
        Button { text: "Atualizar"; onClicked: page.finance.refresh() }
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: page.finance.expenses
        clip: true
        spacing: 8
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width
            height: 74
            color: "white"
            radius: 6
            border.color: "#e2e8e5"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                Label { text: modelData.description + "\nVencimento: " + modelData.due_date; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: page.money(modelData.amount_cents) + "\n" + modelData.status; Layout.preferredWidth: 130; wrapMode: Text.Wrap }
                Button {
                    visible: modelData.status === "open"
                    text: "Baixar no caixa"
                    onClicked: paymentDialog.expenseId = modelData.id
                }
            }
        }
        Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Nenhuma despesa encontrada." }
    }
    Label { text: "Ordens de serviço"; font.bold: true; font.pixelSize: 20; Layout.fillWidth: true }
    ListView {
        Layout.fillWidth: true; Layout.preferredHeight: 180; model: page.finance.serviceOrders; clip: true
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width; height: 66; color: "white"; radius: 6; border.color: "#e2e8e5"
            RowLayout {
                anchors.fill: parent; anchors.margins: 10
                Label { text: "OS #" + modelData.id + " • " + modelData.service_name + "\n" + modelData.customer_name + " — " + modelData.description; Layout.fillWidth: true; wrapMode: Text.Wrap }
                ComboBox { model: ["open", "in_progress", "completed", "cancelled"]; currentIndex: indexOfValue(modelData.status); onActivated: page.finance.updateServiceOrder(modelData.id, currentText) }
                Label { text: page.money(modelData.amount_cents); Layout.preferredWidth: 90 }
            }
        }
    }
    Label { text: "Contas a receber"; font.bold: true; font.pixelSize: 20; Layout.fillWidth: true }
    ListView {
        Layout.fillWidth: true
        Layout.preferredHeight: 180
        model: page.finance.receivables
        clip: true
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width; height: 62; color: "white"; radius: 6; border.color: "#e2e8e5"
            RowLayout {
                anchors.fill: parent; anchors.margins: 10
                Label { text: modelData.description + "\n" + (modelData.customer_name || "Cliente não informado") + " • " + modelData.due_date; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: page.money(modelData.amount_cents) + "\n" + modelData.status; Layout.preferredWidth: 120 }
                Button { visible: modelData.status === "open"; text: "Receber"; onClicked: receiveDialog.receivableId = modelData.id }
            }
        }
    }
    Dialog {
        id: paymentDialog
        property int expenseId: 0
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(500, parent.width - 32)
        modal: true
        title: "Baixar despesa"
        onOpened: cashSession.forceActiveFocus()
        contentItem: ColumnLayout {
            Label { text: "Informe o número da sessão de caixa aberta."; wrapMode: Text.Wrap; Layout.fillWidth: true }
            TextField { id: cashSession; placeholderText: "Sessão de caixa"; inputMethodHints: Qt.ImhDigitsOnly; Layout.fillWidth: true }
            Label { text: page.finance.error; visible: text.length > 0; color: "#b42318"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        }
        footer: DialogButtonBox {
            Button { text: "Cancelar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button {
                text: "Confirmar baixa"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: { if (page.finance.payExpense(paymentDialog.expenseId, Number(cashSession.text))) { paymentDialog.close(); cashSession.clear() } }
            }
            onRejected: paymentDialog.close()
        }
    }
    Dialog {
        id: receiveDialog
        property int receivableId: 0
        parent: Overlay.overlay; anchors.centerIn: parent; width: Math.min(500, parent.width - 32); modal: true
        title: "Receber conta"
        contentItem: ColumnLayout {
            Label { text: "Informe o número da sessão de caixa aberta."; wrapMode: Text.Wrap; Layout.fillWidth: true }
            TextField { id: receiveCashSession; placeholderText: "Sessão de caixa"; inputMethodHints: Qt.ImhDigitsOnly; Layout.fillWidth: true }
            Label { text: page.finance.error; visible: text.length > 0; color: "#b42318"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        }
        footer: DialogButtonBox {
            Button { text: "Cancelar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Confirmar recebimento"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: { if (page.finance.receiveReceivable(receiveDialog.receivableId, Number(receiveCashSession.text))) { receiveDialog.close(); receiveCashSession.clear() } } }
            onRejected: receiveDialog.close()
        }
    }
}
