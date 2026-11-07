#include "uipalette.h"
#include "deadlinecalendarwidget.h"
#include "popuppolicy.h"
#include "dialoglayout.h"
#include "weeklayout.h"
#include <QtWidgets>
#include <algorithm>

namespace {
using C=DeadlineCalendarWidget;
int minutes(const QDateTime &time){const auto t=time.toLocalTime().time();return t.hour()*60+t.minute();}
int visibleCount(const QRect &rect,int total){return qMax(0,qMin(total,(rect.height()+3)/25));}
QRect entryArea(QRect rect,bool month){rect.adjust(8,7,-8,-7);if(month)rect.setTop(rect.top()+28);return rect;}
int shownEntries(const QRect &rect,int total){int shown=visibleCount(rect,total);if(shown<total)shown=qMax(0,shown-1);return shown;}

QVector<QRect> chipRects(const QRect &cell,const QModelIndex &index){
    const bool month=index.data(C::MonthRole).toBool();auto area=entryArea(cell,month);const auto entries=index.data(C::EntriesRole).toList();const int shown=month?shownEntries(area,entries.size()):int(entries.size());QVector<QRect> result;
    for(int i=0;i<shown;++i){int top=area.top()+i*25;
        const auto node=entries[i].toMap();if(!month)top=cell.top()+node["top"].toInt();
        result.append(QRect(area.left(),top,qMax(1,area.width()),month?22:node["height"].toInt()));
    }return result;
}
QRect overflowRect(const QRect &cell,const QModelIndex &index){const auto area=entryArea(cell,index.data(C::MonthRole).toBool());const auto entries=index.data(C::EntriesRole).toList();const int shown=shownEntries(area,entries.size());return QRect(area.left()+4,area.top()+shown*25,area.width()-4,22);}

class CalendarHeader:public QHeaderView {
public:
    explicit CalendarHeader(QWidget *parent):QHeaderView(Qt::Horizontal,parent){setDefaultAlignment(Qt::AlignCenter);}
protected:
    void paintSection(QPainter *p,const QRect &rect,int section) const override {
        p->save();p->setClipRect(rect);p->fillRect(rect,UiPalette::color("#101216"));p->setPen(UiPalette::color("#81868e"));
        const auto lines=model()->headerData(section,orientation()).toString().split('\n');
        if(lines.size()<2)p->drawText(rect,Qt::AlignCenter,lines.value(0));
        else{
            p->drawText(QRect(rect.left(),rect.top()+4,rect.width(),24),Qt::AlignCenter,lines[0]);
            const auto date=model()->headerData(section,orientation(),Qt::UserRole).toDate();const QPointF center(rect.center().x(),rect.top()+45);
            if(date==QDate::currentDate()){p->setPen(Qt::NoPen);p->setBrush(UiPalette::tint(UiPalette::Calendar,.24));p->drawEllipse(center,15,15);}
            p->setPen(UiPalette::color("#d0d0d0"));p->drawText(QRectF(center.x()-20,center.y()-15,40,30),Qt::AlignCenter,QString::number(date.day()));
            const auto labels=model()->headerData(section,orientation(),C::LabelsRole).toStringList();
            for(int i=0;i<qMin(3,labels.size());++i){p->setPen(UiPalette::color("#d08c9c"));const auto text=i==2&&labels.size()>3?QString("还有 %1 个全天标签").arg(labels.size()-2):labels[i];p->drawText(QRect(rect.left()+6,rect.top()+66+i*22,rect.width()-12,22),Qt::AlignCenter,fontMetrics().elidedText(text,Qt::ElideRight,rect.width()-12));}
        }
        p->setPen(UiPalette::color("#343841"));p->drawLine(rect.bottomLeft(),rect.bottomRight());p->restore();
    }
};
class CalendarDelegate:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        p->save();p->setRenderHint(QPainter::Antialiasing);p->setClipRect(option.rect);
        const auto date=index.data(C::DateRole).toDate();const bool month=index.data(C::MonthRole).toBool(),today=date==QDate::currentDate();
        p->fillRect(option.rect,index.data(Qt::BackgroundRole).value<QBrush>());
        if(!month){
            const auto top=index.data(C::HighlightTopRole),bottom=index.data(C::HighlightBottomRole);
            if(top.isValid()&&bottom.isValid())p->fillRect(QRect(option.rect.left(),option.rect.top()+top.toInt(),option.rect.width(),bottom.toInt()-top.toInt()),UiPalette::color(127,38,54,65));
        }
        p->setPen(UiPalette::color("#292d35"));p->drawRect(option.rect.adjusted(0,0,-1,-1));p->setFont(option.font);
        auto rect=entryArea(option.rect,month);
        if(month){
            const auto head=option.rect.adjusted(8,7,-8,-7);
            if(today){p->setPen(Qt::NoPen);p->setBrush(UiPalette::tint(UiPalette::Calendar,.24));p->drawEllipse(QRectF(head.left(),head.top(),25,25));}
            p->setPen(today?UiPalette::color("#ffffff"):index.data(Qt::ForegroundRole).value<QBrush>().color());p->drawText(QRect(head.left(),head.top(),25,25),Qt::AlignCenter,QString::number(date.day()));
            p->setPen(UiPalette::color("#d08c9c"));p->drawText(QRect(head.left()+30,head.top(),qMax(0,head.width()-30),25),Qt::AlignVCenter,fontMetrics(option).elidedText(index.data(C::LabelsRole).toStringList().join("，"),Qt::ElideRight,qMax(0,head.width()-30)));
        }
        const auto titles=index.data(C::DetailsRole).toStringList();const int shown=month?shownEntries(rect,titles.size()):int(titles.size());
        const auto chips=chipRects(option.rect,index);
        for(int i=0;i<chips.size();++i){const auto chip=chips[i];
            p->setPen(Qt::NoPen);p->setBrush(UiPalette::color("#242832"));p->drawRoundedRect(chip,3,3);p->setPen(index.data(Qt::ForegroundRole).value<QBrush>().color());p->drawText(chip.adjusted(5,0,-5,0),Qt::AlignVCenter,fontMetrics(option).elidedText(titles[i],Qt::ElideRight,qMax(0,chip.width()-10)));
        }
        if(shown<titles.size()){p->setPen(UiPalette::color("#a1a6af"));p->drawText(overflowRect(option.rect,index),Qt::AlignVCenter,QString("还有 %1 项 · 双击查看").arg(titles.size()-shown));}
        const auto nowLine=index.data(C::NowLineRole);
        if(!month&&nowLine.isValid()){const int y=option.rect.top()+nowLine.toInt();p->setPen(QPen(UiPalette::color("#ce5975"),2));p->drawLine(option.rect.left(),y,option.rect.right(),y);}
        if(option.state&(QStyle::State_MouseOver|QStyle::State_Selected)){p->setBrush(Qt::NoBrush);p->setPen(QPen(UiPalette::color("#7f2636"),(option.state&QStyle::State_Selected)?2:1));p->drawRect(option.rect.adjusted(1,1,-2,-2));}
        p->restore();
    }
