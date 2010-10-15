import Qt 4.7
import QtDee 1.0

Item {
    BorderImage {
        id: background

        anchors.fill: parent
        source: "artwork/border_glow.png"
        smooth: false
        border.left: 11
        border.right: 11
        border.top: 12
        border.bottom: 12
        /* It might be more efficient to have a png with a bigger transparent
           middle and setting the tile modes to Repeat */
        horizontalTileMode: BorderImage.Stretch
        verticalTileMode: BorderImage.Stretch

        Image {
            anchors.fill: parent
            anchors.margins: 6
            source: "artwork/checker.png"
            fillMode: Image.Tile
        }
    }

    Item {
        id: search_entry

        width: 279
        anchors.left: parent.left
        anchors.leftMargin: 85
        anchors.top: parent.top
        anchors.topMargin: 7
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6

        BorderImage {
            id: border

            anchors.fill: parent
            source: "artwork/border_glow.png"
            smooth: false
            border.left: 11
            border.right: 11
            border.top: 12
            border.bottom: 12
            /* It might be more efficient to have a png with a bigger transparent
               middle and setting the tile modes to Repeat */
            horizontalTileMode: BorderImage.Stretch
            verticalTileMode: BorderImage.Stretch
        }

        Item {
            anchors.fill: parent
            anchors.topMargin: 7
            anchors.bottomMargin: 6
            anchors.leftMargin: 8
            anchors.rightMargin: 11
            opacity: 0.9

            Image {
                id: search_icon

                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 2
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
                width: 20

                source: "/usr/share/unity/search_icon.png"
                fillMode: Image.PreserveAspectCrop
            }

            TextInput {
                anchors.left: search_icon.right
                anchors.leftMargin: 5
                anchors.right: clear_button.left
                anchors.rightMargin: 5

                text: "Search"
                font.italic: true
                color: "#ffffff"

                Keys.onPressed: {
                    if (event.key == Qt.Key_Return) {
                        current_page.search(text)
                        event.accepted = true;
                    }
                }
            }

            Image {
                id: clear_button

                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 1
                width: 10

                source: "artwork/cross.png"
                fillMode: Image.PreserveAspectFit
            }
        }


    }

    ListView {
        id: sections

        interactive: false
        orientation: ListView.Horizontal

        anchors.left: search_entry.right
        anchors.leftMargin: 13
        anchors.right: parent.right
        anchors.rightMargin: 15
        spacing: 10

        height: parent.height

        delegate: Section {
            anchors.verticalCenter: parent.verticalCenter
            horizontalPadding: 4
            verticalPadding: 3
            label: column_0

            onClicked: {
                ListView.view.currentIndex = model.index
                current_page.setActiveSection(model.index)
            }
        }

        model: DeeListModel {
            service: current_page.dBusService
            objectPath: current_page.dBusDeePrefix + "SectionsModel"
        }
    }
}
