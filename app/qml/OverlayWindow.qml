import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

ApplicationWindow {
    id: overlayWindow
    width: 600
    height: 400
    visible: false
    flags: Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint
    color: "transparent"
    
    Rectangle {
        anchors.fill: parent
        color: Kirigami.Theme.backgroundColor
        opacity: 0.95
        radius: 10
    }
}
