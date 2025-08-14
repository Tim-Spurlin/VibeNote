import QtQuick 2.15
import QtQuick.Controls 2.15
import org.kde.kirigami 2.20 as Kirigami

Kirigami.ApplicationWindow {
    id: root
    title: "VibeNote"
    width: 1024
    height: 768
    
    pageStack.initialPage: DashboardPage {}
}
