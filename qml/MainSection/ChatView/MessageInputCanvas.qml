// MessageInputCanvas.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Avila 1.0

Rectangle {
    id: root
    color: "#202225"
    radius: 8

    signal sendMessage(string msgText)
    signal inputHeightChanged()

    onHeightChanged: inputHeightChanged()

    readonly property int minHeight: 28
    readonly property int maxHeight: 100

    Layout.preferredHeight: Math.max(minHeight, Math.min(messageInput.implicitHeight, maxHeight))

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 10

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            rightPadding: 6
            clip: true
            // --- FIX: All vertical scrollbar properties are now in one block ---
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: ScrollBar {
                id: inputScrollbar
                policy: ScrollBar.AsNeeded // The policy is now set here
                width: 8
                size: 0.6
                position: 0.2
                active: true
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
                background: Rectangle {
                    color: "transparent"
                }
                orientation: Qt.Vertical
                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 5
                    implicitHeight: scrollView.height
                    color: inputScrollbar.pressed ? Qt.darker("grey", 1.5) : "grey"
                    opacity: inputScrollbar.policy === ScrollBar.AlwaysOn || (inputScrollbar.active && inputScrollbar.size < 1.0) ? 0.75 : 0
                }
            }

            TextArea {
                id: messageInput
                width: scrollView.availableWidth

                placeholderText: "Type a message..."
                color: "#DCDDDE"
                placeholderTextColor: "#444444"
                font.pixelSize: 14
                wrapMode: Text.Wrap

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Return && !(event.modifiers & Qt.ShiftModifier)) {
                        if (text.trim() !== "") {
                            root.sendMessage(text);
                            text = "";
                        }
                        event.accepted = true;
                    }
                }
            }
        }

        Rectangle {
            id: btnSend
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            Layout.alignment: Qt.AlignBottom
            color: "transparent"

            Text {
                text: "➤"
                anchors.centerIn: parent
                color: "white"
                font.pixelSize: 22
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (messageInput.text.trim() !== "") {
                        root.sendMessage(messageInput.text);
                        messageInput.text = "";
                    }
                }
            }

        }

    }
}
