#pragma once
#include "uistyle.h"
#include "accentbutton.h"
#include "dialoglayout.h"
#include <QtWidgets>

namespace NoteStyle {
inline const QColor accent{"#176a39"};
inline const QString sheet=UiStyle::sheet+QStringLiteral(R"qss(
QWidget#notePage,QWidget#noteCanvas { background:#101216; }
QLabel { background:transparent; }
QTabBar::tab { padding:10px 22px; }
QTabBar::tab:selected { color:#eeeeee; border-bottom:2px solid #176a39; }
QTabBar::tab:hover { background:transparent; color:#eeeeee; }
QLineEdit:focus,QTextEdit:focus,QPlainTextEdit:focus,QComboBox:focus,QComboBox:on { border:1px solid #176a39; }
QLineEdit,QTextEdit,QComboBox { selection-background-color:#224a32; }
QComboBox QAbstractItemView { background:#14171d; border:1px solid #176a39; selection-background-color:#224a32; }
QPushButton[noteAction="true"] { color:#cbd0d8; background:transparent; border:0; border-radius:0; padding:8px 28px; min-height:22px; }
QPushButton[noteAction="true"][compact="true"] { padding:5px 14px; min-height:20px; }
QPushButton[noteAction="true"]:hover,QPushButton[noteAction="true"]:pressed { background:transparent; font-weight:600; }
QPushButton[noteAction="true"]:focus { background:transparent; outline:0; }
QPushButton[noteAction="true"]:disabled { color:#555b64; }
QPushButton[noteAction="true"][danger="true"] { color:#cfa895; }
QPushButton#popupClose { padding:5px 8px; }
QFrame#noteCard { background:#14171d; border:2px solid #292f35; border-radius:5px; }
QFrame#noteCard:hover,QFrame#noteCard[keyboardFocus="true"],QFrame#noteCard[selected="true"] { border:2px solid #176a39; }
QWidget#noteCardSummary,QWidget#noteFields,QWidget#noteCardEditor { background:transparent; }
QWidget#noteLockMark { background:transparent; }
QWidget#noteInlineViewport,QWidget#noteCardEditor { background:#14171d; }
QLabel[noteRole="field"] { color:#929a9f; font-size:12px; }
QLabel#noteSaveStatus { color:#81868e; font-size:12px; }
QLabel#noteSaveStatus[failed="true"],QLabel#noteError { color:#dca79d; }
QLabel#noteDialogHeading { color:#e0e4e3; font-size:20px; font-weight:600; }
QLabel#noteEmpty { color:#929a9f; font-size:14px; }
QScrollBar::handle:vertical { background:#30473b; }
QScrollBar::handle:vertical:hover { background:#176a39; }
)qss");

class Button:public AccentButton {
public:
    explicit Button(const QString &text,bool compact=false,QWidget *parent=nullptr)
        :AccentButton(text,accent,QColor("#28362e"),true,parent){
        setProperty("noteAction",true);setProperty("compact",compact);
    }
};

// Keep dialogs inside both the app window and the active screen's work area.
inline void placeDialog(QDialog &dialog,QWidget *page,QSize preferred,bool atTopRight){
    const auto bounds=DialogLayout::fit(dialog,page,preferred);
    QPoint position=atTopRight?QPoint(bounds.right()-dialog.width()+1,page->mapToGlobal(QPoint(0,70)).y()):bounds.center()-QPoint(dialog.width()/2,dialog.height()/2);
    dialog.move(DialogLayout::clamp(position,dialog.size(),bounds));
}
}
