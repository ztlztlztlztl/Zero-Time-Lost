#include "navigationrail.h"
#include "cloudconnection.h"
#include "uimotion.h"
#include "uipalette.h"
#include <QtWidgets>
#include <cmath>

namespace {
void drawNavIcon(QPainter &p, int index, const QRectF &bounds) {
    p.save(); p.translate(bounds.topLeft());
    const qreal scale = bounds.width()/24.; p.scale(scale,scale);
    p.setBrush(Qt::NoBrush);
    switch (index) {
    case 0: {
        QPainterPath wave; wave.moveTo(2,10);wave.cubicTo(7,3,12,18,22,8);
        p.drawPath(wave);wave=QPainterPath();wave.moveTo(2,16);wave.cubicTo(9,9,14,23,22,14);p.drawPath(wave);break;
    }
    case 1: p.drawRoundedRect(QRectF(2,5,20,15),3,3);p.drawPolyline(QPolygonF{QPointF(3,7),QPointF(12,13),QPointF(21,7)});break;
    case 2: p.drawRoundedRect(QRectF(3,5,18,17),3,3);p.drawLine(3,10,21,10);p.drawLine(8,2,8,7);p.drawLine(16,2,16,7);p.drawLine(8,15,11,15);p.drawLine(14,15,17,15);break;
    case 3: {QPainterPath page;page.moveTo(5,3);page.lineTo(15,3);page.lineTo(20,8);page.lineTo(20,21);page.lineTo(5,21);page.closeSubpath();p.drawPath(page);p.drawLine(9,12,16,12);p.drawLine(9,16,14,16);break;}
    case 4: p.drawEllipse(QRectF(5,5,14,14));p.drawEllipse(QRectF(9,9,6,6));for(int i=0;i<8;++i){p.save();p.translate(12,12);p.rotate(i*45);p.drawLine(0,-10,0,-8);p.restore();}break;
    default: p.drawRoundedRect(QRectF(5,10,14,11),2,2);p.drawArc(QRectF(8,2,8,14),0,180*16);break;
    }
    p.restore();
}
class NavButton : public QAbstractButton {
  public:
    int index;
    bool active = false, compact = false;
    NavButton(int index, const QString &name, QWidget *parent) : QAbstractButton(parent), index(index) {
        setText(name);setAccessibleName(name);setCursor(Qt::PointingHandCursor);setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setFixedHeight(48);
    }
  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        if(underMouse()&&!active){p.setPen(Qt::NoPen);p.setBrush(UiPalette::Surface);p.drawRoundedRect(rect().adjusted(0,1,0,-1),7,7);}
        auto ink=UiPalette::pageAccent(index==5?3:index);ink.setAlpha(active||underMouse()?255:190);
        p.setPen(QPen(ink,1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        drawNavIcon(p,index,QRectF(compact?(width()-22)/2:14,13,22,22));
        p.setPen(active||underMouse()?UiPalette::Text:UiPalette::Muted);
        if(!compact){auto f=font();f.setPixelSize(15);f.setWeight(active?QFont::DemiBold:QFont::Normal);p.setFont(f);p.drawText(rect().adjusted(48,0,-8,0),Qt::AlignVCenter|Qt::AlignLeft,text());}
        if(hasFocus()){p.setPen(QPen(ink,1));p.setBrush(Qt::NoBrush);p.drawRoundedRect(rect().adjusted(2,2,-3,-3),5,5);}
    }
    void enterEvent(QEnterEvent *event) override {QAbstractButton::enterEvent(event);update();}
    void leaveEvent(QEvent *event) override {QAbstractButton::leaveEvent(event);update();}
};
}
NavigationRail::NavigationRail(QWidget *parent):QWidget(parent) {
    setObjectName("navigationRail");setFixedWidth(168);setAttribute(Qt::WA_StyledBackground,false);
    brand=new QLabel("Z.T.L",this);brand->setObjectName("navigationBrand");
    brand->setStyleSheet("background:transparent;color:#E7E8EB;font:22px 'Bahnschrift';");
    const QStringList names{"Home","Mail","Calendar","Note","设置"};
    const QStringList shortcuts{"Alt+H","Alt+M","Alt+C","Alt+N","Alt+S"};
    for(int i=0;i<names.size();++i){
        auto button=new NavButton(i,names[i],this);button->setObjectName("nav"+QString::number(i));
        button->setToolTip(names[i]+"  "+shortcuts[i]);buttons.append(button);
        connect(button,&QAbstractButton::clicked,this,[this,i]{emit pageRequested(i);});
    }
    lockButton=new NavButton(5,"锁定秘密",this);lockButton->setObjectName("navLock");lockButton->setToolTip("锁定秘密  Alt+L");
    connect(lockButton,&QAbstractButton::clicked,this,&NavigationRail::lockRequested);
    cloud=new QLabel(this);cloud->setObjectName("navigationCloudStatus");cloud->setTextFormat(Qt::PlainText);
    cloud->setStyleSheet("background:transparent;color:#A0A4AD;font-size:12px;");
    motion.setDuration(180);motion.setEasingCurve(QEasingCurve::OutCubic);
    connect(&motion,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){selectionY=v.toReal();update();});
    connect(CloudConnection::instance(),&CloudConnection::settingsChanged,this,[this]{setCloudStatus();});
    connect(CloudConnection::instance(),&CloudConnection::status,this,&NavigationRail::setCloudStatus);
    qApp->installEventFilter(this);setCloudStatus();arrange();setCurrent(0);
}
void NavigationRail::arrange() {
    brand->setGeometry(compact?8:26,28,compact?56:120,34);
    for(int i=0;i<buttons.size();++i)buttons[i]->setGeometry(10,i==4?qMax(340,height()-170):108+i*58,width()-20,48);
    lockButton->setGeometry(10,qMax(392,height()-112),width()-20,42);
    cloud->setGeometry(compact?10:25,qMax(440,height()-52),width()-(compact?20:40),26);
    settle();
}
void NavigationRail::settle(){motion.stop();if(buttons.isEmpty())return;selectionY=buttons[current]->y();update();}
void NavigationRail::setCurrent(int page) {
    if(page<0||page>=buttons.size())return;
    current=page;
    for(int i=0;i<buttons.size();++i){auto button=static_cast<NavButton*>(buttons[i]);button->active=i==current;button->update();}
    motion.stop();
    if(UiMotion::enabled()&&isVisible()){motion.setStartValue(selectionY);motion.setEndValue(qreal(buttons[current]->y()));motion.start();}
    else settle();
}
void NavigationRail::setCompact(bool value) {
    if(compact==value)return;
    compact=value;setFixedWidth(compact?72:168);
    for(auto item:buttons)static_cast<NavButton*>(item)->compact=compact;
    static_cast<NavButton*>(lockButton)->compact=compact;
    brand->setText(compact?"Z.T.L":"Z.T.L");brand->setStyleSheet(QString("background:transparent;color:#E7E8EB;font:%1px 'Bahnschrift';").arg(compact?16:22));
    arrange();setCloudStatus(cloudMessage);
}
void NavigationRail::setCloudStatus(const QString &message) {
    cloudMessage=message;
    QString text=CloudConnection::enabled()?"云端启用":"云端停用";
    if(CloudConnection::enabled()&&!message.isEmpty()){
        if(message.contains("正在"))text="正在同步";
        else if(message.contains("冲突"))text="待处理冲突";
        else if(message.contains("失败")||message.contains("暂停")||message.contains("异常"))text="需要检查";
        else if(message.contains("同步完成"))text="同步完成";
    }
    cloud->setText(compact?QString("云端"):text);cloud->setToolTip(message.isEmpty()?text:message);
}
void NavigationRail::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(),UiPalette::Base);
    // Stationary, low-contrast flecks: no texture allocation or timer in work pages.
    p.setPen(QColor(180,180,190,7));for(int y=8;y<height();y+=19)for(int x=5;x<width()-2;x+=23)p.drawPoint(x+(y%5),y);
    p.setPen(UiPalette::Line);p.drawLine(width()-1,0,width()-1,height());
    const auto accent=UiPalette::pageAccent(current);
    p.setPen(Qt::NoPen);p.setBrush(UiPalette::tint(accent));p.drawRoundedRect(QRectF(10,selectionY,width()-20,48),7,7);
    p.setBrush(accent);p.drawRoundedRect(QRectF(10,selectionY+15,3,18),1.5,1.5);
    QPainterPath ripple;ripple.moveTo(26,79);ripple.cubicTo(54,68,72,89,102,78);ripple.cubicTo(118,73,130,72,143,76);
    if(!compact){
        p.setBrush(Qt::NoBrush);QLinearGradient ink(26,0,143,0);
        ink.setColorAt(0,UiPalette::Home);ink.setColorAt(.5,UiPalette::Settings);ink.setColorAt(1,UiPalette::Mail);
        p.setPen(QPen(QBrush(ink),1.3));p.drawPath(ripple);
    }
}
void NavigationRail::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);arrange();}
void NavigationRail::hideEvent(QHideEvent *event){settle();QWidget::hideEvent(event);}
bool NavigationRail::eventFilter(QObject *object,QEvent *event){
    if(object==qApp&&event->type()==QEvent::DynamicPropertyChange&&static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName()=="ztlAnimation"&&!UiMotion::enabled())settle();
    return QWidget::eventFilter(object,event);
}
