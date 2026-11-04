#include "deadlineagendawidget.h"
#include <QtWidgets>
#include <functional>
#include <algorithm>

namespace {
class AgendaCard:public QAbstractButton {
public:
    std::function<void()> open,hover,leave;
    AgendaCard(){setFocusPolicy(Qt::StrongFocus);}
    bool selected=false;
    QSize sizeHint() const override {return minimumSizeHint();}
    QSize minimumSizeHint() const override {auto size=layout()?layout()->minimumSize():QSize(120,76);size.setHeight(qMax(property("interval").toBool()?56:76,size.height()));return size;}
protected:
    void enterEvent(QEnterEvent *event) override {QAbstractButton::enterEvent(event);if(hover)hover();update();}
    void leaveEvent(QEvent *event) override {QAbstractButton::leaveEvent(event);if(leave)leave();update();}
    void mouseDoubleClickEvent(QMouseEvent *event) override {if(event->button()==Qt::LeftButton&&open){open();event->accept();}else QAbstractButton::mouseDoubleClickEvent(event);}
    void keyPressEvent(QKeyEvent *event) override {if((event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter)&&open){open();event->accept();}else QAbstractButton::keyPressEvent(event);}
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const bool active=selected;
        p.setPen(QPen(active?QColor("#7f2636"):hasFocus()?QColor("#646a73"):QColor("#14171d"),1));p.setBrush(QColor(isDown()?"#1b1c23":"#14171d"));p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),6,6);
        if(underMouse()&&!selected&&property("hoverEnabled").toBool()){p.fillRect(rect().adjusted(3,3,-3,-3),QColor(255,255,255,8));}
        p.setPen(QPen(active?QColor("#7f2636"):QColor("#343841"),2));p.drawLine(2,8,2,height()-8);
    }
};
class DayGroup:public QWidget {
public:
    QString active,preview;
    QMap<QString,QList<QWidget*>> endpoints;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        for(const auto &identity:{active,preview}){
            if(!endpoints.contains(identity)||endpoints[identity].size()!=2)continue;
            const auto nodes=endpoints[identity];const QPoint a=nodes[0]->mapTo(this,QPoint(10,24)),b=nodes[1]->mapTo(this,QPoint(10,24));
            painter.fillRect(QRect(QPoint(a.x()+8,qMin(a.y(),b.y())),QPoint(width(),qMax(a.y(),b.y()))),QColor(127,38,54,45));
            painter.setPen(QPen(QColor("#ce5975"),3));painter.drawLine(a,b);
        }
    }
};
class LabelButton:public QPushButton {
public:
    std::function<void()> open;
    explicit LabelButton(const QString &text):QPushButton(){setAccessibleName(text);setToolTip(text+" · 双击查看详情");auto body=new QVBoxLayout(this);body->setContentsMargins(0,2,0,2);auto label=new QLabel(text);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);label->setMaximumHeight(60);label->setMinimumWidth(0);label->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);label->setAttribute(Qt::WA_TransparentForMouseEvents);body->addWidget(label);}
    void keyPressEvent(QKeyEvent *event) override {if((event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter||event->key()==Qt::Key_Space)&&open){open();event->accept();}else QPushButton::keyPressEvent(event);}
protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override {if(event->button()==Qt::LeftButton&&open){open();event->accept();}else QPushButton::mouseDoubleClickEvent(event);}
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
    selectionScope=this;qApp->installEventFilter(this);
}
bool DeadlineAgendaWidget::eventFilter(QObject *object,QEvent *event){
    if(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonDblClick){
        auto target=qobject_cast<QWidget*>(object);auto scope=selectionScope.data();
        if(target&&scope&&target->window()==scope->window()&&(target==scope||scope->isAncestorOf(target))){
            bool card=false;for(auto part=target;part&&part!=scope;part=part->parentWidget())if(part->objectName()=="ddlAgendaCard"){card=true;break;}
            if(!card){selectedIdentity.clear();hoverIdentity.clear();updateHighlight();}
        }
    }
    return QScrollArea::eventFilter(object,event);
}
void DeadlineAgendaWidget::updateHighlight(){
    const auto preview=hoverIdentity==selectedIdentity?QString():hoverIdentity;
    const auto active=selectedIdentity.isEmpty()?preview:selectedIdentity;
    for(auto widget:findChildren<QWidget*>("ddlDateGroup")){
        auto group=static_cast<DayGroup*>(widget);group->active=active;group->preview=selectedIdentity.isEmpty()?QString():preview;group->setProperty("activeIdentity",active);group->setProperty("hoverIdentity",preview);
        for(auto child:group->findChildren<QAbstractButton*>("ddlAgendaCard")){
            auto card=static_cast<AgendaCard*>(child);card->selected=card->property("identity").toString()==selectedIdentity;
            card->setProperty("selected",card->selected);card->setProperty("hoverEnabled",!card->selected);card->update();
        }
        group->update();
    }
}
void DeadlineAgendaWidget::setOccurrences(const QList<TaskOccurrence> &items,const QDate &anchor,bool filtered){
    bool same=allItems.size()==items.size()&&selectedAnchor==anchor&&this->filtered==filtered;
    if(same)for(int i=0;i<items.size();++i){const auto &a=allItems[i],&b=items[i];if(a.taskId!=b.taskId||a.key!=b.key||a.due!=b.due||a.title!=b.title||a.note!=b.note||a.tags!=b.tags||a.done!=b.done||a.paused!=b.paused||a.priority!=b.priority||a.type!=b.type||a.start!=b.start){same=false;break;}}
    if(same&&property("loading")==property("renderLoading")&&property("renderMore").toBool()==externalMore&&property("renderMinute").toString()==QDateTime::currentDateTime().toString("yyyyMMddHHmm")){bool statesSame=true;for(const auto &o:items)if(o.isInterval()&&property(qPrintable("state/"+o.identity())).toString()!=o.state())statesSame=false;if(statesSame)return;}
    setProperty("renderMore",externalMore);setProperty("renderLoading",property("loading"));setProperty("renderMinute",QDateTime::currentDateTime().toString("yyyyMMddHHmm"));for(const auto &o:items)if(o.isInterval())setProperty(qPrintable("state/"+o.identity()),o.state());
    auto contains=[&items](const QString &identity){return std::any_of(items.begin(),items.end(),[&](const auto &o){return o.identity()==identity;});};
    if(!contains(selectedIdentity))selectedIdentity.clear();if(!contains(hoverIdentity))hoverIdentity.clear();
    allItems=items;selectedAnchor=anchor;this->filtered=filtered;render();
}
void DeadlineAgendaWidget::render(){
    QList<TaskOccurrence> items;int count=0;for(const auto &o:allItems){if(o.isLabel()||count<shown||o.date()==selectedAnchor)items.append(o);if(!o.isLabel())++count;}const auto anchor=selectedAnchor;
    const int oldScroll=verticalScrollBar()->value();dateAnchors.clear();
    QString topIdentity,topEndpoint,focusIdentity,focusEndpoint,focusName;int topOffset=0,topY=INT_MAX;
    if(widget()){for(auto card:widget()->findChildren<QAbstractButton*>("ddlAgendaCard")){const int y=card->mapTo(widget(),QPoint()).y();if(y+card->height()>=oldScroll&&y<topY){topY=y;topIdentity=card->property("identity").toString();topEndpoint=card->property("endpoint").toString();topOffset=y-oldScroll;}if(card==QApplication::focusWidget()||card->isAncestorOf(QApplication::focusWidget())){focusIdentity=card->property("identity").toString();focusEndpoint=card->property("endpoint").toString();focusName=QApplication::focusWidget()->objectName();}}}
    auto content=widget()?widget():new QWidget;auto layout=qobject_cast<QVBoxLayout*>(content->layout());QMap<QDate,DayGroup*> previous;
    if(layout){while(auto item=layout->takeAt(0)){if(auto w=item->widget()){if(w->objectName()=="ddlDateGroup")previous[w->property("date").toDate()]=static_cast<DayGroup*>(w);else {w->setParent(nullptr);w->deleteLater();}}delete item;}}else layout=new QVBoxLayout(content);layout->setContentsMargins(0,8,10,20);layout->setSpacing(22);
    const auto now=QDateTime::currentDateTime();QMap<QDate,QList<TaskOccurrence>> byDate,labels;
    struct Entry {TaskOccurrence item;QDateTime time;bool end=false;};
    for(const auto &o:items){const auto date=o.due.toLocalTime().date();
        if(o.isLabel()){labels[date].append(o);if(!byDate.contains(date))byDate[date]={};}
        else byDate[date].append(o);
    }
    if(!byDate.contains(anchor))byDate[anchor]={};
    bool boundaryAdded=false;
    auto makeBoundary=[&]{auto boundary=new QLabel("现在  "+now.toString("HH:mm"));boundary->setObjectName("ddlNowBoundary");boundary->setStyleSheet("color:#81868e;font-size:12px;border:0;border-bottom:1px solid #7f2636;padding:4px 0 10px 0;");boundaryAdded=true;return boundary;};
    auto appendGroups=[&](const QMap<QDate,QList<TaskOccurrence>> &groups){
        for(auto it=groups.begin();it!=groups.end();++it){
            const auto date=it.key();QByteArray signature;QDataStream sig(&signature,QIODevice::WriteOnly);sig<<date<<filtered<<property("loading").toBool()<<(date==now.date()?now.toString("yyyyMMddHHmm"):QString());
            for(const auto &o:it.value())sig<<o.taskId<<o.key<<o.identity()<<o.title<<o.note<<o.tags<<o.start<<o.due<<o.done<<o.paused<<o.priority<<o.state();for(const auto &o:labels[date])sig<<o.taskId<<o.key<<o.identity()<<o.title;
            if(auto existing=previous.value(date);existing&&existing->property("signature").toByteArray()==signature){previous.remove(date);dateAnchors[date]=existing;layout->addWidget(existing);if(existing->findChild<QLabel*>("ddlNowBoundary"))boundaryAdded=true;continue;}
            auto group=new DayGroup;group->setProperty("signature",signature);group->setObjectName("ddlDateGroup");group->setProperty("date",date);
            if(!dateAnchors.contains(date))dateAnchors[date]=group;
            auto groupLayout=new QHBoxLayout(group);groupLayout->setContentsMargins(0,0,0,0);groupLayout->setSpacing(14);
            auto dateBox=new QWidget;dateBox->setFixedWidth(94);auto dateLayout=new QVBoxLayout(dateBox);dateLayout->setContentsMargins(0,5,0,0);dateLayout->setSpacing(3);
            auto day=new QLabel(date.isValid()?QString::number(date.day()):"待办");QFont dayFont=day->font();dayFont.setPixelSize(date.isValid()?28:20);dayFont.setWeight(QFont::DemiBold);day->setFont(dayFont);
            day->setStyleSheet(QString("font-size:%1px;font-weight:600;color:#d0d0d0;background:transparent;").arg(date.isValid()?28:20));dateLayout->addWidget(day);
            auto caption=new QLabel(date.isValid()?QLocale(QLocale::Chinese).toString(date,date.year()==now.date().year()?"M月 ddd":"yyyy年M月 ddd"):"无截止时间");caption->setProperty("role","muted");caption->setWordWrap(true);dateLayout->addWidget(caption);
            if(date==now.date()){auto today=new QLabel("今天");today->setStyleSheet("color:#d0d0d0;border-left:2px solid #7f2636;padding-left:6px;");dateLayout->addWidget(today);}
            else if(date==now.date().addDays(1)){auto tomorrow=new QLabel("明天");tomorrow->setProperty("role","muted");dateLayout->addWidget(tomorrow);}
            for(const auto &o:labels[date]){
                auto label=new LabelButton(o.title);label->setObjectName("ddlAllDayLabel");label->setProperty("taskId",o.taskId);label->setStyleSheet("text-align:left;color:#ce8394;border:0;padding:3px 0;");label->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);label->setMinimumHeight(26);label->open=[this,o]{QTimer::singleShot(0,this,[this,o]{emit detailRequested(o);});};dateLayout->addWidget(label);
            }
            dateLayout->addStretch();groupLayout->addWidget(dateBox,0,Qt::AlignTop);
            auto entries=new QVBoxLayout;entries->setContentsMargins(0,0,0,0);entries->setSpacing(10);groupLayout->addLayout(entries,1);
            if(it.value().isEmpty()){
                if(!boundaryAdded&&date>=now.date())entries->addWidget(makeBoundary());
                auto empty=new QLabel(property("loading").toBool()?"正在加载日程…":filtered?"当天没有匹配的事务":"当天没有事务");empty->setObjectName("ddlDayEmpty");empty->setProperty("role","muted");empty->setMinimumHeight(64);empty->setContentsMargins(80,0,0,0);entries->addWidget(empty);
            }
            QList<Entry> nodes;for(const auto &o:it.value()){
                if(o.isInterval()){nodes.append({o,o.start,false});nodes.append({o,o.due,true});}else nodes.append({o,o.due,false});
            }
            std::stable_sort(nodes.begin(),nodes.end(),[](const auto &a,const auto &b){return a.time<b.time;});
            for(const auto &entry:nodes){const auto &o=entry.item;
                if(!boundaryAdded&&entry.time.isValid()&&entry.time>now)entries->addWidget(makeBoundary());
                auto row=new QHBoxLayout;row->setSpacing(10);
                auto time=new QLabel(entry.time.isValid()?entry.time.toLocalTime().toString("HH:mm"):"—");time->setFixedWidth(44);time->setAlignment(Qt::AlignRight|Qt::AlignTop);time->setContentsMargins(0,14,0,0);time->setProperty("role","muted");row->addWidget(time);
                auto dot=new TimelineDot;row->addWidget(dot);if(o.isInterval())group->endpoints[o.identity()].append(dot);
                auto card=new AgendaCard;card->setObjectName("ddlAgendaCard");card->setProperty("taskId",o.taskId);card->setProperty("occurrenceKey",o.key);card->setProperty("title",o.title);card->setProperty("done",o.done);card->setProperty("due",o.due);
                card->setProperty("interval",o.isInterval());card->setProperty("identity",o.identity());card->setProperty("endpoint",o.isInterval()?(entry.end?"end":"start"):"ddl");card->selected=selectedIdentity==o.identity();card->setProperty("selected",card->selected);
                card->setAccessibleName(o.title);card->setToolTip("单击选择，双击查看详情");card->setCursor(Qt::PointingHandCursor);card->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
                card->setStyleSheet("QLabel {background:transparent;border:0;padding:0;}");
                auto cardLayout=new QHBoxLayout(card);cardLayout->setContentsMargins(10,10,16,10);cardLayout->setSpacing(10);
                CompleteBox *check=nullptr;if(o.isDDL()){check=new CompleteBox(o.done,o.title,card);cardLayout->addWidget(check,0,Qt::AlignTop);}
                auto body=new QVBoxLayout;body->setSpacing(5);cardLayout->addLayout(body,1);
                QString meta=o.due.isValid()?o.due.toLocalTime().toString("HH:mm")+" 截止":"无截止时间";
                if(o.isInterval())meta=QString(entry.end?"结束":"开始")+" · "+o.state()+"   "+o.start.toLocalTime().toString("HH:mm")+" — "+o.due.toLocalTime().toString("HH:mm");
                if(o.repeat!="none")meta+="   "+(o.repeat=="daily"?QString("每天"):o.repeat=="weekly"?QString("每周"):QString("每月"));
                if(o.paused)meta+="   已暂停";
                auto metaLabel=new ElidedLabel(meta);metaLabel->setStyleSheet("color:#81868e;font-size:12px;");body->addWidget(metaLabel);
                auto title=new ElidedLabel((o.priority?"★  ":"")+o.title);QFont font=title->font();font.setWeight(QFont::DemiBold);font.setStrikeOut(o.done);title->setFont(font);title->setStyleSheet(o.done?"color:#81868e;":"color:#d0d0d0;");body->addWidget(title);
                if(o.isDDL()&&!o.note.trimmed().isEmpty()){auto note=new ElidedLabel(o.note.simplified());note->setProperty("role","muted");body->addWidget(note);}
                if(o.isDDL()&&!o.tags.isEmpty()){auto tags=new ElidedLabel(o.tags.split(',',Qt::SkipEmptyParts).join("   "));tags->setStyleSheet("color:#81868e;font-size:12px;");body->addWidget(tags);}
                row->addWidget(card,1);entries->addLayout(row);card->ensurePolished();
                for(auto label:card->findChildren<QLabel*>())label->setFixedHeight(label->fontMetrics().height()+2);
                cardLayout->invalidate();card->updateGeometry();
                card->open=[this,o]{QTimer::singleShot(0,this,[this,o]{emit detailRequested(o);});};
                card->hover=[this,o]{if(o.identity()!=selectedIdentity){hoverIdentity=o.identity();updateHighlight();}};card->leave=[this,o]{if(hoverIdentity==o.identity()){hoverIdentity.clear();updateHighlight();}};
                connect(card,&QAbstractButton::clicked,this,[this,o]{selectedIdentity=o.identity();updateHighlight();});
                if(check)connect(check,&QAbstractButton::clicked,this,[this,o,check]{check->setEnabled(false);QTimer::singleShot(0,this,[this,o]{emit completeRequested(o);});});
            }
            group->active=selectedIdentity;layout->addWidget(group);
        }
    };
    appendGroups(byDate);
    if(!boundaryAdded)layout->addWidget(makeBoundary());
    int transactionCount=0;for(const auto &o:allItems)if(!o.isLabel())++transactionCount;
    if(shown<transactionCount||externalMore){
        auto more=new QPushButton(QString("继续加载（已加载 %1 项）").arg(qMin(shown,transactionCount)));more->setObjectName("ddlAgendaMore");layout->addWidget(more);
        connect(more,&QPushButton::clicked,this,[this]{shown+=200;if(externalMore)emit moreRequested();else render();});
    }
    for(auto group:previous)delete group;
    for(auto nowLabel:content->findChildren<QLabel*>("ddlNowBoundary"))nowLabel->setText("现在  "+now.toString("HH:mm"));
    layout->addStretch();if(widget()!=content)setWidget(content);content->layout()->activate();verticalScrollBar()->setValue(oldScroll);
    updateHighlight();
    QAbstractButton *fallback=nullptr;for(auto card:content->findChildren<QAbstractButton*>("ddlAgendaCard")){if(!fallback)fallback=card;if(card->property("identity")==topIdentity&&card->property("endpoint")==topEndpoint)verticalScrollBar()->setValue(card->mapTo(content,QPoint()).y()-topOffset);if(!focusIdentity.isEmpty()&&card->property("identity")==focusIdentity&&card->property("endpoint")==focusEndpoint){auto target=focusName=="ddlAgendaCard"?card:card->findChild<QAbstractButton*>(focusName);if(target)target->setFocus(Qt::OtherFocusReason);focusIdentity.clear();}}
    if(!focusIdentity.isEmpty()&&fallback)fallback->setFocus(Qt::OtherFocusReason);
}
void DeadlineAgendaWidget::focusDate(const QDate &date){
    const auto group=dateAnchors.value(date);if(!group)return;
    QTimer::singleShot(0,this,[this,date]{if(auto target=dateAnchors.value(date)){widget()->layout()->activate();verticalScrollBar()->setValue(target->mapTo(widget(),QPoint()).y()-8);}});
}

void DeadlineAgendaWidget::selectItem(const QString &identity){selectedIdentity=identity;for(auto card:findChildren<QAbstractButton*>("ddlAgendaCard"))if(card->property("identity")==identity){card->click();ensureWidgetVisible(card);card->setFocus();break;}}
