#include "appcontext.h"
#include "paths.h"
#include "configmanager.h"
#include "cachemanager.h"
#include "mailaccountmanager.h"
#include "mailmanager.h"
#include <QSettings>
#include <QApplication>
AppContext::AppContext(QObject *parent):QObject(parent),clock(this),tasks(Paths::data("database/tasks.db")),
    vault(Paths::data("vault.ztlvault")),ddl(&tasks,this),notes(&vault,this),mail(this),home(this){
    ConfigManager::getInstance()->initialize();CacheManager::getInstance()->initialize();
    MailAccountManager::getInstance()->initialize();MailManager::getInstance()->initialize();
    if(!ddl.purgeExpired(QDateTime::currentDateTime()))qWarning()<<"事务清理失败："<<ddl.error();
    connect(ConfigManager::getInstance(),&ConfigManager::mailBasicalChanged,this,&AppContext::reloadSettings);
    reloadSettings();
    if(ConfigManager::getInstance()->getMailBasical().pollingInterval!=-1)
        clock.every("mail-start",1,[this]{clock.remove("mail-start");mail.mailPolling();},true);
    clock.every("home",300,[this]{home.refresh();},true);
    clock.every("reminders",15,[this]{ddl.checkReminders(BusinessClock::now());},true);
}
AppContext::~AppContext(){shutdown();}
void AppContext::reloadSettings(){
    QSettings s;
    const int interval=ConfigManager::getInstance()->getMailBasical().pollingInterval;
    if(interval==-1){clock.remove("mail");clock.remove("mail-start");}
    else clock.every("mail",qBound(1,interval,1440)*60,[this]{mail.mailPolling();});
    ddl.setQuiet(s.value("quietStart",23).toInt(),s.value("quietEnd",7).toInt());
    qApp->setProperty("ztlAnimation",s.value("animation",true));
}
void AppContext::shutdown(){
    if(stopped)return;stopped=true;clock.stop();mail.stop();notes.lock();
    for(auto engine:ConfigManager::getInstance()->findChildren<HelpJsonIOEngine*>())engine->forceWrite();
    for(auto engine:CacheManager::getInstance()->findChildren<HelpJsonIOEngine*>())engine->forceWrite();
    for(auto engine:MailAccountManager::getInstance()->findChildren<HelpJsonIOEngine*>())engine->forceWrite();
    MailManager::getInstance()->clearUp();
}
