// qml/containers/AvilaButton.qml
import QtQuick
import QtQuick.Controls
import Avila 1.0

Item {
    id: control

    // Existing properties
    property string text: ""
    //property bool enabled: true
    property bool highlighted: true // true = primary solid color, false = secondary outline
    // New tooltip property
    property string tooltip: ""

    signal clicked()

    implicitWidth: 200
    implicitHeight: 45

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: control.enabled
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: control.clicked()
    }
    // Tooltip handling using Qt Quick Controls 2 ToolTip
    ToolTip.visible: mouseArea.containsMouse && control.tooltip.length > 0
    ToolTip.delay: 500
    ToolTip.text: control.tooltip

    // Button Background Canvas
    Rectangle {
        anchors.fill: parent
        radius: 8
        color: {
            if (!control.enabled) return "#252532";
            if (control.highlighted) {
                return mouseArea.pressed ? Qt.darker(ThemeData.accentColor, 1.15) :
                       mouseArea.containsMouse ? Qt.lighter(ThemeData.accentColor, 1.1) : ThemeData.accentColor;
            } else {
                return "transparent";
            }
        }
        border.color: {
            if (control.highlighted || !control.enabled) return "transparent";
            return mouseArea.containsMouse ? ThemeData.accentColor : Qt.rgba(ThemeData.accentColor.r, ThemeData.accentColor.g, ThemeData.accentColor.b, 0.3);
        }
        border.width: control.highlighted ? 0 : 1.5

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }

    // Button Text
    Text {
        text: control.text
        font.pointSize: ThemeData.fontSizeNormal
        font.bold: true
        color: {
            if (!control.enabled) return "#55ffffff";
            return control.highlighted ? "#ffffff" : ThemeData.accentColor;
        }
        anchors.centerIn: parent
    }
}
