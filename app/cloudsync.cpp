#include "cloudsync.h"
#include "cloudconnection.h"
#include "paths.h"
#include "taskrepository.h"
#include "ddldepartment.h"
#include "notedepartment.h"
#include "configmanager.h"
#include "mailaccountmanager.h"
#include "mailmanager.h"
#include <QCryptographicHash>
#include <QtWidgets>
#include <QSaveFile>
#include <QJsonDocument>
#include <QUuid>
#include <QScopedValueRollback>

namespace {
QString keyOf(const QJsonObject &r){return r["kind"].toString()+":"+r["id"].toString();}
bool same(const QJsonObject &a,const QJsonObject &b){
    const bool goneA=a.isEmpty()||a["deleted"].toBool(),goneB=b.isEmpty()||b["deleted"].toBool();
    return goneA==goneB&&(goneA||a["payload"]==b["payload"]);
}
QJsonObject record(const QString &kind,const QString &id,const QJsonObject &payload){return {{"kind",kind},{"id",id},{"payload",payload},{"deleted",false}};}
QStringList strings(QJsonValue v){QStringList list;for(auto x:v.toArray())list.append(x.toString());return list;}
}
CloudSync::CloudSync(TaskRepository *tasks,DDLDepartment *ddl,NoteDepartment *notes,QObject *parent)
    :QObject(parent),tasks(tasks),ddl(ddl),notes(notes),path(Paths::data("cloud/state.json")){
    QFile file(path);
    if(file.exists()){
        QJsonParseError error;
        if(!file.open(QIODevice::ReadOnly))healthy=false;
        else{auto doc=QJsonDocument::fromJson(file.readAll(),&error);healthy=doc.isObject()&&error.error==QJsonParseError::NoError;state=doc.object();}
        file.close();
    }
    if(healthy&&!state["journal"].toObject().isEmpty())healthy=replay();
    debounce.setSingleShot(true);debounce.setInterval(800);
    connect(&debounce,&QTimer::timeout,this,&CloudSync::sync);
    auto changed=[this]{if(applying||!CloudConnection::enabled())return;const auto previous=state["pending"];capture();if(state["pending"]!=previous)debounce.start();};
    connect(ddl,&DDLDepartment::changed,this,changed);connect(notes,&NoteDepartment::changed,this,changed);
    auto config=ConfigManager::getInstance();
    connect(config,&ConfigManager::deadlineChanged,this,changed);connect(config,&ConfigManager::noteTagsChanged,this,changed);
    connect(config,&ConfigManager::homeWeatherChanged,this,changed);connect(config,&ConfigManager::mailBasicalChanged,this,changed);
    connect(MailAccountManager::getInstance(),&MailAccountManager::mailAccountChanged,this,changed);
    connect(MailManager::getInstance(),&MailManager::changed,this,changed);
    auto connection=CloudConnection::instance();
    connect(connection,&CloudConnection::syncRequested,this,&CloudSync::sync);
    connect(connection,&CloudConnection::conflictsRequested,this,&CloudSync::showConflicts);
    connect(connection,&CloudConnection::settingsChanged,this,[this]{debounce.start();});
    connect(this,&CloudSync::status,connection,&CloudConnection::status);
    connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState s){if(s==Qt::ApplicationActive)debounce.start();});
    periodic.setInterval(30000);connect(&periodic,&QTimer::timeout,this,&CloudSync::sync);periodic.start();debounce.start();
}
QJsonObject CloudSync::snapshot() const {
    readable=tasks->ready()&&notes->cloudReady();
    for(auto owner:QList<QObject*>{ConfigManager::getInstance(),MailAccountManager::getInstance()})
        for(auto engine:owner->findChildren<HelpJsonIOEngine*>())readable=readable&&engine->readable();
    QJsonObject result;
    auto add=[&](const QJsonObject &r){result[keyOf(r)]=r;};
    for(const auto &task:tasks->all())add(record("calendar",task["id"].toString(),task));
    QSet<QString> publicIds;for(auto value:notes->cloudEntries()){auto e=value.toObject();publicIds.insert(e["id"].toString());add(record("note",e["id"].toString(),e));}
    for(int kind:{1,2}){
        QJsonArray ids;for(const auto &id:notes->order(kind))if(publicIds.contains(id)||notes->entry(id).isEmpty())ids.append(id);
        add(record("order",QString::number(kind),{{"ids",ids}}));
    }
    bool mailReady=false;const auto flagsList=MailManager::getInstance()->cloudFlags(&mailReady);readable=readable&&tasks->ready()&&mailReady;
    for(auto value:flagsList){
        auto flags=value.toObject();const auto identity=flags["address"].toString().toUtf8()+'\0'+flags["uid"].toString().toUtf8();
        add(record("mailflag",QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex()),flags));
    }
    const auto c=ConfigManager::getInstance();auto weather=c->getHomeweather();auto mail=c->getMailBasical();auto deadline=c->getDeadlineBasical();
    QMap<QString,QJsonObject> accountMap;
    for(const auto &a:MailAccountManager::getInstance()->getMailAccounts())accountMap[a.address]={{"name",a.name},{"address",a.address},{"code",a.code},{"server",a.server},{"port",a.port}};
    QJsonArray accounts;for(const auto &a:accountMap)accounts.append(a);
    add(record("settings","shared",{{"weather",weather.toJson()},{"mail",mail.toJson()},{"calendar",deadline.toJson()},
        {"noteTags",QJsonArray::fromStringList(c->noteTags())},{"weatherKey",QSettings().value("weatherKey").toString()},{"accounts",accounts}}));
    return result;
}
bool CloudSync::save(){
    QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)){healthy=false;emit status("云端同步状态无法保存，已暂停网络写入");return false;}
    auto bytes=QJsonDocument(state).toJson(QJsonDocument::Compact);
    if(file.write(bytes)!=bytes.size()||!file.commit()){healthy=false;emit status("云端同步状态保存失败，请检查磁盘空间");return false;}return true;
}
void CloudSync::capture(){
    if(!healthy||applying||!CloudConnection::enabled())return;
    for(auto owner:QList<QObject*>{ConfigManager::getInstance(),MailAccountManager::getInstance()})
        for(auto engine:owner->findChildren<HelpJsonIOEngine*>())if(!engine->flush()){
            healthy=false;emit status("业务设置未能保存，已暂停同步以保留修改");return;
        }
    const auto local=snapshot(),shadow=state["shadow"].toObject(),conflicts=state["conflicts"].toObject();auto pending=state["pending"].toObject();
    if(!readable){healthy=false;emit status("本地业务数据读取失败，已暂停同步；不会将读取失败当成删除");return;}
    QSet<QString> keys;for(auto it=local.begin();it!=local.end();++it)keys.insert(it.key());for(auto it=shadow.begin();it!=shadow.end();++it)keys.insert(it.key());
    for(const auto &key:keys){
        if(pending.contains(key)||conflicts.contains(key))continue;
        const auto base=shadow[key].toObject();auto current=local[key].toObject();
        if(current.isEmpty()){current=base;current["deleted"]=true;current["payload"]=QJsonObject();}
        if(!base.isEmpty()&&same(current,base))continue;
        if(current["deleted"].toBool()&&base["deleted"].toBool())continue;
        current.remove("rev");current["base"]=base["rev"].toInteger();current["op"]=QUuid::createUuid().toString(QUuid::WithoutBraces);pending[key]=current;
    }
    if(pending!=state["pending"].toObject()){state["pending"]=pending;save();}
}
bool CloudSync::apply(const QJsonObject &r){
    QScopedValueRollback<bool> guard(applying,true);
    const auto kind=r["kind"].toString(),id=r["id"].toString();const auto p=r["payload"].toObject();const bool deleted=r["deleted"].toBool();
    if(kind=="calendar"){
        if(!deleted){
            if(p["id"]!=id)return false;
            if(p["type"]=="interval"){
                // Validate the serialized source dates rather than the receiving device's timezone.
                const auto start=QDateTime::fromString(p["start"].toString(),Qt::ISODate),end=QDateTime::fromString(p["due"].toString(),Qt::ISODate);
                if(!p["hasDue"].toBool()||!start.isValid()||!end.isValid()||start>=end||start.date()!=end.date())return false;
            }else if(!DDLDepartment::validateType(p).isEmpty())return false;
        }
        if(!tasks->applyChanges(deleted?QList<QJsonObject>():QList<QJsonObject>{p},deleted?QStringList{id}:QStringList()))return false;
        ddl->refreshFromSync();return true;
    }
    if(kind=="note")return notes->applyCloudEntry(id,p,deleted);
    if(kind=="mailflag"&&!deleted)return MailManager::getInstance()->applyCloudFlags(p);
    if(kind=="order"){
        const int category=id.toInt();if(category!=1&&category!=2)return false;
        return notes->applyCloudOrder(category,strings(p["ids"]));
    }
    if(kind=="settings"&&id=="shared"&&!deleted){
        auto c=ConfigManager::getInstance();c->setHomeweather(HomeWeatherConfig(p["weather"].toObject()));c->setMailBasical(MailBasicalConfig(p["mail"].toObject()));
        c->setDeadlineBasical(DeadlineBasicalConfig(p["calendar"].toObject()));c->setNoteTags(strings(p["noteTags"]));
        QSettings s;s.setValue("weatherKey",p["weatherKey"].toString());s.sync();if(s.status()!=QSettings::NoError)return false;
        auto accounts=MailAccountManager::getInstance();QSet<QString> keep;
        for(auto v:p["accounts"].toArray()){MailAccount a(v.toObject());if(a.address.isEmpty())return false;keep.insert(a.address);accounts->setMailAccount(a);}
        const auto previous=accounts->getMailAccounts();for(const auto &a:previous)if(!keep.contains(a.address))accounts->delMailAccount(a.address);
        for(auto engine:c->findChildren<HelpJsonIOEngine*>())if(!engine->flush())return false;for(auto engine:accounts->findChildren<HelpJsonIOEngine*>())if(!engine->flush())return false;
        return true;
    }
    return false;
}
bool CloudSync::replay(){
    const auto journal=state["journal"].toObject();if(journal.isEmpty())return true;
    const auto r=journal["record"].toObject();const auto key=keyOf(r);const auto local=snapshot()[key].toObject();
    if(!readable)return false;
    // If a failed apply was followed by new edits, preserve the edits as a conflict.
    auto conflicts=state["conflicts"].toObject();
    if(!same(local,journal["before"].toObject())&&!same(local,r))conflicts[key]=QJsonObject{{"remote",r},{"local",local}};
    else if(!apply(r))return false;
    auto shadow=state["shadow"].toObject();shadow[key]=r;state["shadow"]=shadow;state["conflicts"]=conflicts;
    state["cursor"]=journal["cursor"];state.remove("journal");return save();
}
void CloudSync::sync(){
    if(busy||resolving||!CloudConnection::enabled())return;
    if(!healthy){emit status("云端同步已暂停：本地同步状态损坏或写入失败，请检查 data/cloud");return;}
    capture();if(!healthy)return;busy=true;emit status("正在同步云端数据…");
    CloudConnection::instance()->request("/health",{},[this,guard=QPointer<CloudSync>(this)](const QJsonObject &r,const QString &error){
        if(!guard)return;
        if(!error.isEmpty()){finish(error);return;}
        if(r["protocol"]!=1||r["dataset"].toString().isEmpty()){finish("服务器协议不兼容");return;}
        const auto dataset=state["dataset"].toString();
        if(!dataset.isEmpty()&&dataset!=r["dataset"].toString()){finish("服务器数据集已改变，请核查地址或备份恢复；本地修改保留");return;}
        qint64 known=state["cursor"].toInteger();const auto shadow=state["shadow"].toObject();
        for(auto value:shadow)known=qMax(known,value.toObject()["rev"].toInteger());
        if(r["head"].toInteger()<known){finish("服务器修订号倒退，请核查备份恢复；已暂停上传");return;}
        state["dataset"]=r["dataset"];if(!save()){finish("状态保存失败");return;}next();
    });
}
void CloudSync::next(){
    if(!CloudConnection::enabled()||!healthy){finish(healthy?QString():"同步状态保存失败");return;}
    capture();if(!healthy){finish("状态保存失败");return;}
    const auto pending=state["pending"].toObject();
    if(pending.isEmpty()){pull();return;}
    const auto key=pending.begin().key();const auto request=pending[key].toObject();
    CloudConnection::instance()->request("/v1/mutate",request,[this,key,request,guard=QPointer<CloudSync>(this)](const QJsonObject &r,const QString &error){
        if(!guard)return;
        if(!error.isEmpty()){finish(error);return;}
        if(r["ok"].toBool()){
            const auto accepted=r["record"].toObject();if(keyOf(accepted)!=key||!same(accepted,request)){finish("服务端修改确认无效");return;}
            auto shadow=state["shadow"].toObject();shadow[key]=accepted;state["shadow"]=shadow;
        }else{
            if(!r["conflict"].isObject()){finish("服务端冲突响应无效");return;}
            if(same(request,r["conflict"].toObject())){auto shadow=state["shadow"].toObject();shadow[key]=r["conflict"];state["shadow"]=shadow;}
            else {auto conflicts=state["conflicts"].toObject();conflicts[key]=QJsonObject{{"remote",r["conflict"]},{"local",snapshot()[key]}};state["conflicts"]=conflicts;}
        }
        auto pending=state["pending"].toObject();pending.remove(key);state["pending"]=pending;
        if(!save()){finish("状态保存失败");return;}next();
    },true);
}
void CloudSync::pull(){
    const auto cursor=state["cursor"].toInteger();
    CloudConnection::instance()->request("/v1/changes?cursor="+QString::number(cursor),{},[this,cursor,guard=QPointer<CloudSync>(this)](const QJsonObject &r,const QString &error){
        if(!guard)return;
        if(!error.isEmpty()){finish(error);return;}
        if(r["dataset"]!=state["dataset"]||!r["records"].isArray()||r["cursor"].toInteger()<cursor){finish("服务端增量响应无效");return;}
        capture();if(!healthy){finish("状态保存失败");return;}
        for(auto value:r["records"].toArray()){
            const auto remote=value.toObject();const auto key=keyOf(remote);auto shadow=state["shadow"].toObject();
            if(remote["rev"].toInteger()<=shadow[key].toObject()["rev"].toInteger())continue;
            auto conflicts=state["conflicts"].toObject();auto pending=state["pending"].toObject();
            if(conflicts.contains(key)||(pending.contains(key)&&!same(pending[key].toObject(),remote))){
                conflicts[key]=QJsonObject{{"remote",remote},{"local",snapshot()[key]}};pending.remove(key);state["pending"]=pending;state["conflicts"]=conflicts;
                if(!save()){finish("状态保存失败");return;}continue;
            }
            pending.remove(key);state["pending"]=pending;
            state["journal"]=QJsonObject{{"record",remote},{"before",snapshot()[key]},{"cursor",state["cursor"]}};
            if(!save()||!replay()){healthy=false;finish("云端数据写入失败，应用日志已保留");return;}
        }
        state["cursor"]=r["cursor"];if(!save()){finish("状态保存失败");return;}
        if(r["more"].toBool())pull();else finish();
    });
}
void CloudSync::finish(const QString &error){
    busy=false;
    if(!error.isEmpty()){emit status(error);return;}
    const int count=state["conflicts"].toObject().size();
    emit status(count?QString("有 %1 项同步冲突，请在设置中处理").arg(count):"云端同步完成 "+QDateTime::currentDateTime().toString("HH:mm:ss"));
    emit synced();
}
bool CloudSync::resolveConflict(const QString &key,bool keepLocal){
    auto conflicts=state["conflicts"].toObject();if(!conflicts.contains(key))return false;
    const auto remote=conflicts[key].toObject()["remote"].toObject();const auto local=snapshot()[key].toObject();
    auto archive=state["resolved"].toArray();archive.append(QJsonObject{{"key",key},{"local",local},{"remote",remote},{"chosen",keepLocal?"local":"remote"},{"time",QDateTime::currentDateTime().toString(Qt::ISODate)}});state["resolved"]=archive;
    if(!save())return false;
    if(!keepLocal){state["journal"]=QJsonObject{{"record",remote},{"before",local},{"cursor",state["cursor"]}};if(!save()||!replay()){healthy=false;emit status("冲突版本应用失败，日志已保留");return false;}}
    else{auto shadow=state["shadow"].toObject();shadow[key]=remote;state["shadow"]=shadow;}
    conflicts.remove(key);state["conflicts"]=conflicts;return save();
}
void CloudSync::showConflicts(){
    if(busy){emit status("同步中，请稍后处理冲突");return;}
    if(!healthy){emit status("请先处理同步状态保存错误");return;}
    QScopedValueRollback<bool> resolvingGuard(resolving,true);
    auto conflicts=state["conflicts"].toObject();if(conflicts.isEmpty()){emit status("没有待处理的同步冲突");return;}
    for(const auto &key:conflicts.keys()){
        const auto conflict=conflicts[key].toObject(),remote=conflict["remote"].toObject();
        const auto local=snapshot()[key].toObject();
        QMessageBox dialog(QMessageBox::Question,"选择同步版本",key+"\n本机和云端有不同修改。请选择保留的版本；双方内容会留在同步历史中。",QMessageBox::NoButton,QApplication::activeWindow());
        auto localButton=dialog.addButton("保留本机",QMessageBox::AcceptRole);auto remoteButton=dialog.addButton("使用云端",QMessageBox::DestructiveRole);dialog.addButton("稍后处理",QMessageBox::RejectRole);
        auto visible=[](QJsonObject value){auto p=value["payload"].toObject();if(value["kind"]=="settings"){p.remove("accounts");p.remove("weatherKey");value["payload"]=p;}return QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Indented));};
        dialog.setDetailedText("本机：\n"+visible(local)+"\n云端：\n"+visible(remote));dialog.exec();
        if(dialog.clickedButton()!=localButton&&dialog.clickedButton()!=remoteButton)break;
        if(!resolveConflict(key,dialog.clickedButton()==localButton))return;
    }
    capture();debounce.start();
}
