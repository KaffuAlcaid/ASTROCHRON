import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: row
    property string label
    property string value
    property int valueSize: Theme.numberSize
    property bool numeric: true
    spacing: 8
    Layout.fillWidth: true
    Label {
        text: row.label
        color: Theme.muted
        font.pixelSize: Theme.bodySize
        Layout.preferredWidth: 108
        wrapMode: Text.Wrap
    }
    TextEdit {
        text: row.value
        color: Theme.text
        font.family: row.numeric ? Theme.numberFont : Theme.fontFamily
        font.pixelSize: row.valueSize
        font.features: ({
                "tnum": 1
            })
        horizontalAlignment: Text.AlignRight
        readOnly: true
        selectByMouse: true
        wrapMode: TextEdit.Wrap
        Layout.fillWidth: true
    }
}
