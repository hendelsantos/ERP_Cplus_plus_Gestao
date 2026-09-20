import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var pos
    required property bool cashMode
    property bool ready: false
    property int selectedCustomerId: 0
    function money(cents) { return "R$ " + (Number(cents || 0)/100).toLocaleString(Qt.locale("pt_BR"),'f',2) }
    function reload() { if (ready) pos.refresh(search.text) }
    Component.onCompleted: { ready = true; reload(); pos.refreshCustomers() }
    onVisibleChanged: { if (visible) { reload(); if (ready) pos.refreshCustomers() } }
    onCashModeChanged: { if (ready && !cashMode) pos.refreshCustomers() }
    spacing: 12
    Label { text: page.pos.cash.id ? "Caixa #" + page.pos.cash.id + " aberto • Dinheiro esperado: " + page.money(page.pos.cash.cash_expected) : "Caixa fechado — abra o caixa para finalizar vendas"; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true }
    Label { visible: page.pos.error.length > 0; text: page.pos.error; color: "#b42318"; wrapMode: Text.Wrap; Layout.fillWidth: true }
    RowLayout {
        Layout.fillWidth: true
        Label { text: "Responsável" }
        TextField { id: operatorField; readOnly: true; text: authStore.user.name || ""; objectName: "posOperator"; placeholderText: "Nome do operador"; Layout.fillWidth: true }
        Label { text: "Identificação manual"; color: "#6d7781" }
    }
    RowLayout {
        visible: page.cashMode
        Layout.fillWidth: true
        TextField { id: cashAmount; objectName: "cashAmount"; Layout.fillWidth: true; placeholderText: page.pos.cash.id ? "Dinheiro contado no fechamento" : "Dinheiro inicial (ex.: 100,00)" }
        Button {
            text: page.pos.cash.id ? "Fechar caixa" : "Abrir caixa"
            onClicked: {
                var ok = page.pos.cash.id ? page.pos.closeCash(page.pos.cash.id,cashAmount.text,operatorField.text) : page.pos.openCash(cashAmount.text,operatorField.text)
                if (ok) cashAmount.text = ""
            }
        }
    }
    RowLayout {
        visible: page.cashMode && !!page.pos.cash.id
        Layout.fillWidth: true
        Button { text: "Suprimento"; onClicked: cashMovement.openFor("supply") }
        Button { text: "Sangria"; onClicked: cashMovement.openFor("withdrawal") }
        Label { text: "Entradas e retiradas de dinheiro físico"; color: "#6d7781"; Layout.fillWidth: true; wrapMode: Text.Wrap }
    }
    Label { visible: page.cashMode; text: "Últimas 100 sessões • Conferência apenas do dinheiro físico; PIX e cartões não entram no saldo da gaveta."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#6d7781" }
    ListView {
        visible: page.cashMode
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 8
        model: page.pos.sessions
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width
            height: sessionText.implicitHeight + 24
            color: "white"
            radius: 6
            Label {
                id: sessionText
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12; rightMargin: 132 }
                wrapMode: Text.Wrap
                text: "Caixa #" + modelData.id + (modelData.status === "open" ? " • Aberto" : " • Fechado")
                    + "\n" + modelData.opened_local + (modelData.closed_local ? " → " + modelData.closed_local : "")
                    + "\nAbertura: " + page.money(modelData.opening_cents) + " • Vendas: " + page.money(modelData.sales_cents)
                    + "\nSuprimentos: " + page.money(modelData.supply_cents) + " • Sangrias: " + page.money(modelData.withdrawal_cents)
                    + (modelData.status === "closed" && modelData.expected_cents !== null ? "\nEsperado: " + page.money(modelData.expected_cents) + " • Contado: " + page.money(modelData.counted_cents)
                        + " • Diferença: " + page.money(modelData.counted_cents - modelData.expected_cents) : "")
                    + "\nResponsável: " + modelData.operator_name + (modelData.closed_by ? " • Fechado por: " + modelData.closed_by : "")
            }
            Button {
                anchors { right: parent.right; top: parent.top; margins: 12 }
                text: "Movimentações"
                onClicked: {
                    cashHistory.sessionId = modelData.id
                    page.pos.selectCashHistory(modelData.id)
                    cashHistory.open()
                }
            }
        }
    }
    RowLayout {
        visible: !page.cashMode
        Layout.fillWidth: true
        Label { text: "Cliente" }
        ComboBox {
            id: customerChoice
            objectName: "posCustomer"
            Layout.fillWidth: true
            model: page.pos.customers
            textRole: "label"
            valueRole: "id"
            currentIndex: { page.pos.customers; return indexOfValue(page.selectedCustomerId) }
            displayText: currentIndex >= 0 ? currentText : "Cliente indisponível (#" + page.selectedCustomerId + ")"
            onActivated: page.selectedCustomerId = currentValue
        }
    }
    RowLayout {
        visible: !page.cashMode
        Layout.fillWidth: true
        TextField {
            id: search
            Layout.fillWidth: true
            placeholderText: "Produto, código ou código de barras • Enter adiciona correspondência única"
            onTextChanged: page.reload()
            onAccepted: {
                if (page.pos.products.length === 1 && page.pos.add(page.pos.products[0].id)) text = ""
            }
        }
        Button { text: "Limpar carrinho"; enabled: page.pos.cart.length > 0; onClicked: { page.pos.clearCart(); page.selectedCustomerId = 0 } }
    }
    RowLayout {
        visible: !page.cashMode
        Layout.fillWidth: true
        Layout.fillHeight: true
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: page.pos.products
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                height: 70
                text: modelData.code + " — " + modelData.name + "\n" + page.money(modelData.sale_price_cents) + " • Estoque: " + modelData.stock_quantity
                onClicked: page.pos.add(modelData.id)
            }
            Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Nenhum produto encontrado." }
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: page.pos.cart
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 100
                color: "white"
                radius: 6
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Label { text: modelData.name + " • " + page.money(modelData.total_cents); elide: Text.ElideRight; Layout.fillWidth: true }
                    RowLayout {
                        Button { text: "−"; onClicked: page.pos.setQuantityValue(modelData.id,String(Number(modelData.quantity)-1)) }
                        TextField {
                            text: modelData.quantity
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator { bottom: 0; decimals: 3 }
                            onEditingFinished: page.pos.setQuantityValue(modelData.id,text)
                            Layout.preferredWidth: 90
                        }
                        Button { text: "+"; onClicked: page.pos.add(modelData.id) }
                        Button { text: "Remover"; onClicked: page.pos.setQuantity(modelData.id,0) }
                    }
                }
            }
            Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Selecione produtos à esquerda." }
        }
    }
    RowLayout {
        visible: !page.cashMode
        Layout.fillWidth: true
        Button {
            text: "Desconto / acréscimo"
            enabled: authStore.user.role === "admin" && page.pos.cart.length > 0
            onClicked: {
                adjustmentDiscount.text = String(page.pos.discount/100)
                adjustmentSurcharge.text = String(page.pos.surcharge/100)
                adjustmentReason.text = page.pos.adjustmentReason
                adjustmentError.text = ""
                adjustments.open()
            }
        }
        Label {
            visible: page.pos.discount > 0 || page.pos.surcharge > 0
            text: "Subtotal: " + page.money(page.pos.subtotal) + " • Desconto: " + page.money(page.pos.discount) + " • Acréscimo: " + page.money(page.pos.surcharge)
            Layout.fillWidth: true; wrapMode: Text.Wrap
        }
    }
    Dialog {
        id: adjustments
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520,parent.width-32)
        modal: true
        title: "Ajustes da venda"
        contentItem: ColumnLayout {
            Label { text: "Valores em reais sobre a venda. Alterar o carrinho remove os ajustes. Use zero em ambos para remover."; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Label { text: "Desconto (R$)" }
            TextField { id: adjustmentDiscount; objectName: "saleDiscount"; Layout.fillWidth: true }
            Label { text: "Acréscimo (R$)" }
            TextField { id: adjustmentSurcharge; objectName: "saleSurcharge"; Layout.fillWidth: true }
            TextField { id: adjustmentReason; objectName: "saleAdjustmentReason"; placeholderText: "Justificativa obrigatória"; maximumLength: 200; Layout.fillWidth: true }
            Label { id: adjustmentError; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#b42318" }
        }
        footer: DialogButtonBox {
            Button { text: "Cancelar ajuste"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Aplicar ajuste"; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    if (page.pos.setAdjustments(adjustmentDiscount.text,adjustmentSurcharge.text,adjustmentReason.text)) adjustments.close()
                    else adjustmentError.text = page.pos.error
                }
            }
            onRejected: adjustments.close()
        }
    }
    RowLayout {
        visible: !page.cashMode
        Layout.fillWidth: true
        Label { text: "Total: " + page.money(page.pos.total); font.bold: true; font.pixelSize: 22; Layout.fillWidth: true }
        ComboBox { id: method; model: ["Dinheiro","PIX","Crédito","Débito","Outros"] }
        TextField { id: received; objectName: "posReceived"; visible: method.currentIndex === 0; Layout.preferredWidth: 120; placeholderText: "Recebido" }
        Button {
            text: "Finalizar venda"
            enabled: !!page.pos.cash.id && page.pos.cart.length > 0
            onClicked: {
                if (page.pos.checkout(page.pos.cash.id,["cash","pix","credit","debit","other"][method.currentIndex],received.text,operatorField.text,page.selectedCustomerId)) {
                    received.text = ""
                    page.selectedCustomerId = 0
                    receipt.open()
                }
            }
        }
    }
    Dialog {
        id: cashMovement
        property int sessionId: 0
        property string movementType: "supply"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(500,parent.width-32)
        height: Math.min(400,parent.height-32)
        modal: true
        title: (movementType === "supply" ? "Suprimento" : "Sangria") + " — Caixa #" + sessionId
        function openFor(type) {
            sessionId = page.pos.cash.id
            movementType = type
            movementAmount.text = ""
            movementReason.text = ""
            movementOperator.text = operatorField.text
            movementError.text = ""
            open()
            movementAmount.forceActiveFocus()
        }
        contentItem: ScrollView {
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                Label { text: "Valor (R$)" }
                TextField { id: movementAmount; objectName: "cashMovementAmount"; Layout.fillWidth: true; placeholderText: "Ex.: 50,00" }
                Label { text: "Motivo *" }
                TextField { id: movementReason; objectName: "cashMovementReason"; Layout.fillWidth: true; placeholderText: "Descreva a entrada ou retirada" }
                Label { text: "Responsável *" }
                TextField { id: movementOperator; readOnly: true; objectName: "cashMovementOperator"; Layout.fillWidth: true }
                Label { id: movementError; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#b42318" }
            }
        }
        footer: DialogButtonBox {
            Button { text: "Cancelar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button {
                text: "Registrar movimentação"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    if (page.pos.moveCash(cashMovement.sessionId,cashMovement.movementType,movementAmount.text,movementReason.text,movementOperator.text))
                        cashMovement.close()
                    else movementError.text = page.pos.error
                }
            }
            onRejected: cashMovement.close()
        }
    }
    Dialog {
        id: cashHistory
        property int sessionId: 0
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(620,parent.width-32)
        height: Math.min(500,parent.height-32)
        title: "Movimentações — Caixa #" + sessionId
        modal: true
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            Label { text: "Últimos 200 suprimentos e sangrias • Horário local"; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#6d7781" }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 12
                model: page.pos.cashMovements
                ScrollBar.vertical: ScrollBar {}
                delegate: Label {
                    required property var modelData
                    width: ListView.view.width
                    wrapMode: Text.Wrap
                    text: modelData.local_created_at + " • " + (modelData.type === "supply" ? "Suprimento" : "Sangria") + ": " + page.money(modelData.amount_cents)
                        + "\nSaldo: " + page.money(modelData.previous_cents) + " → " + page.money(modelData.balance_cents)
                        + "\n" + modelData.operator_name + " — " + modelData.reason
                }
                Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Nenhuma movimentação manual nesta sessão." }
            }
        }
    }
    Dialog {
        id: receipt
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(500,parent.width-32)
        height: Math.min(450,parent.height-32)
        title: "Venda concluída"
        modal: true
        footer: DialogButtonBox {
            Button {
                text: "Exportar PDF"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: { receiptFile.text = ""; receiptExport.open() }
            }
            Button { text: "Fechar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            onRejected: receipt.close()
        }
        contentItem: ScrollView {
            clip: true
            TextArea { text: page.pos.receipt; readOnly: true; wrapMode: TextEdit.Wrap; selectByMouse: true }
        }
    }
    Dialog {
        id: receiptExport
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520,parent.width-32)
        modal: true
        title: "Exportar comprovante não fiscal"
        contentItem: ColumnLayout {
            Label { text: "Informe um caminho absoluto para o PDF."; wrapMode: Text.Wrap; Layout.fillWidth: true }
            TextField { id: receiptFile; objectName: "receiptExportFile"; placeholderText: "/caminho/comprovante.pdf"; Layout.fillWidth: true }
            Label { text: page.pos.error; visible: text.length > 0; color: "#b42318"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        }
        footer: DialogButtonBox {
            Button { text: "Cancelar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button {
                text: "Exportar"
                enabled: receiptFile.text.length > 0
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: if (page.pos.exportReceiptPdf(receiptFile.text)) receiptExport.close()
            }
            onRejected: receiptExport.close()
        }
    }
}
