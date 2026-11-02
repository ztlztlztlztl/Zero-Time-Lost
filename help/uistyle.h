#pragma once
#include <QString>
namespace UiStyle {
inline const QString sheet=R"qss(
QWidget { color:#d0d0d0; background:#101216; font-family:"Microsoft YaHei UI"; font-size:14px; }
QDialog { border:1px solid #35383e; }
QLabel[role="title"] { font-size:22px; color:#eeeeee; padding:8px 0; }
QLabel[role="muted"] { color:#81868e; }
QPushButton { color:#b9bec7; background:transparent; border:1px solid #35383e; border-radius:3px; padding:7px 16px; min-height:20px; }
QPushButton:hover { color:#eeeeee; background:#191c22; border-color:#777b87; }
QPushButton:pressed { background:#232630; }
QPushButton[role="primary"] { color:#d5d4ff; border-left:2px solid #7774dd; }
QPushButton[role="danger"] { color:#dca79d; border-left:2px solid #bf705e; }
QPushButton[role="check"] { color:#acb8e2; padding:3px 12px; margin:8px; }
QPushButton:disabled { color:#555b64; border-color:#25282e; }
QLineEdit,QTextEdit,QPlainTextEdit,QDateTimeEdit,QSpinBox,QComboBox {
 background:#14171d; border:1px solid #343841; border-radius:3px; padding:6px; selection-background-color:#343b55;
}
QLineEdit:focus,QTextEdit:focus,QDateTimeEdit:focus { border-bottom:1px solid #8984d2; }
QTreeWidget { background:#101216; border:0; outline:0; alternate-background-color:#14171c; }
QTreeWidget::item { padding:4px 8px; border-bottom:1px dashed #30333a; }
QTreeWidget::item:hover { background:#1a1e26; }
QTreeWidget::item:selected { background:#202534; color:#efeff5; }
QHeaderView::section { color:#777e8a; background:#101216; border:0; padding:9px; border-bottom:1px solid #373b43; }
QTabBar::tab { color:#737a86; background:transparent; border:0; border-bottom:2px solid transparent; padding:12px 26px; }
QTabBar::tab:selected { color:#dddded; border-bottom:2px solid #8780d0; }
QTabBar::tab:hover { color:#d0d0d0; background:#171a20; }
QScrollArea { border:0; }
QScrollBar:vertical { background:#101216; width:8px; margin:0; }
QScrollBar::handle:vertical { background:#555b66; min-height:24px; border-radius:4px; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:transparent; }
QCheckBox { spacing:8px; }
QToolTip { background:#20242d; color:#eeeeee; border:1px solid #51576a; }
)qss";
}
