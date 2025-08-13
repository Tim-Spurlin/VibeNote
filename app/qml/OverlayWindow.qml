import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtGraphicalEffects 1.15
import org.kde.kirigami 2.20 as Kirigami
import org.saphyre.vibenote 1.0

Window {
    id: overlayWindow
    flags: Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.Tool
    color: "transparent"
    width: 600
    height: 400
    x: (Screen.width - width) / 2
    y: Screen.height * 0.3
    
    // Semi-transparent background with blur
    Rectangle {
        anchors.fill: parent
        color: Kirigami.Theme.backgroundColor
        opacity: 0.95
        radius: 12
        
        // Drop shadow
        layer.enabled: true
        layer.effect: DropShadow {
            transparentBorder: true
            horizontalOffset: 0
            verticalOffset: 4
            radius: 20
            samples: 41
            color: "#40000000"
        }
    }
    
    // Main content
    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15
        
        // Input field
        TextField {
            id: queryInput
            width: parent.width
            placeholderText: "Ask VibeNote anything... (Esc to close)"
            font.pixelSize: 16
            
            Keys.onEscapePressed: {
                Overlay.hideOverlay()
            }
            
            Keys.onReturnPressed: {
                if (text.length > 0) {
                    Overlay.submitQuery(text)
                    text = ""
                }
            }
            
            Component.onCompleted: forceActiveFocus()
        }
        
        // Response area
        ScrollView {
            width: parent.width
            height: parent.height - queryInput.height - parent.spacing
            
            TextArea {
                id: responseArea
                text: Overlay.response
                readOnly: true
                wrapMode: Text.WordWrap
                selectByMouse: true
                font.pixelSize: 14
                color: Kirigami.Theme.textColor
                
                // Loading indicator
                BusyIndicator {
                    anchors.centerIn: parent
                    running: Overlay.loading
                    visible: running
                }
            }
        }
    }
    
    // Show/hide animations
    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }
    
    onVisibleChanged: {
        if (visible) {
            opacity = 1
            queryInput.forceActiveFocus()
        } else {
            opacity = 0
            queryInput.text = ""
            responseArea.text = ""
        }
    }
}
