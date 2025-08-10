// MemberListCanvas.qml
import QtQuick
import QtQuick.Layouts
import Avila 1.0
import "../ChatView/ChatHandler.js" as ChatHandler

Rectangle {
    id: root
    width: 175
    color: ThemeData.viewsBackground
    // Anchors are controlled by the parent (MainPanel.qml)

    // Data model to hold the list of members
    ListModel {
        id: memberModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 5
        spacing: 10

        // Header for the member list
        Text {
            text: "MEMBERS"
            font.bold: true
            color: "#8e9297"
            font.pixelSize: 12
            Layout.topMargin: 10
        }

        // ListView to display each member
        ListView {
            id: memberListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: memberModel
            spacing: 1

            // Use the MemberItem component as the delegate
            delegate: MemberItem {
                // Bind the component's properties to the model's data
                memberName: model.name
                memberAvatar: model.avatar
            }
        }
    }

    // When the component is ready, load the member data
    Component.onCompleted: {
        ChatHandler.loadMembers(memberModel);
    }
}
