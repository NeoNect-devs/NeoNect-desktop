// qml/components/NeoNectBrandButton.qml
import QtQuick
import QtQuick.Layouts

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
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 6

            Image {
                id: logoImage
                source: "qrc:/qt/qml/NeoNect/assets/NeoNect/icon.png"
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                Layout.alignment: Qt.AlignVCenter
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true

                scale: brandRoot.isHovered ? 1.06 : 1.0
                Behavior on scale {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutBack
                    }
                }
            }

            Row {
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                // "Neo": Clean, high-contrast off-white (#F0F4F8)
                Text {
                    text: "Neo"
                    color: "#F0F4F8"
                    font.family: "Segoe UI"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }

                // "Nect": Soft neon cyan (#C6EBF9) with soft cyan glow on hover
                Item {
                    width: nectText.implicitWidth
                    height: nectText.implicitHeight

                    // Soft cyan outer glow layer 1
                    Text {
                        anchors.centerIn: parent
                        text: "Nect"
                        color: "#C6EBF9"
                        font.family: nectText.font.family
                        font.pixelSize: nectText.font.pixelSize
                        font.weight: nectText.font.weight
                        opacity: brandRoot.isHovered ? 0.45 : 0.0
                        scale: 1.12

                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                    }

                    // Soft cyan outer glow layer 2
                    Text {
                        anchors.centerIn: parent
                        text: "Nect"
                        color: "#C6EBF9"
                        font.family: nectText.font.family
                        font.pixelSize: nectText.font.pixelSize
                        font.weight: nectText.font.weight
                        opacity: brandRoot.isHovered ? 0.25 : 0.0
                        scale: 1.25

                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                    }

                    // Foreground "Nect" text
                    Text {
                        id: nectText
                        anchors.centerIn: parent
                        text: "Nect"
                        color: "#C6EBF9"
                        font.family: "Segoe UI"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }
                }
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
