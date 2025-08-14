import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

Kirigami.Page {
    title: "Dashboard"
    
    ColumnLayout {
        anchors.centerIn: parent
        
        Label {
            text: "Welcome to VibeNote"
            font.pixelSize: 24
        }
        
        Button {
            text: "Start Capture"
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
