#include "deadlinecalendarwidget.h"
#include "ddldepartment.h"
#include <QtWidgets>

namespace {
class CalendarHeader:public QHeaderView {
public:
    explicit CalendarHeader(QWidget *parent):QHeaderView(Qt::Horizontal,parent){setMinimumHeight(64);setDefaultAlignment(Qt::AlignCenter);}
protected:
    void paintSection(QPainter *p,const QRect &rect,int section) const override {
        p->save();p->fillRect(rect,QColor("#101216"));p->setPen(QColor("#81868e"));
        const auto lines=model()->headerData(section,orientation()).toString().split('\n');
        if(lines.size()<2){p->drawText(rect,Qt::AlignCenter,lines.value(0));}
        else{
            auto top=rect.adjusted(0,5,0,-34);p->drawText(top,Qt::AlignCenter,lines[0]);
            const auto date=model()->headerData(section,orientation(),Qt::UserRole).toDate();
            const QPointF center(rect.center().x(),rect.bottom()-20);
            if(date==QDate::currentDate()){p->setPen(Qt::NoPen);p->setBrush(QColor("#7f2636"));p->drawEllipse(center,15,15);}
            p->setPen(QColor("#d0d0d0"));QFont font=p->font();font.setPixelSize(17);font.setWeight(QFont::DemiBold);p->setFont(font);
            p->drawText(QRectF(center.x()-20,center.y()-15,40,30),Qt::AlignCenter,QString::number(date.day()));
        }
        p->setPen(QColor("#343841"));p->drawLine(rect.bottomLeft(),rect.bottomRight());p->restore();
    }
};
class CalendarDelegate:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        p->save();p->setRenderHint(QPainter::Antialiasing);p->setClipRect(option.rect);
        const auto date=index.data(DeadlineCalendarWidget::DateRole).toDate();
        const bool month=index.data(DeadlineCalendarWidget::MonthRole).toBool(),today=date==QDate::currentDate();
        p->fillRect(option.rect,index.data(Qt::BackgroundRole).value<QBrush>());
        p->setPen(QColor("#292d35"));p->drawRect(option.rect.adjusted(0,0,-1,-1));
        auto rect=option.rect.adjusted(8,7,-8,-7);p->setFont(option.font);
        if(month){
            if(today){p->setPen(Qt::NoPen);p->setBrush(QColor("#7f2636"));p->drawEllipse(QRectF(rect.left(),rect.top(),25,25));}
            p->setPen(today?QColor("#ffffff"):index.data(Qt::ForegroundRole).value<QBrush>().color());
            p->drawText(QRect(rect.left(),rect.top(),25,25),Qt::AlignCenter,QString::number(date.day()));
            if(index.data(DeadlineCalendarWidget::CountRole).toInt()>0){p->setPen(QColor("#81868e"));p->drawText(QRect(rect.left()+30,rect.top(),rect.width()-30,25),Qt::AlignRight|Qt::AlignVCenter,QString("%1 项").arg(index.data(DeadlineCalendarWidget::CountRole).toInt()));}
            rect.setTop(rect.top()+28);
        }
        const auto titles=index.data(DeadlineCalendarWidget::DetailsRole).toStringList();
        const QFontMetrics metrics(option.font);
        const int chipHeight=month?20:44,gap=month?3:4,step=chipHeight+gap,capacity=qMax(0,(rect.height()+gap)/step);
        int shown=qMin(titles.size(),capacity);
        if(shown<titles.size()&&rect.height()-shown*step<metrics.height()+2)shown=qMax(0,shown-1);
        for(int i=0;i<shown;++i){
            const QRect chip(rect.left(),rect.top()+i*step,rect.width(),chipHeight);
            p->setPen(Qt::NoPen);p->setBrush(QColor(month?"#20242c":"#242832"));p->drawRoundedRect(chip,4,4);
            p->setPen(index.data(Qt::ForegroundRole).value<QBrush>().color());
            if(month)p->drawText(chip.adjusted(6,0,-6,0),Qt::AlignVCenter|Qt::AlignLeft,metrics.elidedText(titles[i],Qt::ElideRight,qMax(0,chip.width()-12)));
            else{
                const auto lines=titles[i].split('\n');
                p->drawText(chip.adjusted(6,3,-6,-21),Qt::AlignVCenter|Qt::AlignLeft,metrics.elidedText(lines.value(0),Qt::ElideRight,qMax(0,chip.width()-12)));
                auto small=option.font;small.setPixelSize(12);p->setFont(small);p->setPen(QColor("#a1a6af"));
                p->drawText(chip.adjusted(6,23,-6,-3),Qt::AlignVCenter|Qt::AlignLeft,QFontMetrics(small).elidedText(lines.value(1),Qt::ElideRight,qMax(0,chip.width()-12)));p->setFont(option.font);
            }
        }
        if(shown<titles.size()&&rect.height()>=metrics.height()){
            p->setPen(QColor("#a1a6af"));p->drawText(QRect(rect.left()+4,rect.top()+shown*step,rect.width()-4,metrics.height()+2),Qt::AlignVCenter|Qt::AlignLeft,QString("还有 %1 项").arg(titles.size()-shown));
        }
        if(!month&&today&&index.row()==QTime::currentTime().hour()){
            const int y=option.rect.top()+qRound(option.rect.height()*QTime::currentTime().minute()/60.0);
            p->setPen(QPen(QColor("#7f2636"),2));p->drawLine(option.rect.left(),y,option.rect.right(),y);
        }
        if(option.state&(QStyle::State_MouseOver|QStyle::State_Selected)){p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor("#7f2636"),(option.state&QStyle::State_Selected)?2:1));p->drawRect(option.rect.adjusted(1,1,-2,-2));}
        if((option.state&QStyle::State_HasFocus)&&!(option.state&QStyle::State_Selected)){p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor("#81868e"),1,Qt::DashLine));p->drawRect(option.rect.adjusted(3,3,-4,-4));}
        p->restore();
    }
};
}
DeadlineCalendarWidget::DeadlineCalendarWidget(DDLDepartment *department,QWidget *parent):QWidget(parent),dept(department){
    setObjectName("ddlCalendar");auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    grid=new QTableWidget;grid->setObjectName("ddlCalendarGrid");grid->setColumnCount(7);grid->setItemDelegate(new CalendarDelegate(grid));
    grid->setHorizontalHeader(new CalendarHeader(grid));grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);grid->horizontalHeader()->setSectionsClickable(true);grid->horizontalHeader()->setHighlightSections(false);
    grid->setSelectionMode(QAbstractItemView::SingleSelection);grid->setSelectionBehavior(QAbstractItemView::SelectItems);grid->setEditTriggers(QAbstractItemView::NoEditTriggers);grid->setFocusPolicy(Qt::StrongFocus);
    grid->setShowGrid(false);grid->setMouseTracking(true);grid->setCursor(Qt::PointingHandCursor);
    grid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);grid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    grid->setStyleSheet("QTableWidget {border:1px solid #343841;border-radius:6px;background:#101216;} QHeaderView::section {background:#101216;color:#81868e;border:0;padding:6px;font-size:12px;}");
    layout->addWidget(grid,1);
    connect(grid,&QTableWidget::cellDoubleClicked,this,[this](int row,int column){emit dateActivated(grid->item(row,column)->data(DateRole).toDate());});
    connect(grid->horizontalHeader(),&QHeaderView::sectionClicked,this,[this](int column){if(!monthView)grid->setCurrentCell(qMax(0,grid->currentRow()>=0?grid->currentRow():grid->rowAt(0)),column,QItemSelectionModel::ClearAndSelect);});
    connect(grid->horizontalHeader(),&QHeaderView::sectionDoubleClicked,this,[this](int column){if(!monthView)emit dateActivated(grid->item(0,column)->data(DateRole).toDate());});
    auto open=new QShortcut(QKeySequence(Qt::Key_Return),grid);open->setContext(Qt::WidgetWithChildrenShortcut);
    connect(open,&QShortcut::activated,this,[this]{if(auto cell=grid->currentItem())emit dateActivated(cell->data(DateRole).toDate());});
    refresh();
}
void DeadlineCalendarWidget::setDate(const QDate &date){if(date.isValid()&&anchor!=date){grid->clearSelection();anchor=date;scrollPending=true;refresh();}}
void DeadlineCalendarWidget::setMonthView(bool month){if(monthView!=month){grid->clearSelection();monthView=month;scrollPending=true;refresh();}}
void DeadlineCalendarWidget::setFilter(const QString &text,const QString &selectedTag,int star){keyword=text.trimmed();tag=selectedTag;starred=star;refresh();}
void DeadlineCalendarWidget::refresh(){
    QDate first=monthView?QDate(anchor.year(),anchor.month(),1):anchor;first=first.addDays(1-first.dayOfWeek());
    const auto start=QDate(anchor.year(),anchor.month(),1);
    const int rows=monthView?qMax(5,(start.dayOfWeek()-1+start.daysInMonth()+6)/7):24;
    const auto last=first.addDays(monthView?rows*7-1:6);QMap<QDate,QList<TaskOccurrence>> byDate;
    for(const auto &o:dept->occurrences(first,last)){
        if(o.done||o.deleted||!o.due.isValid())continue;
        const auto date=o.due.toLocalTime().date();if(o.repeat!="none"&&date<QDate::currentDate())continue;
        if(!(o.title+" "+o.note+" "+o.tags).contains(keyword,Qt::CaseInsensitive))continue;
        if(!tag.isEmpty()&&!o.tags.split(',',Qt::SkipEmptyParts).contains(tag))continue;
        if(starred>=0&&((o.priority>0)!=(starred==1)))continue;
        byDate[date].append(o);
    }
    const auto selected=grid->selectedItems();const auto selectedDate=selected.isEmpty()?QDate():selected.first()->data(DateRole).toDate();const int selectedRow=selected.isEmpty()?-1:selected.first()->row();
    const int scroll=grid->verticalScrollBar()->value();grid->setUpdatesEnabled(false);grid->clear();grid->setRowCount(rows);grid->horizontalHeader()->setSectionsClickable(!monthView);
    grid->verticalHeader()->setVisible(!monthView);grid->verticalHeader()->setMinimumSectionSize(monthView?78:80);
    grid->verticalHeader()->setSectionResizeMode(monthView?QHeaderView::Stretch:QHeaderView::Fixed);
    if(!monthView)for(int r=0;r<rows;++r)grid->setRowHeight(r,112);
    const QStringList weekdays{"周一","周二","周三","周四","周五","周六","周日"};
    for(int col=0;col<7;++col){
        const auto date=first.addDays(col);auto heading=new QTableWidgetItem(monthView?weekdays[col]:weekdays[col]+"\n"+QString::number(date.day()));
        heading->setData(Qt::UserRole,date);heading->setToolTip(date.toString("yyyy-MM-dd")+QString("，%1 项未完成事务").arg(byDate[date].size()));grid->setHorizontalHeaderItem(col,heading);
    }
    for(int row=0;row<rows;++row){
        if(!monthView)grid->setVerticalHeaderItem(row,new QTableWidgetItem(QString("%1:00").arg(row,2,10,QChar('0'))));
        for(int col=0;col<7;++col){
            const auto date=first.addDays(monthView?row*7+col:col);QStringList titles,details,tooltip;
            for(const auto &o:byDate[date])if(monthView||o.due.toLocalTime().time().hour()==row){
                titles.append(o.title);const auto time=o.due.toLocalTime().toString("HH:mm");
                details.append((o.priority?QString("★ "):QString())+o.title+(o.paused?"（暂停）":"")+(monthView?QString():"\n"+time+" 截止"));
                tooltip.append(time+" 截止  "+o.title);
            }
            auto cell=new QTableWidgetItem;cell->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);cell->setData(DateRole,date);cell->setData(TitlesRole,titles);cell->setData(DetailsRole,details);cell->setData(CountRole,titles.size());cell->setData(MonthRole,monthView);
            cell->setText(date.toString("yyyy-MM-dd")+"\n"+details.join('\n'));
            const bool outside=monthView&&date.month()!=anchor.month();
            cell->setBackground(QColor(date==QDate::currentDate()?"#191b23":outside?"#0e1014":"#14171d"));cell->setForeground(QColor(outside?"#666d78":"#d0d0d0"));
            cell->setToolTip(date.toString("yyyy-MM-dd")+(titles.isEmpty()?"\n没有匹配的事务":"\n"+tooltip.join('\n'))+"\n单击选中，双击查看"+(monthView?"该周":"当天日程"));grid->setItem(row,col,cell);
            if(date==selectedDate&&(monthView||row==selectedRow))grid->setCurrentCell(row,col,QItemSelectionModel::ClearAndSelect);
        }
    }
    grid->setUpdatesEnabled(true);grid->horizontalHeader()->viewport()->update();
    if(!monthView&&scrollPending){
        QTimer::singleShot(0,this,[this]{if(!monthView)grid->scrollToItem(grid->item(qMax(0,QTime::currentTime().hour()-1),0),QAbstractItemView::PositionAtTop);});
    }else grid->verticalScrollBar()->setValue(scroll);
    scrollPending=false;
}
