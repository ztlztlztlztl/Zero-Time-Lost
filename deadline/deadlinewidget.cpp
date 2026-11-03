#include "pageheader.h"
#include "deadlineeditor.h"
#include "deadlinewidget.h"
#include "deadlinecalendarwidget.h"
#include "deadlineagendawidget.h"
#include "configmanager.h"
#include "uistyle.h"
#include "dialoglayout.h"
#include <QtWidgets>
#include <QTimeZone>
#include <algorithm>
#include "popuppolicy.h"
#include "draftstore.h"
#include "recurrence.h"

namespace {
class PeriodLabel:public QLabel {
public:
    void setPeriod(const QString &value){full=value;setToolTip(value);updateText();}
protected:
    void resizeEvent(QResizeEvent *event) override {QLabel::resizeEvent(event);updateText();}
private:
    void updateText(){setText(fontMetrics().elidedText(full,Qt::ElideRight,width()));}
    QString full;
};
}
DeadlineWidget::DeadlineWidget(DDLDepartment *department,QWidget *parent):QWidget(parent),dept(department){
    setAttribute(Qt::WA_StyledBackground,true);setObjectName("deadlinePage");
    setStyleSheet(UiStyle::sheet+QString(R"qss(
QWidget#deadlinePage QTabBar::tab {padding:9px 18px;border:1px solid transparent;color:#81868e;}
QWidget#deadlinePage QTabBar::tab:selected {color:#d0d0d0;border:1px solid #7f2636;border-radius:4px;background:#1c1921;}
QWidget#deadlinePage QTabBar::tab:hover {background:#1b1c23;}
QWidget#deadlinePage QPushButton[role="primary"] {border-left:2px solid #7f2636;color:#d0d0d0;}
QWidget#deadlinePage QLineEdit:focus,QWidget#deadlinePage QComboBox:focus {border:1px solid #7f2636;}
QWidget#deadlinePage QLabel[role="muted"] {font-size:12px;color:#81868e;}
)qss"));
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(28,22,28,20);layout->setSpacing(18);
    addPageHeader(layout,"Calendar");
    auto bar=new QHBoxLayout;bar->setSpacing(10);
    heading=new PeriodLabel;heading->setObjectName("ddlPeriod");heading->setStyleSheet("font-size:28px;font-weight:600;color:#d0d0d0;");
    heading->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);heading->setMinimumWidth(120);
    tabs=new QTabBar;tabs->setObjectName("ddlTabs");tabs->setExpanding(false);tabs->addTab("日程");tabs->addTab("周");tabs->addTab("月");
    auto previous=new QPushButton("‹");previous->setObjectName("ddlCalendarPrevious");previous->setAccessibleName("上一个日期范围");previous->setFixedWidth(38);
    auto today=new QPushButton("今天");today->setObjectName("ddlCalendarToday");
    auto next=new QPushButton("›");next->setObjectName("ddlCalendarNext");next->setAccessibleName("下一个日期范围");next->setFixedWidth(38);
    bar->addWidget(heading,1);bar->addWidget(tabs);bar->addSpacing(6);bar->addWidget(previous);bar->addWidget(today);bar->addWidget(next);layout->addLayout(bar);
    auto filters=new QHBoxLayout;filters->setSpacing(10);
    filterButton=new QPushButton("启用筛选");filterButton->setObjectName("ddlFilter");
    showCompleted=new QCheckBox("显示已完成");showCompleted->setObjectName("ddlShowCompleted");showCompleted->setChecked(QSettings().value("ddl/showCompleted",false).toBool());
    showCompleted->setStyleSheet("QCheckBox::indicator {width:14px;height:14px;border:1px solid #646a73;border-radius:2px;background:#101216;} QCheckBox::indicator:checked {border-color:#7f2636;background:#7f2636;}");
    auto add=new QPushButton("＋新增");add->setObjectName("ddlAdd");add->setProperty("role","primary");
    filters->addWidget(filterButton);filters->addWidget(showCompleted);filters->addStretch();filters->addWidget(add);layout->addLayout(filters);
    pages=new QStackedWidget;pages->setObjectName("ddlPages");agenda=new DeadlineAgendaWidget;calendar=new DeadlineCalendarWidget(dept);pages->addWidget(agenda);pages->addWidget(calendar);layout->addWidget(pages,1);
    connect(add,&QPushButton::clicked,this,[this]{edit();});connect(tabs,&QTabBar::currentChanged,this,&DeadlineWidget::changeView);
    connect(previous,&QPushButton::clicked,this,[this]{navigate(-1);});connect(next,&QPushButton::clicked,this,[this]{navigate(1);});
    connect(today,&QPushButton::clicked,this,[this]{anchor=QDate::currentDate();focusedDayInstances=false;pendingFocus=tabs->currentIndex()==0;calendar->setDate(anchor);refresh();});
    connect(calendar,&DeadlineCalendarWidget::dateActivated,this,[this](const QDate &date){
        anchor=date;if(tabs->currentIndex()==2)tabs->setCurrentIndex(1);
        else{focusedDayInstances=true;pendingFocus=true;tabs->setCurrentIndex(0);}
    });
    connect(agenda,&DeadlineAgendaWidget::detailRequested,this,&DeadlineWidget::showDetail);
    connect(agenda,&DeadlineAgendaWidget::completeRequested,this,[this](const TaskOccurrence &o){if(!dept->complete(o)){QMessageBox::warning(this,"操作失败",dept->error());refresh();}});
    connect(ConfigManager::getInstance(),&ConfigManager::deadlineChanged,this,&DeadlineWidget::refresh);
    connect(showCompleted,&QCheckBox::toggled,this,[this](bool checked){QSettings().setValue("ddl/showCompleted",checked);refresh();});
    connect(dept,&DDLDepartment::changed,this,&DeadlineWidget::refresh);
    connect(filterButton,&QPushButton::clicked,this,&DeadlineWidget::toggleFilter);
    auto create=new QShortcut(QKeySequence("Ctrl+N"),this);create->setContext(Qt::WidgetWithChildrenShortcut);connect(create,&QShortcut::activated,this,[this]{edit();});
    auto find=new QShortcut(QKeySequence::Find,this);find->setContext(Qt::WidgetWithChildrenShortcut);connect(find,&QShortcut::activated,this,[this]{AdvancedFilter next;if(AdvancedFilterDialog::open(AdvancedFilterDialog::Calendar,filterButton,next)){filter=next;refresh();}});
    refresh();
}
void DeadlineWidget::changeView(int index){
    pages->setCurrentIndex(index==0?0:1);showCompleted->setVisible(index==0);
    if(index>0){focusedDayInstances=false;calendar->setMonthView(index==2);calendar->setDate(anchor);}else pendingFocus=true;
    refresh();
}
void DeadlineWidget::showDay(const QDate &date){
    if(!date.isValid())return;
    {QSignalBlocker block(tabs);tabs->setCurrentIndex(0);filter={};}
    anchor=date;focusedDayInstances=true;pendingFocus=true;pages->setCurrentIndex(0);showCompleted->show();calendar->setDate(date);refresh();
}
void DeadlineWidget::navigate(int direction){
    anchor=tabs->currentIndex()==2?anchor.addMonths(direction):anchor.addDays(direction*(tabs->currentIndex()==1?7:1));
    if(tabs->currentIndex()==0){focusedDayInstances=true;pendingFocus=true;}else calendar->setDate(anchor);refresh();
}
void DeadlineWidget::updateHeading(){
    const auto locale=QLocale(QLocale::Chinese);
    QString text;
    if(tabs->currentIndex()==2)text=locale.toString(anchor,"yyyy 年 M 月");
    else if(tabs->currentIndex()==1){const auto first=anchor.addDays(1-anchor.dayOfWeek());text=locale.toString(first,first.year()==QDate::currentDate().year()?"M月d日":"yyyy年M月d日")+" 至 "+locale.toString(first.addDays(6),first.year()==first.addDays(6).year()?"M月d日":"yyyy年M月d日");}
    else text=locale.toString(anchor,anchor.year()==QDate::currentDate().year()?"M月d日 dddd":"yyyy年M月d日 dddd");
    static_cast<PeriodLabel*>(heading)->setPeriod(text);
}
void DeadlineWidget::refresh(){
    if(refreshing)return;QScopedValueRollback<bool> guard(refreshing,true);
    const auto now=QDateTime::currentDateTime();if(!dept->purgeExpired(now))emit saveFailed("事务清理失败："+dept->error());
    auto matches=[&](const TaskOccurrence &o){return filter.matchesTitle(o.title)&&filter.matchesTags(o.tags)&&filter.matchesDate(o.due)&&(filter.starred<0||((o.priority>0)==(filter.starred==1)));};
    auto items=filter.limitDates?dept->occurrences(filter.from,filter.to):dept->listOccurrences(now,showCompleted->isChecked());
    items.erase(std::remove_if(items.begin(),items.end(),[&](const auto &o){return o.deleted||(o.done&&!showCompleted->isChecked());}),items.end());
    if(!filter.limitDates&&focusedDayInstances&&anchor>=now.date()){
        QSet<QString> keys;for(const auto &o:items)keys.insert(o.taskId+"/"+o.key);
        for(const auto &o:dept->occurrences(anchor,anchor))if(o.repeat!="none"&&!o.deleted&&(!o.done||showCompleted->isChecked())&&!(o.done&&o.due<=now)&&!keys.contains(o.taskId+"/"+o.key))items.append(o);
    }
    visible.clear();for(const auto &o:items)if(matches(o))visible.append(o);
    std::stable_sort(visible.begin(),visible.end(),[](const TaskOccurrence &a,const TaskOccurrence &b){if(a.due!=b.due)return a.due<b.due;return a.taskId==b.taskId?a.key<b.key:a.taskId<b.taskId;});
    const bool filtered=filter.active();filterButton->setText(filtered?"关闭筛选":"启用筛选");
    agenda->setOccurrences(visible,anchor,filtered);if(pendingFocus&&tabs->currentIndex()==0){agenda->focusDate(anchor);pendingFocus=false;}
    calendar->setFilter(filter);updateHeading();
}
bool DeadlineWidget::removeTask(const TaskOccurrence &o){
    if(o.repeat=="none")return dept->setDeleted(o.taskId,true);
    QMessageBox box(this);box.setWindowTitle("删除");box.setText("永久删除本次还是整个循环？");
    auto one=box.addButton("本次",QMessageBox::AcceptRole);auto all=box.addButton("整个循环",QMessageBox::DestructiveRole);box.addButton(QMessageBox::Cancel);box.exec();
    if(box.clickedButton()==one)return dept->removeOccurrence(o);
    if(box.clickedButton()==all)return dept->setDeleted(o.taskId,true);
    return true;
}
void DeadlineWidget::showDetail(const TaskOccurrence &source){
    const auto o=source;QDialog dialog(this);dialog.setObjectName("ddlDetail");dialog.setWindowTitle(o.title);dialog.resize(580,390);
    auto layout=new QVBoxLayout(&dialog);layout->setContentsMargins(24,20,24,20);layout->setSpacing(16);
    auto title=new QLabel(o.title);title->setTextFormat(Qt::PlainText);title->setWordWrap(true);title->setProperty("role","title");layout->addWidget(title);
    auto meta=new QLabel((o.due.isValid()?o.due.toLocalTime().toString("yyyy-MM-dd HH:mm"):"无截止时间")+"  ·  "+(o.repeat=="daily"?"每天":o.repeat=="weekly"?"每周":o.repeat=="monthly"?"每月":"单次")+"\n"+o.tags);
    if(o.repeat!="none"){const auto task=dept->task(o.taskId);if(task["countLimited"].toBool())meta->setText(meta->text()+QString("\n剩余 %1 次").arg(Recurrence::remaining(task)));}
    meta->setTextFormat(Qt::PlainText);meta->setProperty("role","muted");meta->setWordWrap(true);layout->addWidget(meta);
    auto note=new QTextEdit;note->setReadOnly(true);note->setPlainText(o.note);layout->addWidget(note);
    auto actions=new QHBoxLayout;bool editing=false;
    {
        auto modify=new QPushButton("编辑"),remove=new QPushButton("删除");remove->setProperty("role","danger");
        if(!o.done){auto check=new QPushButton("完成本次");check->setProperty("role","primary");actions->addWidget(check);
            connect(check,&QPushButton::clicked,&dialog,[&]{if(dept->complete(o))dialog.accept();else QMessageBox::warning(&dialog,"操作失败",dept->error());});}
        if(!o.done&&o.due.isValid()){
            auto later=new QPushButton("稍后10分钟");actions->addWidget(later);connect(later,&QPushButton::clicked,&dialog,[&]{if(dept->snooze(o))dialog.accept();else QMessageBox::warning(&dialog,"提醒设置失败",dept->error());});
        }
        if(o.repeat!="none"){
            auto pause=new QPushButton(o.paused?"恢复循环":"暂停循环");actions->addWidget(pause);connect(pause,&QPushButton::clicked,&dialog,[&]{if(dept->setPaused(o.taskId,!o.paused))dialog.accept();else QMessageBox::warning(&dialog,"循环设置失败",dept->error());});
        }
        actions->addStretch();actions->addWidget(modify);actions->addWidget(remove);
        connect(modify,&QPushButton::clicked,&dialog,[&]{editing=true;dialog.accept();});
        connect(remove,&QPushButton::clicked,&dialog,[&]{if(removeTask(o))dialog.accept();else QMessageBox::warning(&dialog,"删除失败",dept->error());});
    }
    layout->addLayout(actions);PopupPolicy::exec(dialog);if(editing)edit(&o);
}
void DeadlineWidget::addDraft(const QString &title,const QString &note) {
    tabs->setCurrentIndex(0);setProperty("draftTitle",title); setProperty("draftNote",note); edit();
}
void DeadlineWidget::edit(const TaskOccurrence *item) {
    for(auto dialog:findChildren<QDialog*>("ddlEditor"))if(dialog->isVisible())return;
    auto task=item?dept->task(item->taskId):QJsonObject();
    bool onlyThis=false;
    if(item&&item->repeat!="none") {
        QMessageBox choice(this); choice.setWindowTitle("编辑循环"); choice.setText("选择修改范围");
        auto one=choice.addButton("仅本次标题/备注/时间/标签",QMessageBox::AcceptRole);
        auto series=choice.addButton("本次及以后",QMessageBox::ActionRole); choice.addButton(QMessageBox::Cancel); choice.exec();
        if(choice.clickedButton()!=one&&choice.clickedButton()!=series) return;
        onlyThis=choice.clickedButton()==one;
    }
    if(item&&task["countLimited"].toBool())task["remainingCount"]=Recurrence::remaining(task);
    if(item){task["title"]=item->title;task["note"]=item->note;task["tags"]=item->tags;}
    else {task["title"]=property("draftTitle").toString();task["note"]=property("draftNote").toString();}
    const QString draftKey=item?("calendar/"+item->taskId+"/"+item->key+"/"+(onlyThis?"one":"series")):"calendar/new";
    const auto recovery=DraftStore::read(draftKey);if(!recovery.isEmpty())task=recovery;
    const auto initial=item&&item->due.isValid()?item->due.toLocalTime():QDateTime::currentDateTime().addDays(1);
    DeadlineEditor dialog(task,recovery.isEmpty()?initial:QDateTime::fromString(task["due"].toString(),Qt::ISODate),item!=nullptr,onlyThis,this);
    setProperty("draftTitle",QVariant()); setProperty("draftNote",QVariant());
    connect(&dialog,&DeadlineEditor::submitted,&dialog,[&](QJsonObject updated){
        if(onlyThis) {
            if(dept->setOccurrence(item->taskId,item->key,{{"title",updated["title"]},{"note",updated["note"]},{"due",updated["due"]},{"tags",updated["tags"]}}))dialog.accept();
            else dialog.setError(dept->error());return;
        }
        // Split a recurring series so completed/past instances keep their original template.
        if(item&&item->repeat!="none") {
            if(dept->splitSeries(item->taskId,QDate::fromString(item->key,Qt::ISODate),updated))dialog.accept();
            else dialog.setError(dept->error());return;
        } else if(item) {
            updated["id"]=task["id"]; updated["instances"]=task["instances"];
        }
        if(dept->save(updated)) dialog.accept(); else dialog.setError(dept->error());
    });
    connect(this,&DeadlineWidget::locking,&dialog,[&]{
        auto draft=dialog.values();if(draft["title"].toString().isEmpty()&&draft["note"].toString().isEmpty())return;
        // A private-lock draft is saved separately, never truncating an existing recurring series.
        if(draft["title"].toString().isEmpty())draft["title"]="未命名草稿";draft["repeat"]="none";draft["remind"]=false;
        if(!dept->save(draft)){emit saveFailed("草稿保存失败："+dept->error());}
    });
    const auto screen=DialogLayout::fit(dialog,this,dialog.size());auto position=mapToGlobal(rect().center())-QPoint(dialog.width()/2,dialog.height()/2);
    position.setX(qBound(screen.left(),position.x(),qMax(screen.left(),screen.right()-dialog.width()+1)));position.setY(qBound(screen.top(),position.y(),qMax(screen.top(),screen.bottom()-dialog.height()+1)));
    dialog.move(position);
    PopupPolicy::exec(dialog);
    if(dialog.property("outsideDismiss").toBool()){
        if(!DraftStore::save(draftKey,dialog.values()))emit saveFailed("事务草稿保存失败");
    }else DraftStore::clear(draftKey);
}

void DeadlineWidget::toggleFilter(){
    if(filter.active()){filter={};refresh();return;}
    AdvancedFilter next;if(AdvancedFilterDialog::open(AdvancedFilterDialog::Calendar,filterButton,next)){filter=next;refresh();}
}
