import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var catalog
    required property string section
    signal customerHistoryRequested(int customerId)
    spacing: 16
    property bool ready: false

    function reload() {
        if (ready && catalog && section.length > 0)
            catalog.search(section, searchField.text, inactive.checked)
    }
    onSectionChanged: reload()
    onVisibleChanged: { if (visible) reload() }
    Component.onCompleted: { ready = true; reload() }

    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: page.section === "Produtos" ? "Buscar por nome, código ou código de barras" : "Buscar por nome, código, documento ou telefone"
            onTextChanged: page.reload()
        }        CheckBox { id: inactive; text: "Mostrar inativos"; onToggled: page.reload() }
        Button { text: "Novo cadastro"; onClicked: editor.openRecord({}) }
    }
    Label {
        visible: page.catalog.error.length > 0 && !editor.opened
        text: page.catalog.error
        color: "#b42318"
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    Label { text: page.catalog.rows.length + " registro(s)"; color: "#6d7781" }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 8
        model: page.catalog.rows
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width
            height: 84
            radius: 6
            color: "white"
            border.color: "#e2e8e5"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: modelData.name + (modelData.active ? "" : " • Inativo"); font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        color: "#6d7781"
                        text: page.section === "Produtos"
                            ? modelData.code + "  |  R$ " + Number(modelData.sale_price).toLocaleString(Qt.locale("pt_BR"), 'f', 2) + "  |  Estoque: " + modelData.stock_quantity
                            : page.section === "Categorias" ? "Código: " + modelData.id
                            : [modelData.document, modelData.phone, modelData.email].filter(function(v) { return v }).join("  |  ")
                    }
                }
                Button { text: "Compras"; visible: page.section === "Clientes"; onClicked: page.customerHistoryRequested(modelData.id) }
                Button { text: "Editar"; onClicked: editor.openRecord(modelData) }
                Button {
                    text: modelData.active ? "Inativar" : "Reativar"
                    onClicked: page.catalog.setActive(page.section, modelData.id, !modelData.active)
                }
            }
        }
        Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Nenhum cadastro encontrado."; color: "#6d7781" }
    }
    Dialog {
        id: editor
        property int recordId: 0
        property var categoryOptions: []
        property var supplierOptions: []
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 32)
        height: Math.min(650, parent.height - 32)
        modal: true
        title: (recordId ? "Editar — " : "Novo cadastro — ") + page.section
        standardButtons: Dialog.NoButton

        function openRecord(row) {
            recordId = row.id || 0
            nameField.text = row.name || ""
            codeField.text = row.code || ""
            barcodeField.text = row.barcode || ""
            costField.text = String(row.cost_price || 0).replace('.', ',')
            priceField.text = String(row.sale_price || 0).replace('.', ',')
            minimumField.text = String(row.minimum_stock || 0).replace('.', ',')
            brandField.text = row.brand || ""
            unitField.text = row.unit || ""
            sizeField.text = row.size || ""
            colorField.text = row.color || ""
            variantGroupField.text = row.variant_group || ""
            typeField.currentIndex = row.product_type === "service" ? 1 : 0
            maximumField.text = String(row.maximum_stock || 0).replace('.', ',')
            locationField.text = row.location || ""
            notesField.text = row.notes || ""
            addressField.text = row.address || ""
            birthField.text = row.birth_date ? row.birth_date.split('-').reverse().join('/') : ""
            documentField.text = row.document || ""
            phoneField.text = row.phone || ""
            emailField.text = row.email || ""
            categoryOptions = [{id: 0, name: "Sem categoria"}].concat(page.catalog.categories.filter(function(c) { return c.active || c.id === row.category_id }))
            categoryField.currentIndex = 0
            for (var i = 0; i < categoryOptions.length; ++i)
                if (categoryOptions[i].id === row.category_id) categoryField.currentIndex = i
            supplierOptions = [{id: 0, name: "Sem fornecedor"}].concat(page.catalog.suppliers.filter(function(s) { return s.active || s.id === row.supplier_id }))
            supplierField.currentIndex = 0
            for (var j = 0; j < supplierOptions.length; ++j)
                if (supplierOptions[j].id === row.supplier_id) supplierField.currentIndex = j
            formError.text = ""
            open()
            nameField.forceActiveFocus()
        }
        contentItem: ScrollView {
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 8
                Label { text: "Nome / descrição *" }
                TextField { id: nameField; objectName: "catalogName"; Layout.fillWidth: true; placeholderText: "Nome do cadastro" }
                Label { visible: page.section === "Produtos"; text: "Código interno * / código de barras" }
                ComboBox { id: typeField; objectName: "catalogProductType"; visible: page.section === "Produtos"; model: ["Produto", "Serviço"] }
                RowLayout {
                    visible: page.section === "Produtos"
                    TextField { id: codeField; objectName: "catalogCode"; Layout.fillWidth: true; placeholderText: "Código interno" }
                    TextField { id: barcodeField; Layout.fillWidth: true; placeholderText: "Código de barras" }
                }
                TextField { id: variantGroupField; objectName: "catalogVariantGroup"; visible: page.section === "Produtos"; placeholderText: "Grupo de variação (ex.: CAM-01)"; maximumLength: 60; Layout.fillWidth: true }
                Label { visible: page.section === "Produtos"; text: "Categoria" }
                ComboBox { id: categoryField; visible: page.section === "Produtos"; Layout.fillWidth: true; model: editor.categoryOptions; textRole: "name" }
                Label { visible: page.section === "Produtos"; text: "Fornecedor" }
                ComboBox { id: supplierField; objectName: "supplierField"; visible: page.section === "Produtos"; Layout.fillWidth: true; model: editor.supplierOptions; textRole: "name" }
                Label { visible: page.section === "Produtos"; text: "Custo (R$) / venda (R$) / estoque mínimo" }
                RowLayout {
                    visible: page.section === "Produtos"
                    TextField { id: costField; Layout.fillWidth: true; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    TextField { id: priceField; objectName: "catalogPrice"; Layout.fillWidth: true; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    TextField { id: minimumField; Layout.fillWidth: true; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                }
                Label { visible: page.section === "Produtos"; text: "Use o módulo Estoque para registrar entradas, saídas e ajustes."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#6d7781" }
                Label { visible: page.section === "Clientes" || page.section === "Fornecedores"; text: page.section === "Clientes" ? "CPF/CNPJ (opcional)" : "CPF/CNPJ (opcional, único)" }
                TextField { id: documentField; objectName: "documentField"; visible: page.section === "Clientes" || page.section === "Fornecedores"; Layout.fillWidth: true }
                Label { visible: page.section === "Clientes" || page.section === "Fornecedores"; text: "Telefone" }
                TextField { id: phoneField; objectName: "phoneField"; visible: page.section === "Clientes" || page.section === "Fornecedores"; Layout.fillWidth: true }
                Label { visible: page.section === "Clientes" || page.section === "Fornecedores"; text: "E-mail" }
                TextField { id: emailField; objectName: "emailField"; visible: page.section === "Clientes" || page.section === "Fornecedores"; Layout.fillWidth: true }
                RowLayout {
                    visible: page.section === "Produtos"
                    TextField { id: brandField; objectName: "catalogBrand"; placeholderText: "Marca"; maximumLength: 60; Layout.fillWidth: true }
                    TextField { id: unitField; objectName: "catalogUnit"; placeholderText: "Unidade (UN, KG...)"; maximumLength: 10; Layout.fillWidth: true }
                }
                RowLayout {
                    visible: page.section === "Produtos"
                    TextField { id: sizeField; objectName: "catalogSize"; placeholderText: "Tamanho (P, M, 42...)"; maximumLength: 20; Layout.fillWidth: true }
                    TextField { id: colorField; objectName: "catalogColor"; placeholderText: "Cor"; maximumLength: 40; Layout.fillWidth: true }
                }
                Label { visible: page.section === "Produtos"; text: "Estoque máximo (0 = sem máximo) / localização" }
                RowLayout {
                    visible: page.section === "Produtos"
                    TextField { id: maximumField; objectName: "catalogMaximum"; Layout.fillWidth: true }
                    TextField { id: locationField; objectName: "catalogLocation"; placeholderText: "Localização"; maximumLength: 60; Layout.fillWidth: true }
                }
                Label { visible: page.section === "Produtos"; text: "Tamanho e cor identificam a variação; o estoque continua sendo controlado pelo produto."; Layout.fillWidth: true; wrapMode: Text.Wrap }
                TextField { id: addressField; objectName: "catalogAddress"; visible: page.section === "Clientes"; placeholderText: "Endereço"; maximumLength: 160; Layout.fillWidth: true }
                TextField { id: birthField; objectName: "catalogBirth"; visible: page.section === "Clientes"; placeholderText: "Nascimento (DD/MM/AAAA)"; maximumLength: 10; Layout.fillWidth: true }
                TextField { id: notesField; objectName: "catalogNotes"; visible: page.section === "Clientes" || page.section === "Produtos"; placeholderText: "Observações"; maximumLength: 500; Layout.fillWidth: true }
                Label { id: formError; color: "#b42318"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            }
        }
        footer: DialogButtonBox {
            Button { text: "Cancelar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Salvar"; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    var ok = page.catalog.save(page.section, editor.recordId, {
                        name: nameField.text, code: codeField.text, barcode: barcodeField.text,
                        cost_price: costField.text, sale_price: priceField.text, minimum_stock: minimumField.text,
                        category_id: editor.categoryOptions[categoryField.currentIndex].id,
                        supplier_id: editor.supplierOptions[supplierField.currentIndex].id,
                        brand: brandField.text, unit: unitField.text, size: sizeField.text, color: colorField.text, variant_group: variantGroupField.text, product_type: typeField.currentIndex === 1 ? "service" : "product", maximum_stock: maximumField.text,
                        location: locationField.text, notes: notesField.text, address: addressField.text, birth_date: birthField.text,
                        document: documentField.text, phone: phoneField.text, email: emailField.text
                    })
                    if (ok) editor.close()
                    else formError.text = page.catalog.error
                }
            }
            onRejected: editor.close()
        }
    }
}
