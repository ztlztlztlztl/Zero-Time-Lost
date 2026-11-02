#include "settingcontrols.h"
#include "uistyle.h"
#include "dialoglayout.h"
#include "popuppolicy.h"
namespace {
const QColor background("#101216");
QColor tint(const QColor &accent,qreal amount){
    return QColor(qRound(background.red()*(1-amount)+accent.red()*amount),
                  qRound(background.green()*(1-amount)+accent.green()*amount),
                  qRound(background.blue()*(1-amount)+accent.blue()*amount));
}
QString themeStyle(const QColor &accent){
    return QString(R"qss(
QLabel[role="settingTitle"] { color:#ffffff; font-size:30px; font-weight:600; padding:8px 0 10px 0; border:0; }
QLabel[role="settingTitle"][afterBoundary="true"] { padding-top:0; }
QLabel[role="settingLabel"] { font-size:16px; padding:4px 8px; border:0; border-left:3px solid %1; }
QLineEdit,QTextEdit,QPlainTextEdit,QDateTimeEdit,QSpinBox,QComboBox {
    background:#14171d; border:1px solid #343841; border-radius:3px; padding:6px; selection-background-color:%4;
}
QLineEdit:focus,QTextEdit:focus,QPlainTextEdit:focus,QDateTimeEdit:focus,QSpinBox:focus,QComboBox:focus,QComboBox:on { border:1px solid %1; }
QAbstractSpinBox QLineEdit { border:0; background:transparent; padding:0; }
QComboBox QAbstractItemView { background:#101216; border:1px solid %1; selection-background-color:%4; selection-color:#ffffff; }
QPushButton {
    color:#cbd0d8; background:transparent; border:0; border-radius:0;
    border-left:1px solid transparent; border-right:1px solid transparent;
    padding:8px 6px; min-width:0; min-height:22px; font-weight:400;
}
QPushButton:focus { color:%1; }
QPushButton:hover {
    color:#ffffff; background:transparent; border:0;
    border-left:3px solid transparent; border-right:3px solid transparent;
    padding:8px 4px; font-weight:600;
}
QPushButton:pressed {
    color:#ffffff; background:transparent; border:0;
    border-left:3px solid transparent; border-right:3px solid transparent;
    padding:10px 2px 6px 6px; font-weight:600;
}
QPushButton:disabled { color:#555b64; border-left:1px solid transparent; border-right:1px solid transparent; }
QPushButton[listAction="true"] { padding:8px 28px; }
QPushButton[listAction="true"]:hover { padding:8px 26px; }
QPushButton[listAction="true"]:pressed { padding:10px 24px 6px 28px; }
QPushButton#popupClose { border:0; padding:8px 6px; }
QPushButton#popupClose:hover { border:0; padding:8px 6px; }
QPushButton#popupClose:pressed { border:0; padding:10px 4px 6px 8px; }
QListWidget { background:#101216; alternate-background-color:%2; border:0; padding:0; outline:0; }
QListWidget::item { color:#d0d0d0; background:#101216; border:1px solid transparent; padding:0 13px; margin:0; height:48px; }
QListWidget::item:alternate { background:%2; }
QListWidget::item:hover { border:1px solid %1; }
QListWidget::item:selected { border:1px solid %1; }
QListWidget::item:selected:hover { border:1px solid %1; }
QScrollBar::handle:vertical { background:%3; }
QScrollBar::handle:vertical:hover { background:%1; }
QCheckBox:checked { color:%1; }
)qss").arg(accent.name(),tint(accent,.09).name(),tint(accent,.22).name(),
            tint(accent,.36).name());
}
class SettingsListDelegate:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        auto result=QStyledItemDelegate::sizeHint(option,index);result.setHeight(SettingControls::ListRowHeight);return result;
    }
};
class DashedDivider:public QWidget {
public:
    explicit DashedDivider(const QColor &color):accent(color){setFixedHeight(2);setProperty("accent",color);}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        // Explicit short rectangles avoid platform-dependent dotted QSS borders.
        for(int x=0;x<width();x+=14)painter.fillRect(QRect(x,0,qMin(8,width()-x),height()),accent);
    }
private:
    QColor accent;
};
}
void SettingButton::paintEvent(QPaintEvent *event){
    QPushButton::paintEvent(event);
    if(objectName()=="popupClose")return;
    const auto accent=SettingControls::accentColor(this);
    const int stroke=isEnabled()&&(underMouse()||isDown())?3:1;
    // Keep the original hit target while centring short strokes beside the text.
    const int length=qMin(height(),qRound(QFontMetrics(font()).tightBoundingRect(QStringLiteral("字")).height()*1.2));
    const int top=(height()-length)/2;
    QPainter painter(this);const auto color=isEnabled()?accent:tint(accent,.09);
    painter.fillRect(QRect(0,top,stroke,length),color);
    painter.fillRect(QRect(width()-stroke,top,stroke,length),color);
}
void SettingsListWidget::mousePressEvent(QMouseEvent *event){
    toggleCandidate=QPersistentModelIndex();pressPosition=event->position().toPoint();
    const auto index=indexAt(pressPosition);
    if(event->button()==Qt::LeftButton&&event->modifiers()==Qt::NoModifier&&index.isValid()&&selectionModel()->isSelected(index))toggleCandidate=index;
    QListWidget::mousePressEvent(event);
}
void SettingsListWidget::mouseMoveEvent(QMouseEvent *event){
    if((event->position().toPoint()-pressPosition).manhattanLength()>=QApplication::startDragDistance())toggleCandidate=QPersistentModelIndex();
    QListWidget::mouseMoveEvent(event);
}
void SettingsListWidget::mouseDoubleClickEvent(QMouseEvent *event){
    // A rapid second click must toggle just like a separate second click.
    mousePressEvent(event);
}
void SettingsListWidget::mouseReleaseEvent(QMouseEvent *event){
    const bool deselect=event->button()==Qt::LeftButton&&toggleCandidate.isValid()&&indexAt(event->position().toPoint())==toggleCandidate;
    QListWidget::mouseReleaseEvent(event);
    toggleCandidate=QPersistentModelIndex();
    if(deselect){clearSelection();setCurrentRow(-1);}
}
QColor SettingControls::accentColor(const QWidget *widget){
    for(auto current=widget;current;current=current->parentWidget()){
        const auto color=current->property("settingsAccent").value<QColor>();if(color.isValid())return color;
    }
    return QColor("#463d73");
}
void SettingControls::applyTheme(QWidget *widget,const QColor &accent){
    widget->setProperty("settingsAccent",accent);widget->setStyleSheet(themeStyle(accent));
}
QWidget *SettingControls::boundary(const QColor &above,const QColor &below,QWidget *parent){
    auto widget=new QWidget(parent);widget->setProperty("role","settingsBoundary");
    auto layout=new QVBoxLayout(widget);layout->setContentsMargins(0,48,0,48);layout->setSpacing(0);
    auto line=[&](const QColor &color,const QString &name){
        auto divider=new DashedDivider(color);divider->setObjectName(name);return divider;
    };
    // Leave about three characters of height next to each section, keeping the inter-line gap.
    layout->addWidget(line(above,"boundaryAbove"));layout->addSpacing(64);layout->addWidget(line(below,"boundaryBelow"));
    widget->setFixedHeight(164);return widget;
}
void SettingControls::configureList(QListWidget *list){
    // QListWidget already is a QAbstractScrollArea; keep one viewport exactly three rows tall.
    list->setFrameShape(QFrame::NoFrame);list->setAlternatingRowColors(true);list->setSpacing(0);
    list->setUniformItemSizes(true);list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);list->setTextElideMode(Qt::ElideRight);
    list->setItemDelegate(new SettingsListDelegate(list));list->setFixedHeight(3*ListRowHeight);
}
QWidget *SettingControls::buttonSlot(QPushButton *button,bool listAction){
    auto slot=new QWidget;slot->setObjectName("settingsButtonSlot");slot->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    auto layout=new QHBoxLayout(slot);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
    // Keep each action in its existing layout cell, with padding chosen for its role.
    button->setProperty("listAction",listAction);
    button->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    layout->addStretch();layout->addWidget(button);layout->addStretch();return slot;
}
QFormLayout *SettingControls::form(QWidget *widget){
    auto layout=new QFormLayout(widget);layout->setContentsMargins(0,0,0,0);
    layout->setHorizontalSpacing(24);layout->setVerticalSpacing(14);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->setLabelAlignment(Qt::AlignLeft|Qt::AlignVCenter);return layout;
}
QLabel *SettingControls::label(const QString &text){
    auto result=new QLabel(text);result->setProperty("role","settingLabel");result->setFixedWidth(220);return result;
}
QDialog *SettingControls::popup(QWidget *owner,const QString &title,const QString &name){
    auto dialog=new QDialog(owner,Qt::Tool|Qt::FramelessWindowHint);dialog->setObjectName(name);dialog->setWindowTitle(title);
    const auto accent=accentColor(owner);dialog->setProperty("settingsAccent",accent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    new PopupPolicy(dialog,[dialog]{dialog->hide();});dialog->setStyleSheet(UiStyle::sheet+themeStyle(accent));
    auto layout=new QVBoxLayout(dialog);layout->setContentsMargins(20,16,20,20);layout->setSpacing(14);
    auto header=new QHBoxLayout;auto heading=new QLabel(title);heading->setProperty("role","settingLabel");
    auto close=new SettingButton("×");close->setObjectName("popupClose");close->setFixedWidth(38);
    header->addWidget(heading,1);header->addWidget(close);layout->addLayout(header);
    QObject::connect(close,&QPushButton::clicked,dialog,&QDialog::close);return dialog;
}
void SettingControls::showPopup(QDialog *dialog,QWidget *anchor){
    dialog->adjustSize();const auto screen=DialogLayout::fit(*dialog,anchor,dialog->size());
    auto position=anchor->mapToGlobal(QPoint(0,anchor->height()+4));
    position.setX(qBound(screen.left(),position.x(),qMax(screen.left(),screen.right()-dialog->width())));
    position.setY(qBound(screen.top(),position.y(),qMax(screen.top(),screen.bottom()-dialog->height())));
    dialog->move(position);dialog->show();
}
