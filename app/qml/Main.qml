import QtQuick 2.15
import QtQuick.Controls 2.15
import org.kde.kirigami 2.20 as Kirigami
import org.saphyre.vibenote 1.0

Kirigami.ApplicationWindow {
    id: root
    title: "VibeNote"
    width: 1200
    height: 800

    // Access singletons
    property var settings: Settings
    property var api: Api
    property var overlay: Overlay
    property var metrics: Metrics

    // Global drawer with navigation
    globalDrawer: Kirigami.GlobalDrawer {
        title: "VibeNote"
        titleIcon: "org.saphyre.vibenote"
        modal: false
        width: 250

        actions: [
            Kirigami.Action {
                text: "Dashboard"
                icon.name: "dashboard-show"
                onTriggered: pageStack.replace("qrc:/DashboardPage.qml")
            },
            Kirigami.Action {
                text: "Notes"
                icon.name: "view-list-text"
                onTriggered: pageStack.replace("qrc:/NotesPage.qml")
            },
            Kirigami.Action {
                text: "Settings"
                icon.name: "settings-configure"
                onTriggered: pageStack.replace("qrc:/SettingsPage.qml")
            },
            Kirigami.Action {
                separator: true
            },
            Kirigami.Action {
                text: "Export"
                icon.name: "document-export"
                onTriggered: exportDialog.open()
            }
        ]
    }

    // Page stack for navigation
    pageStack.initialPage: DashboardPage {}

    // Include overlay component
    OverlayWindow {
        id: overlayWindow
        visible: overlay.visible
    }

    // Export dialog
    ExportDialog {
        id: exportDialog
    }

    // Status bar
    footer: StatusBar {
        height: 30
    }

    // Handle daemon connection
    Component.onCompleted: {
        api.getStatus()
    }

    // Error handling
    Connections {
        target: api
        function onError(message) {
            showPassiveNotification(message, "long")
        }
    }
}
