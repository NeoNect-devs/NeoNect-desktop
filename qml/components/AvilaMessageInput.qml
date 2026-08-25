import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila.Core 1.0

Rectangle {
    id: inputRoot

    property string channelName: "general"
    property bool isDM: false

    signal messageSent(string text)
    signal attachmentRequested()
    signal stickerRequested()

    implicitHeight: Math.min(Math.max(52, inputArea.contentHeight + 24), 160)
    color: ThemeData.inputBackgroundInactive
    radius: 10

    // Dynamic Gradient Outer Border
    Rectangle {
        id: borderContainer
        anchors.fill: parent
        radius: parent.radius
        color: inputArea.activeFocus ? "transparent" : ThemeData.inputSolidBorder

        // Animated Color-Shifting Gradient Frame
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: inputArea.activeFocus

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { id: gradStop1; position: 0.0; color: "#5865F2" }
                GradientStop { id: gradStop2; position: 0.5; color: "#EB459E" }
                GradientStop { id: gradStop3; position: 1.0; color: "#00A36C" }
            }

            ParallelAnimation {
                running: inputArea.activeFocus
                loops: Animation.Infinite

                SequentialAnimation {
                    ColorAnimation { target: gradStop1; property: "color"; to: "#EB459E"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop1; property: "color"; to: "#00A36C"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop1; property: "color"; to: "#5865F2"; duration: 2500; easing.type: Easing.InOutSine }
                }

                SequentialAnimation {
                    ColorAnimation { target: gradStop2; property: "color"; to: "#00A36C"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop2; property: "color"; to: "#5865F2"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop2; property: "color"; to: "#EB459E"; duration: 2500; easing.type: Easing.InOutSine }
                }

                SequentialAnimation {
                    ColorAnimation { target: gradStop3; property: "color"; to: "#5865F2"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop3; property: "color"; to: "#EB459E"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop3; property: "color"; to: "#00A36C"; duration: 2500; easing.type: Easing.InOutSine }
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: inputArea.activeFocus ? 2 : 1
            radius: parent.radius > 2 ? parent.radius - 2 : 0
            color: ThemeData.inputBackgroundInactive
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Rectangle {
            width: 32; height: 32
            radius: 16
            color: attachMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
            Layout.alignment: Qt.AlignBottom

            IconImage {
                anchors.centerIn: parent
                source: "qrc:/qt/qml/Avila/assets/icons/plus-circle.svg"
                width: 20; height: 20
                color: attachMouse.containsMouse ? ThemeData.accentColor : ThemeData.textSecondary
            }

            MouseArea {
                id: attachMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: inputRoot.attachmentRequested()
            }
        }

        ScrollView {
            id: inputScrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            background: null

            ScrollBar.vertical: ScrollBar {
                id: inputScrollBar
                parent: inputScrollView
                anchors.top: inputScrollView.top
                anchors.right: inputScrollView.right
                anchors.bottom: inputScrollView.bottom
                width: 6
                policy: ScrollBar.AsNeeded
                active: true
                palette.window: "transparent"
                palette.base: "transparent"

                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: inputScrollBar.pressed ? ThemeData.accentColor : (inputScrollBar.hovered ? "#7289DA" : "#4E5058")
                }
                background: Item {}
            }

            TextArea {
                id: inputArea
                width: inputScrollView.width
                placeholderText: "Message " + (inputRoot.isDM ? "@" : "#") + inputRoot.channelName
                placeholderTextColor: ThemeData.placeholderColor
                color: ThemeData.textPrimary
                font.family: "Segoe UI"
                font.pixelSize: 14
                wrapMode: Text.Wrap
                selectByMouse: true
                leftPadding: 4; rightPadding: 14
                topPadding: 6; bottomPadding: 6
                background: null

                Keys.onReturnPressed: function(event) {
                    if (event.modifiers & Qt.ShiftModifier) {
                        event.accepted = false;
                        return;
                    }
                    if (text.trim() !== "") {
                        inputRoot.messageSent(text.trim());
                        clear();
                    }
                    event.accepted = true;
                }
            }
        }

        Rectangle {
            width: 32; height: 32
            radius: 6
            color: stickerMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
            Layout.alignment: Qt.AlignBottom

            IconImage {
                anchors.centerIn: parent
                source: "qrc:/qt/qml/Avila/assets/icons/sticker.svg"
                width: 20; height: 20
                color: stickerMouse.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
            }

            MouseArea {
                id: stickerMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: inputRoot.stickerRequested()
            }
        }

        Rectangle {
            width: 32; height: 32
            radius: 6
            color: emojiMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
            Layout.alignment: Qt.AlignBottom

            IconImage {
                anchors.centerIn: parent
                source: "qrc:/qt/qml/Avila/assets/icons/smile.svg"
                width: 20; height: 20
                color: emojiMouse.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
            }

            MouseArea {
                id: emojiMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: inputArea.insert(inputArea.cursorPosition, "😀")
            }
        }

        Rectangle {
            width: 32; height: 32
            radius: 6
            color: inputArea.text.trim() !== "" ? ThemeData.accentColor : (sendMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent")
            Layout.alignment: Qt.AlignBottom

            IconImage {
                anchors.centerIn: parent
                source: "qrc:/qt/qml/Avila/assets/icons/send.svg"
                width: 18; height: 18
                color: inputArea.text.trim() !== "" ? "#FFFFFF" : ThemeData.textSecondary
            }

            MouseArea {
                id: sendMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (inputArea.text.trim() !== "") {
                        inputRoot.messageSent(inputArea.text.trim())
                        inputArea.clear()
                    }
                }
            }
        }
    }
}