private:
    static QFontMetrics fontMetrics(const QStyleOptionViewItem &option){return QFontMetrics(option.font);}
};
}
DeadlineCalendarWidget::DeadlineCalendarWidget(DDLDepartment *department,QWidget *parent):QWidget(parent),dept(department){
    setObjectName("ddlCalendar");auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    grid=new QTableWidget;grid->setObjectName("ddlCalendarGrid");grid->setColumnCount(7);grid->setItemDelegate(new CalendarDelegate(grid));grid->setHorizontalHeader(new CalendarHeader(grid));
    grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);grid->horizontalHeader()->setHighlightSections(false);
    grid->setSelectionMode(QAbstractItemView::SingleSelection);grid->setSelectionBehavior(QAbstractItemView::SelectItems);grid->setEditTriggers(QAbstractItemView::NoEditTriggers);grid->setFocusPolicy(Qt::StrongFocus);
    grid->setShowGrid(false);grid->setMouseTracking(true);grid->setCursor(Qt::PointingHandCursor);grid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);grid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    grid->setStyleSheet(UiPalette::style("QTableWidget {border:1px solid #343841;border-radius:6px;background:#101216;} QHeaderView::section {background:#101216;color:#81868e;border:0;padding:6px;font-size:12px;}"));layout->addWidget(grid,1);
    grid->viewport()->installEventFilter(this);grid->horizontalHeader()->viewport()->installEventFilter(this);
    auto openItems=[this]{if(auto cell=grid->currentItem()){QList<int> ids;for(const auto &entry:cell->data(EntriesRole).toList()){const int id=entry.toMap()["index"].toInt();if(!ids.contains(id))ids.append(id);}for(int i=0;i<displayed.size();++i)if(displayed[i].isLabel()&&displayed[i].date()==cell->data(DateRole).toDate())ids.append(i);if(ids.isEmpty())emit dateActivated(cell->data(DateRole).toDate());else showItems(ids,grid->mapToGlobal(grid->visualItemRect(cell).center()));}};
    for(int key:{Qt::Key_Return,Qt::Key_Enter,Qt::Key_Space}){auto open=new QShortcut(QKeySequence(key),grid);open->setContext(Qt::WidgetWithChildrenShortcut);connect(open,&QShortcut::activated,this,openItems);}
    grid->setContextMenuPolicy(Qt::CustomContextMenu);connect(grid,&QWidget::customContextMenuRequested,this,[this](const QPoint &pos){const auto index=grid->indexAt(pos);if(!index.isValid())return;QMenu menu;for(const auto &entry:QList<QPair<QString,QString>>{{"新增 DDL","ddl"},{"新增区间事务","interval"},{"新增全天标签","label"}}){auto action=menu.addAction(entry.first);connect(action,&QAction::triggered,this,[this,index,kind=entry.second]{emit createRequested(index.data(DateRole).toDate(),monthView?600:index.data(RowStartRole).toInt(),kind);});}menu.exec(grid->viewport()->mapToGlobal(pos));});
    refresh();
}
void DeadlineCalendarWidget::setDate(const QDate &date){if(date.isValid()&&anchor!=date){grid->clearSelection();anchor=date;scrollPending=true;refresh();}}
void DeadlineCalendarWidget::setMonthView(bool month){if(monthView!=month){grid->clearSelection();monthView=month;scrollPending=true;refresh();}}
void DeadlineCalendarWidget::setFilter(const AdvancedFilter &value){filter=value;refresh();}
void DeadlineCalendarWidget::setFilter(const QString &text,const QString &tag,int star){AdvancedFilter value;value.keywords=text;if(!tag.isEmpty())value.tags={tag};value.starred=star;setFilter(value);}
void DeadlineCalendarWidget::showItems(const QList<int> &indexes,const QPoint &position){
    if(indexes.isEmpty())return;
    if(indexes.size()==1){const auto item=displayed.value(indexes.first());QTimer::singleShot(0,this,[this,item]{emit detailRequested(item);});return;}
    QDialog dialog(this);dialog.setWindowTitle("当天事务");auto layout=new QVBoxLayout(&dialog);auto list=new QListWidget;list->setObjectName("calendarOverflow");layout->addWidget(new QLabel("双击或按 Enter 查看详情"));layout->addWidget(list);list->setFocus();
    QList<TaskOccurrence> choices;for(int id:indexes){const auto o=displayed.value(id);choices.append(o);auto item=new QListWidgetItem((o.isLabel()?QString("全天 · "):o.isInterval()?o.start.toLocalTime().toString("HH:mm")+" — "+o.due.toLocalTime().toString("HH:mm")+" · ":o.due.toLocalTime().toString("HH:mm")+" · ")+o.title,list);item->setData(Qt::UserRole,choices.size()-1);}
    if(list->count())list->setCurrentRow(0);int chosen=-1;connect(list,&QListWidget::itemActivated,&dialog,[&](QListWidgetItem *item){chosen=item->data(Qt::UserRole).toInt();dialog.accept();});const auto bounds=DialogLayout::fit(dialog,this,QSize(460,320));dialog.move(DialogLayout::clamp(position,dialog.size(),bounds));PopupPolicy::exec(dialog);
    if(chosen>=0){const auto item=choices.value(chosen);QTimer::singleShot(0,this,[this,item]{emit detailRequested(item);});}
}
void DeadlineCalendarWidget::activateCell(const QModelIndex &index,const QPoint &position){
    if(!index.isValid())return;const auto cell=grid->visualRect(index);const auto entries=index.data(EntriesRole).toList();const auto rect=entryArea(cell,monthView);
    if(monthView&&position.y()<rect.top()&&position.x()>cell.left()+36){QList<int> labels;for(int i=0;i<displayed.size();++i)if(displayed[i].isLabel()&&displayed[i].due.toLocalTime().date()==index.data(DateRole).toDate())labels.append(i);if(!labels.isEmpty()){showItems(labels,grid->viewport()->mapToGlobal(position));return;}}
    const auto chips=chipRects(cell,index);for(int i=0;i<chips.size();++i)if(chips[i].contains(position)){showItems({entries[i].toMap()["index"].toInt()},grid->viewport()->mapToGlobal(position));return;}
    if(chips.size()<entries.size()&&overflowRect(cell,index).contains(position)){QList<int> ids;for(const auto &entry:entries){const int id=entry.toMap()["index"].toInt();if(!ids.contains(id))ids.append(id);}showItems(ids,grid->viewport()->mapToGlobal(position));return;}
    emit dateActivated(index.data(DateRole).toDate());
}
void DeadlineCalendarWidget::highlight(const QString &identity){
    TaskOccurrence active;for(const auto &o:displayed)if(o.identity()==identity&&o.isInterval()&&CalendarRules::matches(o,filter))active=o;
    for(int r=0;r<grid->rowCount();++r)for(int c=0;c<7;++c){
        auto item=grid->item(r,c);if(!item)continue;const bool match=!active.taskId.isEmpty()&&item->data(DateRole).toDate()==active.due.toLocalTime().date();
        item->setData(HighlightStartRole,match?QVariant(minutes(active.start)):QVariant());item->setData(HighlightEndRole,match?QVariant(minutes(active.due)):QVariant());
        QVariant top,bottom;const int low=item->data(RowStartRole).toInt(),high=item->data(RowEndRole).toInt();
        if(match&&!monthView&&minutes(active.start)<high&&minutes(active.due)>=low){
            top=0;bottom=grid->rowHeight(r);
            for(const auto &value:item->data(EntriesRole).toList()){
                const auto entry=value.toMap();if(displayed.value(entry["index"].toInt()).identity()!=identity)continue;
                const int center=entry["top"].toInt()+entry["height"].toInt()/2;
                if(entry["endpoint"].toInt()==0)top=center;else bottom=center;
            }
        }
        item->setData(HighlightTopRole,top);item->setData(HighlightBottomRole,bottom);
    }
}
bool DeadlineCalendarWidget::eventFilter(QObject *object,QEvent *event){
    if(object==grid->horizontalHeader()->viewport()&&!monthView&&event->type()==QEvent::MouseButtonDblClick){auto mouse=static_cast<QMouseEvent*>(event);const int col=grid->horizontalHeader()->logicalIndexAt(mouse->position().toPoint());if(col>=0){const auto date=grid->horizontalHeaderItem(col)->data(Qt::UserRole).toDate();QList<int> ids;for(int i=0;i<displayed.size();++i)if(displayed[i].isLabel()&&displayed[i].due.toLocalTime().date()==date)ids.append(i);if(mouse->position().y()>=64&&!ids.isEmpty()){const int row=int(mouse->position().y()-66)/22;if(row>=0&&row<qMin(3,ids.size())&&!(row==2&&ids.size()>3))showItems({ids[row]},grid->horizontalHeader()->viewport()->mapToGlobal(mouse->position().toPoint()));else showItems(ids,grid->horizontalHeader()->viewport()->mapToGlobal(mouse->position().toPoint()));}else emit dateActivated(date);}return true;}
    if(object==grid->viewport()){
        if(event->type()==QEvent::ToolTip){auto help=static_cast<QHelpEvent*>(event);const auto index=grid->indexAt(help->pos());if(index.isValid()){const auto chips=chipRects(grid->visualRect(index),index);const auto titles=index.data(DetailsRole).toStringList();for(int i=0;i<chips.size();++i)if(chips[i].contains(help->pos())){QToolTip::showText(help->globalPos(),titles.value(i),grid->viewport(),chips[i]);return true;}}}
        if(event->type()==QEvent::Leave){hoverIdentity.clear();highlight(selectedIdentity);}
        if(event->type()==QEvent::MouseButtonDblClick){auto mouse=static_cast<QMouseEvent*>(event);if(mouse->button()==Qt::LeftButton){activateCell(grid->indexAt(mouse->position().toPoint()),mouse->position().toPoint());return true;}}
        if(event->type()==QEvent::MouseMove||event->type()==QEvent::MouseButtonPress){const auto mouse=static_cast<QMouseEvent*>(event);const auto index=grid->indexAt(mouse->position().toPoint());QString id;
            if(index.isValid()){const auto chips=chipRects(grid->visualRect(index),index);const auto entries=index.data(EntriesRole).toList();for(int i=0;i<chips.size();++i)if(chips[i].contains(mouse->position().toPoint())){id=displayed.value(entries[i].toMap()["index"].toInt()).identity();break;}}
            if(event->type()==QEvent::MouseButtonPress)selectedIdentity=id;hoverIdentity=id;highlight(id.isEmpty()?selectedIdentity:id);
        }
    }
    return QWidget::eventFilter(object,event);
}
void DeadlineCalendarWidget::refresh(){
    QDate first=monthView?QDate(anchor.year(),anchor.month(),1):anchor;first=first.addDays(1-first.dayOfWeek());const auto start=QDate(anchor.year(),anchor.month(),1);
    const int monthRows=qMax(5,(start.dayOfWeek()-1+start.daysInMonth()+6)/7);const auto last=first.addDays(monthView?monthRows*7-1:6);
    const auto all=dept->occurrences(first,last);QByteArray signature;QDataStream sig(&signature,QIODevice::WriteOnly);sig<<first<<last<<monthView<<QDateTime::currentDateTime().toString("yyyyMMddHHmm")<<filter.keywords<<filter.tags<<filter.calendarDDL<<filter.calendarIntervals<<filter.starred<<filter.limitDates<<filter.from<<filter.to;
    sig<<grid->font().toString();for(const auto &o:all)sig<<o.identity()<<o.title<<o.note<<o.tags<<o.start<<o.due<<o.done<<o.paused<<o.priority<<o.state();if(property("snapshot").toByteArray()==signature)return;setProperty("snapshot",signature);bool occupied[24]={false};
    for(const auto &o:all)if(o.due.isValid()&&!o.isLabel()){
        if(o.isInterval()){for(int hour=minutes(o.start)/60;hour<24&&hour*60<minutes(o.due);++hour)occupied[hour]=true;occupied[minutes(o.due)/60]=true;}
        else occupied[minutes(o.due)/60]=true;
    }
    // Compression is based on the complete week, before applying display filters.
    timeRows.clear();for(int hour=0;hour<24;){int end=hour+1;if(!occupied[hour])while(end<24&&!occupied[end])++end;if(end-hour>=2){timeRows.append({hour*60,end*60});hour=end;}else{timeRows.append({hour*60,(hour+1)*60});++hour;}}
    displayed.clear();QMap<QDate,QList<int>> byDate,labels;
    for(const auto &o:all){if(!o.due.isValid()||o.deleted)continue;const int id=displayed.size();displayed.append(o);if(o.isLabel())labels[o.due.toLocalTime().date()].append(id);else byDate[o.due.toLocalTime().date()].append(id);}
    const auto selectedCell=grid->currentItem();const bool hadSelection=selectedCell&&selectedCell->isSelected();
    const auto selectedDate=hadSelection?selectedCell->data(DateRole).toDate():QDate();const int selectedMinute=hadSelection?selectedCell->data(RowStartRole).toInt():-1;
    const int scroll=grid->verticalScrollBar()->value();grid->setUpdatesEnabled(false);grid->clear();const int rows=monthView?monthRows:timeRows.size();grid->setRowCount(rows);grid->horizontalHeader()->setSectionsClickable(!monthView);grid->verticalHeader()->setVisible(!monthView);grid->verticalHeader()->setMinimumSectionSize(28);grid->verticalHeader()->setSectionResizeMode(monthView?QHeaderView::Stretch:QHeaderView::Fixed);
    const QStringList weekdays{"周一","周二","周三","周四","周五","周六","周日"};int maxLabels=0;
    for(int col=0;col<7;++col){const auto date=first.addDays(col);auto head=new QTableWidgetItem(monthView?weekdays[col]:weekdays[col]+"\n"+QString::number(date.day()));head->setData(Qt::UserRole,date);QStringList texts;for(int id:labels[date])texts.append(displayed[id].title);head->setData(LabelsRole,texts);head->setToolTip(date.toString("yyyy-MM-dd")+"\n"+texts.join('\n'));grid->setHorizontalHeaderItem(col,head);maxLabels=qMax(maxLabels,qMin(3,texts.size()));}
    grid->horizontalHeader()->setFixedHeight(monthView?40:66+22*maxLabels);
    const WeekLayout::Metrics metrics{qMax(22,QFontMetrics(grid->font()).height()+6),3,7};
    for(int row=0;row<rows;++row){int maxEntries=0;const int low=monthView?0:timeRows[row].first,high=monthView?1440:timeRows[row].second;
        if(!monthView){const auto time=[](int minute){return QString("%1:00").arg(minute/60,2,10,QChar('0'));};grid->setVerticalHeaderItem(row,new QTableWidgetItem(high-low>60?time(low)+"\n— "+time(high):time(low)));}
        struct Node{int id,time,endpoint;QString caption;};QList<Node> columns[7];
        for(int col=0;col<7;++col){const auto date=first.addDays(monthView?row*7+col:col);auto &nodes=columns[col];
            for(int id:byDate[date]){const auto &o=displayed[id];const auto prefix=(o.priority?QString("★ "):QString())+(o.done?QString("✓ "):QString())+o.title+(o.paused?"（暂停）":"");
                if(monthView){if(CalendarRules::matches(o,filter))nodes.append({id,minutes(o.due),0,prefix});}
                else if(o.isInterval()){
                    if(minutes(o.start)>=low&&minutes(o.start)<high)nodes.append({id,minutes(o.start),0,o.start.toLocalTime().toString("HH:mm")+" 开始 · "+prefix+" · "+o.state()});
                    if(minutes(o.due)>=low&&minutes(o.due)<high)nodes.append({id,minutes(o.due),1,o.due.toLocalTime().toString("HH:mm")+" 结束 · "+prefix+" · "+o.state()});
                }else if(minutes(o.due)>=low&&minutes(o.due)<high)nodes.append({id,minutes(o.due),0,o.due.toLocalTime().toString("HH:mm")+" · "+prefix});
            }
            std::sort(nodes.begin(),nodes.end(),[this](const auto &a,const auto &b){if(a.time!=b.time)return a.time<b.time;const auto aid=displayed[a.id].identity(),bid=displayed[b.id].identity();return aid==bid?a.endpoint<b.endpoint:aid<bid;});maxEntries=qMax(maxEntries,int(nodes.size()));
        }
        const int height=WeekLayout::rowHeight(maxEntries,high-low>60,metrics);if(!monthView)grid->setRowHeight(row,height);
        for(int col=0;col<7;++col){const auto date=first.addDays(monthView?row*7+col:col);const auto &nodes=columns[col];QVector<int> nodeMinutes;for(const auto &node:nodes)nodeMinutes.append(node.time);const auto tops=monthView?QVector<int>():WeekLayout::textTops(int(nodes.size()),metrics);
            QStringList titles,details,labelTexts;QVariantList entries;for(int i=0;i<nodes.size();++i){const auto &node=nodes[i];if(!CalendarRules::matches(displayed[node.id],filter))continue;titles.append(displayed[node.id].title);details.append(node.caption);QVariantMap entry{{"index",node.id},{"minute",node.time},{"endpoint",node.endpoint}};if(!monthView){entry["top"]=tops[i];entry["height"]=metrics.lineHeight;}entries.append(entry);}for(int id:labels[date])labelTexts.append(displayed[id].title);
            auto cell=new QTableWidgetItem;cell->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);cell->setData(DateRole,date);cell->setData(TitlesRole,titles);cell->setData(DetailsRole,details);cell->setData(CountRole,entries.size());cell->setData(MonthRole,monthView);cell->setData(EntriesRole,entries);cell->setData(LabelsRole,labelTexts);cell->setData(RowStartRole,low);cell->setData(RowEndRole,high);cell->setText(date.toString("yyyy-MM-dd")+"\n"+details.join('\n'));
            const auto now=QTime::currentTime();const int currentMinute=now.hour()*60+now.minute();if(!monthView&&date==QDate::currentDate()&&currentMinute>=low&&currentMinute<high)cell->setData(NowLineRole,WeekLayout::insertionY(nodeMinutes,currentMinute,metrics));
            const bool outside=monthView&&date.month()!=anchor.month();cell->setBackground(UiPalette::color(date==QDate::currentDate()?"#191b23":outside?"#0e1014":"#14171d"));cell->setForeground(UiPalette::color(outside?"#666d78":"#d0d0d0"));cell->setToolTip(date.toString("yyyy-MM-dd")+"\n"+labelTexts.join('\n')+"\n"+details.join('\n')+"\n双击事务查看详情；双击空白查看"+(monthView?"该周":"当天日程"));grid->setItem(row,col,cell);if(date==selectedDate&&(monthView||(selectedMinute>=low&&selectedMinute<high)))grid->setCurrentCell(row,col,QItemSelectionModel::ClearAndSelect);
        }
    }
    grid->setUpdatesEnabled(true);grid->horizontalHeader()->viewport()->update();highlight(selectedIdentity);
    if(!monthView&&scrollPending){const int minute=QTime::currentTime().hour()*60+QTime::currentTime().minute();int target=0;for(int r=0;r<timeRows.size();++r)if(minute>=timeRows[r].first&&minute<timeRows[r].second)target=r;QTimer::singleShot(0,this,[this,target]{if(!monthView&&target<grid->rowCount())grid->scrollToItem(grid->item(qMax(0,target-1),0),QAbstractItemView::PositionAtTop);});}else grid->verticalScrollBar()->setValue(scroll);scrollPending=false;
}
