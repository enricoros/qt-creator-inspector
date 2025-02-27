import Qt 4.6
import Bauhaus 1.0

QScrollArea {
    widgetResizable: true;
    styleSheetFile: ":/qmldesigner/scrollbar.css";
    horizontalScrollBarPolicy: "Qt::ScrollBarAlwaysOff";
    id: layoutPane;
    visible: false;
    content: properyEditorLayout;
    QFrame {
        id: properyEditorLayout;
        enabled: isBaseState
        minimumHeight: 460;
        layout: QVBoxLayout {
            topMargin: 0;
            bottomMargin: 0;
            leftMargin: 0;
            rightMargin: 0;
            Layout {
                id: layoutBox;
            }
            QScrollArea {
            }
        }
    }
}
