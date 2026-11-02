#include "settingmailpage.h"
#include "settingmailaccountswidget.h"
#include "configmanager.h"
#include "constants.h"
SettingMailPage::SettingMailPage(QWidget *parent):QWidget(parent){
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(14);
    auto fields=new QWidget;auto form=SettingControls::form(fields);
    polling=new NoWheelComboBox;initial=new NoWheelComboBox;earlier=new NoWheelComboBox;
    polling->setObjectName("mailPollingInterval");initial->setObjectName("mailInitialNumber");earlier->setObjectName("mailEarlierNumber");
    for(const auto &item:ZTL::Setting::mailpage::intervals)polling->addItem(item.first,item.second);
    for(const auto &item:ZTL::Setting::mailpage::initNumbers)initial->addItem(item.first,item.second);
    for(const auto &item:ZTL::Setting::mailpage::pollNumbers)earlier->addItem(item.first,item.second);
    form->addRow(SettingControls::label("轮询间隔"),polling);form->addRow(SettingControls::label("开机读取邮件数"),initial);form->addRow(SettingControls::label("本地每次多显示数"),earlier);
    storage=new QLineEdit;storage->setObjectName("mailStorageLimit");storage->setValidator(new QIntValidator(1,100000,storage));
    storage->setToolTip("1–100000，回车或移开焦点后应用");form->addRow(SettingControls::label("正文缓存保留数"),storage);layout->addWidget(fields);
    layout->addWidget(SettingControls::label("邮箱账号"));accounts=new SettingsListWidget;accounts->setObjectName("savedMailAccounts");SettingControls::configureList(accounts);layout->addWidget(accounts);
    auto actions=new QHBoxLayout;
    for(const auto &name:{"修改","删除","添加"}){
        auto button=new SettingButton(name);button->setObjectName("account"+QString(name));actions->addWidget(SettingControls::buttonSlot(button,true));
        connect(button,&QPushButton::clicked,this,[this,name]{
            const auto action=QString(name);if(action=="添加"||action=="修改"){editAccount(action=="添加");return;}
            if(auto selected=accounts->currentItem())MailAccountManager::getInstance()->delMailAccount(selected->data(Qt::UserRole).toString());
        });
    }
    layout->addLayout(actions);resetUI();
    connect(polling,&QComboBox::currentIndexChanged,this,[this](int i){if(i<0)return;auto c=ConfigManager::getInstance()->getMailBasical();c.pollingInterval=polling->currentData().toInt();ConfigManager::getInstance()->setMailBasical(c);});
    connect(initial,&QComboBox::currentIndexChanged,this,[this](int i){if(i<0)return;auto c=ConfigManager::getInstance()->getMailBasical();c.initialNumber=initial->currentData().toInt();ConfigManager::getInstance()->setMailBasical(c);});
    connect(earlier,&QComboBox::currentIndexChanged,this,[this](int i){if(i<0)return;auto c=ConfigManager::getInstance()->getMailBasical();c.earlierNumber=earlier->currentData().toInt();ConfigManager::getInstance()->setMailBasical(c);});
    connect(storage,&QLineEdit::editingFinished,this,[this]{auto c=ConfigManager::getInstance()->getMailBasical();if(storage->hasAcceptableInput()){const int value=storage->text().toInt();if(value!=c.storageLimit){c.storageLimit=value;ConfigManager::getInstance()->setMailBasical(c);}}else storage->setText(QString::number(c.storageLimit));});
    connect(ConfigManager::getInstance(),&ConfigManager::mailBasicalChanged,this,&SettingMailPage::resetUI);
    connect(MailAccountManager::getInstance(),&MailAccountManager::mailAccountChanged,this,&SettingMailPage::resetUI);
}
void SettingMailPage::resetUI(){
    const auto c=ConfigManager::getInstance()->getMailBasical();QSignalBlocker a(polling),b(initial),d(earlier);
    auto select=[](QComboBox *combo,int value,const QString &suffix){int i=combo->findData(value);if(i<0){combo->addItem(QString::number(value)+suffix,value);i=combo->count()-1;}combo->setCurrentIndex(i);};
    select(polling,c.pollingInterval," 分钟");select(initial,c.initialNumber,"");select(earlier,c.earlierNumber,"");if(!storage->hasFocus())storage->setText(QString::number(c.storageLimit));
    const auto selected=accounts->currentItem()?accounts->currentItem()->data(Qt::UserRole).toString():QString();accounts->clear();
    for(const auto &account:MailAccountManager::getInstance()->getMailAccounts()){
        auto item=new QListWidgetItem(account.name+" · "+account.address,accounts);item->setData(Qt::UserRole,account.address);if(account.address==selected)accounts->setCurrentItem(item);
    }
}
void SettingMailPage::editAccount(bool creating){
    if(!creating&&!accounts->currentItem())return;
    const auto draftKey=creating?QString("new"):accounts->currentItem()->data(Qt::UserRole).toString();
    for(auto draft:findChildren<QDialog*>("accountPopup",Qt::FindDirectChildrenOnly))if(draft->property("draftKey").toString()==draftKey){SettingControls::showPopup(draft,accounts);return;}
    auto popup=SettingControls::popup(this,creating?"添加邮箱":"修改邮箱","accountPopup");popup->setProperty("draftKey",draftKey);
    auto editor=new SettingMailAccountsWidget(popup);if(!creating)editor->setAccount(MailAccountManager::getInstance()->getMailAcc(accounts->currentItem()->data(Qt::UserRole).toString()));
    popup->layout()->addWidget(editor);connect(editor,&SettingMailAccountsWidget::cancel,popup,&QDialog::close);
    popup->setMinimumWidth(520);SettingControls::showPopup(popup,accounts);
}
