import QtQuick
import Avila 1.0
    Rectangle{
        id:root
        property bool memberlistVisibility: true
        height: 50
        clip:true
        color: "#00000000"
        Rectangle{
            id:topbar
            //width: parent.width
            height: 50

            // border.width:2
            // border.color:"#171717" //Qt.darker("lightgrey",1.5)
            color: ThemeData.viewsBackground
            anchors.fill:parent


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

                        onClicked: {
                            root.memberlistVisibility = !root.memberlistVisibility;

                        }
                    }
                }
            }
        }



