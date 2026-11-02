#include "deadlineeditor.h"
#include "configmanager.h"
#include "settingcontrols.h"
#include "uistyle.h"
#include <QTimeZone>
#include <functional>
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
DeadlineTagPicker::DeadlineTagPicker(const QString &tags,QWidget *parent):QWidget(parent){
    setObjectName("ddlTagChoices");chosen=tags.split(',',Qt::SkipEmptyParts);chosen.removeDuplicates();
    originalTags=chosen;
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(5);
    summary=new QLabel;summary->setProperty("role","muted");layout->addWidget(summary);
    selected=new TagChoiceList;selected->setObjectName("ddlSelectedTags");selected->setFixedHeight(52);selected->setSelectionMode(QAbstractItemView::NoSelection);
    available=new TagChoiceList;available->setObjectName("ddlAvailableTags");available->setFixedHeight(66);available->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(selected);auto hint=new QLabel("可选标签 · 单击添加");hint->setProperty("role","muted");layout->addWidget(hint);layout->addWidget(available);
    for(auto list:{selected,available}){list->setFlow(QListView::LeftToRight);list->setWrapping(true);list->setResizeMode(QListView::Adjust);list->setSpacing(3);list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);}
    setStyleSheet("QListWidget { border:1px solid #343841; background:#14171d; } QListWidget::item { height:24px; padding:2px 8px; } QListWidget::item:disabled { color:#555b64; } QListWidget::item:hover { border:1px solid #7f2636; }");
    auto remove=[this](QListWidgetItem *item){if(!item)return;const int row=selected->row(item);chosen.removeAll(item->data(Qt::UserRole).toString());refresh();selected->setCurrentRow(qMin(row,selected->count()-1));emit selectionChanged();};
    auto add=[this](QListWidgetItem *item){if(!item||chosen.size()>=3)return;const int row=available->row(item);chosen.append(item->data(Qt::UserRole).toString());refresh();available->setCurrentRow(qMin(row,available->count()-1));emit selectionChanged();};
    connect(selected,&QListWidget::itemClicked,this,remove);
    connect(available,&QListWidget::itemClicked,this,add);
    static_cast<TagChoiceList*>(selected)->activate=remove;
    static_cast<TagChoiceList*>(available)->activate=add;
    connect(ConfigManager::getInstance(),&ConfigManager::deadlineChanged,this,&DeadlineTagPicker::refresh);refresh();
}
void DeadlineTagPicker::refresh(){
    const auto common=ConfigManager::getInstance()->commonTags();for(const auto &tag:chosen)if(!originalTags.contains(tag))originalTags.append(tag);selected->clear();available->clear();
    for(const auto &tag:chosen){auto item=new QListWidgetItem("×  "+tag+(common.contains(tag)?"":"（历史）"),selected);item->setData(Qt::UserRole,tag);item->setToolTip("单击移除，移回可选列表");}
    auto candidates=common;for(const auto &tag:originalTags)if(!candidates.contains(tag))candidates.append(tag);
    for(const auto &tag:candidates)if(!chosen.contains(tag)){auto item=new QListWidgetItem(tag+(common.contains(tag)?"":"（历史）"),available);item->setData(Qt::UserRole,tag);if(chosen.size()>=3)item->setFlags(Qt::NoItemFlags);}
    summary->setText(QString("已选 %1 / 3 · 单击移除").arg(chosen.size())+(chosen.size()>3?"；请减到三个后保存":""));
    available->setToolTip(common.isEmpty()?"请先在设置 → DDL 中添加共享标签":"最多选择三个标签");
}
QDateTime DeadlineDateTimeInput::roundUp(const QDateTime &value){
    if(!value.isValid())return {};
    auto result=value;const auto time=value.time();const int remainder=(time.minute()%5)*60+time.second();
    if(remainder||time.msec())result=result.addSecs(300-remainder);
    result.setTime(QTime(result.time().hour(),result.time().minute(),0));return result;
}
DeadlineDateTimeInput::DeadlineDateTimeInput(const QDateTime &value,QWidget *parent):QWidget(parent){
    setObjectName("ddlDue");auto layout=new QHBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(5);
    auto number=[&](const QString &name,int low,int high,int width){auto box=new NoWheelSpinBox;box->setObjectName(name);box->setRange(low,high);box->setButtonSymbols(QAbstractSpinBox::NoButtons);box->setFixedWidth(width);return box;};
    year=number("ddlYear",1,9999,66);month=number("ddlMonth",1,12,44);day=number("ddlDay",1,31,44);hour=number("ddlHour",0,23,44);
    auto minutes=new MinuteSpinBox;minute=minutes;minute->setObjectName("ddlMinute");minute->setRange(0,55);minute->setSingleStep(5);minute->setFixedWidth(76);minutes->makeReadOnly();
    minute->setStyleSheet("QSpinBox#ddlMinute { padding-right:22px; } QSpinBox#ddlMinute::up-button { subcontrol-origin:border; subcontrol-position:top right; width:18px; height:15px; } QSpinBox#ddlMinute::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:18px; height:15px; }");
    auto rounded=roundUp(value.toLocalTime());if(!rounded.isValid())rounded=roundUp(QDateTime::currentDateTime().addDays(1));
    year->setValue(rounded.date().year());month->setValue(rounded.date().month());day->setValue(rounded.date().day());hour->setValue(rounded.time().hour());minute->setValue(rounded.time().minute());
    for(const auto &pair:QList<QPair<QSpinBox*,QString>>{{year,"年"},{month,"月"},{day,"日  —"},{hour,"时"},{minute,"分"}}){layout->addWidget(pair.first);layout->addWidget(new QLabel(pair.second));}layout->addStretch();
    auto updateDays=[this]{day->setMaximum(QDate(year->value(),month->value(),1).daysInMonth());};
    connect(year,&QSpinBox::valueChanged,this,updateDays);connect(month,&QSpinBox::valueChanged,this,updateDays);updateDays();
    setToolTip("年月日和小时可输入范围内整数；分钟用上下按钮每次调整 5 分钟");
}
QDateTime DeadlineDateTimeInput::dateTime() const {return QDateTime(QDate(year->value(),month->value(),day->value()),QTime(hour->value(),minute->value(),0));}
DeadlineEditor::DeadlineEditor(const QJsonObject &task,const QDateTime &date,bool editing,bool onlyThis,QWidget *parent):QDialog(parent,Qt::Dialog|Qt::FramelessWindowHint),original(task){
    setObjectName("ddlEditor");setWindowTitle(editing?"编辑事务":"新增事务");SettingControls::applyTheme(this,QColor("#7f2636"));setStyleSheet(UiStyle::sheet+styleSheet());
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(18,14,18,18);layout->setSpacing(12);
    auto header=new QHBoxLayout;auto heading=new QLabel(windowTitle());heading->setProperty("role","settingLabel");auto close=new SettingButton("×");close->setObjectName("popupClose");close->setFixedWidth(38);
    header->addWidget(heading,1);header->addWidget(close);layout->addLayout(header);connect(close,&QPushButton::clicked,this,&QDialog::reject);
    auto form=new QFormLayout;form->setHorizontalSpacing(20);form->setVerticalSpacing(10);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);layout->addLayout(form);
    title=new QLineEdit(task["title"].toString());title->setObjectName("ddlTitle");
    note=new QTextEdit;note->setObjectName("ddlNote");note->setPlainText(task["note"].toString());note->setFixedHeight(66);
    tags=new DeadlineTagPicker(task["tags"].toString());due=new DeadlineDateTimeInput(date);
    repeat=new NoWheelComboBox;repeat->setObjectName("ddlRepeat");repeat->addItem("不循环","none");repeat->addItem("每天","daily");repeat->addItem("每周","weekly");repeat->addItem("每月","monthly");repeat->setCurrentIndex(qMax(0,repeat->findData(task["repeat"].toString())));
    auto yesNo=[&](const QString &name,bool enabled){auto combo=new NoWheelComboBox;combo->setObjectName(name);combo->addItem("否",false);combo->addItem("是",true);combo->setCurrentIndex(enabled?1:0);return combo;};
    star=yesNo("ddlStar",task["priority"].toInt()>0);remind=yesNo("ddlRemind",task["remind"].toBool(ConfigManager::getInstance()->getDeadlineBasical().remindTime!=-1));
    form->addRow("标题",title);form->addRow("备注",note);form->addRow("标签",tags);form->addRow("截止时间",due);form->addRow("循环",repeat);form->addRow("是否标星",star);form->addRow("是否提醒",remind);
    if(task["repeat"]=="weekly"&&task["weekdays"].toArray().size()>1)repeat->setToolTip("已有多星期循环将保留；更改循环类型后按新的截止日期重新建立规则");
    connect(repeat,&QComboBox::currentIndexChanged,this,[this]{if(original["repeat"]=="weekly"&&repeat->currentData()!="weekly")weeklyReset=true;});
    if(onlyThis){repeat->setEnabled(false);star->setEnabled(false);remind->setEnabled(false);}
    feedback=new QLabel;feedback->setObjectName("ddlEditorFeedback");feedback->setWordWrap(true);feedback->setTextFormat(Qt::PlainText);feedback->hide();layout->addWidget(feedback);
    auto buttons=new QHBoxLayout;auto save=new SettingButton("保存");save->setObjectName("ddlSave");auto cancel=new SettingButton("取消");cancel->setObjectName("ddlCancel");
    buttons->addWidget(SettingControls::buttonSlot(save));buttons->addWidget(SettingControls::buttonSlot(cancel));layout->addLayout(buttons);
    connect(cancel,&QPushButton::clicked,this,&QDialog::reject);
    connect(save,&QPushButton::clicked,this,[this]{
        if(title->text().trimmed().isEmpty()){setError("请输入标题");return;}
        if(tags->selectedCount()>3){setError("最多选择三个标签，请先移除多余的标签");return;}
        if(!due->dateTime().isValid()){setError("请填写有效的截止日期和时间");return;}
        emit submitted(values());
    });
    resize(660,sizeHint().height());
    initialValues=values();
}
void DeadlineEditor::done(int result){
    if(deciding)return;QScopedValueRollback<bool> guard(deciding,true);
    if(result!=QDialog::Accepted&&values()!=initialValues&&
       QMessageBox::question(this,"放弃修改","事务尚未保存，放弃这些输入？")!=QMessageBox::Yes)return;
    QDialog::done(result);
}
void DeadlineEditor::setError(const QString &message){feedback->setText(message);feedback->show();}
QJsonObject DeadlineEditor::values() const {
    const auto date=due->dateTime();const auto rule=repeat->currentData().toString();QJsonArray weekdays;
    if(rule=="weekly")weekdays=!weeklyReset&&original["repeat"]=="weekly"&&original["weekdays"].toArray().size()>1?original["weekdays"].toArray():QJsonArray{date.date().dayOfWeek()};
    return {{"title",title->text().trimmed()},{"note",note->toPlainText()},{"tags",tags->selectedTags()},{"hasDue",true},{"due",date.toString(Qt::ISODate)},
        {"zone",QString::fromUtf8(QTimeZone::systemTimeZoneId())},{"repeat",rule},{"weekdays",weekdays},{"priority",star->currentData().toBool()?1:0},
        {"remind",remind->currentData().toBool()},{"advanceMinutes",original["advanceMinutes"].toInt(qBound<qint64>(0LL,ConfigManager::getInstance()->getDeadlineBasical().remindTime/60,10080LL))}};
}
