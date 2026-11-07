#include "uipalette.h"
#include "settingcontrols.h"
#include "uistyle.h"
#include "dialoglayout.h"
#include "popuppolicy.h"
namespace {
QString themeStyle(const QColor &accent) {
    return UiPalette::style(UiStyle::sheet + QString(R"qss(
QLabel[role="settingTitle"] { color:#E5EEF2; font-size:24px; font-weight:600; padding:8px 0 10px; border:0; }
QLabel[role="settingLabel"] { color:#9FB6C3; font-size:15px; padding:4px 8px; border:0; }
QPushButton[listAction="true"] { padding:7px 18px; }
QPushButton#popupClose { padding:5px 8px; }
QListWidget { background:#142936; alternate-background-color:#18313F; border:1px solid #304B5B; border-radius:5px; padding:0; outline:0; }
QListWidget::item { color:#E5EEF2; border:1px solid transparent; padding:0 13px; margin:0; height:48px; }
QListWidget::item:hover { border:1px solid %1; background:#1B3645; }
QListWidget::item:selected { border:1px solid %1; background:#244858; }
)qss").arg(accent.name()),accent);
}
class SettingsListDelegate:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        auto result=QStyledItemDelegate::sizeHint(option,index);result.setHeight(SettingControls::ListRowHeight);return result;
    }
};
class SectionDivider:public QWidget {
public:
    SectionDivider(const QColor &above,const QColor &below):above(above),below(below){setFixedHeight(1);}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        QLinearGradient ink(0,0,width(),0);
        ink.setColorAt(0,UiPalette::tint(above,.7));ink.setColorAt(.45,UiPalette::Line);
        ink.setColorAt(.55,UiPalette::Line);ink.setColorAt(1,UiPalette::tint(below,.7));
        painter.fillRect(rect(),ink);
    }
private:
    QColor above,below;
};
}
void SettingButton::paintEvent(QPaintEvent *event) {
    QPushButton::paintEvent(event);
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
    auto accent=SettingControls::accentColor(this);
    if(isEnabled()&&objectName()!="popupClose"){
        auto stroke=accent;stroke.setAlpha(underMouse()||hasFocus()?240:155);
        painter.setPen(QPen(stroke,1.5,Qt::SolidLine,Qt::RoundCap));const qreal y=height()/2.;
        painter.drawLine(QPointF(5,y-5),QPointF(5,y+5));painter.drawLine(QPointF(width()-6,y-5),QPointF(width()-6,y+5));
    }
    if(!hasFocus()||!isEnabled())return;
    painter.setBrush(Qt::NoBrush);painter.setPen(QPen(accent,1));
    painter.drawRoundedRect(rect().adjusted(2,2,-3,-3),3,3);
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
    return UiPalette::color("#463d73");
}
void SettingControls::applyTheme(QWidget *widget,const QColor &accent){
    widget->setProperty("settingsAccent",accent);widget->setStyleSheet(UiPalette::style(themeStyle(accent)));
}
QWidget *SettingControls::boundary(const QColor &above,const QColor &below,QWidget *parent){
    auto widget=new QWidget(parent);widget->setProperty("role","settingsBoundary");
    auto layout=new QVBoxLayout(widget);layout->setContentsMargins(0,18,0,18);
    layout->addWidget(new SectionDivider(above,below));
    widget->setFixedHeight(44);return widget;
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
    new PopupPolicy(dialog,[dialog]{dialog->hide();});dialog->setStyleSheet(UiPalette::style(UiStyle::sheet+themeStyle(accent)));
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
