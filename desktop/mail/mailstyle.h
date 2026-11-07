#pragma once
#include "uipalette.h"
#include "uistyle.h"
#include "accentbutton.h"
#include <QtWidgets>

namespace MailStyle {
inline const QColor accent = UiPalette::color("#754218");
inline const QString sheet=UiPalette::style(UiStyle::sheet+QStringLiteral(R"qss(
QPushButton[mailAction="true"] { background:#142936; color:#cbd0d8; border:1px solid #343841; border-radius:5px; padding:8px 28px; min-height:22px; }
QPushButton[mailAction="true"][compact="true"] { padding:8px 14px; }
QPushButton[mailAction="true"]:hover { background:#1B3645; border-color:#80BECE; }
QPushButton[mailAction="true"]:pressed { background:#1B3645; border-color:#80BECE; }
QPushButton[mailAction="true"]:focus { outline:0; background:transparent; }
QPushButton[mailAction="true"]:disabled { color:#555b64; }
QPushButton[mailAction="true"][danger="true"] { color:#cfa895; }
QPushButton#mailCheck { min-width:56px; }
QLineEdit:focus,QComboBox:focus,QComboBox:on { border:1px solid #754218; }
QComboBox QAbstractItemView { background:#14171d; border:1px solid #754218; selection-background-color:#36271c; selection-color:#eeeeee; }
QTabBar::tab { padding:10px 22px; }
QTabBar::tab:selected { color:#eeeeee; border-bottom:2px solid #754218; }
QTreeWidget#mailRows { alternate-background-color:#14171b; }
QWidget#mailRowActions { background:transparent; }
QLabel#mailSubject { font-size:24px; font-weight:600; color:#eeeeee; background:transparent; }
QLabel#mailSender { font-size:14px; color:#cbd0d8; background:transparent; }
QLabel#mailRecipient,QLabel#mailDate { font-size:12px; color:#9298a2; background:transparent; }
QLabel#mailSyncStatus { font-size:12px; color:#9298a2; }
QLabel#mailEmpty { color:#9298a2; padding:24px; }
QFrame#mailReaderDivider { background:#343841; border:0; max-height:1px; }
QScrollBar::handle:vertical { background:#514133; }
QScrollBar::handle:vertical:hover { background:#754218; }
)qss"),accent);

class Button:public AccentButton {
public:
    explicit Button(const QString &text,bool compact=false,QWidget *parent=nullptr)
        :AccentButton(text,accent,UiPalette::color("#302820"),false,parent){
        setProperty("mailAction",true);setProperty("compact",compact);
    }
};
}
