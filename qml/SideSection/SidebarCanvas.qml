import QtQuick


    Rectangle{
        id:root
        implicitWidth: 60
        color: "transparent"

        Rectangle{
            id: sidebar

            color: "#101210"
            anchors{
                fill: parent
                rightMargin: 5
            }

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

