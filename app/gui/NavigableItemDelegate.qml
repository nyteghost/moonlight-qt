import QtQuick 2.0
import QtQuick.Controls 2.2

ItemDelegate {
    property GridView grid

    highlighted: grid.activeFocus && grid.currentItem === this

    // Rounded card tile in the Vibepollo style: dark navy surface, thin border that
    // turns indigo on hover or keyboard focus.
    background: Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        radius: 16
        color: parent.pressed ? "#1A2340" : (parent.hovered || parent.highlighted ? "#131B30" : "#0D1424")
        border.width: parent.highlighted ? 2 : 1
        border.color: parent.highlighted || parent.hovered ? "#6366F1" : "#1E2640"
    }

    Keys.onLeftPressed: {
        grid.moveCurrentIndexLeft()
    }
    Keys.onRightPressed: {
        grid.moveCurrentIndexRight()
    }
    Keys.onDownPressed: {
        grid.moveCurrentIndexDown()
    }
    Keys.onUpPressed: {
        grid.moveCurrentIndexUp()

        // If we've reached the top of the grid, move focus to the toolbar
        if (grid.currentItem === this) {
            nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
        }
    }
    Keys.onReturnPressed: {
        clicked()
    }
    Keys.onEnterPressed: {
        clicked()
    }
}
