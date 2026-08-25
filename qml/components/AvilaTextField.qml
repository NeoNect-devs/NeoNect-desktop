// qml/containers/AvilaTextField.qml
import QtQuick
import Avila 1.0

Item {
    id: control

    property alias text: input.text
    property string placeholderText: ""
    property int echoMode: TextInput.Normal
    property alias selectByMouse: input.selectByMouse
    readonly property bool isFocused: input.activeFocus

    property bool useGradient: true
    property bool animationEnabled: true

    property color solidBorderColor: (ThemeData.accentColor !== undefined) ? ThemeData.accentColor : "#7289da"
    property color gradientColor1: (ThemeData.inputGradientStart !== undefined) ? ThemeData.inputGradientStart : "#7289da"
    property color gradientColor2: (ThemeData.inputGradientEnd !== undefined) ? ThemeData.inputGradientEnd : "#ff3366"

    // ─── RELATIVELY PRIME FLUID ENGINE ───────────────────────────────
    // Three asynchronous factors that fake organic randomness
    property real colorFactor: 0.0
    property real waveFactor: 0.5
    property real shiftFactor: 0.0

    // Dynamic color calculations based on the mixed timelines
    property color activeColor1: Qt.rgba(
        gradientColor1.r + (gradientColor2.r - gradientColor1.r) * colorFactor,
        gradientColor1.g + (gradientColor2.g - gradientColor1.g) * colorFactor,
        gradientColor1.b + (gradientColor2.b - gradientColor1.b) * colorFactor,
        1.0
    )
    property color activeColor2: Qt.rgba(
        gradientColor2.r + (gradientColor1.r - gradientColor2.r) * shiftFactor,
        gradientColor2.g + (gradientColor1.g - gradientColor2.g) * shiftFactor,
        gradientColor2.b + (gradientColor1.b - gradientColor2.b) * shiftFactor,
        1.0
    )

    implicitWidth: 300
    implicitHeight: 46

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.IBeamCursor
        onClicked: input.forceActiveFocus()
    }

    // Border Frame
    Rectangle {
        id: borderOutline
        anchors.fill: parent
        radius: 8
        color: !control.isFocused ? ((ThemeData.inputSolidBorder !== undefined) ? ThemeData.inputSolidBorder : "#323242") :
               !control.useGradient ? control.solidBorderColor : "#ffffff"

        gradient: (control.isFocused && control.useGradient) ? dynamicFluidGradient : null

        Gradient {
            id: dynamicFluidGradient
            orientation: Gradient.Horizontal

            // ➔ 3-Stop layout allows colors to compress and expand dynamically
            GradientStop { position: 0.0; color: control.activeColor1 }
            GradientStop { position: control.waveFactor; color: control.activeColor2 }
            GradientStop { position: 1.0; color: control.activeColor1 }
        }
    }

    // Inner Mask
    Rectangle {
        anchors.fill: parent
        anchors.margins: 2.0
        radius: 6.0
        color: control.isFocused ?
               ((ThemeData.inputBackgroundActive !== undefined) ? ThemeData.inputBackgroundActive : "#181822") :
               ((ThemeData.inputBackgroundInactive !== undefined) ? ThemeData.inputBackgroundInactive : "#21212b")

        Behavior on color { ColorAnimation { duration: 150 } }
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 16; anchors.rightMargin: 16
        verticalAlignment: TextInput.AlignVCenter
        color: (ThemeData.textPrimary !== undefined) ? ThemeData.textPrimary : "#ffffff"
        font.pointSize: (ThemeData.fontSizeNormal !== undefined) ? ThemeData.fontSizeNormal : 14
        echoMode: control.echoMode
        selectByMouse: true

        Text {
            text: control.placeholderText
            color: (ThemeData.placeholderColor !== undefined) ? ThemeData.placeholderColor : "#55ffffff"
            font.pointSize: (ThemeData.fontSizeNormal !== undefined) ? ThemeData.fontSizeNormal : 14
            visible: input.text === "" && !input.activeFocus
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // ─── ASYMMETRICAL MULTI-TIMELINE LOOPS ───────────────────────────
    // Using prime intervals makes the repetition loop virtually unnoticeable
    ParallelAnimation {
        id: fluidEngine
        running: control.isFocused && control.useGradient && control.animationEnabled
        loops: Animation.Infinite

        // Timeline A: Primary Color Swapping (Slow Wave)
        SequentialAnimation {
            NumberAnimation { target: control; property: "colorFactor"; from: 0.0; to: 1.0; duration: 3100; easing.type: Easing.InOutSine }
            NumberAnimation { target: control; property: "colorFactor"; from: 1.0; to: 0.0; duration: 2300; easing.type: Easing.InOutQuad }
        }

        // Timeline B: Center Point Squeeze (Fast Wave)
        SequentialAnimation {
            NumberAnimation { target: control; property: "waveFactor"; from: 0.2; to: 0.8; duration: 1700; easing.type: Easing.InOutCubic }
            NumberAnimation { target: control; property: "waveFactor"; from: 0.8; to: 0.2; duration: 2100; easing.type: Easing.InOutSine }
        }

        // Timeline C: Secondary Contrast Shifting (Asymmetric Wave)
        SequentialAnimation {
            NumberAnimation { target: control; property: "shiftFactor"; from: 1.0; to: 0.0; duration: 2700; easing.type: Easing.InOutQuad }
            NumberAnimation { target: control; property: "shiftFactor"; from: 0.0; to: 1.0; duration: 1900; easing.type: Easing.InOutCubic }
        }
    }

    onIsFocusedChanged: {
        if (!isFocused) {
            colorFactor = 0.0
            waveFactor = 0.5
            shiftFactor = 0.0
        }
    }
}
