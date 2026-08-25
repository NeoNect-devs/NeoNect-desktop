// qml/components/ScrollableArea.qml
import QtQuick
import QtQuick.Controls

ScrollView {
    id: root

    // Crucial for desktop development: cuts off text/elements
    // that scroll past the boundary limits.
    clip: true

    // Modern, minimal custom vertical scrollbar
    ScrollBar.vertical: ScrollBar {
        id: verticalBar
        policy: ScrollBar.AsNeeded
        active: root.hovered || pressed
        width: 8

        background: Rectangle {
            color: "transparent"
        }

        contentItem: Rectangle {
            implicitWidth: 8
            radius: 4
            // Dynamic color changes based on user hover/press actions
            color: verticalBar.pressed ? "#4b5563" : (verticalBar.hovered ? "#9ca3af" : "#6b7280")

            // Fades into view when moving the mouse over the area, hides when idle
            opacity: verticalBar.active ? 1.0 : 0.0
            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }
        }
    }

    // Modern, minimal custom horizontal scrollbar
    ScrollBar.horizontal: ScrollBar {
        id: horizontalBar
        policy: ScrollBar.AsNeeded
        active: root.hovered || pressed
        height: 8

        background: Rectangle {
            color: "transparent"
        }

        contentItem: Rectangle {
            implicitHeight: 8
            radius: 4
            color: horizontalBar.pressed ? "#4b5563" : (horizontalBar.hovered ? "#9ca3af" : "#6b7280")

            opacity: horizontalBar.active ? 1.0 : 0.0
            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }
        }
    }
}
