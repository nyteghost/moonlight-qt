import QtQuick 2.0
import QtQuick.Controls 2.2

import SystemProperties 1.0

NavigableDialog {
    id: dialog

    title: qsTr("What's new in Vibelight %1").arg(SystemProperties.versionString)
    standardButtons: Dialog.Ok
    width: Math.min(560, parent ? parent.width - 40 : 560)

    // Remember that this version's notes were shown as soon as they appear
    onOpened: SystemProperties.markWhatsNewSeen()

    contentItem: Flickable {
        id: flick
        implicitHeight: Math.min(notesLabel.implicitHeight + 8, 380)
        contentWidth: width
        contentHeight: notesLabel.implicitHeight + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Label {
            id: notesLabel
            width: flick.width - 14
            wrapMode: Text.Wrap
            font.pointSize: 11
            text: SystemProperties.whatsNewText()
        }
    }
}
