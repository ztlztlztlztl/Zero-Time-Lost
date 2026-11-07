#include "mailgetservice.h"
#include "mailmanager.h"
#include "configmanager.h"
#include "cloudconnection.h"
#include <QPointer>
#include <QJsonArray>
namespace {
QJsonObject accountJson(const MailAccount &a){return {{"address",a.address},{"code",a.code},{"server",a.server},{"port",a.port}};}
}
MailGetService::MailGetService(QObject *parent):QObject(parent),worker(this){}
void MailGetService::run(const QJsonObject &request,std::function<void(const QJsonObject &,const QString &)> callback){
    if(!CloudConnection::enabled()){worker.run(request,callback);return;}
    auto req=request;req["address"]=req["account"].toObject()["address"];req.remove("account");req.remove("limit");
    QPointer<MailGetService> guard(this);const auto current=generation;
    CloudConnection::instance()->request("/v1/mail",req,[guard,callback,current](const QJsonObject &r,const QString &error){
        if(!guard||guard->generation!=current)return;
        callback(r,!error.isEmpty()?error:r["ok"].toBool()?QString():r["error"].toString("云端邮件任务失败"));
    },true);
}
void MailGetService::mailPolling(bool force){
    if(busy)return;drainBatches=0;forceCloudPoll=force;historyMode=false;accounts=MailAccountManager::getInstance()->getMailAccounts();index=0;found=false;errors.clear();busy=true;emit status("正在同步邮件…");nextAccount();
}
void MailGetService::mailHistory(){
    if(busy)return;historyMode=true;accounts=MailAccountManager::getInstance()->getMailAccounts();index=0;found=false;errors.clear();busy=true;emit status("正在拉取更早邮件…");nextAccount();
}
void MailGetService::nextAccount(){
    if(index>=accounts.size()){busy=false;emit status(errors.isEmpty()?"同步完成 "+QDateTime::currentDateTime().toString("HH:mm:ss"):errors.join("；"));emit pollingOver(found&&!historyMode);return;}
    const auto account=accounts[index++];if(index>1&&accounts[index-2].address!=account.address)drainBatches=0;auto state=MailManager::getInstance()->syncState(account.address);
    QJsonObject req{{"command","sync"},{"account",accountJson(account)},{"limit",qBound(10,ConfigManager::getInstance()->getMailBasical().initialNumber,100)},
        {"cursor",state["cursor"].toInteger(0)},{"validity",state["validity"].toString()},{"retryUids",state["retryUids"].toArray()},{"force",forceCloudPoll&&drainBatches==0}};
    const bool historyBatch=historyMode&&!state["validity"].toString().isEmpty();
    if(historyBatch){req["command"]="history";req["before"]=MailManager::getInstance()->oldestUid(account.address,state["validity"].toString());}
    run(req,[this,account,historyBatch](const QJsonObject &result,const QString &error){
        if(!error.isEmpty())errors<<account.name+"："+error;
        else if(MailAccountManager::getInstance()->getMailAcc(account.address).address==account.address){
            const auto data=result["data"].toObject();
            if(!data["warning"].toString().isEmpty())errors<<account.name+"："+data["warning"].toString();
            if(!MailManager::getInstance()->saveBatch(account.address,data,!historyBatch))errors<<account.name+"：本地保存失败";
            else {found|=!data["messages"].toArray().isEmpty();if(data["remaining"].toInt()>0&&!historyBatch){if(++drainBatches<4){--index;}else errors<<account.name+"：本轮已保存多批邮件，仍有积压，下次继续收取";}}
        }
        if(error.isEmpty()){markPendingSeen(account);return;}
        nextAccount();
    });
}
void MailGetService::mailBody(const MailAccount &a,const QString &id){
    if(a.address.isEmpty()||id.startsWith("legacy:")){const QString message="离线账号或旧邮件记录，无法从服务器获取";emit status(message);emit bodyFailed(a.address,id,message);emit getBodyOver(a.address,id);return;}
    run({{"command","body"},{"account",accountJson(a)},{"id",id}},[this,a,id](const QJsonObject &r,const QString &error){
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
    run({{"command","restore"},{"account",accountJson(account)},{"id",id}},[this,address,id,key](const QJsonObject &result,const QString &error){
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


















void MailGetService::markPendingSeen(const MailAccount &account){
    if(CloudConnection::enabled()){
        const auto pending=MailManager::getInstance()->pendingSeen(account.address);
        if(!pending.isEmpty()&&!MailManager::getInstance()->acknowledgeSeen(account.address,pending)){errors<<"本地已读确认保存失败";nextAccount();return;}
        if(pending.size()==100){markPendingSeen(account);return;}nextAccount();return;
    }
    if(MailAccountManager::getInstance()->getMailAcc(account.address).address!=account.address){nextAccount();return;}
    const auto pending=MailManager::getInstance()->pendingSeen(account.address);
    if(pending.isEmpty()){nextAccount();return;}
    worker.run({{"command","markSeen"},{"account",accountJson(account)},{"ids",QJsonArray::fromStringList(pending)}},
        [this,account,pending](const QJsonObject &result,const QString &failure){
        if(!failure.isEmpty())errors<<account.name+"：摘要已保存，服务器标记已读失败，下次同步重试："+failure;
        else if(result["data"].toObject()["acknowledged"].toArray()!=QJsonArray::fromStringList(pending))errors<<account.name+"：服务器已读确认无效，下次同步重试";
        else if(!MailManager::getInstance()->acknowledgeSeen(account.address,pending))errors<<account.name+"：已读确认保存失败，下次重试";
        else {markPendingSeen(account);return;}
        nextAccount();
    });
}
