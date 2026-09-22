import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0

Rectangle {
    id: headerRoot
    property string selectedServer: ""
    property string activeChannel: ""
    property bool membersPanelExpanded: false
    property string contactStatus: "offline"
    property int updateRevision: 0

    signal toggleMembersPanel()

    readonly property string contactDisplayName: {
        headerRoot.updateRevision;
        if (selectedServer === "dms") {
            if (activeChannel === "saved-messages") return "Saved Messages";
            if (typeof NetworkManager !== "undefined" && NetworkManager && activeChannel) {
                return NetworkManager.getDisplayName(activeChannel);
            }
            return activeChannel ? activeChannel.replace(/^\w/, c => c.toUpperCase()) : "";
        }
        return activeChannel;
    }

    readonly property string contactAvatarUrl: {
        headerRoot.updateRevision;
        if (selectedServer === "dms" && activeChannel && activeChannel !== "saved-messages") {
            if (typeof NetworkManager !== "undefined" && NetworkManager) {
                return NetworkManager.getAvatarUrl(activeChannel);
            }
        }
        return "";
    }

    function getStatusLabel(st) {
        var s = (st || "offline").toLowerCase();
        if (s === "online") return "Online";
        if (s === "afk" || s === "idle") return "Idle";
        if (s === "dnd") return "Do Not Disturb";
        return "Offline";
    }

    function getStatusColor(st) {
        var s = (st || "offline").toLowerCase();
        if (s === "online") return "#23A55A";
        if (s === "afk" || s === "idle") return "#FAA81A";
        if (s === "dnd") return "#F23F43";
        return "#80848E";
    }

    function updateContactStatus() {
        if (selectedServer === "dms" && activeChannel && activeChannel !== "saved-messages" && activeChannel !== "friends") {
            if (typeof NetworkManager !== "undefined" && NetworkManager && NetworkManager.getFriendStatus) {
                headerRoot.contactStatus = NetworkManager.getFriendStatus(activeChannel.toLowerCase());
            } else {
                headerRoot.contactStatus = "offline";
            }
            if (typeof NetworkManager !== "undefined" && NetworkManager) {
                NetworkManager.checkUserStatus(activeChannel.toLowerCase());
            }
        }
    }

    onActiveChannelChanged: {
        headerRoot.updateRevision++;
        updateContactStatus();
    }
    onSelectedServerChanged: {
        headerRoot.updateRevision++;
        updateContactStatus();
    }
    Component.onCompleted: updateContactStatus()

    Connections {
        target: (typeof NetworkManager !== "undefined") ? NetworkManager : null
        ignoreUnknownSignals: true
        function onFriendStatusUpdated(username, status) {
            if (headerRoot.selectedServer === "dms" && headerRoot.activeChannel.toLowerCase() === (username || "").toLowerCase()) {
                headerRoot.contactStatus = status;
            }
        }
        function onPeerDisplayNameUpdated(username, displayName) {
            if (headerRoot.selectedServer === "dms" && headerRoot.activeChannel.toLowerCase() === (username || "").toLowerCase()) {
                headerRoot.updateRevision++;
            }
        }
        function onDisplayNameChanged() {
            headerRoot.updateRevision++;
        }
        function onPeerAvatarUpdated(username, avatarUrl) {
            if (headerRoot.selectedServer === "dms" && headerRoot.activeChannel.toLowerCase() === (username || "").toLowerCase()) {
                headerRoot.updateRevision++;
            }
        }
        function onAvatarUrlChanged() {
            headerRoot.updateRevision++;
        }
        function onOpenConversationsChanged() {
            headerRoot.updateRevision++;
        }
        function onFriendsChanged() {
            headerRoot.updateRevision++;
        }
        function onIsConnectedChanged() {
            if (NetworkManager && !NetworkManager.isConnected) {
                headerRoot.contactStatus = "offline";
            } else {
                headerRoot.updateContactStatus();
            }
        }
    }

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

        // DM Contact Avatar (Squircle with 1:1 image & initial letter fallback)
        Item {
            visible: selectedServer === "dms" && activeChannel !== "saved-messages" && activeChannel !== "friends" && activeChannel !== ""
            width: 28; height: 28
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                anchors.fill: parent
                radius: 7
                color: ThemeData.accentColor

                CircularImage {
                    id: headerAvatarImg
                    anchors.fill: parent
                    source: headerRoot.contactAvatarUrl
                    cornerRadius: 7
                }

                Text {
                    anchors.centerIn: parent
                    visible: !headerAvatarImg.ready
                    text: headerRoot.contactDisplayName ? headerRoot.contactDisplayName.charAt(0).toUpperCase() : (headerRoot.activeChannel ? headerRoot.activeChannel.charAt(0).toUpperCase() : "@")
                    color: "#FFFFFF"
                    font.bold: true
                    font.pixelSize: 13
                }
            }
        }

        // Channel / Contact Title
        Text {
            text: headerRoot.contactDisplayName
            color: ThemeData.textPrimary
            font.family: "Segoe UI"
            font.pixelSize: 15
            font.bold: true
        }

        // Real-time Status Indicator Dot for Direct Messages
        Rectangle {
            visible: headerRoot.selectedServer === "dms" && headerRoot.activeChannel !== "saved-messages" && headerRoot.activeChannel !== "friends" && headerRoot.activeChannel !== ""
            width: 9
            height: 9
            radius: 4.5
            color: headerRoot.getStatusColor(headerRoot.contactStatus)
            border.color: Qt.rgba(0, 0, 0, 0.4)
            border.width: 1
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: -2

            Behavior on color { ColorAnimation { duration: 150 } }
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
                    var label = headerRoot.getStatusLabel(headerRoot.contactStatus);
                    return "@" + activeChannel + " • " + label + " • NeoNect Zero-Knowledge E2EE";
                }
                return "Secure Workspace Channel";
            }
            color: (headerRoot.selectedServer === "dms" && headerRoot.activeChannel !== "saved-messages") ? headerRoot.getStatusColor(headerRoot.contactStatus) : ThemeData.textSecondary
            font.family: "Segoe UI"
            font.pixelSize: 12
            elide: Text.ElideRight

            Behavior on color { ColorAnimation { duration: 150 } }
        }

        Rectangle {
            width: 32; height: 32
            radius: 6
            visible: headerRoot.selectedServer !== "dms" && headerRoot.selectedServer !== ""
            color: membersToggleMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"

            IconImage {
                anchors.centerIn: parent
                source: "qrc:/qt/qml/NeoNect/assets/icons/users.svg"
                width: 20; height: 20
                color: headerRoot.membersPanelExpanded ? ThemeData.textPrimary : ThemeData.textSecondary
            }

            MouseArea {
                id: membersToggleMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: headerRoot.toggleMembersPanel()
            }
        }
    }
}
