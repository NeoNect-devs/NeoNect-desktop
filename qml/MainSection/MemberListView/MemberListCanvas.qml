// MemberListCanvas.qml
import QtQuick
import QtQuick.Layouts
import Avila 1.0
import "../ChatView/ChatHandler.js" as ChatHandler

Rectangle {
    id: root
    width: 200 // Increased from 175 to comfortably allow space for long profile names without text elision clip clipping
    color: ThemeData.viewsBackground

    ListModel {
        id: memberModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 8
        spacing: 8

        Text {
            text: "MEMBERS — " + memberModel.count
            font.family: "Segoe UI"
            font.bold: true
            color: "#8E9297"
            font.pixelSize: 12
            Layout.topMargin: 16
            Layout.fillWidth: true
        }

        ListView {
            id: memberListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: memberModel
            spacing: 2
            clip: true

            delegate: MemberItem {
                memberName: model.name
                memberAvatar: model.avatar
            }
        }
    }

    Component.onCompleted: {
        ChatHandler.loadMembers(memberModel);
    }
}
