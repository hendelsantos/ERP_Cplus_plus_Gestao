import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var pos
    signal backToCustomers()
    property int customerId: 0
    property int pageNumber: 0
    property bool ready: false
    property string exportStatus: ""
    function money(cents) { return "R$ " + (Number(cents || 0)/100).toLocaleString(Qt.locale("pt_BR"),'f',2) }
    function payment(method) { return ({cash:"Dinheiro",pix:"PIX",credit:"Crédito",debit:"Débito",other:"Outros"})[method] || "Pagamento não disponível" }
    function status(value) { return value === "completed" ? "Concluída" : value }
    function reload() { if (ready) pos.searchSales(number.text,pageNumber,customerId,fromDate.text,toDate.text) }
    function showCustomer(id) {
        customerId = id
        pageNumber = 0
        number.text = ""
        reload()
    }
    Component.onCompleted: { ready = true; reload() }
    onVisibleChanged: { if (visible) reload() }
    spacing: 12
    Label {
        visible: page.customerId > 0 && !page.pos.salesError.length
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: (page.pos.customerSummary.name || "") + " (#" + page.customerId + ")"
            + (page.pos.customerSummary.active ? "" : " • Inativo")
            + "\nCompras concluídas: " + (page.pos.customerSummary.purchase_count || 0)
            + " • Total gasto: " + page.money(page.pos.customerSummary.spent_cents)
            + "\nÚltima compra: " + (page.pos.customerSummary.last_purchase || "Nenhuma compra")
    }
    RowLayout {
        visible: page.customerId > 0
        Button { text: "Voltar para clientes"; onClicked: page.backToCustomers() }
        Button { text: "Todas as vendas"; onClicked: page.showCustomer(0) }
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: number
            objectName: "salesNumber"
            placeholderText: "Número da venda (vazio para todas)"
            Layout.fillWidth: true
            onTextChanged: { page.pageNumber = 0; page.reload() }
        }
        TextField {
            id: fromDate
            objectName: "salesFromDate"
            placeholderText: "De (AAAA-MM-DD)"
            Layout.preferredWidth: 140
            onTextChanged: { page.pageNumber = 0; page.reload() }
        }
        TextField {
            id: toDate
            objectName: "salesToDate"
            placeholderText: "Até (AAAA-MM-DD)"
            Layout.preferredWidth: 140
            onTextChanged: { page.pageNumber = 0; page.reload() }
        }
        Button { text: "Atualizar vendas"; onClicked: page.reload() }
    }
    RowLayout {
        Layout.fillWidth: true
        Button { text: "Exportar CSV"; onClicked: { exportFile.text = ""; exportDialog.open() } }
        Button { text: "Exportar PDF"; onClicked: { exportFile.text = ""; exportDialog.pdf = true; exportDialog.open() } }
        Label { text: page.exportStatus; color: "#166534"; wrapMode: Text.Wrap; Layout.fillWidth: true }
    }
    Label { text: page.pos.salesError; visible: text.length > 0; color: "#b42318"; wrapMode: Text.Wrap; Layout.fillWidth: true }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: page.pos.sales
        clip: true
        spacing: 8
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width
            height: saleLabel.implicitHeight + 24
            color: "white"
            radius: 6
            Label {
                id: saleLabel
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12; rightMargin: 120 }
                wrapMode: Text.Wrap
                text: "Venda #" + modelData.id + " • " + page.money(modelData.total_cents) + " • " + page.status(modelData.status)
                    + "\n" + modelData.local_created_at + " • Caixa #" + (modelData.cash_session_id || "—")
                    + "\n" + page.payment(modelData.method) + " • " + (modelData.operator_name || "Responsável não informado")
            }
            Button {
                text: "Ver venda"
                anchors { right: parent.right; top: parent.top; margins: 12 }
                onClicked: { if (page.pos.loadSale(modelData.id)) details.open() }
            }
        }
        Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Nenhuma venda encontrada." }
    }
    RowLayout {
        Layout.fillWidth: true
        Button { text: "Anterior"; enabled: page.pageNumber > 0; onClicked: { page.pageNumber--; page.reload() } }
        Label { text: "Página " + (page.pageNumber+1) + " • Até 50 vendas por página"; Layout.fillWidth: true }
        Button { text: "Próxima"; enabled: page.pos.moreSales; onClicked: { page.pageNumber++; page.reload() } }
    }
    Dialog {
        id: exportDialog
        property bool pdf: false
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(560,parent.width-32)
        modal: true
        title: exportDialog.pdf ? "Exportar vendas para PDF" : "Exportar vendas para CSV"
        contentItem: ColumnLayout {
            Label {
                text: "Informe o caminho absoluto do arquivo. O período preenchido na tela será aplicado ao relatório."
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            TextField {
                id: exportFile
                objectName: "salesExportFile"
                placeholderText: exportDialog.pdf ? "/caminho/relatorio.pdf" : "/caminho/relatorio.csv"
                Layout.fillWidth: true
            }
            Label {
                visible: page.pos.error.length > 0
                text: page.pos.error
                color: "#b42318"
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        footer: DialogButtonBox {
            Button { text: "Cancelar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button {
                text: exportDialog.pdf ? "Exportar PDF" : "Exportar CSV"
                enabled: exportFile.text.length > 0
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    const exported = exportDialog.pdf
                        ? page.pos.exportSalesPdf(exportFile.text,fromDate.text,toDate.text)
                        : page.pos.exportSalesCsv(exportFile.text,fromDate.text,toDate.text)
                    if (exported) {
                        page.exportStatus = "CSV exportado: " + exportFile.text
                        if (exportDialog.pdf) page.exportStatus = "PDF exportado: " + exportFile.text
                        exportDialog.close()
                    }
                }
            }
            onRejected: exportDialog.close()
        }
    }
    Dialog {
        id: details
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(620,parent.width-32)
        height: Math.min(540,parent.height-32)
        title: "Venda #" + (page.pos.selectedSale.id || "")
        modal: true
        contentItem: ColumnLayout {
            Label {
                text: (page.pos.selectedSale.local_created_at || "") + " • " + page.status(page.pos.selectedSale.status || "")
                    + "\nCliente: " + (page.pos.selectedSale.customer_id ? (page.pos.selectedSale.customer_name || "Cadastro indisponível") + " (#" + page.pos.selectedSale.customer_id + ")" : "Consumidor não identificado")
                    + "\nCaixa #" + (page.pos.selectedSale.cash_session_id || "—") + " • " + (page.pos.selectedSale.operator_name || "Responsável não informado")
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 12
                model: page.pos.saleItems
                ScrollBar.vertical: ScrollBar {}
                delegate: Label {
                    required property var modelData
                    width: ListView.view.width
                    wrapMode: Text.Wrap
                    text: modelData.product_code + " — " + modelData.product_name
                        + "\n" + modelData.quantity + " × " + page.money(modelData.unit_price_cents) + " = " + page.money(modelData.total_cents)
                }
                Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Itens não disponíveis neste registro antigo." }
            }
            Label {
                visible: !!page.pos.selectedSale.discount_cents || !!page.pos.selectedSale.surcharge_cents
                Layout.fillWidth: true; wrapMode: Text.Wrap
                text: "Subtotal: " + page.money(page.pos.selectedSale.subtotal_cents)
                    + " • Desconto: " + page.money(page.pos.selectedSale.discount_cents)
                    + " • Acréscimo: " + page.money(page.pos.selectedSale.surcharge_cents)
                    + "\nMotivo: " + (page.pos.selectedSale.adjustment_reason || "")
            }
            Label { text: "Total: " + page.money(page.pos.selectedSale.total_cents); font.bold: true; font.pixelSize: 20 }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: page.payment(page.pos.selectedSale.method)
                    + (page.pos.selectedSale.method ? "\nPago: " + page.money(page.pos.selectedSale.paid_cents)
                       + " • Recebido: " + page.money(page.pos.selectedSale.tendered_cents)
                       + " • Troco: " + page.money(page.pos.selectedSale.change_cents) : "")
            }
            Label { text: "Nome do cliente conforme cadastro atual • Não é documento fiscal"; color: "#6d7781" }
        }
        footer: DialogButtonBox {
            Button { text: "Fechar detalhes"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            onRejected: details.close()
        }
    }
}
