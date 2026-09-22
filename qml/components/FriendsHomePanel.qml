// qml/components/FriendsHomePanel.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0

Rectangle {
    id: root
    color: ThemeData.windowBackground

    signal messageFriendRequested(string username)
    signal addFriendSubmitted(string username)

    property string activeTab: "online" // "online", "all", "pending", "blocked", "add_friend"
    property string searchQuery: ""
    property var pendingRequests: NetworkManager.pendingRequests
    property var activeStatus: (typeof NetworkManager !== "undefined" && NetworkManager && NetworkManager.allFriendStatuses) ? NetworkManager.allFriendStatuses() : ({})
    property string addFriendStatusMsg: ""
    property bool addFriendSuccess: false

    Component.onCompleted: {
        if (typeof NetworkManager !== "undefined" && NetworkManager) {
            root.activeStatus = NetworkManager.allFriendStatuses();
            NetworkManager.checkFriendsStatus();
        }
    }

    Connections {
        target: NetworkManager
        ignoreUnknownSignals: true
        function onFriendStatusUpdated(username, status) {
            var copy = Object.assign({}, root.activeStatus);
            copy[username.toLowerCase()] = status;
            root.activeStatus = copy;
        }
        function onFriendsChanged() {
            if (typeof NetworkManager !== "undefined" && NetworkManager) {
                root.activeStatus = NetworkManager.allFriendStatuses();
            }
        }
        function onIsConnectedChanged() {
            if (!NetworkManager.isConnected) {
                var copy = Object.assign({}, root.activeStatus);
                for (var key in copy) {
                    copy[key] = "offline";
                }
                root.activeStatus = copy;
            } else {
                NetworkManager.checkFriendsStatus();
            }
        }
        function onAddFriendResult(success, message, username) {
            root.addFriendStatusMsg = message;
            root.addFriendSuccess = success;
        }
    }

    function getFriendStatus(username) {
        if (!username) return "offline";
        var u = username.toLowerCase();
        if (root.activeStatus && root.activeStatus[u] !== undefined) {
            return root.activeStatus[u];
        }
        if (typeof NetworkManager !== "undefined" && NetworkManager && NetworkManager.getFriendStatus) {
            return NetworkManager.getFriendStatus(u);
        }
        return "offline";
    }

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
    property var allFriends: typeof NetworkManager !== "undefined" ? NetworkManager.friends : []

    function countOnline() {
        var c = 0;
        for (var i = 0; i < allFriends.length; i++) {
            var f = allFriends[i];
            var st = root.getFriendStatus(f);
            if (st === "online" || st === "afk" || st === "idle" || st === "dnd") {
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
                        source: "qrc:/qt/qml/NeoNect/assets/icons/friends.svg"
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
                    implicitWidth: pendingRow.implicitWidth + 16

                    RowLayout {
                        id: pendingRow
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            id: pendingText
                            text: "Pending"
                            color: root.activeTab === "pending" ? ThemeData.textPrimary : ThemeData.textSecondary
                            font.family: "Segoe UI"
                            font.pixelSize: 13
                            font.weight: root.activeTab === "pending" ? Font.DemiBold : Font.Normal
                        }

                        Rectangle {
                            visible: root.pendingRequests && root.pendingRequests.length > 0
                            width: Math.max(16, pendingBadgeText.implicitWidth + 8)
                            height: 16
                            radius: 8
                            color: "#F23F43"

                            Text {
                                id: pendingBadgeText
                                anchors.centerIn: parent
                                text: root.pendingRequests ? root.pendingRequests.length : 0
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
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
                    visible: false
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

        // ─── MAIN CONTENT VIEW (Lazy Tab Loader) ───
        Loader {
            id: tabContentLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: root.activeTab === "add_friend" ? addFriendTabComponent : friendsListTabComponent
        }
    }

    // ─── COMPONENT 1: Add Friend Tab View ───
    Component {
        id: addFriendTabComponent

        ColumnLayout {
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
                text: "You can add friends with their NeoNect E2EE username or handle."
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

            Text {
                visible: root.addFriendStatusMsg !== ""
                text: root.addFriendStatusMsg
                color: root.addFriendSuccess ? "#23A55A" : "#F23F43"
                font.family: "Segoe UI"
                font.pixelSize: 13
                font.weight: Font.Medium
                Layout.topMargin: 10
            }

            Item { Layout.fillHeight: true }
        }
    }

    // ─── COMPONENT 2: Friends List (Online / All / Pending / Blocked) ───
    Component {
        id: friendsListTabComponent

        ColumnLayout {
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
                        source: "qrc:/qt/qml/NeoNect/assets/icons/search.svg"
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
                    if (root.activeTab === "pending") return "PENDING — " + root.pendingRequests.length;
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
                    var sourceList = root.activeTab === "pending" ? root.pendingRequests : root.allFriends;
                    for (var i = 0; i < sourceList.length; i++) {
                        var uName = sourceList[i];
                        var statusVal = root.getFriendStatus(uName);
                        var item = { name: uName, tag: "", status: statusVal, customStatus: "", avatarColor: "#0A84FF" };

                        if (root.searchQuery !== "" && item.name.toLowerCase().indexOf(root.searchQuery.toLowerCase()) === -1) {
                            continue;
                        }
                        if (root.activeTab === "online") {
                            if (statusVal !== "offline") {
                                filtered.push(item);
                            }
                        } else if (root.activeTab === "all" || root.activeTab === "pending") {
                            filtered.push(item);
                        }
                    }
                    return root.activeTab === "blocked" ? [] : filtered;
                }

                delegate: Rectangle {
                    id: friendDelegate
                    width: friendsListView.width
                    height: 56
                    radius: 8
                    color: delegateMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent"
                    border.color: delegateMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                    border.width: 1

                    MouseArea {
                        id: delegateMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        z: 0
                        onDoubleClicked: {
                            if (root.activeTab !== "pending") {
                                root.messageFriendRequested(modelData.name.toLowerCase());
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12; anchors.rightMargin: 12
                        spacing: 12
                        z: 1

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

                                CircularImage {
                                    id: friendAvatarImg
                                    anchors.fill: parent
                                    source: (NetworkManager && NetworkManager.getAvatarUrl) ? NetworkManager.getAvatarUrl(modelData.name) : ""
                                    cornerRadius: 8
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: !friendAvatarImg.ready
                                    text: (NetworkManager ? NetworkManager.getDisplayName(modelData.name) : modelData.name).charAt(0).toUpperCase()
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
                                spacing: 6
                                Text {
                                    text: (NetworkManager ? NetworkManager.getDisplayName(modelData.name) : modelData.name)
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                                Text {
                                    text: "@" + modelData.name
                                    color: "#949BA4"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 12
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

                        // Action Buttons (Message & Call or Accept & Reject)
                        RowLayout {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            // Pending Mode Actions: Accept and Decline Buttons
                            RowLayout {
                                visible: root.activeTab === "pending"
                                spacing: 8

                                // Accept Button
                                Rectangle {
                                    width: 36; height: 36
                                    radius: 18
                                    color: acceptBtnMouse.containsMouse ? Qt.rgba(35, 165, 90, 0.35) : Qt.rgba(35, 165, 90, 0.15)
                                    border.color: "#23A55A"
                                    border.width: 1

                                    IconImage {
                                        anchors.centerIn: parent
                                        source: "qrc:/qt/qml/NeoNect/assets/icons/check.svg"
                                        width: 18; height: 18
                                        color: "#23A55A"
                                    }

                                    MouseArea {
                                        id: acceptBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var targetName = modelData.name;
                                            NetworkManager.acceptFriend(targetName);
                                            if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                                NotificationManager.dismissBySender(targetName, "friend_request");
                                            }
                                        }
                                    }
                                }

                                // Decline Button
                                Rectangle {
                                    width: 36; height: 36
                                    radius: 18
                                    color: rejectBtnMouse.containsMouse ? Qt.rgba(242, 63, 67, 0.35) : Qt.rgba(242, 63, 67, 0.15)
                                    border.color: "#F23F43"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "✕"
                                        color: "#F23F43"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: rejectBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var targetName = modelData.name;
                                            NetworkManager.rejectFriend(targetName);
                                            if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                                NotificationManager.dismissBySender(targetName, "friend_request");
                                            }
                                        }
                                    }
                                }
                            }

                            // Normal Mode Actions: Message Button
                            RowLayout {
                                visible: root.activeTab !== "pending"
                                spacing: 8

                                // Message Action Button
                                Rectangle {
                                    width: 36; height: 36
                                    radius: 18
                                    color: msgBtnMouse.containsMouse ? Qt.rgba(10, 132, 255, 0.25) : "#1E1F22"
                                    border.color: msgBtnMouse.containsMouse ? "#0A84FF" : "transparent"
                                    border.width: 1

                                    IconImage {
                                        anchors.centerIn: parent
                                        source: "qrc:/qt/qml/NeoNect/assets/icons/chat.svg"
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
                            }
                        }
                    }
                }
            }

            // Empty State Placeholder
            ColumnLayout {
                visible: friendsListView.count === 0
                Layout.alignment: Qt.AlignCenter
                Layout.fillHeight: true
                spacing: 12

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 64; height: 64; radius: 32
                    color: Qt.rgba(255, 255, 255, 0.05)

                    IconImage {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/NeoNect/assets/icons/friends.svg"
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
