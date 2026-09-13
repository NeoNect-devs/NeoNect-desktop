import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl

Rectangle {
    id: headerRoot
    property string selectedServer: ""
    property string activeChannel: ""
    property bool membersPanelExpanded: false
    signal toggleMembersPanel()

                Layout.fillWidth: true
                height: 48
                color: ThemeData.panelBackground
                border.color: Qt.darker(ThemeData.panelBackground, 1.25)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 10

                    // Server Channel Icon
                    IconImage {
                        visible: selectedServer !== "dms"
                        source: "qrc:/qt/qml/NeoNect/assets/icons/hash.svg"
                        width: 18; height: 18
                        color: ThemeData.textSecondary
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // Saved Messages Vector Icon
                    IconImage {
                        visible: selectedServer === "dms" && activeChannel === "saved-messages"
                        source: "qrc:/qt/qml/NeoNect/assets/icons/bookmark.svg"
                        width: 20; height: 20
                        color: "#00E5FF"
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // DM Contact @ Prefix
                    Text {
                        visible: selectedServer === "dms" && activeChannel !== "saved-messages"
                        text: "@"
                        color: ThemeData.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 20
                        font.weight: Font.Light
                    }

                    // Channel / Contact Title
                    Text {
                        text: {
                            if (selectedServer === "dms") {
                                if (activeChannel === "saved-messages") return "Saved Messages";
                                return activeChannel.replace(/^\w/, c => c.toUpperCase());
                            }
                            return activeChannel;
                        }
                        color: ThemeData.textPrimary
                        font.family: "Segoe UI"
                        font.pixelSize: 15
                        font.bold: true
                    }

                    Rectangle {
                        width: 1; height: 16
                        color: ThemeData.textSecondary
                        opacity: 0.3
                        Layout.leftMargin: 4; Layout.rightMargin: 4
                    }

                    // Channel / DM Subtitle
                    Text {
                        Layout.fillWidth: true
                        text: {
                            if (selectedServer === "dms") {
                                if (activeChannel === "saved-messages") return "Your Personal Cloud Storage & Notes";
                                return "Danisa Zero-Knowledge E2EE Direct Messages";
                            }
                            return "Secure Workspace Channel";
                        }
                        color: ThemeData.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        width: 32; height: 32
                        radius: 6
                        visible: selectedServer !== "dms"
                        color: membersToggleMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"

                        IconImage {
                            anchors.centerIn: parent
                            source: "qrc:/qt/qml/NeoNect/assets/icons/users.svg"
                            width: 20; height: 20
                            color: membersPanelExpanded ? ThemeData.textPrimary : ThemeData.textSecondary
                        }

                        MouseArea {
                            id: membersToggleMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: membersPanel.userToggledExpanded = !membersPanel.userToggledExpanded
                        }
                    }
                }
            }
