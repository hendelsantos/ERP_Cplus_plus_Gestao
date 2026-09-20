import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var settings
    function reload() {
        company.text = settings.values.company || ""
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
    Button { text: "Salvar configurações"; onClicked: {
        var selection = {}
        for (var i = 0; i < moduleChoices.count; ++i) {
            var choice = moduleChoices.itemAt(i)
            selection[choice.modelData.id] = choice.checked
        }
        page.settings.saveModules(company.text,profile.currentValue,selection)
    } }
    Label { text: page.settings.message; Layout.fillWidth: true; wrapMode: Text.Wrap }
    Item { Layout.fillHeight: true }
}
