#include "deadlineagendawidget.h"
#include <QtWidgets>

namespace {
class AgendaCard:public QAbstractButton {
public:
    AgendaCard(){setFocusPolicy(Qt::StrongFocus);}
    QSize sizeHint() const override {return minimumSizeHint();}
    QSize minimumSizeHint() const override {auto size=layout()?layout()->minimumSize():QSize(120,76);size.setHeight(qMax(76,size.height()));return size;}
protected:
    void enterEvent(QEnterEvent *event) override {QAbstractButton::enterEvent(event);update();}
    void leaveEvent(QEvent *event) override {QAbstractButton::leaveEvent(event);update();}
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const bool active=underMouse()||hasFocus();
        p.setPen(QPen(active?QColor("#7f2636"):QColor("#14171d"),1));p.setBrush(QColor(isDown()?"#1b1c23":"#14171d"));p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),6,6);
        p.setPen(QPen(active?QColor("#7f2636"):QColor("#343841"),2));p.drawLine(2,8,2,height()-8);
    }
};
class CompleteBox:public QAbstractButton {
public:
    explicit CompleteBox(bool done,const QString &title,QWidget *parent):QAbstractButton(parent){
        setObjectName("ddlComplete");setCheckable(true);setChecked(done);setEnabled(!done);
        setFixedSize(32,32);setCursor(Qt::PointingHandCursor);setFocusPolicy(Qt::StrongFocus);
        setAccessibleName((done?"已完成：":"完成：")+title);setToolTip(done?"已完成":"完成本次事务");
    }
protected:
    void enterEvent(QEnterEvent *event) override {QAbstractButton::enterEvent(event);update();}
    void leaveEvent(QEvent *event) override {QAbstractButton::leaveEvent(event);update();}
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const QRectF box(6,6,20,20);p.setPen(QPen(underMouse()||hasFocus()?QColor("#7f2636"):QColor("#646a73"),underMouse()?2:1));
        p.setBrush(isDown()?QColor("#30202a"):Qt::transparent);p.drawRoundedRect(box,3,3);
        if(isChecked()){p.setPen(QPen(QColor("#81868e"),2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawPolyline(QPolygonF{QPointF(10,16),QPointF(14,20),QPointF(22,12)});}
        if(hasFocus()){p.setPen(QPen(QColor("#d0d0d0"),1,Qt::DotLine));p.setBrush(Qt::NoBrush);p.drawRoundedRect(QRectF(2,2,28,28),4,4);}
    }
};
class ElidedLabel:public QLabel {
public:
    explicit ElidedLabel(const QString &value,QWidget *parent=nullptr):QLabel(parent),full(value){
        setTextFormat(Qt::PlainText);setText(value);setToolTip(value);setMinimumWidth(0);setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    QSize sizeHint() const override {return QSize(80,fontMetrics().height()+2);}
    QSize minimumSizeHint() const override {return QSize(0,fontMetrics().height()+2);}
protected:
    void resizeEvent(QResizeEvent *event) override {QLabel::resizeEvent(event);setText(fontMetrics().elidedText(full,Qt::ElideRight,width()));}
private:
    QString full;
};
class TimelineDot:public QWidget {
public:
    explicit TimelineDot(QWidget *parent=nullptr):QWidget(parent){setFixedWidth(20);setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Expanding);}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#343841"),1));p.drawLine(10,0,10,height());
        p.setBrush(QColor("#101216"));p.setPen(QPen(QColor("#81868e"),1.2));p.drawEllipse(QPointF(10,24),4,4);
    }
};
}
DeadlineAgendaWidget::DeadlineAgendaWidget(QWidget *parent):QScrollArea(parent){
    setObjectName("ddlAgenda");setWidgetResizable(true);setFrameShape(QFrame::NoFrame);setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet("QScrollArea#ddlAgenda { border:0; background:#101216; }");
}
void DeadlineAgendaWidget::setOccurrences(const QList<TaskOccurrence> &items,const QDate &anchor,bool filtered){
    bool same=allItems.size()==items.size()&&selectedAnchor==anchor&&this->filtered==filtered;
    if(same)for(int i=0;i<items.size();++i){const auto &a=allItems[i],&b=items[i];if(a.taskId!=b.taskId||a.key!=b.key||a.due!=b.due||a.title!=b.title||a.note!=b.note||a.tags!=b.tags||a.done!=b.done||a.paused!=b.paused||a.priority!=b.priority){same=false;break;}}
    if(!same)shown=200;allItems=items;selectedAnchor=anchor;this->filtered=filtered;render();
}
void DeadlineAgendaWidget::render(){
    const auto items=allItems.mid(0,shown);const auto anchor=selectedAnchor;
    const int oldScroll=verticalScrollBar()->value();dateAnchors.clear();
    auto content=new QWidget;auto layout=new QVBoxLayout(content);layout->setContentsMargins(0,8,10,20);layout->setSpacing(22);
    const auto now=QDateTime::currentDateTime();QMap<QDate,QList<TaskOccurrence>> overdue,future;
    for(const auto &o:items){const auto date=o.due.toLocalTime().date();
        if(o.repeat=="none"&&(!o.due.isValid()||o.due<=now))overdue[date].append(o);else future[date].append(o);}
    if(!overdue.contains(anchor)&&!future.contains(anchor)){
        if(anchor<now.date())overdue[anchor]={};else future[anchor]={};
    }
    auto appendGroups=[&](const QMap<QDate,QList<TaskOccurrence>> &groups){
        for(auto it=groups.begin();it!=groups.end();++it){
            const auto date=it.key();auto group=new QWidget;group->setObjectName("ddlDateGroup");group->setProperty("date",date);
            if(!dateAnchors.contains(date))dateAnchors[date]=group;
            auto groupLayout=new QHBoxLayout(group);groupLayout->setContentsMargins(0,0,0,0);groupLayout->setSpacing(14);
            auto dateBox=new QWidget;dateBox->setFixedWidth(94);auto dateLayout=new QVBoxLayout(dateBox);dateLayout->setContentsMargins(0,5,0,0);dateLayout->setSpacing(3);
            auto day=new QLabel(date.isValid()?QString::number(date.day()):"待办");QFont dayFont=day->font();dayFont.setPixelSize(date.isValid()?28:20);dayFont.setWeight(QFont::DemiBold);day->setFont(dayFont);
            day->setStyleSheet(QString("font-size:%1px;font-weight:600;color:#d0d0d0;background:transparent;").arg(date.isValid()?28:20));dateLayout->addWidget(day);
            auto caption=new QLabel(date.isValid()?QLocale(QLocale::Chinese).toString(date,date.year()==now.date().year()?"M月 ddd":"yyyy年M月 ddd"):"无截止时间");caption->setProperty("role","muted");caption->setWordWrap(true);dateLayout->addWidget(caption);
            if(date==now.date()){auto today=new QLabel("今天");today->setStyleSheet("color:#d0d0d0;border-left:2px solid #7f2636;padding-left:6px;");dateLayout->addWidget(today);}
            else if(date==now.date().addDays(1)){auto tomorrow=new QLabel("明天");tomorrow->setProperty("role","muted");dateLayout->addWidget(tomorrow);}
            dateLayout->addStretch();groupLayout->addWidget(dateBox,0,Qt::AlignTop);
            auto entries=new QVBoxLayout;entries->setContentsMargins(0,0,0,0);entries->setSpacing(10);groupLayout->addLayout(entries,1);
            if(it.value().isEmpty()){
                auto empty=new QLabel(filtered?"当天没有匹配的事务":"当天没有事务");empty->setObjectName("ddlDayEmpty");empty->setProperty("role","muted");empty->setMinimumHeight(64);empty->setContentsMargins(80,0,0,0);entries->addWidget(empty);
            }
            for(const auto &o:it.value()){
                auto row=new QHBoxLayout;row->setSpacing(10);
                auto time=new QLabel(o.due.isValid()?o.due.toLocalTime().toString("HH:mm"):"—");time->setFixedWidth(44);time->setAlignment(Qt::AlignRight|Qt::AlignTop);time->setContentsMargins(0,14,0,0);time->setProperty("role","muted");row->addWidget(time);
                row->addWidget(new TimelineDot);
                auto card=new AgendaCard;card->setObjectName("ddlAgendaCard");card->setProperty("taskId",o.taskId);card->setProperty("occurrenceKey",o.key);card->setProperty("title",o.title);card->setProperty("done",o.done);card->setProperty("due",o.due);
                card->setAccessibleName(o.title);card->setToolTip("查看事务详情");card->setCursor(Qt::PointingHandCursor);card->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
                card->setStyleSheet("QLabel {background:transparent;border:0;padding:0;}");
                auto cardLayout=new QHBoxLayout(card);cardLayout->setContentsMargins(10,10,16,10);cardLayout->setSpacing(10);
                auto check=new CompleteBox(o.done,o.title,card);cardLayout->addWidget(check,0,Qt::AlignTop);
                auto body=new QVBoxLayout;body->setSpacing(5);cardLayout->addLayout(body,1);
                QString meta=o.due.isValid()?o.due.toLocalTime().toString("HH:mm")+" 截止":"无截止时间";
                if(o.repeat!="none")meta+="   "+(o.repeat=="daily"?QString("每天"):o.repeat=="weekly"?QString("每周"):QString("每月"));
                if(o.paused)meta+="   已暂停";
                auto metaLabel=new ElidedLabel(meta);metaLabel->setStyleSheet("color:#81868e;font-size:12px;");body->addWidget(metaLabel);
                auto title=new ElidedLabel((o.priority?"★  ":"")+o.title);QFont font=title->font();font.setWeight(QFont::DemiBold);font.setStrikeOut(o.done);title->setFont(font);title->setStyleSheet(o.done?"color:#81868e;":"color:#d0d0d0;");body->addWidget(title);
                if(!o.note.trimmed().isEmpty()){auto note=new ElidedLabel(o.note.simplified());note->setProperty("role","muted");body->addWidget(note);}
                if(!o.tags.isEmpty()){auto tags=new ElidedLabel(o.tags.split(',',Qt::SkipEmptyParts).join("   "));tags->setStyleSheet("color:#81868e;font-size:12px;");body->addWidget(tags);}
                row->addWidget(card,1);entries->addLayout(row);card->ensurePolished();
                for(auto label:card->findChildren<QLabel*>())label->setFixedHeight(label->fontMetrics().height()+2);
                cardLayout->invalidate();card->updateGeometry();
                connect(card,&QAbstractButton::clicked,this,[this,o]{QTimer::singleShot(0,this,[this,o]{emit detailRequested(o);});});
                connect(check,&QAbstractButton::clicked,this,[this,o,check]{check->setEnabled(false);QTimer::singleShot(0,this,[this,o]{emit completeRequested(o);});});
            }
            layout->addWidget(group);
        }
    };
    appendGroups(overdue);
    auto boundary=new QLabel("现在  "+now.toString("HH:mm"));boundary->setObjectName("ddlNowBoundary");boundary->setStyleSheet("color:#81868e;font-size:12px;border:0;border-bottom:1px solid #7f2636;padding:4px 0 10px 108px;");layout->addWidget(boundary);
    appendGroups(future);
    if(shown<allItems.size()){
        auto more=new QPushButton(QString("显示更多（已显示 %1 / %2 项）").arg(shown).arg(allItems.size()));more->setObjectName("ddlAgendaMore");layout->addWidget(more);
        connect(more,&QPushButton::clicked,this,[this]{shown+=200;render();});
    }
    layout->addStretch();setWidget(content);verticalScrollBar()->setValue(oldScroll);
}
void DeadlineAgendaWidget::focusDate(const QDate &date){
    const auto group=dateAnchors.value(date);if(!group)return;
    QTimer::singleShot(0,this,[this,date]{if(auto target=dateAnchors.value(date)){widget()->layout()->activate();verticalScrollBar()->setValue(target->mapTo(widget(),QPoint()).y()-8);}});
}
