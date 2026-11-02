#include "mailgetservice.h"
#include "mailmanager.h"
#include "configmanager.h"
#include <QJsonArray>
namespace {
QJsonObject accountJson(const MailAccount &a){return {{"address",a.address},{"code",a.code},{"server",a.server},{"port",a.port}};}
}
MailGetService::MailGetService(QObject *parent):QObject(parent),worker(this){}
void MailGetService::mailPolling(){
    if(busy)return;historyMode=false;accounts=MailAccountManager::getInstance()->getMailAccounts();index=0;found=false;errors.clear();busy=true;emit status("正在同步邮件…");nextAccount();
}
void MailGetService::mailHistory(){
    if(busy)return;historyMode=true;accounts=MailAccountManager::getInstance()->getMailAccounts();index=0;found=false;errors.clear();busy=true;emit status("正在拉取更早邮件…");nextAccount();
}
void MailGetService::nextAccount(){
    if(index>=accounts.size()){busy=false;emit status(errors.isEmpty()?"同步完成 "+QDateTime::currentDateTime().toString("HH:mm:ss"):errors.join("；"));emit pollingOver(found&&!historyMode);return;}
    const auto account=accounts[index++];auto state=MailManager::getInstance()->syncState(account.address);
    QJsonObject req{{"command","sync"},{"account",accountJson(account)},{"limit",qBound(10,ConfigManager::getInstance()->getMailBasical().initialNumber,100)},
        {"cursor",state["cursor"].toInteger(0)},{"validity",state["validity"].toString()}};
    const bool historyBatch=historyMode&&!state["validity"].toString().isEmpty();
    if(historyBatch){req["command"]="history";req["before"]=MailManager::getInstance()->oldestUid(account.address,state["validity"].toString());}
    worker.run(req,[this,account,historyBatch](const QJsonObject &result,const QString &error){
        if(!error.isEmpty())errors<<account.name+"："+error;
        else if(MailAccountManager::getInstance()->getMailAcc(account.address).address==account.address){
            const auto data=result["data"].toObject();
            if(!MailManager::getInstance()->saveBatch(account.address,data,!historyBatch))errors<<account.name+"：本地保存失败";
            else {found|=!data["messages"].toArray().isEmpty();if(data["remaining"].toInt()>0)emit status("仍有积压邮件，下次同步继续");}
        }
        nextAccount();
    });
}
void MailGetService::mailBody(const MailAccount &a,const QString &id){
    if(a.address.isEmpty()||id.startsWith("legacy:")){const QString message="离线账号或旧邮件记录，无法从服务器获取";emit status(message);emit bodyFailed(a.address,id,message);emit getBodyOver(a.address,id);return;}
    worker.run({{"command","body"},{"account",accountJson(a)},{"id",id}},[this,a,id](const QJsonObject &r,const QString &error){
        if(!error.isEmpty()){emit status(error);emit bodyFailed(a.address,id,error);}
        else if(!MailManager::getInstance()->isDeleted(a.address,id)){auto data=r["data"].toObject();emit newMailBody(a.address,id,data["body"].toString());emit status("正文已获取");}
        emit getBodyOver(a.address,id);
    });
}
void MailGetService::restoreMail(const QString &address,const QString &id){
    const auto key=address+"/"+id;if(restoring.contains(key))return;
    const auto account=MailAccountManager::getInstance()->getMailAcc(address);
    if(account.address.isEmpty()||id.startsWith("legacy:")){
        emit status("无法恢复：请先配置原邮箱账号；旧版离线记录无法联网定位");emit restoreFinished(address,id,false);return;
    }
    if(!MailManager::getInstance()->isDeleted(address,id))return;
    restoring.insert(key);emit status("正在从服务器恢复邮件…");
    worker.run({{"command","restore"},{"account",accountJson(account)},{"id",id}},[this,address,id,key](const QJsonObject &result,const QString &error){
        restoring.remove(key);bool success=false;
        if(!error.isEmpty())emit status("恢复失败："+error+"；已删除记录保留");
        else if(MailAccountManager::getInstance()->getMailAcc(address).address!=address)emit status("恢复失败：原账号已移除；已删除记录保留");
        else {
            success=MailManager::getInstance()->restoreMail(address,id,result["data"].toObject()["message"].toObject());
            emit status(success?"邮件已恢复，正文在打开时重新获取":"恢复失败：响应无效或本地保存失败；已删除记录保留");
        }
        emit restoreFinished(address,id,success);
    });
}

















