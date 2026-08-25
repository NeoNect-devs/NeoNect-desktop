// qml/components/AvilaBrandButton.qml
import QtQuick
import QtQuick.Layouts
import Avila 1.0

Item {
    id: brandRoot
    signal clicked

    width: 120
    height: 56

    property bool isHovered: brandMouseArea.containsMouse

    // ─── 1. OUTER ANIMATED GRADIENT & BORDER CONTAINER ──────────────────
    Rectangle {
        id: outerFrame
        anchors.fill: parent
        topLeftRadius: 0
        topRightRadius: 0
        bottomLeftRadius: 0
        bottomRightRadius: 16

        color: brandRoot.isHovered ? "transparent" : ThemeData.borderColor

        // Animated gradient overlay during hover
        Rectangle {
            anchors.fill: parent
            bottomRightRadius: parent.bottomRightRadius
            opacity: brandRoot.isHovered ? 1.0 : 0.0

            Behavior on opacity {
                NumberAnimation {
                    duration: 200
                }
            }

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: monoAnim.c1
                }
                GradientStop {
                    position: 0.5
                    color: monoAnim.c2
                }
                GradientStop {
                    position: 1.0
                    color: monoAnim.c3
                }
            }
        }
    }

    // ─── 2. INNER CONTENT CONTAINER (INSET BY 1.5px ON BOTTOM/RIGHT) ────
    Rectangle {
        id: innerBg
        anchors.top: parent.top
        anchors.left: parent.left
        width: parent.width - 1.5
        height: parent.height - 1.5

        topLeftRadius: 0
        topRightRadius: 0
        bottomLeftRadius: 0
        bottomRightRadius: Math.max(0, outerFrame.bottomRightRadius - 1.5)

        color: brandRoot.isHovered ? "#000000" : ThemeData.sidebarBackground

        Behavior on color {
            ColorAnimation {
                duration: 180
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 0
            anchors.rightMargin: 8
            spacing: 0

            Image {
                id: logoImage
                source: "../../assets/logo.png"
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true

                scale: brandRoot.isHovered ? 1.08 : 1.0
                Behavior on scale {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutBack
                    }
                }
            }

            Text {
                text: "AVILA"
                color: ThemeData.textPrimary
                font.family: "Segoe UI"
                font.pixelSize: 14
                font.weight: Font.Black
                font.letterSpacing: 1.8
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: brandMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: brandRoot.clicked()
        }
    }

    // ─── 3. DYNAMIC MONOCHROME SHIFT ANIMATION DRIVER ──────────────────
    Item {
        id: monoAnim
        property real shift: 0.0
        property color c1: Qt.rgba(0.5 + 0.5 * Math.sin(shift * 6.28), 0.5 + 0.5 * Math.sin(shift * 6.28), 0.5 + 0.5 * Math.sin(shift * 6.28), 1.0)
        property color c2: Qt.rgba(0.5 + 0.5 * Math.sin((shift + 0.33) * 6.28), 0.5 + 0.5 * Math.sin((shift + 0.33) * 6.28), 0.5 + 0.5 * Math.sin((shift + 0.33) * 6.28), 1.0)
        property color c3: Qt.rgba(0.5 + 0.5 * Math.sin((shift + 0.66) * 6.28), 0.5 + 0.5 * Math.sin((shift + 0.66) * 6.28), 0.5 + 0.5 * Math.sin((shift + 0.66) * 6.28), 1.0)

        NumberAnimation on shift {
            from: 0.0
            to: 1.0
            duration: 2500
            loops: Animation.Infinite
            running: brandRoot.isHovered
        }
    }
}
