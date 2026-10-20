import QtQuick 2.0
import QtQuick.Controls 2.2

import SystemProperties 1.0

NavigableDialog {
    id: dialog

    title: qsTr("How Vibelight differs from Moonlight")
    standardButtons: Dialog.Ok
    width: Math.min(640, parent ? parent.width - 40 : 640)

    contentItem: Column {
        spacing: 10

        Flickable {
            id: flick
            width: parent.width
            height: Math.min(notesLabel.implicitHeight + 8, 400)
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
                text: SystemProperties.differencesText()
            }
        }

        Button {
            text: qsTr("View the full code diff on GitHub")
            onClicked: Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-qt/compare/master...nyteghost:moonlight-qt:master")
        }
    }
}
