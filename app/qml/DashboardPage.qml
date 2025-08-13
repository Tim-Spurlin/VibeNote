import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtCharts 2.15
import org.kde.kirigami 2.20 as Kirigami
import org.saphyre.vibenote 1.0

Kirigami.ScrollablePage {
    title: "Dashboard"
    
    GridLayout {
        columns: 2
        columnSpacing: 15
        rowSpacing: 15
        
        // Note Rate Chart Card
        Kirigami.Card {
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            
            header: RowLayout {
                Kirigami.Heading {
                    text: "Note Generation Rate"
                    level: 3
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "AI Analysis"
                    icon.name: "ai-assistant"
                    onClicked: Metrics.requestAiAnalysis()
                }
            }
            
            contentItem: ChartView {
                antialiasing: true
                theme: ChartView.ChartThemeDark
                
                LineSeries {
                    name: "Notes/Hour"
                    axisX: ValueAxis {
                        min: 0
                        max: 60
                        labelFormat: "%d"
                    }
                    axisY: ValueAxis {
                        min: 0
                        max: Math.max(...Metrics.noteRateHistory) * 1.2
                    }
                    
                    Component.onCompleted: {
                        for (var i = 0; i < Metrics.noteRateHistory.length; i++) {
                            append(i, Metrics.noteRateHistory[i])
                        }
                    }
                }
            }
        }
        
        // GPU Usage Card  
        Kirigami.Card {
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            
            header: Kirigami.Heading {
                text: "GPU Utilization"
                level: 3
            }
            
            contentItem: Item {
                CircularGauge {
                    anchors.centerIn: parent
                    width: 200
                    height: 200
                    value: Metrics.currentGpuUsage
                    minimumValue: 0
                    maximumValue: 100
                    
                    style: CircularGaugeStyle {
                        needle: Rectangle {
                            width: 2
                            height: parent.height * 0.4
                            color: value > 85 ? "red" : value > 60 ? "yellow" : "green"
                        }
                    }
                }
                
                Label {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Math.round(Metrics.currentGpuUsage) + "%"
                    font.pixelSize: 24
                    font.bold: true
                }
            }
        }
        
        // Queue Status Card
        Kirigami.Card {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            
            header: Kirigami.Heading {
                text: "Queue Status"
                level: 3
            }
            
            contentItem: RowLayout {
                spacing: 20
                
                Column {
                    Label {
                        text: "Queue Depth"
                        opacity: 0.7
                    }
                    Label {
                        text: Metrics.queueDepth
                        font.pixelSize: 36
                        font.bold: true
                        color: Metrics.queueDepth > 10 ? "orange" : Kirigami.Theme.textColor
                    }
                }
                
                ProgressBar {
                    Layout.fillWidth: true
                    value: Metrics.queueDepth / 100
                    
                    background: Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        radius: 3
                    }
                }
            }
        }
        
        // Watch Mode Status Card
        Kirigami.Card {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            
            header: Kirigami.Heading {
                text: "Watch Mode"
                level: 3
            }
            
            contentItem: RowLayout {
                Switch {
                    id: watchModeSwitch
                    checked: Settings.watchModeEnabled
                    onToggled: {
                        Settings.watchModeEnabled = checked
                        Api.toggleWatchMode(checked)
                    }
                }
                
                Column {
                    Label {
                        text: watchModeSwitch.checked ? "Active" : "Inactive"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Label {
                        text: watchModeSwitch.checked ? 
                              "Capturing at " + Settings.captureFramerate + " fps" :
                              "Click to enable screen monitoring"
                        opacity: 0.7
                    }
                }
            }
        }
    }
    
    // Refresh timer
    Timer {
        interval: 5000
        running: true
        repeat: true
        onTriggered: Api.getStatus()
    }
}
