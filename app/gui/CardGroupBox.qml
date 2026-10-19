import QtQuick 2.9
import QtQuick.Controls 2.2

// A settings section drawn as a rounded dark card, matching the Vibepollo web UI.
GroupBox {
    id: control

    background: Rectangle {
        y: control.topPadding - control.bottomPadding
        width: parent.width
        height: parent.height - control.topPadding + control.bottomPadding
        radius: 14
        color: "#0D1424"
        border.width: 1
        border.color: "#1E2640"
    }

    label: Label {
        x: control.leftPadding
        width: control.availableWidth
        text: control.title
        font: control.font
        color: "#A5B4FC"
        elide: Text.ElideRight
    }
}
