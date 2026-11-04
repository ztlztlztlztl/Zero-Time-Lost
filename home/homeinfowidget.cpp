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
    setObjectName("homeTimeGroup");setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(12);
    date=new QLabel;date->setObjectName("homeDate");date->setAlignment(Qt::AlignCenter);date->setStyleSheet("color:#8c939d;background:transparent;font:18px 'Microsoft YaHei UI';");layout->addWidget(date);
    labels=new QLabel;labels->setObjectName("homeCalendarLabels");intervals=new QLabel;intervals->setObjectName("homeCurrentIntervals");
    for(auto row:{labels,intervals}){row->setAlignment(Qt::AlignCenter);row->setWordWrap(true);row->setTextFormat(Qt::PlainText);row->setStyleSheet("color:#c7cbd2;background:transparent;font:15px 'Microsoft YaHei UI';");layout->addWidget(row);row->hide();}
    clock=new ClockLabel;clock->setObjectName("homeClock");clock->setStyleSheet("font-family:'Segoe UI';font-weight:300;");clock->setMinimumWidth(0);clock->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);clock->setAlignment(Qt::AlignCenter);clock->setTextFormat(Qt::PlainText);layout->addWidget(clock);
    for(auto row:{labels,intervals}){row->setMaximumHeight(58);row->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::LinksAccessibleByKeyboard);connect(row,&QLabel::linkActivated,this,[this](const QString &link){
        if(link.startsWith("item/")){const int index=link.mid(5).toInt();if(index>=0&&index<calendarItems.size()){const auto o=calendarItems[index];emit itemRequested(o.identity(),o.date());}return;}
        const bool wantLabels=link=="labels";QDialog dialog(this);dialog.setWindowTitle(wantLabels?"今日全天标签":"正在进行的事务");auto layout=new QVBoxLayout(&dialog);auto list=new QListWidget;layout->addWidget(list);QList<TaskOccurrence> choices;
        auto fill=[&]{QString selected;if(list->currentRow()>=0&&list->currentRow()<choices.size())selected=choices[list->currentRow()].identity();list->clear();choices.clear();for(const auto &o:calendarItems)if(o.isLabel()==wantLabels){choices.append(o);auto item=new QListWidgetItem(o.title+(o.isInterval()?" · "+o.due.toLocalTime().toString("HH:mm")+" 结束":QString()),list);item->setToolTip(o.note);if(o.identity()==selected)list->setCurrentItem(item);}if(list->currentRow()<0&&list->count())list->setCurrentRow(0);};
        connect(this,&HomeInfoWidget::calendarItemsChanged,&dialog,fill);fill();
        connect(list,&QListWidget::itemActivated,&dialog,[&](QListWidgetItem *item){const int index=list->row(item);if(index<0||index>=choices.size())return;const auto o=choices[index];dialog.accept();emit itemRequested(o.identity(),o.date());});dialog.resize(480,320);dialog.exec();
    });}
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

void HomeInfoWidget::setCalendarContext(const QStringList &dayLabels,const QStringList &current){
    labels->setText(dayLabels.isEmpty()?QString():"今天  "+dayLabels.join("，"));labels->setVisible(!dayLabels.isEmpty());
    intervals->setText(current.isEmpty()?QString():"现在  "+current.join("，"));intervals->setVisible(!current.isEmpty());
}

void HomeInfoWidget::setCalendarItems(const QList<TaskOccurrence> &items){
    calendarItems=items;emit calendarItemsChanged();
    for(bool isLabel:{true,false}){QStringList texts,full;int total=0;for(int i=0;i<items.size();++i){const auto &o=items[i];if(o.isLabel()!=isLabel)continue;++total;const auto title=o.title+(o.isInterval()?" · "+o.due.toLocalTime().toString("HH:mm")+" 结束":QString());full.append(title);if(texts.size()<2)texts.append(QString("<a style='color:#c7cbd2;text-decoration:none' href='item/%1'>%2</a>").arg(i).arg(fontMetrics().elidedText(title,Qt::ElideRight,240).toHtmlEscaped()));}
        if(total>2)texts.append(QString("<a style='color:#ce8394' href='%1'>还有 %2 项</a>").arg(isLabel?"labels":"intervals").arg(total-2));auto row=isLabel?labels:intervals;row->setTextFormat(Qt::RichText);row->setText((isLabel?QString("今天  "):QString("现在  "))+texts.join("，"));row->setToolTip(full.join('\n'));row->setVisible(total>0);
    }
}
