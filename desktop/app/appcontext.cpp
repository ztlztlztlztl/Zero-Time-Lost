#include "appcontext.h"
#include "paths.h"
#include "configmanager.h"
#include "cachemanager.h"
#include "mailaccountmanager.h"
#include "mailmanager.h"
#include "cloudconnection.h"
#include <QSettings>
#include <QApplication>
#include "draftstore.h"
#include "filebatch.h"
AppContext::AppContext(QObject *parent):QObject(parent),clock(this),tasks(Paths::data("database/tasks.db")),
    vault(Paths::data("vault.ztlvault")),ddl(&tasks,this),notes(&vault,this),mail(this),home(this){
    ConfigManager::getInstance()->initialize();CacheManager::getInstance()->initialize();
    MailAccountManager::getInstance()->initialize();
    if(!MailManager::getInstance()->initialize())qWarning()<<"邮件数据库初始化失败："<<MailManager::getInstance()->error();
    cloud=new CloudSync(&tasks,&ddl,&notes,this);
    connect(cloud,&CloudSync::sharedSettingsUploaded,this,[this]{home.refresh(true);});
    connect(cloud,&CloudSync::synced,this,[this]{if(CloudConnection::enabled()&&ConfigManager::getInstance()->getMailBasical().pollingInterval!=-1)mail.mailPolling(false);});
    connect(CloudConnection::instance(),&CloudConnection::settingsChanged,this,[this]{mail.stop();reloadSettings();});
    if(!ddl.purgeExpired(QDateTime::currentDateTime()))qWarning()<<"事务清理失败："<<ddl.error();
    connect(ConfigManager::getInstance(),&ConfigManager::mailBasicalChanged,this,&AppContext::reloadSettings);
    clock.every("reminder-prune",86400,[this]{if(!tasks.pruneReminderReceipts())qWarning()<<"提醒回执清理失败";});
    reloadSettings();
    if(ConfigManager::getInstance()->getMailBasical().pollingInterval!=-1)
        clock.every("mail-start",1,[this]{clock.remove("mail-start");mail.mailPolling(false);},true);
    clock.every("home",300,[this]{home.refresh();},true);
    clock.every("reminders",15,[this]{ddl.checkReminders(BusinessClock::now());},true);
}
AppContext::~AppContext(){shutdown();}
void AppContext::reloadSettings(){
    QSettings s;
    const int interval=ConfigManager::getInstance()->getMailBasical().pollingInterval;
    if(qApp->property("ztlSetupActive").toBool()||interval==-1||CloudConnection::enabled()){clock.remove("mail");clock.remove("mail-start");}
    else clock.every("mail",qBound(1,interval,1440)*60,[this]{mail.mailPolling(false);});
    ddl.setQuiet(s.value("quietStart",23).toInt(),s.value("quietEnd",7).toInt());
    qApp->setProperty("ztlAnimation",s.value("animation",true));
}
QStringList AppContext::flushAll(){
    QStringList errors;
    if(QFile::exists(FileBatch::journal())){if(!FileBatch::recover()){errors.append("共享设置提交日志尚未恢复");return errors;}ConfigManager::getInstance()->reload();MailAccountManager::getInstance()->reload();}
    if(!notes.flush())errors.append("秘密恢复草稿："+notes.error());
    if(!DraftStore::flush())errors.append("编辑恢复草稿");
    for(auto owner:QList<QObject*>{ConfigManager::getInstance(),CacheManager::getInstance(),MailAccountManager::getInstance()})
        for(auto engine:owner->findChildren<HelpJsonIOEngine*>())if(!engine->flush())errors.append(engine->error());
    QSettings settings;settings.sync();if(settings.status()!=QSettings::NoError)errors.append("通用设置");
    if(cloud&&!cloud->flush())errors.append("云端同步状态");
    return errors;
}
void AppContext::shutdown(){
    if(stopped)return;flushAll();stopped=true;clock.stop();mail.stop();notes.lock();
    for(auto engine:ConfigManager::getInstance()->findChildren<HelpJsonIOEngine*>())engine->forceWrite();
    for(auto engine:CacheManager::getInstance()->findChildren<HelpJsonIOEngine*>())engine->forceWrite();
    for(auto engine:MailAccountManager::getInstance()->findChildren<HelpJsonIOEngine*>())engine->forceWrite();
    MailManager::getInstance()->clearUp();
}
