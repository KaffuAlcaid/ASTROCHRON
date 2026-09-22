pragma Singleton
import QtQuick

QtObject {
    property bool dark: false
    readonly property string fontFamily: "Microsoft YaHei UI"
    readonly property string numberFont: "Consolas"
    readonly property int bodySize: 13
    readonly property int numberSize: 14
    readonly property int captionSize: 11
    readonly property int sectionGap: 16
    readonly property int inset: 14
    readonly property int radius: 3
    readonly property color background: dark ? "#202628" : "#ffffff"
    readonly property color surface: dark ? "#292f31" : "#f3f5f5"
    readonly property color statusSurface: dark ? "#252c2e" : "#edf1f1"
    readonly property color text: dark ? "#e4ebed" : "#263135"
    readonly property color muted: dark ? "#a8b7bc" : "#64757b"
    readonly property color line: dark ? "#39464a" : "#dce4e6"
    readonly property color hover: dark ? "#303c3e" : "#edf3f2"
    readonly property color selection: dark ? "#344c45" : "#e0efea"
    readonly property color accent: dark ? "#55c8ae" : "#087e6f"
    readonly property color past: dark ? "#c59a6e" : "#b47741"
    readonly property color shadow: dark ? "#a38acb" : "#8265a5"
    readonly property color ocean: dark ? "#233136" : "#eaf1f3"
    readonly property color land: dark ? "#496269" : "#c4d2d6"
    readonly property color border: dark ? "#657c82" : "#a4b8be"
    readonly property color city: dark ? "#9dafb5" : "#637d86"
    readonly property color grid: dark ? "#384e54" : "#d8e4e7"
    readonly property color marker: dark ? "#b4c3dd" : "#526a8c"
    function alpha(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity);
    }
}
