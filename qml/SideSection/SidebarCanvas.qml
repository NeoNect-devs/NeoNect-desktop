import QtQuick


    Rectangle{
        id:root

        color: "transparent"

        Rectangle{
            id: btnMenu
            height: 60
            width: 60
            color: Qt.darker("white",1.5)
            anchors{
                //bottom: sidebar.top
                top: parent.top
                left: parent.left
                right: parent.right
                leftMargin: 4
                rightMargin: 4
            }
            radius: 56

        }

        Rectangle{
            id: sidebar

            color: "#101210"
            anchors{
                top: btnMenu.bottom
                bottom: parent.bottom
                left: parent.left
                right: parent.right
                rightMargin: 7
                leftMargin: 3
                topMargin: 5
            }
            radius: 20

            Rectangle{
                id: btnSetting
                height: 45
                width: 45
                color: Qt.darker("white",3)
                anchors{

                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    margins: 5
                }
                radius: 45
            }
        }
    }

