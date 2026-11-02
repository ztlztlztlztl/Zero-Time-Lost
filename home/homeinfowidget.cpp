#include "homeinfowidget.h"
#include "homepalette.h"
#include <QtWidgets>

namespace {
class ClockLabel:public QLabel {
public:
    using QLabel::QLabel;
    qreal flowTime=32;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::TextAntialiasing);
        QPainterPath path;path.addText(0,0,font(),text());const auto bounds=path.boundingRect();
        p.translate((width()-bounds.width())/2-bounds.left(),(height()-bounds.height())/2-bounds.top());
        const auto colors=HomePalette::colors(flowTime);
        QLinearGradient gradient(bounds.topLeft(),bounds.bottomLeft());gradient.setColorAt(0,QColor("#dce0e5"));gradient.setColorAt(.68,QColor("#dce0e5"));
        gradient.setColorAt(1,HomePalette::blend(QColor("#dce0e5"),colors[1].lighter(230),.22));
        p.fillPath(path,gradient);
    }
};
}
HomeInfoWidget::HomeInfoWidget(QWidget *parent):QWidget(parent){
    setObjectName("homeTimeGroup");setAttribute(Qt::WA_TransparentForMouseEvents);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(12);
    date=new QLabel;date->setObjectName("homeDate");date->setAlignment(Qt::AlignCenter);date->setStyleSheet("color:#8c939d;background:transparent;font:18px 'Microsoft YaHei UI';");layout->addWidget(date);
    clock=new ClockLabel;clock->setObjectName("homeClock");clock->setStyleSheet("font-family:'Segoe UI';font-weight:300;");clock->setMinimumWidth(0);clock->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);clock->setAlignment(Qt::AlignCenter);clock->setTextFormat(Qt::PlainText);layout->addWidget(clock);
    timer=new QTimer(this);timer->setObjectName("homeClockTimer");timer->setInterval(1000);connect(timer,&QTimer::timeout,this,[this]{refreshDateTime();});refreshDateTime();
}
void HomeInfoWidget::refreshDateTime(const QDateTime &now){
    const auto value=now.toString("HH:mm");if(clock->text()!=value){clock->setText(value);clock->setAccessibleName("当前时间 "+value);}
    date->setText(QLocale(QLocale::English).toString(now.date(),"yyyy.MM.dd  dddd"));
    if(displayedDay!=now.date()){displayedDay=now.date();emit dayChanged();}
}
void HomeInfoWidget::setFlowTime(qreal seconds){static_cast<ClockLabel*>(clock)->flowTime=seconds;clock->update();}
void HomeInfoWidget::resizeEvent(QResizeEvent *event){
    QWidget::resizeEvent(event);QFont font("Segoe UI");font.setWeight(QFont::Light);font.setPixelSize(qBound(80,qRound(width()*.14),156));clock->setFont(font);
    clock->setFixedHeight(QFontMetrics(font).height()+12);
}
void HomeInfoWidget::showEvent(QShowEvent *event){QWidget::showEvent(event);refreshDateTime();timer->start();}
void HomeInfoWidget::hideEvent(QHideEvent *event){QWidget::hideEvent(event);timer->stop();}
