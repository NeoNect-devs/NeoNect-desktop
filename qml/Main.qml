// qml/Main.qml
import QtQuick
import QtQuick.Layouts
import Avila 1.0
import "entrypage"
import "MainSection"
import "SideSection"

Window {
    id: root
    color: ThemeData.mainWindowBackground
    width: 640
    height: 640
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint

    // ===========================================================
    // APPLICATION WORKSPACE STATE PROPERTIES
    // ===========================================================
    property string appState: "gateway"
    property string activeTitleText: "Server Connection"

    property string sessionToken: ""
    property string verifiedServerUrl: ""

    Rectangle {
        id: mainBackground
        anchors.fill: parent
        color: root.color

        // ===========================================================
        // MAIN ROUTING GATEWAY LAYER (LOADER)
        // ===========================================================
        Loader {
            id: viewFlowLoader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: root.appState === "gateway" ? titleBar.bottom : parent.top

            source: root.appState === "gateway" ? "entrypage/entry.qml" : ""
            sourceComponent: root.appState === "authenticated" ? chatDashboardComponent : null

            Connections {
                target: viewFlowLoader.item
                ignoreUnknownSignals: true
                function onRequestTitleChange(newTitle) { root.activeTitleText = newTitle }
                function onAuthenticationSuccess(token, serverUrl) {
                    root.sessionToken = token
                    root.verifiedServerUrl = serverUrl
                    root.appState = "authenticated"
                }
            }
        }

        // ===========================================================
        // DYNAMIC CONTEXT-AWARE SYSTEM TITLE BAR
        // ===========================================================
        Rectangle {
            id: titleBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 45
            z: 100
            color: root.appState === "gateway" ? Qt.darker(ThemeData.mainWindowBackground, 1.15) : "transparent"

            // PURE DRAG HANDLE
            MouseArea {
                anchors.fill: parent
                property point clickPos: Qt.point(0, 0)

                onPressed: (mouse) => {
                    clickPos = Qt.point(mouse.x, mouse.y)
                }

                onPositionChanged: (mouse) => {
                    if (root.visibility === Window.Maximized) {
                        let ratioX = mouse.x / root.width
                        root.visibility = Window.Windowed
                        clickPos.x = root.width * ratioX
                        clickPos.y = mouse.y
                        return;
                    }

                    let delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                    root.x += delta.x
                    root.y += delta.y
                }

                onDoubleClicked: {
                    if (root.appState === "authenticated") {
                        root.visibility = (root.visibility === Window.Maximized) ? Window.Windowed : Window.Maximized
                    }
                }
            }

            // --- VIEWPORT STATE A: INITIAL SECURITY GATEWAY LAYOUT ---
            RowLayout {
                anchors.fill: parent
                visible: root.appState === "gateway"
                spacing: 0

                Rectangle {
                    Layout.leftMargin: 10
                    Layout.alignment: Qt.AlignVCenter
                    width: 35; height: 35; radius: 4
                    color: backMouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
                    visible: viewFlowLoader.item && viewFlowLoader.item.currentSubScreen !== "address_entry"

                    Image {
                        anchors.centerIn: parent
                        width: 18; height: 18
                        source: "../assets/icons/arrow-back.svg"
                        fillMode: Image.PreserveAspectFit
                    }
                    MouseArea {
                        id: backMouseArea; anchors.fill: parent; hoverEnabled: true
                        onClicked: { viewFlowLoader.item.currentSubScreen = "address_entry" }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: root.activeTitleText
                    color: "gray"
                    font.pointSize: 11
                }

                Rectangle {
                    Layout.rightMargin: 10
                    Layout.alignment: Qt.AlignVCenter
                    width: 35; height: 35; radius: 4
                    color: closeMouseArea.containsMouse ? "#e81123" : "transparent"
                    Text { anchors.centerIn: parent; text: "✕"; color: closeMouseArea.containsMouse ? "white" : "gray"; font.pointSize: 12 }
                    MouseArea { id: closeMouseArea; anchors.fill: parent; hoverEnabled: true; onClicked: root.close() }
                }
            }

            // --- VIEWPORT STATE B: AUTHENTICATED CHAT DASHBOARD WINDOW ---
            Item {
                anchors.fill: parent
                visible: root.appState === "authenticated"

                // BRANDING CONTAINER PILL
                Item {
                    id: brandingContainer
                    anchors.left: parent.left
                    anchors.leftMargin: 0
                    anchors.bottom: parent.bottom

                    height: 60
                    width: logoRow.implicitWidth + 16
                    z: 105

                    // FIX: TRUE GRADIENT BORDER LAYOUT
                    // Layer 1: The back layer that holds the raw gradient filling the whole area
                    Rectangle {
                        id: gradientBorderLayer
                        anchors.fill: parent

                        topLeftRadius: 0
                        bottomLeftRadius: 0
                        topRightRadius: 16
                        bottomRightRadius: 16

                        opacity: brandingMouseArea.containsMouse ? 1.0 : 0.0
                        Behavior on opacity { NumberAnimation { duration: 200 } }

                        gradient: Gradient {
                            orientation: Gradient.Diagonal
                            GradientStop { position: 0.0; color: "#FFFFFF" }
                            GradientStop { position: 1.0; color: "#3A3A3A" }
                        }
                    }

                    // Layer 2: The solid background mask stacked over layer 1 with a 1px gap.
                    // This leaves exactly a 1px gradient outline around the right/top sides!
                    Rectangle {
                        id: innerSolidBackground
                        anchors.fill: parent
                        // Inset by 1px on the sides we want the border to show on
                        anchors.topMargin: gradientBorderLayer.opacity > 0 ? 1 : 0
                        anchors.rightMargin: gradientBorderLayer.opacity > 0 ? 1 : 0
                        anchors.bottomMargin: gradientBorderLayer.opacity > 0 ? 1 : 0

                        topLeftRadius: 0
                        bottomLeftRadius: 0
                        topRightRadius: gradientBorderLayer.opacity > 0 ? 15 : 16 // Clamps nicely to matching curves
                        bottomRightRadius: gradientBorderLayer.opacity > 0 ? 15 : 16

                        color: "#101210" // Solid dark color restored safely
                    }

                    Row {
                        id: logoRow
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        spacing: 2

                        // LARGE LOGO CELL
                        Item {
                            width: 48
                            height: 48
                            anchors.verticalCenter: parent.verticalCenter

                            Image {
                                id: chatLogo
                                anchors.fill: parent
                                source: "../assets/logo.png"
                                fillMode: Image.PreserveAspectFit
                                antialiasing: true

                                scale: brandingMouseArea.containsMouse ? 1.06 : 1.0
                                opacity: brandingMouseArea.containsMouse ? 1.0 : 0.88

                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                            }
                        }

                        // BRANDING TEXT
                        Text {
                            text: "Avila"
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.5
                            renderType: Text.NativeRendering
                            anchors.verticalCenter: parent.verticalCenter

                            opacity: brandingMouseArea.containsMouse ? 1.0 : 0.9
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                        }
                    }

                    // HOVER MASTER TRACKER
                    MouseArea {
                        id: brandingMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.ArrowCursor
                    }
                }

                RowLayout {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 8
                    spacing: 4

                    Rectangle {
                        Layout.preferredWidth: 38; Layout.preferredHeight: 32; radius: 4
                        color: minM.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                        Text { anchors.centerIn: parent; text: "—"; color: "white"; font.pointSize: 10 }
                        MouseArea { id: minM; anchors.fill: parent; hoverEnabled: true; onClicked: root.visibility = Window.Minimized }
                    }

                    Rectangle {
                        Layout.preferredWidth: 38; Layout.preferredHeight: 32; radius: 4
                        color: maxM.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: root.visibility === Window.Maximized ? "🗗" : "🗖"
                            color: "white"; font.pointSize: 11
                        }
                        MouseArea {
                            id: maxM; anchors.fill: parent; hoverEnabled: true
                            onClicked: root.visibility = (root.visibility === Window.Maximized) ? Window.Windowed : Window.Maximized
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 38; Layout.preferredHeight: 32; radius: 4
                        color: chatCloseM.containsMouse ? "#e81123" : "transparent"
                        Text { anchors.centerIn: parent; text: "✕"; color: "white"; font.pointSize: 11 }
                        MouseArea { id: chatCloseM; anchors.fill: parent; hoverEnabled: true; onClicked: root.close() }
                    }
                }
            }
        }

        // ===========================================================
        // PRE-COMPILED CONTEXT CONTAINER ARCHITECTURES
        // ===========================================================
        Component {
            id: chatDashboardComponent

            RowLayout {
                spacing: 0

                SidebarCanvas {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 70
                    Layout.topMargin: 41
                }

                MainPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: 40
                }
            }
        }

        // ===========================================================
        // CROSS-PLATFORM FRAMELESS RESIZE HITBOXES
        // ===========================================================
        Item {
            anchors.fill: parent
            z: 101
            visible: root.appState === "authenticated" && root.visibility !== Window.Maximized

            // Left Edge
            MouseArea {
                width: 6; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                cursorShape: Qt.SizeHorCursor
                onPressed: root.startSystemResize(Qt.LeftEdge)
            }
            // Right Edge
            MouseArea {
                width: 6; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
                cursorShape: Qt.SizeHorCursor
                onPressed: root.startSystemResize(Qt.RightEdge)
            }
            // Bottom Edge
            MouseArea {
                height: 6; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
                cursorShape: Qt.SizeVerCursor
                onPressed: root.startSystemResize(Qt.BottomEdge)
            }
            // Top Edge
            MouseArea {
                height: 6; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                cursorShape: Qt.SizeVerCursor
                onPressed: root.startSystemResize(Qt.TopEdge)
            }
            // Bottom-Right Corner
            MouseArea {
                width: 10; height: 10; anchors.right: parent.right; anchors.bottom: parent.bottom
                cursorShape: Qt.SizeFDiagCursor
                onPressed: root.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
            }
            // Bottom-Left Corner
            MouseArea {
                width: 10; height: 10; anchors.left: parent.left; anchors.bottom: parent.bottom
                cursorShape: Qt.SizeBDiagCursor
                onPressed: root.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
            }
            // Top-Right Corner
            MouseArea {
                width: 10; height: 10; anchors.right: parent.right; anchors.top: parent.top
                cursorShape: Qt.SizeBDiagCursor
                onPressed: root.startSystemResize(Qt.RightEdge | Qt.TopEdge)
            }
            // Top-Left Corner
            MouseArea {
                width: 10; height: 10; anchors.left: parent.left; anchors.top: parent.top
                cursorShape: Qt.SizeFDiagCursor
                onPressed: root.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
            }
        }
    }
}
