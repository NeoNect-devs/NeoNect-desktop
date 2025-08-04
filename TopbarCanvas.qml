import QtQuick

    Rectangle{
        id:root
        property bool memberlistVisibility: true
        height: 50
        clip:true
        anchors{
            right: parent.right
            left: parent.left
            top: parent.top

            //topMargin: -border.width

        }

        Rectangle{
            id:topbar
            //width: parent.width
            height: 50

            border.width:2
            border.color:"#171717" //Qt.darker("lightgrey",1.5)
            color: "#101210"
            anchors{
                right: parent.right
                left: parent.left
                top: parent.top
                bottom: parent.bottom
                topMargin: -border.width
                leftMargin: -border.width
                rightMargin: -border.width
            }


                Rectangle{
                    id:btnServerSetting
                    width: 40
                    height: 40
                    color: "grey"
                    anchors{
                        left: parent.left
                        verticalCenter: parent.verticalCenter
                        margins: 5
                    }
                    radius: 40
                }

                Text {
                    id:lblServerDesc
                    text:"Avila"
                    font.pointSize: 16
                    //font.family:"Bangers"
                    color: "white"
                    anchors{
                        verticalCenter: parent.verticalCenter
                        left: btnServerSetting.right
                        leftMargin: 15
                    }
                }


                Rectangle{
                    id: btnShowMemberlist
                    width: 40
                    height: 40
                    color: "grey"
                    anchors{
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        margins: 5
                    }
                    radius: 40
                    MouseArea{
                        id:showMemberMouseArea
                        anchors.fill: parent

                        onClicked: root.memberlistVisibility = !root.memberlistVisibility
                    }
                }
            }
        }



