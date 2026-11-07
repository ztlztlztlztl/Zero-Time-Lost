#include "uipalette.h"
#include "mailsimpledisplaywidget.h"
#include "mailmanager.h"
#include "configmanager.h"
#include "mailstyle.h"

namespace {
enum { SenderRole=Qt::UserRole+1,AccountRole,UnreadRole,StarRole,TimestampRole,TrashRole };
class MailDelegate:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        p->save();p->setClipRect(option.rect);
        p->fillRect(option.rect,index.row()%2?UiPalette::color("#14171b"):UiPalette::color("#101216"));
        const bool selected=option.state&QStyle::State_Selected,hover=option.state&QStyle::State_MouseOver;
        if(selected)p->fillRect(option.rect,UiPalette::tint(UiPalette::Mail));
        p->setPen(UiPalette::color(selected||hover?"#754218":"#292d35"));
        if(selected||hover){
            p->drawLine(option.rect.topLeft(),option.rect.topRight());p->drawLine(option.rect.bottomLeft(),option.rect.bottomRight());
            if(index.column()==0)p->drawLine(option.rect.topLeft(),option.rect.bottomLeft());
            if(index.column()==2)p->drawLine(option.rect.topRight(),option.rect.bottomRight());
        }else p->drawLine(option.rect.bottomLeft(),option.rect.bottomRight());
        const auto first=index.siblingAtColumn(0);const bool trash=first.data(TrashRole).toBool();
        if(index.column()==0){
            const bool unread=first.data(UnreadRole).toBool();auto font=option.font;font.setBold(unread);p->setFont(font);
            auto area=option.rect.adjusted(24,12,-12,-12);
            if(unread){p->setPen(Qt::NoPen);p->setBrush(MailStyle::accent);p->drawEllipse(QPointF(option.rect.left()+11,option.rect.top()+23),3,3);}
            const auto title=(first.data(StarRole).toBool()?QStringLiteral("★ "):QString())+index.data().toString();
            p->setPen(unread?UiPalette::color("#eeeeee"):UiPalette::color("#cbd0d8"));
            if(trash)p->drawText(area,Qt::AlignVCenter|Qt::AlignLeft,QFontMetrics(font).elidedText(title,Qt::ElideRight,area.width()));
            else{
                p->drawText(QRect(area.left(),area.top(),area.width(),23),Qt::AlignVCenter|Qt::AlignLeft,QFontMetrics(font).elidedText(title,Qt::ElideRight,area.width()));
                font.setBold(false);font.setPixelSize(12);p->setFont(font);p->setPen(UiPalette::color("#9298a2"));
                const auto detail=first.data(SenderRole).toString()+QStringLiteral("    收件账号：")+first.data(AccountRole).toString();
                p->drawText(QRect(area.left(),area.top()+27,area.width(),20),Qt::AlignVCenter|Qt::AlignLeft,QFontMetrics(font).elidedText(detail,Qt::ElideRight,area.width()));
            }
        }else if(index.column()==1&&!trash){
            const auto when=QDateTime::fromSecsSinceEpoch(first.data(TimestampRole).toLongLong());
            const auto date=when.date();const auto today=QDate::currentDate();
            const auto day=date==today?QStringLiteral("今天"):date==today.addDays(-1)?QStringLiteral("昨天"):date.toString("yyyy-MM-dd");
            p->setFont(option.font);p->setPen(UiPalette::color("#cbd0d8"));p->drawText(option.rect.adjusted(8,12,-8,-39),Qt::AlignVCenter|Qt::AlignLeft,day);
            auto small=option.font;small.setPixelSize(12);p->setFont(small);p->setPen(UiPalette::color("#9298a2"));p->drawText(option.rect.adjusted(8,39,-8,-12),Qt::AlignVCenter|Qt::AlignLeft,when.toString("HH:mm"));
        }
        p->restore();
    }
};
}
MailSimpleDisplayWidget::MailSimpleDisplayWidget(QWidget *parent):QWidget(parent){
    setObjectName("mailListPage");
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(14);
    auto top=new QHBoxLayout;tabs=new QTabBar;tabs->setObjectName("mailTabs");for(const auto &label:{"全部","未读","收藏","已删除"})tabs->addTab(label);
    top->addWidget(tabs);top->addStretch();layout->addLayout(top);
    auto filters=new QHBoxLayout;filters->setSpacing(14);
    filterButton=new MailStyle::Button("启用筛选");filterButton->setObjectName("mailFilter");filters->addWidget(filterButton);filters->addStretch();layout->addLayout(filters);
    rows=new QTreeWidget;rows->setObjectName("mailRows");rows->setRootIsDecorated(false);rows->setAlternatingRowColors(true);rows->setMouseTracking(true);
    rows->setItemDelegate(new MailDelegate(rows));rows->setHeaderLabels({"邮件","时间","操作"});rows->setSelectionMode(QAbstractItemView::SingleSelection);
    rows->setSelectionBehavior(QAbstractItemView::SelectRows);rows->setEditTriggers(QAbstractItemView::NoEditTriggers);rows->setUniformRowHeights(true);
    rows->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);rows->header()->setStretchLastSection(false);
    rows->header()->setSectionResizeMode(0,QHeaderView::Stretch);rows->header()->setSectionResizeMode(1,QHeaderView::Fixed);rows->header()->setSectionResizeMode(2,QHeaderView::Fixed);
    rows->setColumnWidth(1,118);rows->setColumnWidth(2,232);layout->addWidget(rows,1);
    empty=new QLabel;empty->setObjectName("mailEmpty");empty->setAlignment(Qt::AlignCenter);empty->setWordWrap(true);layout->addWidget(empty,1);empty->hide();
    auto actions=new QHBoxLayout;refresh=new MailStyle::Button("收取新邮件");refresh->setObjectName("mailRefresh");
    more=new MailStyle::Button("显示更多");auto history=new MailStyle::Button("收取更早邮件");history->setObjectName("mailHistory");more->setObjectName("mailMore");
    actions->addWidget(refresh);actions->addWidget(history);actions->addStretch();actions->addWidget(more);layout->addLayout(actions);
    loaded=qBound(1,ConfigManager::getInstance()->getMailBasical().initialNumber,1000);
    connect(MailAccountManager::getInstance(),&MailAccountManager::mailAccountChanged,this,&MailSimpleDisplayWidget::reload);
    connect(filterButton,&QPushButton::clicked,this,&MailSimpleDisplayWidget::toggleFilter);
    connect(MailManager::getInstance(),&MailManager::changed,this,&MailSimpleDisplayWidget::reload);
    connect(tabs,&QTabBar::currentChanged,this,&MailSimpleDisplayWidget::reload);
    connect(refresh,&QPushButton::clicked,this,[this]{refresh->setEnabled(false);emit refreshRequest();});
    connect(history,&QPushButton::clicked,this,&MailSimpleDisplayWidget::historyRequest);
    connect(more,&QPushButton::clicked,this,[this]{loaded+=ConfigManager::getInstance()->getMailBasical().earlierNumber;reload();});
    auto open=[this](QTreeWidgetItem *row,int){
        if(!row||tabs->currentIndex()==3)return;
        const int i=rows->indexOfTopLevelItem(row);if(i<0||i>=visible.size())return;
        const auto mail=visible[i];const auto owner=MailAccountManager::getInstance()->getMailAcc(mail.ownerEmail);
        emit readRequest(owner,mail);MailManager::getInstance()->checkMail(mail.ownerEmail,mail.uid);
    };
    connect(rows,&QTreeWidget::itemDoubleClicked,this,open);
    for(const auto key:{Qt::Key_Return,Qt::Key_Enter}){auto enter=new QShortcut(QKeySequence(key),rows);enter->setContext(Qt::WidgetShortcut);connect(enter,&QShortcut::activated,this,[this,open]{open(rows->currentItem(),0);});}
    auto find=new QShortcut(QKeySequence::Find,this);find->setContext(Qt::WidgetWithChildrenShortcut);connect(find,&QShortcut::activated,this,&MailSimpleDisplayWidget::openFilter);
    reload();
}
void MailSimpleDisplayWidget::enablePollingBtn(){refresh->setEnabled(true);}
void MailSimpleDisplayWidget::showUnread(){
    {QSignalBlocker c(tabs);filter={};tabs->setCurrentIndex(1);}
    rows->clearSelection();rows->setCurrentItem(nullptr);reload();rows->setFocus();
}
void MailSimpleDisplayWidget::reload(){
    QString selection;if(rows->currentItem())selection=rows->currentItem()->data(0,Qt::UserRole).toString();
    const int scroll=rows->verticalScrollBar()->value();
    QList<MailBrief> candidate;
    const bool trash=tabs->currentIndex()==3;rows->setColumnHidden(1,trash);rows->setColumnWidth(2,trash?124:232);
    filterButton->setText(filter.active()?"关闭筛选":"启用筛选");
    // Keep SQL batches bounded while allowing the view to pass the old 1000-row ceiling.
    while(candidate.size()<loaded){
        const int count=qMin(1000,loaded-int(candidate.size()));
        const auto batch=MailManager::getInstance()->queryFiltered(filter,tabs->currentIndex(),count,int(candidate.size()));
        if(!MailManager::getInstance()->querySucceeded()){empty->setText("邮件查询失败，已保留上次列表："+MailManager::getInstance()->error());empty->show();return;}
        candidate.append(batch);if(batch.size()<count)break;
    }
    const bool hasMore=!MailManager::getInstance()->queryFiltered(filter,tabs->currentIndex(),1,int(candidate.size())).isEmpty();
    if(!MailManager::getInstance()->querySucceeded()){empty->setText("邮件查询失败，已保留上次列表");empty->show();return;}
    visible=candidate;rows->clear();
    more->setEnabled(hasMore);more->setText(hasMore?"显示更多":"已显示全部");
    empty->setText(trash?"没有匹配的已删除邮件\n删除的邮件可在这里联网恢复。":"没有匹配的邮件\n可调整筛选条件，或点击下方收取新邮件。");
    rows->setVisible(!visible.isEmpty());empty->setVisible(visible.isEmpty());
    for(const auto &mail:visible){
        auto row=new QTreeWidgetItem(rows);row->setText(0,mail.subject.isEmpty()?"（无主题）":mail.subject);row->setSizeHint(0,QSize(0,76));
        const auto accountName=MailAccountManager::getInstance()->getAddressName(mail.ownerEmail);
        row->setData(0,SenderRole,mail.sender);row->setData(0,AccountRole,accountName);row->setData(0,UnreadRole,!trash&&!mail.isRead);row->setData(0,StarRole,!trash&&mail.starred);row->setData(0,TimestampRole,mail.timestamp);row->setData(0,TrashRole,trash);
        const auto detail=trash?mail.subject:mail.subject+"\n发件人："+mail.sender+"\n收件账号："+accountName+" <"+mail.ownerEmail+">\n"+mail.dateText();
        for(int c=0;c<3;++c){row->setToolTip(c,detail);row->setData(c,Qt::AccessibleTextRole,detail);}
        const auto id=mail.ownerEmail+"/"+mail.uid;row->setData(0,Qt::UserRole,id);
        auto actions=new QWidget;actions->setObjectName("mailRowActions");auto buttons=new QHBoxLayout(actions);buttons->setContentsMargins(8,8,8,8);buttons->setSpacing(8);buttons->addStretch();
        if(trash){
            auto restore=new MailStyle::Button(pendingRestores.contains(id)?"恢复中…":"恢复");restore->setObjectName("mailRestore");restore->setEnabled(!pendingRestores.contains(id));buttons->addWidget(restore);
            connect(restore,&QPushButton::clicked,this,[this,mail,id,restore]{pendingRestores.insert(id);restore->setEnabled(false);restore->setText("恢复中…");emit restoreRequest(mail.ownerEmail,mail.uid);});
        }else{
            auto check=new MailStyle::Button(mail.isRead?"已读":"标为已读");check->setObjectName("mailCheck");check->setEnabled(!mail.isRead);buttons->addWidget(check);
            auto remove=new MailStyle::Button("删除");remove->setObjectName("mailDelete");remove->setProperty("danger",true);buttons->addWidget(remove);
            connect(check,&QPushButton::clicked,this,[mail]{MailManager::getInstance()->checkMail(mail.ownerEmail,mail.uid);});
            connect(remove,&QPushButton::clicked,this,[mail]{MailManager::getInstance()->deleteMail(mail.ownerEmail,mail.uid);});
        }
        rows->setItemWidget(row,2,actions);
        if(id==selection)rows->setCurrentItem(row);
    }
    rows->verticalScrollBar()->setValue(scroll);
}
void MailSimpleDisplayWidget::restoreFinished(const QString &address,const QString &id,bool){pendingRestores.remove(address+"/"+id);reload();}

void MailSimpleDisplayWidget::toggleFilter(){if(filter.active()){filter={};loaded=qBound(1,ConfigManager::getInstance()->getMailBasical().initialNumber,1000);reload();}else openFilter();}
void MailSimpleDisplayWidget::openFilter(){
    QStringList addresses;for(const auto &account:MailAccountManager::getInstance()->getMailAccounts())addresses.append(account.address);
    AdvancedFilter next;if(AdvancedFilterDialog::open(AdvancedFilterDialog::Mail,filterButton,next,addresses)){filter=next;loaded=qBound(1,ConfigManager::getInstance()->getMailBasical().initialNumber,1000);reload();}
}
