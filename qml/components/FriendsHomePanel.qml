// qml/components/FriendsHomePanel.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila 1.0
import Avila.Core 1.0

Rectangle {
    id: root
    color: ThemeData.windowBackground

    signal messageFriendRequested(string username)
    signal addFriendSubmitted(string username)

    property string activeTab: "online" // "online", "all", "pending", "blocked", "add_friend"
    property string searchQuery: ""

    function getStatusColor(st) {
        switch(st) {
            case "online": return "#23A55A"; // Emerald Green
            case "afk":
            case "idle": return "#FAA81A";   // Amber Yellow
            case "dnd": return "#F23F43";    // Crimson Red
            case "offline":
            default: return "#80848E";      // Muted Gray
        }
    }

    function getStatusLabel(st) {
        switch(st) {
            case "online": return "Online";
            case "afk":
            case "idle": return "Idle / AFK";
            case "dnd": return "Do Not Disturb";
            case "offline":
            default: return "Offline";
        }
    }

    // Master Friends Model (Mocked for testing with rich activity & real-time statuses)
    property var allFriends: [
        { name: "Alex", tag: "#1337", status: "online", customStatus: "Developing Avila E2EE relay node", avatarColor: "#0A84FF" },
        { name: "Beatrice", tag: "#2048", status: "online", customStatus: "Listening to Spotify • Synthwave", avatarColor: "#06B6D4" },
        { name: "Charlie", tag: "#4096", status: "afk", customStatus: "AFK • Grabbing coffee ☕", avatarColor: "#10B981" },
        { name: "David", tag: "#8192", status: "offline", customStatus: "Last seen 2 hours ago", avatarColor: "#F59E0B" },
        { name: "Eva", tag: "#9901", status: "dnd", customStatus: "Do Not Disturb • Deep Focus Mode", avatarColor: "#EC4899" },
        { name: "Frank", tag: "#3321", status: "offline", customStatus: "Offline", avatarColor: "#3B82F6" },
        { name: "Grace", tag: "#7744", status: "online", customStatus: "Reviewing security pull requests", avatarColor: "#22C55E" },
        { name: "Henry", tag: "#5512", status: "afk", customStatus: "Away from desk", avatarColor: "#F97316" }
    ]

    function countOnline() {
        var c = 0;
        for (var i = 0; i < allFriends.length; i++) {
            if (allFriends[i].status === "online" || allFriends[i].status === "afk" || allFriends[i].status === "dnd") {
                c++;
            }
        }
        return c;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ─── TOP NAVIGATION HEADER BAR ───
        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: ThemeData.panelBackground
            border.color: Qt.darker(ThemeData.panelBackground, 1.25)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                // Friends Icon & Title
                RowLayout {
                    spacing: 8
                    IconImage {
                        source: "qrc:/qt/qml/Avila/assets/icons/friends.svg"
                        width: 20; height: 20
                        color: ThemeData.textSecondary
                    }

                    Text {
                        text: "Friends"
                        color: ThemeData.textPrimary
                        font.family: "Segoe UI"
                        font.pixelSize: 15
                        font.bold: true
                    }
                }

                Rectangle {
                    width: 1; height: 20
                    color: ThemeData.textSecondary
                    opacity: 0.25
                    Layout.leftMargin: 4; Layout.rightMargin: 4
                }

                // Tab: Online
                Rectangle {
                    height: 28
                    radius: 4
                    color: root.activeTab === "online" ? Qt.rgba(255, 255, 255, 0.1) : (onlineMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent")
                    implicitWidth: onlineText.implicitWidth + 16

                    Text {
                        id: onlineText
                        anchors.centerIn: parent
                        text: "Online (" + root.countOnline() + ")"
                        color: root.activeTab === "online" ? ThemeData.textPrimary : ThemeData.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                        font.weight: root.activeTab === "online" ? Font.DemiBold : Font.Normal
                    }

                    MouseArea {
                        id: onlineMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = "online"
                    }
                }

                // Tab: All
                Rectangle {
                    height: 28
                    radius: 4
                    color: root.activeTab === "all" ? Qt.rgba(255, 255, 255, 0.1) : (allMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent")
                    implicitWidth: allText.implicitWidth + 16

                    Text {
                        id: allText
                        anchors.centerIn: parent
                        text: "All (" + root.allFriends.length + ")"
                        color: root.activeTab === "all" ? ThemeData.textPrimary : ThemeData.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                        font.weight: root.activeTab === "all" ? Font.DemiBold : Font.Normal
                    }

                    MouseArea {
                        id: allMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = "all"
                    }
                }

                // Tab: Pending
                Rectangle {
                    height: 28
                    radius: 4
                    color: root.activeTab === "pending" ? Qt.rgba(255, 255, 255, 0.1) : (pendingMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent")
                    implicitWidth: pendingText.implicitWidth + 16

                    Text {
                        id: pendingText
                        anchors.centerIn: parent
                        text: "Pending"
                        color: root.activeTab === "pending" ? ThemeData.textPrimary : ThemeData.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                        font.weight: root.activeTab === "pending" ? Font.DemiBold : Font.Normal
                    }

                    MouseArea {
                        id: pendingMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = "pending"
                    }
                }

                // Tab: Blocked
                Rectangle {
                    height: 28
                    radius: 4
                    color: root.activeTab === "blocked" ? Qt.rgba(255, 255, 255, 0.1) : (blockedMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent")
                    implicitWidth: blockedText.implicitWidth + 16

                    Text {
                        id: blockedText
                        anchors.centerIn: parent
                        text: "Blocked"
                        color: root.activeTab === "blocked" ? ThemeData.textPrimary : ThemeData.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                        font.weight: root.activeTab === "blocked" ? Font.DemiBold : Font.Normal
                    }

                    MouseArea {
                        id: blockedMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = "blocked"
                    }
                }

                // Tab: Add Friend Button (Green Pill)
                Rectangle {
                    height: 28
                    radius: 4
                    color: root.activeTab === "add_friend" ? "transparent" : (addMouse.containsMouse ? "#1E8E4D" : "#23A55A")
                    border.color: root.activeTab === "add_friend" ? "#23A55A" : "transparent"
                    border.width: 1
                    implicitWidth: addText.implicitWidth + 16

                    Text {
                        id: addText
                        anchors.centerIn: parent
                        text: "Add Friend"
                        color: root.activeTab === "add_friend" ? "#23A55A" : "#FFFFFF"
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    MouseArea {
                        id: addMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = "add_friend"
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        // ─── MAIN CONTENT VIEW ───
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // VIEW A: Add Friend Tab View
            ColumnLayout {
                visible: root.activeTab === "add_friend"
                anchors.fill: parent
                anchors.margins: 24
                spacing: 16

                Text {
                    text: "ADD FRIEND"
                    color: "#FFFFFF"
                    font.family: "Segoe UI"
                    font.pixelSize: 15
                    font.bold: true
                }

                Text {
                    text: "You can add friends with their Avila E2EE username or handle."
                    color: ThemeData.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 600
                    height: 48
                    radius: 8
                    color: "#111214"
                    border.color: addFriendInput.activeFocus ? "#0A84FF" : "#1E1F22"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14; anchors.rightMargin: 6
                        spacing: 10

                        TextInput {
                            id: addFriendInput
                            Layout.fillWidth: true
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.pixelSize: 14
                            clip: true
                            selectByMouse: true

                            Text {
                                text: "You can add friends by their Username"
                                color: "#6D6F78"
                                font.family: "Segoe UI"
                                font.pixelSize: 14
                                visible: !addFriendInput.text && !addFriendInput.activeFocus
                            }

                            onAccepted: sendReqBtn.clicked()
                        }

                        Rectangle {
                            id: sendReqBtn
                            width: 140; height: 36
                            radius: 4
                            color: addFriendInput.text.trim() ? (sendReqMouse.containsMouse ? "#0066CC" : "#0A84FF") : Qt.rgba(10, 132, 255, 0.4)
                            enabled: addFriendInput.text.trim() !== ""

                            Text {
                                anchors.centerIn: parent
                                text: "Send Friend Request"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                                font.bold: true
                            }

                            MouseArea {
                                id: sendReqMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: parent.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: {
                                    if (addFriendInput.text.trim() !== "") {
                                        root.addFriendSubmitted(addFriendInput.text.trim());
                                        addFriendInput.clear();
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // VIEW B: Friends List (Online / All / Pending / Blocked)
            ColumnLayout {
                visible: root.activeTab !== "add_friend"
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                // Search Bar
                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    radius: 6
                    color: "#111214"
                    border.color: searchInput.activeFocus ? "#0A84FF" : "#1E1F22"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10; anchors.rightMargin: 10
                        spacing: 8

                        IconImage {
                            source: "qrc:/qt/qml/Avila/assets/icons/search.svg"
                            width: 14; height: 14
                            color: ThemeData.textSecondary
                        }

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.pixelSize: 13
                            clip: true
                            selectByMouse: true
                            onTextChanged: root.searchQuery = text.toLowerCase()

                            Text {
                                text: "Search"
                                color: "#6D6F78"
                                font.family: "Segoe UI"
                                font.pixelSize: 13
                                visible: !searchInput.text && !searchInput.activeFocus
                            }
                        }

                        Rectangle {
                            visible: searchInput.text !== ""
                            width: 16; height: 16; radius: 8
                            color: Qt.rgba(255, 255, 255, 0.1)
                            Text { anchors.centerIn: parent; text: "✕"; color: "#949BA4"; font.pixelSize: 9 }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: searchInput.clear()
                            }
                        }
                    }
                }

                // Section Header Title
                Text {
                    text: {
                        if (root.activeTab === "online") return "ONLINE — " + root.countOnline();
                        if (root.activeTab === "all") return "ALL FRIENDS — " + root.allFriends.length;
                        if (root.activeTab === "pending") return "PENDING — 0";
                        return "BLOCKED — 0";
                    }
                    color: ThemeData.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 0.5
                    Layout.topMargin: 6
                }

                // Friends ListView
                ListView {
                    id: friendsListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4

                    model: {
                        var filtered = [];
                        for (var i = 0; i < root.allFriends.length; i++) {
                            var item = root.allFriends[i];
                            var matchesSearch = root.searchQuery === "" || item.name.toLowerCase().indexOf(root.searchQuery) !== -1 || item.tag.indexOf(root.searchQuery) !== -1;
                            if (!matchesSearch) continue;

                            if (root.activeTab === "online") {
                                if (item.status === "online" || item.status === "afk" || item.status === "dnd") {
                                    filtered.push(item);
                                }
                            } else if (root.activeTab === "all") {
                                filtered.push(item);
                            }
                        }
                        return (root.activeTab === "pending" || root.activeTab === "blocked") ? [] : filtered;
                    }

                    delegate: Rectangle {
                        id: friendDelegate
                        width: friendsListView.width
                        height: 56
                        radius: 8
                        color: delegateMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent"
                        border.color: delegateMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12; anchors.rightMargin: 12
                            spacing: 12

                            // Squircle Avatar with Status Pill
                            Item {
                                width: 36; height: 42
                                Layout.alignment: Qt.AlignVCenter

                                Rectangle {
                                    id: avatarBox
                                    width: 36; height: 36
                                    anchors.top: parent.top
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    radius: 8
                                    color: modelData.avatarColor || "#0A84FF"

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.name ? modelData.name.charAt(0) : "?"
                                        color: "#FFFFFF"
                                        font.bold: true
                                        font.pixelSize: 15
                                    }
                                }

                                // Status Pill Under Avatar
                                Rectangle {
                                    id: statusPill
                                    width: 24
                                    height: 7
                                    radius: 3.5
                                    color: root.getStatusColor(modelData.status)
                                    border.color: ThemeData.windowBackground
                                    border.width: 1.2
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }

                            // Name & Activity Details
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Layout.alignment: Qt.AlignVCenter

                                RowLayout {
                                    spacing: 4
                                    Text {
                                        text: modelData.name
                                        color: "#FFFFFF"
                                        font.family: "Segoe UI"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }
                                    Text {
                                        text: modelData.tag || ""
                                        color: "#949BA4"
                                        font.family: "Segoe UI"
                                        font.pixelSize: 12
                                        visible: delegateMouse.containsMouse
                                    }
                                }

                                Text {
                                    text: modelData.customStatus || root.getStatusLabel(modelData.status)
                                    color: "#949BA4"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            // Action Buttons (Message & Call)
                            RowLayout {
                                spacing: 8
                                Layout.alignment: Qt.AlignVCenter

                                // Message Action Button
                                Rectangle {
                                    width: 36; height: 36
                                    radius: 18
                                    color: msgBtnMouse.containsMouse ? Qt.rgba(10, 132, 255, 0.25) : "#1E1F22"
                                    border.color: msgBtnMouse.containsMouse ? "#0A84FF" : "transparent"
                                    border.width: 1

                                    IconImage {
                                        anchors.centerIn: parent
                                        source: "qrc:/qt/qml/Avila/assets/icons/chat.svg"
                                        width: 18; height: 18
                                        color: msgBtnMouse.containsMouse ? "#0A84FF" : "#B5BAC1"
                                    }

                                    MouseArea {
                                        id: msgBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            root.messageFriendRequested(modelData.name.toLowerCase());
                                        }
                                    }
                                }

                                // Voice Call Action Button
                                Rectangle {
                                    width: 36; height: 36
                                    radius: 18
                                    color: callBtnMouse.containsMouse ? Qt.rgba(35, 165, 90, 0.25) : "#1E1F22"
                                    border.color: callBtnMouse.containsMouse ? "#23A55A" : "transparent"
                                    border.width: 1

                                    IconImage {
                                        anchors.centerIn: parent
                                        source: "qrc:/qt/qml/Avila/assets/icons/headphones.svg"
                                        width: 18; height: 18
                                        color: callBtnMouse.containsMouse ? "#23A55A" : "#B5BAC1"
                                    }

                                    MouseArea {
                                        id: callBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: delegateMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onDoubleClicked: {
                                root.messageFriendRequested(modelData.name.toLowerCase());
                            }
                        }
                    }
                }

                // Empty State Placeholder
                ColumnLayout {
                    visible: friendsListView.count === 0 && root.activeTab !== "add_friend"
                    Layout.alignment: Qt.AlignCenter
                    Layout.fillHeight: true
                    spacing: 12

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 64; height: 64; radius: 32
                        color: Qt.rgba(255, 255, 255, 0.05)

                        IconImage {
                            anchors.centerIn: parent
                            source: "qrc:/qt/qml/Avila/assets/icons/friends.svg"
                            width: 32; height: 32
                            color: ThemeData.textSecondary
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: {
                            if (root.activeTab === "pending") return "There are no pending friend requests.";
                            if (root.activeTab === "blocked") return "You haven't blocked anyone.";
                            if (root.activeTab === "online") return "No one is around to play with Wumpus.";
                            return "No friends found matching your search.";
                        }
                        color: ThemeData.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 14
                    }
                }
            }
        }
    }
}
