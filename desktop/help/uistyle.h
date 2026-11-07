#pragma once
#include "uipalette.h"
namespace UiStyle {
inline const QString sheet = UiPalette::style(R"qss(
QWidget { color:#E5EEF2; background:#0D1C27; font-family:"DengXian"; font-size:15px; }
QLabel { background:transparent; }
QDialog { background:#142936; border:1px solid #304B5B; }
QLabel[role="title"] { font-size:28px; font-weight:600; color:#E5EEF2; padding:4px 0; }
QLabel[role="muted"] { color:#9FB6C3; font-size:13px; }
QPushButton { color:#E5EEF2; background:#142936; border:1px solid #304B5B; border-radius:5px; padding:7px 14px; min-height:20px; }
QPushButton:hover { background:#1B3645; border-color:#80BECE; }
QPushButton:pressed { background:#244858; }
QPushButton:focus { border-color:#80BECE; }
QPushButton[role="primary"] { background:#285366; color:#E5EEF2; border-color:#80BECE; }
QPushButton[role="danger"],QPushButton[danger="true"] { color:#DC8E99; }
QPushButton:disabled { color:#718C9D; border-color:#233E4D; background:#142936; }
QLineEdit,QTextEdit,QPlainTextEdit,QDateTimeEdit,QSpinBox,QComboBox {
 background:#142936; border:1px solid #304B5B; border-radius:5px; padding:7px; selection-background-color:#285366; selection-color:#E5EEF2;
}
QLineEdit:focus,QTextEdit:focus,QPlainTextEdit:focus,QDateTimeEdit:focus,QSpinBox:focus,QComboBox:focus { border-color:#80BECE; }
QAbstractSpinBox QLineEdit { border:0; padding:0; background:transparent; }
QComboBox QAbstractItemView { color:#E5EEF2; background:#1B3645; border:1px solid #80BECE; selection-background-color:#285366; }
QTreeWidget,QListWidget,QTableWidget { background:#0D1C27; border:0; outline:0; alternate-background-color:#142936; }
QTreeWidget::item { padding:5px 8px; border-bottom:1px solid #233E4D; }
QTreeWidget::item:hover,QListWidget::item:hover { background:#1B3645; }
QTreeWidget::item:selected,QListWidget::item:selected { background:#244858; color:#E5EEF2; }
QHeaderView::section { color:#9FB6C3; background:#0D1C27; border:0; padding:9px; border-bottom:1px solid #304B5B; }
QTabBar::tab { color:#9FB6C3; background:transparent; border:0; border-bottom:2px solid transparent; padding:11px 20px; }
QTabBar::tab:selected { color:#E5EEF2; border-bottom:2px solid #80BECE; }
QTabBar::tab:hover { color:#E5EEF2; background:#142936; }
QScrollArea { border:0; }
QScrollBar:vertical { background:transparent; width:7px; margin:0; }
QScrollBar::handle:vertical { background:#304B5B; min-height:28px; border-radius:3px; }
QScrollBar::handle:vertical:hover { background:#80BECE; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:transparent; }
QScrollBar:horizontal { background:transparent; height:7px; }
QScrollBar::handle:horizontal { background:#304B5B; min-width:28px; border-radius:3px; }
QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal { width:0; }
QCheckBox { spacing:9px; background:transparent; }
QCheckBox::indicator { width:16px; height:16px; border:1px solid #718C9D; border-radius:3px; background:#142936; }
QCheckBox::indicator:checked { background:#285366; border:2px solid #80BECE; }
QGroupBox { border:1px solid #304B5B; border-radius:7px; margin-top:15px; padding:16px 12px 12px; }
QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 6px; color:#80BECE; }
QToolTip { background:#1B3645; color:#E5EEF2; border:1px solid #80BECE; padding:6px; }
QMenu { background:#142936; color:#E5EEF2; border:1px solid #304B5B; padding:5px; }
QMenu::item { padding:7px 22px; border-radius:3px; }
QMenu::item:selected { background:#244858; }
QStatusBar { background:#0D1C27; color:#9FB6C3; border-top:1px solid #233E4D; font-size:12px; }
QProgressBar { background:#142936; border:1px solid #304B5B; border-radius:4px; text-align:center; }
QProgressBar::chunk { background:#80BECE; border-radius:3px; }
)qss");
}
