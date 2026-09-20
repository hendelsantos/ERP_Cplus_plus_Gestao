import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: "MH Store — " + (settingsStore.values.company || "Minha empresa")
    color: palette.window

    property bool businessSettings: false
    property string activeSection: "Dashboard"
    onActiveSectionChanged: { if (activeSection === "Dashboard") posStore.refreshDashboard() }
    Component.onCompleted: posStore.refreshDashboard()
    function dashboardMoney(cents) {
        return posStore.dashboardError.length ? "—" : "R$ " + (Number(cents || 0)/100).toLocaleString(Qt.locale("pt_BR"),'f',2)
    }
    function dashboardCount(value) { return posStore.dashboardError.length ? "—" : String(value || 0) }
    Timer {
        interval: 60000
        running: window.activeSection === "Dashboard"
        repeat: true
        onTriggered: posStore.refreshDashboard()
    }
    readonly property bool isCatalog: ["Produtos", "Categorias", "Clientes"].indexOf(activeSection) >= 0
    property color ink: "#18212b"
    property color muted: "#6d7781"
    property color accent: "#0e7490"
    property color accentSoft: "#dff5f7"
    property color surface: "#ffffff"
    property color canvas: "#f4f6f5"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 238
            color: "#18212b"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 4

                Label {
                    text: "MH Store"
                    color: "#ffffff"
                    font.pixelSize: 25
                    font.bold: true
                    Layout.bottomMargin: 2
                }
                Label {
                    text: "by MHSoftware"
                    color: "#9eabb5"
                    font.pixelSize: 12
                    Layout.bottomMargin: 12
                }

                Repeater {
                    model: settingsStore.navigation

                    delegate: ItemDelegate {
                        objectName: "navigation_" + modelData
                        text: modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        Layout.minimumHeight: 34
                        Layout.maximumHeight: 34
                        highlighted: window.activeSection === modelData
                        onClicked: window.activeSection = modelData

                        background: Rectangle {
                            radius: 6
                            color: parent.highlighted ? window.accent : "transparent"
                        }

                        contentItem: Label {
                            text: modelData
                            color: parent.highlighted ? "#ffffff" : "#b7c1c8"
                            font.pixelSize: 14
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }
                    }
                }

                Item { Layout.fillHeight: true }
                ItemDelegate {
                    Layout.fillWidth: true
                    text: "Configurações"
                    contentItem: Label {
                        text: parent.text
                        color: "#b7c1c8"
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 12
                    }
                    onClicked: window.activeSection = "Configurações"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: window.canvas

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 34
                spacing: 24

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: window.activeSection
                        color: window.ink
                        font.pixelSize: 30
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: "Operação local ativa"
                        color: "#23734f"
                        font.pixelSize: 13
                    }
                }

                Label {
                    visible: window.activeSection !== "Dashboard" && !window.isCatalog && window.activeSection !== "Estoque" && window.activeSection !== "PDV" && window.activeSection !== "Caixa" && window.activeSection !== "Vendas" && window.activeSection !== "Configurações"
                    text: "Módulo preparado para a próxima etapa do MVP."
                    color: window.muted
                    font.pixelSize: 16
                }

                RowLayout {
                    visible: window.activeSection === "Configurações"
                    Button { text: "Empresa e módulos"; onClicked: window.businessSettings = true }
                    Button { text: "Backup local"; onClicked: window.businessSettings = false }
                }
                SettingsPage {
                    visible: window.activeSection === "Configurações" && window.businessSettings
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    settings: settingsStore
                }
                BackupPage {
                    visible: window.activeSection === "Configurações" && !window.businessSettings
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    backup: backupStore
                }

                CatalogPage {
                    visible: window.isCatalog
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onCustomerHistoryRequested: function(customerId) {
                        salesPage.showCustomer(customerId)
                        window.activeSection = "Vendas"
                    }
                    catalog: catalogStore
                    section: window.isCatalog ? window.activeSection : "Produtos"
                }

                InventoryPage {
                    visible: window.activeSection === "Estoque"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    inventory: inventoryStore
                }

                SalesPage {
                    id: salesPage
                    onBackToCustomers: window.activeSection = "Clientes"
                    visible: window.activeSection === "Vendas"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    pos: posStore
                }

                PosPage {
                    visible: window.activeSection === "PDV" || window.activeSection === "Caixa"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    pos: posStore
                    cashMode: window.activeSection === "Caixa"
                }

                GridLayout {
                    visible: window.activeSection === "Dashboard"
                    columns: 3
                    columnSpacing: 16
                    rowSpacing: 16
                    Layout.fillWidth: true

                    Repeater {
                        model: [
                            { title: "Faturamento hoje", value: window.dashboardMoney(posStore.dashboard.today_cents), detail: "Vendas concluídas • Horário local" },
                            { title: "Vendas hoje", value: window.dashboardCount(posStore.dashboard.today_count), detail: "Vendas concluídas no dia" },
                            { title: "Ticket médio hoje", value: window.dashboardMoney(posStore.dashboard.today_count ? posStore.dashboard.today_cents / posStore.dashboard.today_count : 0), detail: "Faturamento ÷ vendas do dia" },
                            { title: "Estoque crítico", value: window.dashboardCount(posStore.dashboard.low_stock) + " produtos", detail: "Produtos ativos com saldo ≤ mínimo" },
                            { title: "Faturamento do mês", value: window.dashboardMoney(posStore.dashboard.month_cents), detail: "Vendas concluídas no mês local" },
                            { title: "Dinheiro no caixa", value: window.dashboardMoney(posStore.dashboard.cash_expected), detail: posStore.dashboard.cash_open ? "Saldo esperado da sessão aberta" : "Nenhum caixa aberto" }

                        ]

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 142
                            color: window.surface
                            radius: 8
                            border.color: "#e2e8e5"

                            Column {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 10
                                Label { text: modelData.title; color: window.muted; font.pixelSize: 13 }
                                Label { text: modelData.value; color: window.ink; font.pixelSize: 25; font.bold: true }
                                Label { text: modelData.detail; color: window.muted; font.pixelSize: 12; elide: Text.ElideRight; width: parent.width }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: window.activeSection === "Dashboard"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: window.surface
                    radius: 8
                    border.color: "#e2e8e5"

                    Column {
                        anchors.centerIn: parent
                        spacing: 8
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: posStore.dashboardError.length ? "Falha ao atualizar o painel" : window.dashboardCount(posStore.dashboard.no_stock) + " produtos sem estoque"; color: window.ink; font.pixelSize: 20; font.bold: true }
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: posStore.dashboardError.length ? "Não foi possível consultar os dados locais." : "Atualizado em " + (posStore.dashboard.updated_at || ""); color: window.muted; font.pixelSize: 14 }
                        Button { anchors.horizontalCenter: parent.horizontalCenter; text: "Atualizar painel"; onClicked: posStore.refreshDashboard() }
                    }
                }
            }
        }
    }
}