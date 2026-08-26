import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Avila.Core 1.0

Rectangle {
    id: pickerRoot

    property int activeTab: 0 // 0: Duck, 1: Crypto, 2: Pepe, 3: Emoji
    property string searchQuery: ""

    signal stickerSelected(string stickerUrl, string packId, string stickerName)
    signal emojiSelected(string emoji)
    signal closeRequested()

    width: 360
    height: 380
    radius: 14
    color: ThemeData.panelBackground
    border.color: Qt.rgba(255, 255, 255, 0.12)
    border.width: 1

    // Drop shadow
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        radius: parent.radius + 1
        color: "transparent"
        border.color: Qt.rgba(0, 0, 0, 0.4)
        border.width: 2
        z: -1
    }

    // Sticker Packs Data
    readonly property var duckStickers: [
        { name: "Happy", url: "qrc:/qt/qml/Avila/assets/stickers/duck/duck_happy.svg", pack: "duck" },
        { name: "Love", url: "qrc:/qt/qml/Avila/assets/stickers/duck/duck_love.svg", pack: "duck" },
        { name: "Cool", url: "qrc:/qt/qml/Avila/assets/stickers/duck/duck_cool.svg", pack: "duck" },
        { name: "Party", url: "qrc:/qt/qml/Avila/assets/stickers/duck/duck_party.svg", pack: "duck" },
        { name: "Thinking", url: "qrc:/qt/qml/Avila/assets/stickers/duck/duck_thinking.svg", pack: "duck" },
        { name: "Angry", url: "qrc:/qt/qml/Avila/assets/stickers/duck/duck_angry.svg", pack: "duck" },
        { name: "Sad", url: "qrc:/qt/qml/Avila/assets/stickers/duck/duck_sad.svg", pack: "duck" },
        { name: "Thumbs Up", url: "qrc:/qt/qml/Avila/assets/stickers/duck/duck_thumbsup.svg", pack: "duck" }
    ]

    readonly property var cryptoStickers: [
        { name: "Rocket Moon", url: "qrc:/qt/qml/Avila/assets/stickers/crypto/crypto_rocket.svg", pack: "crypto" },
        { name: "Diamond Hands", url: "qrc:/qt/qml/Avila/assets/stickers/crypto/crypto_hodl.svg", pack: "crypto" },
        { name: "Fire Lit", url: "qrc:/qt/qml/Avila/assets/stickers/crypto/crypto_fire.svg", pack: "crypto" },
        { name: "Pure Gem", url: "qrc:/qt/qml/Avila/assets/stickers/crypto/crypto_diamond.svg", pack: "crypto" },
        { name: "Popcorn", url: "qrc:/qt/qml/Avila/assets/stickers/crypto/crypto_popcorn.svg", pack: "crypto" },
        { name: "Cheers", url: "qrc:/qt/qml/Avila/assets/stickers/crypto/crypto_cheers.svg", pack: "crypto" },
        { name: "Giga Brain", url: "qrc:/qt/qml/Avila/assets/stickers/crypto/crypto_brain.svg", pack: "crypto" },
        { name: "Hacker", url: "qrc:/qt/qml/Avila/assets/stickers/crypto/crypto_hacker.svg", pack: "crypto" }
    ]

    readonly property var pepeStickers: [
        { name: "Coffee", url: "qrc:/qt/qml/Avila/assets/stickers/pepe/pepe_coffee.svg", pack: "pepe" },
        { name: "Feels Good", url: "qrc:/qt/qml/Avila/assets/stickers/pepe/pepe_ok.svg", pack: "pepe" },
        { name: "Deal With It", url: "qrc:/qt/qml/Avila/assets/stickers/pepe/pepe_sunglasses.svg", pack: "pepe" },
        { name: "Smart Move", url: "qrc:/qt/qml/Avila/assets/stickers/pepe/pepe_smart.svg", pack: "pepe" },
        { name: "Gamer", url: "qrc:/qt/qml/Avila/assets/stickers/pepe/pepe_gamer.svg", pack: "pepe" },
        { name: "Feels Bad", url: "qrc:/qt/qml/Avila/assets/stickers/pepe/pepe_cry.svg", pack: "pepe" }
    ]

    readonly property var popularEmojis: [
        "😀", "😃", "😄", "😁", "😆", "😅", "😂", "🤣", "😊", "😇",
        "🙂", "🙃", "😉", "😌", "😍", "🥰", "😘", "😗", "😙", "😚",
        "😋", "😛", "😝", "😜", "🤪", "🤨", "🧐", "🤓", "😎", "🤩",
        "🥳", "😏", "😒", "😞", "😔", "😟", "😕", "🙁", "☹️", "😣",
        "😖", "😫", "😩", "🥺", "😢", "😭", "😤", "😠", "😡", "🤬",
        "🤯", "😳", "🥵", "🥶", "😱", "😨", "😰", "😥", "😓", "🤗",
        "🤔", "🤭", "🤫", "🤥", "😶", "😐", "😑", "😬", "🙄", "😯",
        "😦", "😧", "😮", "😲", "🥱", "😴", "🤤", "😪", "😵", "🤐",
        "🥴", "🤢", "🤮", "🤧", "😷", "🤒", "🤕", "🤑", "🤠", "😈",
        "👍", "👎", "👏", "🙌", "👐", "🤲", "🤝", "🙏", "✌️", "🤞",
        "🤟", "🤘", "🤙", "👈", "👉", "👆", "👇", "☝️", "✋", "🤚",
        "🖐️", "🖖", "👋", "🤙", "💪", "🦾", "🖕", "✍️", "🤳", "💅",
        "❤️", "🧡", "💛", "💚", "💙", "💜", "🖤", "🤍", "🤎", "💔",
        "🔥", "✨", "🌟", "💫", "💥", "💯", "🎉", "🎊", "🚀", "💎"
    ]

    function getCurrentList() {
        var list = [];
        if (activeTab === 0) list = duckStickers;
        else if (activeTab === 1) list = cryptoStickers;
        else if (activeTab === 2) list = pepeStickers;
        else return [];

        if (searchQuery.trim() === "") return list;
        return list.filter(function(item) {
            return item.name.toLowerCase().indexOf(searchQuery.toLowerCase()) !== -1;
        });
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Header & Search
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                height: 34
                radius: 8
                color: ThemeData.inputBackgroundInactive
                border.color: searchField.activeFocus ? ThemeData.accentColor : ThemeData.borderColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    spacing: 6

                    Text {
                        text: "🔍"
                        font.pixelSize: 12
                    }

                    TextInput {
                        id: searchField
                        Layout.fillWidth: true
                        color: ThemeData.textPrimary
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                        selectByMouse: true

                        Text {
                            text: pickerRoot.activeTab === 3 ? "Search Emojis..." : "Search Stickers..."
                            color: ThemeData.placeholderColor
                            font.pixelSize: 13
                            visible: !searchField.text && !searchField.activeFocus
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        onTextChanged: pickerRoot.searchQuery = text
                    }
                }
            }

            Rectangle {
                width: 28; height: 28
                radius: 6
                color: closeMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: ThemeData.textSecondary
                    font.pixelSize: 14
                }

                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: pickerRoot.closeRequested()
                }
            }
        }

        // Pack Category Tabs (Duck, Crypto, Pepe, Emojis)
        Rectangle {
            Layout.fillWidth: true
            height: 36
            radius: 8
            color: ThemeData.windowBackground

            RowLayout {
                anchors.fill: parent
                spacing: 2

                Repeater {
                    model: [
                        { name: "Cyber Duck", icon: "🦆", tab: 0 },
                        { name: "Crypto", icon: "🚀", tab: 1 },
                        { name: "Pepe", icon: "🐸", tab: 2 },
                        { name: "Emojis", icon: "😀", tab: 3 }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 6
                        color: pickerRoot.activeTab === modelData.tab ? Qt.rgba(88, 101, 242, 0.25) : (tabMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.06) : "transparent")
                        border.color: pickerRoot.activeTab === modelData.tab ? ThemeData.accentColor : "transparent"
                        border.width: 1

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 4

                            Text {
                                text: modelData.icon
                                font.pixelSize: 14
                            }

                            Text {
                                text: modelData.name
                                color: pickerRoot.activeTab === modelData.tab ? ThemeData.textPrimary : ThemeData.textSecondary
                                font.family: "Segoe UI"
                                font.pixelSize: 11
                                font.bold: pickerRoot.activeTab === modelData.tab
                            }
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                pickerRoot.activeTab = modelData.tab;
                                searchField.clear();
                            }
                        }
                    }
                }
            }
        }

        // Stickers Grid View (4 columns x N rows)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: pickerRoot.activeTab !== 3

            GridView {
                id: stickerGrid
                anchors.fill: parent
                cellWidth: Math.floor(width / 4)
                cellHeight: 82
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: pickerRoot.getCurrentList()

                ScrollBar.vertical: ScrollBar {
                    id: stickerScroll
                    parent: stickerGrid
                    anchors.top: stickerGrid.top
                    anchors.right: stickerGrid.right
                    anchors.bottom: stickerGrid.bottom
                    width: 6
                    policy: ScrollBar.AsNeeded
                    active: true
                    palette.window: "transparent"
                    palette.base: "transparent"

                    contentItem: Rectangle {
                        implicitWidth: 6
                        radius: 3
                        color: stickerScroll.pressed ? ThemeData.accentColor : (stickerScroll.hovered ? "#7289DA" : "#4E5058")
                    }
                    background: Item {}
                }

                delegate: Item {
                    width: stickerGrid.cellWidth
                    height: stickerGrid.cellHeight

                    Rectangle {
                        anchors.centerIn: parent
                        width: 72; height: 72
                        radius: 10
                        color: stickerMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
                        border.color: stickerMouse.containsMouse ? ThemeData.accentColor : "transparent"
                        border.width: 1
                        scale: stickerMouse.containsMouse ? 1.08 : 1.0

                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

                        Image {
                            anchors.centerIn: parent
                            width: 60; height: 60
                            source: modelData.url
                            sourceSize: Qt.size(120, 120)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }

                        MouseArea {
                            id: stickerMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                pickerRoot.stickerSelected(modelData.url, modelData.pack, modelData.name);
                            }
                        }
                    }
                }
            }
        }

        // Emojis Grid View (8 columns x N rows)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: pickerRoot.activeTab === 3

            GridView {
                id: emojiGrid
                anchors.fill: parent
                cellWidth: Math.floor(width / 8)
                cellHeight: 40
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: pickerRoot.popularEmojis

                ScrollBar.vertical: ScrollBar {
                    id: emojiScroll
                    parent: emojiGrid
                    anchors.top: emojiGrid.top
                    anchors.right: emojiGrid.right
                    anchors.bottom: emojiGrid.bottom
                    width: 6
                    policy: ScrollBar.AsNeeded
                    active: true
                    palette.window: "transparent"
                    palette.base: "transparent"

                    contentItem: Rectangle {
                        implicitWidth: 6
                        radius: 3
                        color: emojiScroll.pressed ? ThemeData.accentColor : (emojiScroll.hovered ? "#7289DA" : "#4E5058")
                    }
                    background: Item {}
                }

                delegate: Item {
                    width: emojiGrid.cellWidth
                    height: emojiGrid.cellHeight

                    Rectangle {
                        anchors.centerIn: parent
                        width: 36; height: 36
                        radius: 6
                        color: emojiItemMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.12) : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            font.pixelSize: 20
                        }

                        MouseArea {
                            id: emojiItemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: pickerRoot.emojiSelected(modelData)
                        }
                    }
                }
            }
        }
    }
}
