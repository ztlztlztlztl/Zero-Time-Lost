#include "uipalette.h"
#include "deadlineeditor.h"
#include "configmanager.h"
#include "settingcontrols.h"
#include "uistyle.h"
#include <QTimeZone>
#include <functional>
#include "popuppolicy.h"
#include "dialoglayout.h"
#include "recurrence.h"
namespace {
class TagChoiceList:public QListWidget {
public:
    std::function<void(QListWidgetItem*)> activate;
protected:
    void keyPressEvent(QKeyEvent *event) override {
        if(event->modifiers()==Qt::NoModifier&&(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter||event->key()==Qt::Key_Space)){
            if(activate)activate(currentItem()?currentItem():item(0));event->accept();return;
        }
        QListWidget::keyPressEvent(event);
    }
};
class MinuteSpinBox:public NoWheelSpinBox {
public:
    using NoWheelSpinBox::NoWheelSpinBox;
    void makeReadOnly(){lineEdit()->setReadOnly(true);}
};
}
DeadlineTagPicker::DeadlineTagPicker(const QString &tags,QWidget *parent,bool noteScope):QWidget(parent),noteScope(noteScope){
    setObjectName("ddlTagChoices");chosen=tags.split(',',Qt::SkipEmptyParts);chosen.removeDuplicates();
    originalTags=chosen;
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(5);
    summary=new QLabel;summary->setProperty("role","muted");layout->addWidget(summary);
    selected=new TagChoiceList;selected->setObjectName("ddlSelectedTags");selected->setFixedHeight(52);selected->setSelectionMode(QAbstractItemView::NoSelection);
    available=new TagChoiceList;available->setObjectName("ddlAvailableTags");available->setFixedHeight(66);available->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(selected);auto hint=new QLabel("可选标签 · 单击添加");hint->setProperty("role","muted");layout->addWidget(hint);layout->addWidget(available);
    for(auto list:{selected,available}){list->setFlow(QListView::LeftToRight);list->setWrapping(true);list->setResizeMode(QListView::Adjust);list->setSpacing(3);list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);}
    setStyleSheet(UiPalette::style("QListWidget { border:1px solid #343841; background:#14171d; } QListWidget::item { height:24px; padding:2px 8px; } QListWidget::item:disabled { color:#555b64; } QListWidget::item:hover { border:1px solid #7f2636; }"));
    auto remove=[this](QListWidgetItem *item){if(!item)return;const int row=selected->row(item);chosen.removeAll(item->data(Qt::UserRole).toString());refresh();selected->setCurrentRow(qMin(row,selected->count()-1));emit selectionChanged();};
    auto add=[this](QListWidgetItem *item){if(!item||chosen.size()>=3)return;const int row=available->row(item);chosen.append(item->data(Qt::UserRole).toString());refresh();available->setCurrentRow(qMin(row,available->count()-1));emit selectionChanged();};
    connect(selected,&QListWidget::itemClicked,this,remove);
    connect(available,&QListWidget::itemClicked,this,add);
    static_cast<TagChoiceList*>(selected)->activate=remove;
    static_cast<TagChoiceList*>(available)->activate=add;
    connect(ConfigManager::getInstance(),noteScope?&ConfigManager::noteTagsChanged:&ConfigManager::deadlineChanged,this,&DeadlineTagPicker::refresh);refresh();
}
void DeadlineTagPicker::refresh(){
    const auto common=noteScope?ConfigManager::getInstance()->noteTags():ConfigManager::getInstance()->commonTags();for(const auto &tag:chosen)if(!originalTags.contains(tag))originalTags.append(tag);selected->clear();available->clear();
    for(const auto &tag:chosen){auto item=new QListWidgetItem("×  "+tag+(common.contains(tag)?"":"（历史）"),selected);item->setData(Qt::UserRole,tag);item->setToolTip("单击移除，移回可选列表");}
    auto candidates=common;for(const auto &tag:originalTags)if(!candidates.contains(tag))candidates.append(tag);
    for(const auto &tag:candidates)if(!chosen.contains(tag)){auto item=new QListWidgetItem(tag+(common.contains(tag)?"":"（历史）"),available);item->setData(Qt::UserRole,tag);if(chosen.size()>=3)item->setFlags(Qt::NoItemFlags);}
    summary->setText(QString("已选 %1 / 3 · 单击移除").arg(chosen.size())+(chosen.size()>3?"；请减到三个后保存":""));
    available->setToolTip(common.isEmpty()?(noteScope?"请先在设置 → Note 中添加标签":"请先在设置 → Calendar 中添加标签"):"最多选择三个标签");
}
QDateTime DeadlineDateTimeInput::roundUp(const QDateTime &value){
    if(!value.isValid())return {};
    auto result=value;const auto time=value.time();const int remainder=(time.minute()%5)*60+time.second();
    if(remainder||time.msec())result=result.addSecs(300-remainder);
    result.setTime(QTime(result.time().hour(),result.time().minute(),0));return result;
}
DeadlineDateTimeInput::DeadlineDateTimeInput(const QDateTime &value,QWidget *parent,bool dateOnly):QWidget(parent){
    setObjectName("ddlDue");auto layout=new QHBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(5);
    auto number=[&](const QString &name,int low,int high,int width){auto box=new NoWheelSpinBox;box->setObjectName(name);box->setRange(low,high);box->setButtonSymbols(QAbstractSpinBox::NoButtons);box->setFixedWidth(width);return box;};
    year=number("ddlYear",1,9999,66);month=number("ddlMonth",1,12,44);day=number("ddlDay",1,31,44);hour=number("ddlHour",0,23,44);
    auto minutes=new MinuteSpinBox;minute=minutes;minute->setObjectName("ddlMinute");minute->setRange(0,59);minute->setSingleStep(5);minute->setFixedWidth(76);minutes->makeReadOnly();
    minute->setStyleSheet(UiPalette::style("QSpinBox#ddlMinute { padding-right:22px; } QSpinBox#ddlMinute::up-button { subcontrol-origin:border; subcontrol-position:top right; width:18px; height:15px; } QSpinBox#ddlMinute::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:18px; height:15px; }"));
    auto rounded=value.toLocalTime();if(!rounded.isValid())rounded=roundUp(QDateTime::currentDateTime().addDays(1));
    seconds=rounded.time().second();millis=rounded.time().msec();intendedDay=rounded.date().day();
    year->setValue(rounded.date().year());month->setValue(rounded.date().month());day->setValue(rounded.date().day());hour->setValue(rounded.time().hour());minute->setValue(rounded.time().minute());
    for(const auto &pair:QList<QPair<QSpinBox*,QString>>{{year,"年"},{month,"月"},{day,"日  —"},{hour,"时"},{minute,"分"}}){if(dateOnly&&(pair.first==hour||pair.first==minute)){pair.first->setParent(this);pair.first->hide();continue;}layout->addWidget(pair.first);layout->addWidget(new QLabel(dateOnly&&pair.first==day?"日":pair.second));}layout->addStretch();
    auto updateDays=[this]{if(adjusting)return;adjusting=true;day->setMaximum(QDate(year->value(),month->value(),1).daysInMonth());day->setValue(qMin(intendedDay,day->maximum()));adjusting=false;emit changed();};
    connect(year,&QSpinBox::valueChanged,this,updateDays);connect(month,&QSpinBox::valueChanged,this,updateDays);
    connect(day,&QSpinBox::valueChanged,this,[this](int value){if(adjusting)return;intendedDay=value;emit changed();});
    for(auto box:{hour,minute})connect(box,&QSpinBox::valueChanged,this,[this]{seconds=0;millis=0;if(!adjusting)emit changed();});updateDays();
    setToolTip(dateOnly?"年月日可输入范围内整数":"年月日和小时可输入范围内整数；分钟用上下按钮每次调整 5 分钟");
}
QDateTime DeadlineDateTimeInput::dateTime() const {return QDateTime(QDate(year->value(),month->value(),day->value()),QTime(hour->value(),minute->value(),seconds,millis));}
void DeadlineDateTimeInput::setDate(const QDate &value){if(!value.isValid())return;adjusting=true;year->setValue(value.year());month->setValue(value.month());day->setMaximum(value.daysInMonth());day->setValue(value.day());intendedDay=value.day();adjusting=false;}
void DeadlineDateTimeInput::setTime(const QTime &value){adjusting=true;hour->setValue(value.hour());minute->setValue(value.minute());seconds=value.second();millis=value.msec();adjusting=false;}
void DeadlineDateTimeInput::setTimeOnly(bool enabled){for(int i=0;i<6;++i)if(auto w=layout()->itemAt(i)->widget())w->setVisible(!enabled);}
DeadlineEditor::DeadlineEditor(const QJsonObject &task,const QDateTime &date,bool editing,bool onlyThis,QWidget *parent):QDialog(parent,Qt::Dialog|Qt::FramelessWindowHint),original(task){
    setObjectName("ddlEditor");setProperty("ztlScrollWrapped",true);setWindowTitle(editing?"编辑事务":"新增事务");SettingControls::applyTheme(this,UiPalette::Calendar);
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(18,14,18,18);layout->setSpacing(12);
    auto header=new QHBoxLayout;auto heading=new QLabel(windowTitle());heading->setProperty("role","settingLabel");auto close=new SettingButton("×");close->setObjectName("popupClose");close->setFixedWidth(38);
    header->addWidget(close);header->addWidget(heading,1);layout->addLayout(header);connect(close,&QPushButton::clicked,this,&QDialog::reject);
    auto scroll=new QScrollArea;scroll->setObjectName("ddlEditorScroll");scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);scroll->setMinimumHeight(280);
    auto content=new QWidget;auto form=new QFormLayout(content);form->setHorizontalSpacing(20);form->setVerticalSpacing(10);form->setFormAlignment(Qt::AlignTop);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);scroll->setWidget(content);layout->addWidget(scroll,1);
    type=new NoWheelComboBox;type->setObjectName("calendarType");type->addItem("DDL","ddl");type->addItem("全天标签","label");type->addItem("区间事务","interval");type->setCurrentIndex(qMax(0,type->findData(task["type"].toString("ddl"))));type->setEnabled(!editing);form->addRow("类别",type);
    title=new QLineEdit(task["title"].toString());title->setObjectName("ddlTitle");
    note=new QTextEdit;note->setObjectName("ddlNote");note->setPlainText(task["note"].toString());note->setFixedHeight(66);
    tags=new DeadlineTagPicker(task["tags"].toString());due=new DeadlineDateTimeInput(date,content);
    start=new DeadlineDateTimeInput(task.contains("start")?QDateTime::fromString(task["start"].toString(),Qt::ISODate):date.addSecs(-3600));start->setObjectName("intervalStart");
    labelDate=new DeadlineDateTimeInput(QDateTime(date.date(),QTime(0,0)),nullptr,true);labelDate->setObjectName("labelDate");
    repeat=new NoWheelComboBox;repeat->setObjectName("ddlRepeat");repeat->addItem("每天","daily");repeat->addItem("每周","weekly");repeat->addItem("每月","monthly");repeat->setCurrentIndex(qMax(0,repeat->findData(task["repeat"].toString())));
    auto yesNo=[&](const QString &name,bool enabled){auto combo=new NoWheelComboBox;combo->setObjectName(name);combo->addItem("否",false);combo->addItem("是",true);combo->setCurrentIndex(enabled?1:0);return combo;};
    star=yesNo("ddlStar",task["priority"].toInt()>0);remind=yesNo("ddlRemind",task["remind"].toBool(ConfigManager::getInstance()->getDeadlineBasical().remindTime!=-1));
    form->addRow("标题",title);form->addRow("备注",note);form->addRow("标签",tags);form->addRow("日期",labelDate);form->addRow("开始时间",start);form->addRow("截止时间",due);form->addRow("是否标星",star);form->addRow("是否提醒",remind);
    repeatEnabled=new QCheckBox("是否启用循环");repeatEnabled->setObjectName("ddlRepeatEnabled");repeatEnabled->setChecked(task["repeat"]!="none"&&!task["repeat"].toString().isEmpty());form->addRow("循环",repeatEnabled);
    auto advanced=new QWidget;advanced->setObjectName("ddlRecurrence");auto recurring=new QFormLayout(advanced);recurring->setContentsMargins(0,0,0,0);recurring->setHorizontalSpacing(20);recurring->setVerticalSpacing(10);recurring->setFormAlignment(Qt::AlignTop);recurring->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    remainingCount=new NoWheelSpinBox;remainingCount->setObjectName("ddlRemainingCount");const int left=task.contains("remainingCount")?task["remainingCount"].toInt():Recurrence::remaining(task);remainingCount->setRange(editing&&left==0?0:1,1000000);remainingCount->setValue(qMax(editing?0:1,left));recurring->addRow("剩余次数",remainingCount);
    auto rules=new QHBoxLayout;repeat->setFixedWidth(112);rules->addWidget(repeat);rules->addStretch();recurring->addRow("循环规则",rules);
    auto choices=new QStackedWidget;choices->setObjectName("ddlRecurrenceChoices");recurring->addRow(QString(),choices);
    auto none=new QLabel("每天发生一次");choices->addWidget(none);
    auto choose=[&](const QString &name,int maximum,const QJsonArray &selected,int fallback){
        auto group=new QWidget;group->setObjectName(name);auto grid=new QGridLayout(group);grid->setContentsMargins(0,0,0,0);grid->setSpacing(6);
        group->setStyleSheet(UiPalette::style(R"qss(
QPushButton { background:#14171d; color:#cbd0d8; border:1px solid #343841; border-radius:3px; padding:7px 2px; min-width:0; min-height:22px; }
QPushButton:hover { background:#24202a; border-color:#b8445d; }
QPushButton:checked,QPushButton:checked:hover,QPushButton:checked:pressed { background:#a23750; border:1px solid #e87993; color:#ffffff; font-weight:600; }
QPushButton:focus { border:1px solid #f2adc0; }
QPushButton:disabled { color:#737a86; }
QPushButton[emptyDay="true"] { background:transparent; border:1px solid transparent; }
)qss"));
        const QStringList names{"周一","周二","周三","周四","周五","周六","周日"};
        for(int n=1;n<=(maximum==7?7:35);++n){
            auto button=new QPushButton(n>maximum?QString():maximum==7?names[n-1]:QString::number(n));button->setAutoDefault(false);button->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            if(n<=maximum){button->setObjectName(name+QString::number(n));button->setProperty("day",n);button->setCheckable(true);button->setChecked(selected.isEmpty()?n==fallback:selected.contains(n));button->setAccessibleName(maximum==7?names[n-1]:QString("每月 %1 日").arg(n));connect(button,&QPushButton::toggled,group,[group]{group->setProperty("chosenByUser",true);});}
            else{button->setProperty("emptyDay",true);button->setEnabled(false);}
            grid->addWidget(button,(n-1)/7,(n-1)%7);
        }
        for(int col=0;col<7;++col)grid->setColumnStretch(col,1);
        return group;
    };
    weekdays=choose("ddlWeekdays",7,task["weekdays"].toArray(),due->dateTime().date().dayOfWeek());weekdays->setProperty("chosenByUser",!task["weekdays"].toArray().isEmpty());
    monthdays=choose("ddlMonthdays",31,task["monthdays"].toArray(),due->dateTime().date().day());monthdays->setProperty("chosenByUser",!task["monthdays"].toArray().isEmpty());choices->addWidget(weekdays);choices->addWidget(monthdays);
    auto changeRule=[choices](int index){choices->setCurrentIndex(index);choices->setFixedHeight(choices->currentWidget()->sizeHint().height());};
    changeRule(repeat->currentIndex());connect(repeat,&QComboBox::currentIndexChanged,choices,changeRule);
    // Defaults follow the due date until the user explicitly chooses days.
    for(const auto &name:{"ddlYear","ddlMonth","ddlDay"})connect(due->findChild<QSpinBox*>(name),&QSpinBox::valueChanged,this,[this]{
        for(auto list:{weekdays,monthdays})if(!list->property("chosenByUser").toBool()){
            int selected=list==weekdays?due->dateTime().date().dayOfWeek():due->dateTime().date().day();
            for(auto button:list->findChildren<QPushButton*>()){QSignalBlocker block(button);button->setChecked(button->property("day").toInt()==selected);}
        }
    });
    form->addRow(advanced);advanced->setVisible(repeatEnabled->isChecked());
    connect(repeatEnabled,&QCheckBox::toggled,this,[this,advanced](bool enabled){
        advanced->setVisible(enabled);if(isVisible()){const auto bounds=DialogLayout::fit(*this,parentWidget()?parentWidget():this,QSize(660,enabled?880:620).boundedTo(screen()->availableGeometry().size()-QSize(24,24)));move(DialogLayout::clamp(pos(),size(),bounds));}
    });
    auto intervalDay=[this]{if(type->currentData()=="interval"){const auto date=labelDate->dateTime().date();start->setDate(date);due->setDate(date);}};
    connect(labelDate,&DeadlineDateTimeInput::changed,this,intervalDay);
    auto duration=new QLabel;duration->setObjectName("intervalDuration");duration->setWordWrap(true);form->addRow("时长",duration);
    intervalDuration=qMax<qint64>(300,start->dateTime().secsTo(due->dateTime()));
    auto validateInterval=[this,duration,intervalDay]{intervalDay();if(type->currentData()!="interval")return;const int mins=start->dateTime().secsTo(due->dateTime())/60;duration->setText(mins>0?QString("%1 小时 %2 分钟").arg(mins/60).arg(mins%60):QString("结束时间须晚于开始时间"));};
    connect(start,&DeadlineDateTimeInput::changed,this,[this,validateInterval]{if(type->currentData()=="interval"&&!endTimeEdited&&!adjustingInterval){const auto proposed=start->dateTime().addSecs(intervalDuration);if(proposed.date()==start->dateTime().date()){adjustingInterval=true;due->setTime(proposed.time());adjustingInterval=false;}}validateInterval();});
    connect(due,&DeadlineDateTimeInput::changed,this,[this,validateInterval]{if(type->currentData()=="interval"&&!adjustingInterval){endTimeEdited=true;intervalDuration=qMax<qint64>(300,start->dateTime().secsTo(due->dateTime()));}validateInterval();});
    auto updateType=[this,form,advanced,duration,validateInterval,editing]{
        const auto kind=type->currentData().toString();const auto previous=property("lastCalendarType").toString();const bool label=kind=="label",interval=kind=="interval";
        if(!previous.isEmpty()&&previous!=kind){if(previous=="ddl")labelDate->setDate(due->dateTime().date());else if(previous=="label"){due->setDate(labelDate->dateTime().date());start->setDate(labelDate->dateTime().date());}}setProperty("lastCalendarType",kind);
        form->setRowVisible(note,!label);form->setRowVisible(tags,!label);form->setRowVisible(labelDate,label||interval);form->setRowVisible(start,interval);form->setRowVisible(due,!label);
        qobject_cast<QLabel*>(form->labelForField(title))->setText(label?"标签内容":"标题");
        qobject_cast<QLabel*>(form->labelForField(due))->setText(interval?"结束时间":"截止时间");
        start->setTimeOnly(interval);due->setTimeOnly(interval);form->setRowVisible(duration,interval);
        if(interval&&(start->dateTime().date()!=due->dateTime().date()||start->dateTime()>=due->dateTime())&&!editing){start->setTime(QTime(9,0));due->setTime(QTime(10,0));}
        validateInterval();
        form->setRowVisible(star,!label);form->setRowVisible(remind,kind=="ddl");form->setRowVisible(repeatEnabled,!label);form->setRowVisible(advanced,!label&&repeatEnabled->isChecked());
    };
    connect(type,&QComboBox::currentIndexChanged,this,[updateType]{updateType();});connect(repeatEnabled,&QCheckBox::toggled,this,[updateType]{updateType();});updateType();
    if(onlyThis){repeatEnabled->setEnabled(false);advanced->setEnabled(false);star->setEnabled(false);remind->setEnabled(false);}
    feedback=new QLabel;feedback->setObjectName("ddlEditorFeedback");feedback->setWordWrap(true);feedback->setTextFormat(Qt::PlainText);feedback->hide();layout->addWidget(feedback);
    auto buttons=new QHBoxLayout;auto save=new SettingButton("保存");save->setObjectName("ddlSave");auto cancel=new SettingButton("取消");cancel->setObjectName("ddlCancel");
    buttons->addWidget(SettingControls::buttonSlot(save));buttons->addWidget(SettingControls::buttonSlot(cancel));layout->addLayout(buttons);
    connect(cancel,&QPushButton::clicked,this,&QDialog::reject);
    connect(save,&QPushButton::clicked,this,[this,onlyThis]{
        if(title->text().trimmed().isEmpty()){setError("请输入标题");return;}
        if(type->currentData()!="label"&&tags->selectedCount()>3){setError("最多选择三个标签，请先移除多余的标签");return;}
        if(!due->dateTime().isValid()){setError("请填写有效的截止日期和时间");return;}
        auto value=values();value["_remainingEdited"]=value["remainingCount"]!=initialValues["remainingCount"];
        if(value["type"]=="interval"&&(start->dateTime()>=due->dateTime()||start->dateTime().date()!=due->dateTime().date())){setError("开始与结束必须在同一天，且开始时间早于结束时间");return;}
        if(value["repeat"]=="weekly"&&value["weekdays"].toArray().isEmpty()){setError("请选择至少一个星期");return;}
        if(value["repeat"]=="monthly"&&value["monthdays"].toArray().isEmpty()){setError("请选择至少一个月内日期");return;}
        if(!onlyThis&&value["countLimited"].toBool()&&value["remainingCount"].toInt()<1&&initialValues["remainingCount"].toInt()!=0){setError("循环已结束；如需继续，请填写正数剩余次数");return;}
        if(value["type"]=="interval"&&start->dateTime()>=due->dateTime()){setError("结束时间须晚于开始时间");return;}
        emit submitted(value);
    });
    resize(660,repeatEnabled->isChecked()?880:620);
    initialValues=values();
}
void DeadlineEditor::done(int result){
    if(deciding)return;QScopedValueRollback<bool> guard(deciding,true);
    if(!property("outsideDismiss").toBool()&&result!=QDialog::Accepted&&values()!=initialValues&&
       QMessageBox::question(this,"放弃修改","事务尚未保存，放弃这些输入？")!=QMessageBox::Yes)return;
    QDialog::done(result);
}
void DeadlineEditor::setError(const QString &message){feedback->setText(message);feedback->show();}
QJsonObject DeadlineEditor::values() const {
    const auto kind=type->currentData().toString();const bool label=kind=="label";const auto date=label?QDateTime(labelDate->dateTime().date(),QTime(0,0)):due->dateTime();const bool recurring=!label&&repeatEnabled->isChecked();const auto rule=recurring?repeat->currentData().toString():QString("none");QJsonArray selectedWeekdays,selectedMonthdays;
    for(auto button:weekdays->findChildren<QPushButton*>())if(button->isChecked())selectedWeekdays.append(button->property("day").toInt());
    for(auto button:monthdays->findChildren<QPushButton*>())if(button->isChecked())selectedMonthdays.append(button->property("day").toInt());
    auto stamp=[](const QDateTime &value){return value.toString(value.time().msec()?Qt::ISODateWithMs:Qt::ISODate);};
    return {{"type",kind},{"start",kind=="interval"?stamp(start->dateTime()):QString()},{"title",title->text().trimmed()},{"note",label?QString():note->toPlainText()},{"tags",label?QString():tags->selectedTags()},{"hasDue",true},{"due",stamp(date)},
        {"zone",original["zone"].toString(QString::fromUtf8(QTimeZone::systemTimeZoneId()))},{"repeat",rule},{"weekdays",selectedWeekdays},{"monthdays",selectedMonthdays},
        {"countLimited",recurring},{"remainingCount",remainingCount->value()},{"priority",!label&&star->currentData().toBool()?1:0},
        {"remind",kind=="ddl"&&remind->currentData().toBool()},{"advanceMinutes",original["advanceMinutes"].toInt(qBound<qint64>(0LL,ConfigManager::getInstance()->getDeadlineBasical().remindTime/60,10080LL))}};
}
