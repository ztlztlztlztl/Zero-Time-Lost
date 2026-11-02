#include "settingdeadlinepage.h"
#include "settingcontrols.h"
#include "configmanager.h"
#include "constants.h"
SettingDeadlinePage::SettingDeadlinePage(QWidget *parent):QWidget(parent){
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(14);
    auto fields=new QWidget;auto form=SettingControls::form(fields);advance=new NoWheelComboBox;
    advance->setObjectName("defaultAdvance");
    for(const auto &item:ZTL::Setting::deadlinePage::remindTime)advance->addItem(item.first,item.second);
    form->addRow(SettingControls::label("默认提前提醒"),advance);layout->addWidget(fields);
    layout->addWidget(SettingControls::label("共享标签 · Note / DDL"));
    tags=new SettingsListWidget;tags->setObjectName("commonTags");SettingControls::configureList(tags);tags->setDragDropMode(QAbstractItemView::InternalMove);tags->setDefaultDropAction(Qt::MoveAction);layout->addWidget(tags);
    auto controls=new QHBoxLayout;
    for(const auto &name:{"修改","删除","上移","下移","添加"}){
        auto button=new SettingButton(name);button->setObjectName("tag"+QString(name));controls->addWidget(SettingControls::buttonSlot(button,true));
        connect(button,&QPushButton::clicked,this,[this,name]{
            const auto action=QString(name);if(action=="添加"||action=="修改"){editTag(action=="添加");return;}
            const int row=tags->currentRow();auto values=ConfigManager::getInstance()->commonTags();if(row<0||row>=values.size())return;int target=row;
            if(action=="删除")values.removeAt(row);else{target=row+(action=="上移"?-1:1);if(target<0||target>=values.size())return;values.swapItemsAt(row,target);}
            ConfigManager::getInstance()->setCommonTags(values);tags->setCurrentRow(qMin(target,tags->count()-1));
        });
    }
    layout->addLayout(controls);
    connect(tags->model(),&QAbstractItemModel::rowsMoved,this,[this]{
        QStringList values;for(int i=0;i<tags->count();++i)values.append(tags->item(i)->text());
        // Do not reset the model inside its own rowsMoved notification.
        QTimer::singleShot(0,this,[values]{ConfigManager::getInstance()->setCommonTags(values);});
    });
    connect(advance,&QComboBox::currentIndexChanged,this,[this](int index){if(index<0)return;auto c=ConfigManager::getInstance()->getDeadlineBasical();c.remindTime=advance->currentData().toLongLong();ConfigManager::getInstance()->setDeadlineBasical(c);});
    connect(ConfigManager::getInstance(),&ConfigManager::deadlineChanged,this,&SettingDeadlinePage::resetUI);resetUI();
}
void SettingDeadlinePage::resetUI(){
    const auto config=ConfigManager::getInstance()->getDeadlineBasical();QSignalBlocker b(advance),c(tags);
    auto select=[](QComboBox *combo,qint64 value){int i=combo->findData(value);if(i<0){combo->addItem(QString::number(value)+" 秒",value);i=combo->count()-1;}combo->setCurrentIndex(i);};
    select(advance,config.remindTime);
    const auto selected=tags->currentItem()?tags->currentItem()->text():QString();tags->clear();tags->addItems(ConfigManager::getInstance()->commonTags());
    const auto found=tags->findItems(selected,Qt::MatchExactly);if(!found.isEmpty())tags->setCurrentItem(found.first());
}
void SettingDeadlinePage::editTag(bool creating){
    if(!creating&&!tags->currentItem())return;const auto original=creating?QString():tags->currentItem()->text();
    auto popup=SettingControls::popup(this,creating?"添加共享标签":"修改共享标签","tagPopup");auto layout=qobject_cast<QVBoxLayout*>(popup->layout());
    auto input=new QLineEdit(original);input->setObjectName("tagInput");input->setMaxLength(32);input->setPlaceholderText("标签名称，最多 32 字符");layout->addWidget(input);
    auto error=new QLabel;layout->addWidget(error);auto save=new SettingButton(creating?"添加":"应用");save->setObjectName("tagApply");layout->addWidget(SettingControls::buttonSlot(save));
    auto apply=[=]{
        const auto text=input->text().trimmed();auto values=ConfigManager::getInstance()->commonTags();
        if(text.isEmpty()||text.contains(',')||text.contains("，")){error->setText("名称不能为空或含逗号");return;}
        if(values.contains(text)&&(creating||text!=original)){error->setText("此标签已存在");return;}
        if(creating)values.append(text);else {const int index=values.indexOf(original);if(index<0){error->setText("原标签已被移除");return;}values[index]=text;}
        ConfigManager::getInstance()->setCommonTags(values);const auto added=tags->findItems(text,Qt::MatchExactly);if(!added.isEmpty())tags->setCurrentItem(added.first());popup->close();
    };
    connect(save,&QPushButton::clicked,popup,apply);connect(input,&QLineEdit::returnPressed,popup,apply);
    popup->setMinimumWidth(360);SettingControls::showPopup(popup,tags);input->setFocus();input->selectAll();
}
