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

    // Explicit sizing constraint system declarations
    readonly property int minHeight: 44
    readonly property int maxHeight: 120
    implicitHeight: Math.max(minHeight, Math.min(scrollView.contentHeight + 12, maxHeight))

    signal sendMessage(string msgText)
    signal inputHeightChanged()

    onHeightChanged: inputHeightChanged()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Forces programmatic height boundaries calculation matrices
            contentHeight: messageInput.implicitHeight

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: ScrollBar {
                id: inputScrollbar
                policy: ScrollBar.AsNeeded
                width: 6

                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: "grey"
                }
            }

            TextArea {
                id: messageInput
                width: scrollView.availableWidth
                placeholderText: "Type a message..."
                color: "#DCDDDE"
                placeholderTextColor: "#72767d"
                font.family: "Segoe UI"
                font.pixelSize: 14
                wrapMode: Text.Wrap

                // Align input text vertically inside fields center track safely
                verticalAlignment: Text.AlignVCenter
                background: null
                leftPadding: 0
                rightPadding: 6
                topPadding: 10
                bottomPadding: 10

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

        // Send Pill Action Control Button Box
        Item {
            id: btnSend
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignBottom
            Layout.bottomMargin: 8

            Text {
                text: "➤"
                anchors.centerIn: parent
                color: messageInput.text.trim() !== "" ? "#00A36C" : "#72767d"
                font.pixelSize: 18

                Behavior on color { ColorAnimation { duration: 100 } }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: messageInput.text.trim() !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
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
