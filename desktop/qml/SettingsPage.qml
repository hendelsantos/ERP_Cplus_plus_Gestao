import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var settings
    function reload() {
        company.text = settings.values.company || ""
        companyDocument.text = settings.values.document || ""
        companyPhone.text = settings.values.phone || ""
        companyAddress.text = settings.values.address || ""
        profile.currentIndex = Math.max(0, profile.indexOfValue(settings.values.profile))
        for (var i = 0; i < moduleChoices.count; ++i) {
            var choice = moduleChoices.itemAt(i)
            if (choice) choice.checked = !!settings.values[choice.modelData.id]
        }
    }
    Component.onCompleted: reload()
    onVisibleChanged: if (visible) reload()
    spacing: 10
    Label { text: "Empresa e módulos"; font.pixelSize: 22; font.bold: true }
    Label { text: "Nome da empresa" }
    TextField { id: company; objectName: "companyName"; Layout.fillWidth: true; maximumLength: 120 }
    RowLayout {
        TextField { id: companyDocument; objectName: "companyDocument"; placeholderText: "CPF/CNPJ (opcional)"; maximumLength: 20; Layout.fillWidth: true }
        TextField { id: companyPhone; objectName: "companyPhone"; placeholderText: "Telefone (opcional)"; maximumLength: 20; Layout.fillWidth: true }
    }
    TextField { id: companyAddress; objectName: "companyAddress"; placeholderText: "Endereço da empresa (opcional)"; maximumLength: 160; Layout.fillWidth: true }
    Label { text: "Perfil do negócio" }
    ComboBox {
        id: profile
        Layout.fillWidth: true
        textRole: "label"; valueRole: "id"
        model: [{id:"general",label:"Comércio em geral"},{id:"fashion",label:"Roupas e acessórios"},
            {id:"market",label:"Mercado e mercearia"},{id:"services",label:"Serviços"}]
    }
    Label {
        text: "O perfil identifica seu negócio. Grade de roupas, venda por peso e ordens de serviço ainda não estão disponíveis."
        Layout.fillWidth: true; wrapMode: Text.Wrap
    }
    Flow {
        Layout.fillWidth: true
        spacing: 12
        Repeater {
            id: moduleChoices
            model: page.settings.modules.filter(function(module) { return module.configurable })
            delegate: CheckBox {
                required property var modelData
                objectName: "module" + modelData.id.charAt(0).toUpperCase() + modelData.id.slice(1)
                text: modelData.label
                checked: !!page.settings.values[modelData.id]
                ToolTip.visible: hovered && modelData.description.length > 0
                ToolTip.text: modelData.description
            }
        }
    }
    Label {
        text: "Para alterar os módulos, feche o caixa e limpe o carrinho. Cadastros, consulta de vendas, dashboard e backup permanecem disponíveis. Desabilitar módulos preserva seus dados."
        Layout.fillWidth: true; wrapMode: Text.Wrap
    }
    RowLayout {
    Button { text: "Salvar configurações"; onClicked: {
        var selection = {}
        for (var i = 0; i < moduleChoices.count; ++i) {
            var choice = moduleChoices.itemAt(i)
            selection[choice.modelData.id] = choice.checked
        }
        page.settings.saveModules(company.text,profile.currentValue,selection,companyDocument.text,companyPhone.text,companyAddress.text)
    } }
        Button { text: "Diagnóstico"; onClicked: diagnostic.open() }
    }
    Dialog {
        id: diagnostic
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(620,parent.width-32)
        modal: true
        title: "Diagnóstico local"
        property var report: ({})
        onOpened: { report=page.settings.diagnostics(); level.currentIndex=report.level || 0; result.text="" }
        contentItem: ColumnLayout {
            Label { text: "Aplicativo: " + (diagnostic.report.application || "—") + " • Qt: " + (diagnostic.report.qt || "—") + " • Esquema: " + (diagnostic.report.schema || "—"); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Label { text: "Banco: " + (diagnostic.report.database || "—") + "\nLogs: " + (diagnostic.report.logs || "—"); Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
            Label { text: diagnostic.report.unclean_shutdown ? "A sessão anterior terminou inesperadamente. A integridade do banco foi verificada nesta inicialização." : "Sem encerramento inesperado detectado nesta inicialização."; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Label { text: diagnostic.report.error || ""; visible: text.length>0; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#b42318" }
            Label { text: "Registrar eventos a partir de:" }
            ComboBox { id: level; model: ["Informação", "Aviso", "Erro"]; Layout.fillWidth: true }
            Label { text: "Logs locais registram eventos técnicos sem senhas, SQL ou dados de clientes. Mantêm até quatro arquivos de 1 MiB."; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Label { id: result; Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
        footer: DialogButtonBox {
            Button { text: "Fechar diagnóstico"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Salvar nível de log"; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: { page.settings.configureLogging(level.currentIndex); result.text=page.settings.message; diagnostic.report=page.settings.diagnostics() } }
            onRejected: diagnostic.close()
        }
    }
    Label { text: page.settings.message; Layout.fillWidth: true; wrapMode: Text.Wrap }
    Item { Layout.fillHeight: true }
}
