#include "ddldepartment.h"
#include "tagrules.h"
#include "recurrence.h"
#include <QUuid>
#include <QTimeZone>
#include <algorithm>
#include <QSet>
namespace {
QTimeZone taskZone(const QJsonObject &t){const QTimeZone zone(t["zone"].toString().toUtf8());return zone.isValid()?zone:QTimeZone::systemTimeZone();}
bool scheduled(const QJsonObject &t,const QDate &date){
    return Recurrence::scheduled(t,date);
}
TaskOccurrence occurrence(const QJsonObject &t,const QString &key,QDateTime due){
    const auto v=t["instances"].toObject()[key].toObject();TaskOccurrence o;
    o.taskId=t["id"].toString();o.key=key;o.title=v["title"].toString(t["title"].toString());o.note=v["note"].toString(t["note"].toString());
    o.tags=v["tags"].toString(t["tags"].toString());o.repeat=t["repeat"].toString();o.done=v["done"].toBool();
    o.deleted=t["deleted"].toBool()||v["deleted"].toBool()||t["excluded"].toArray().contains(key);o.paused=t["paused"].toBool();
    o.priority=t["priority"].toInt();o.snooze=v["snooze"].toVariant().toLongLong();
    o.due=v.contains("due")?QDateTime::fromString(v["due"].toString(),Qt::ISODate):due;return o;
}
bool earlier(const TaskOccurrence &a,const TaskOccurrence &b){
    if(a.due.isValid()!=b.due.isValid())return a.due.isValid();
    if(a.due!=b.due)return a.due<b.due;
    return a.taskId==b.taskId?a.key<b.key:a.taskId<b.taskId;
}
}
DDLDepartment::DDLDepartment(TaskRepository *repository,QObject *parent):QObject(parent),repo(repository) {}
TaskDaySummary DDLDepartment::daySummary(const QDate &today) const {
    TaskDaySummary result;result.available=repo->ready()&&today.isValid();if(!result.available)return result;
    for(const auto &o:occurrences(today,today.addDays(1))){
        if(o.done||o.deleted||o.paused||!o.due.isValid())continue;
        const auto date=o.due.toLocalTime().date();
        if(date==today)++result.today;else if(date==today.addDays(1))++result.tomorrow;
    }
    // Past recurring instances are omitted from the existing agenda. Count the
    // outstanding single tasks retained there, without expanding years of history.
    for(const auto &t:repo->all()){
        if(t["repeat"]!="none"||!t["hasDue"].toBool())continue;
        const auto start=QDateTime::fromString(t["due"].toString(),Qt::ISODate);
        const auto o=occurrence(t,start.toString(Qt::ISODate),start);
        if(!o.done&&!o.deleted&&!o.paused&&o.due.isValid()&&o.due.toLocalTime().date()<today)++result.overdue;
    }
    return result;
}
QJsonObject DDLDepartment::task(const QString &id) const {
    return repo->find(id);
}
bool DDLDepartment::save(QJsonObject t) {
    message=TagRules::selectionError(t["tags"].toString());if(!message.isEmpty())return false;
    if(t["title"].toString().trimmed().isEmpty()) { message="请输入标题"; return false; }
    if(t["hasDue"].toBool()&&!QDateTime::fromString(t["due"].toString(),Qt::ISODate).isValid()){message="时间无效";return false;}
    if(!QStringList{"none","daily","weekly","monthly"}.contains(t["repeat"].toString())){message="循环规则无效";return false;}
    if(t["id"].toString().isEmpty()) t["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto previous=task(t["id"].toString());
    if(!t.contains("excluded")&&previous.contains("excluded"))t["excluded"]=previous["excluded"];
    if(!previous.isEmpty()&&previous["repeat"]=="none"&&t["repeat"]=="none"){
        const auto oldKey=previous["hasDue"].toBool()?QDateTime::fromString(previous["due"].toString(),Qt::ISODate).toString(Qt::ISODate):QString("undated");
        const auto newKey=t["hasDue"].toBool()?QDateTime::fromString(t["due"].toString(),Qt::ISODate).toString(Qt::ISODate):QString("undated");
        if(oldKey!=newKey){
            auto instances=t.contains("instances")?t["instances"].toObject():previous["instances"].toObject();
            if(instances.contains(oldKey)){auto state=instances.take(oldKey).toObject();state.remove("due");instances[newKey]=state;t["instances"]=instances;}
        }
    }
    t["revision"]=previous["revision"].toInt()+1;
    if(t["repeat"].toString()!="none" && !t["hasDue"].toBool()) { message="循环事务需要时间"; return false; }
    if(t["repeat"]=="weekly") {bool valid=false;for(const auto &day:t["weekdays"].toArray())if(day.toInt()>=1&&day.toInt()<=7)valid=true;if(!valid){message="每周循环至少选择一天";return false;}}
    const auto remainingCount=t.take("remainingCount");
    if(!remainingCount.isUndefined()&&t["countLimited"].toBool()&&t["repeat"]!="none"){
        const int requested=remainingCount.toInt();
        if(requested<1||requested>1000000){message="剩余次数须为 1–1000000 的正数";return false;}
        t["countLimit"]=requested;t.remove("repeatLast");
        if(!previous.isEmpty()){
            // Find the shortest allocation with the requested number of unconsumed
            // slots. An early completion beyond the new tail must not add a slot.
            const auto now=QDateTime::currentDateTime();
            int low=requested,high=qMin(1000000,Recurrence::ordinal(t,now.toTimeZone(taskZone(t)).date())+requested+t["instances"].toObject().size()+t["excluded"].toArray().size()+t["consumed"].toArray().size()+1);
            while(low<high){const int mid=low+(high-low)/2;t["countLimit"]=mid;t["repeatLast"]=Recurrence::nth(t,mid).toString(Qt::ISODate);if(Recurrence::remaining(t,now)>=requested)high=mid;else low=mid+1;}
            t["countLimit"]=low;t["repeatLast"]=Recurrence::nth(t,low).toString(Qt::ISODate);
            if(Recurrence::remaining(t,now)!=requested){message="循环有效日期不足，请调整循环规则或次数";return false;}
        }
    }
    t.remove("repeatLast");message=Recurrence::validate(t);if(!message.isEmpty())return false;
    if(t["countLimited"].toBool())t["repeatLast"]=Recurrence::nth(t,t["countLimit"].toInt()).toString(Qt::ISODate);
    if(!repo->save(t)) { message=repo->lastError(); return false; }
    emit changed(); return true;
}
bool DDLDepartment::setOccurrence(const QString &id,const QString &key,const QJsonObject &values) {
    if(values.contains("tags")){message=TagRules::selectionError(values["tags"].toString());if(!message.isEmpty())return false;}
    auto t=task(id); if(t.isEmpty()) return false;
    if(t["excluded"].toArray().contains(key)){message="本次事务已永久移除";return false;}
    auto overrides=t["instances"].toObject(), item=overrides[key].toObject();
    if(t["countLimited"].toBool()&&values.contains("due")){
        const auto nominal=QDateTime(QDate::fromString(key,Qt::ISODate),Recurrence::start(t).time(),taskZone(t));
        const auto previous=occurrence(t,key,nominal);
        if(previous.done||previous.due<=QDateTime::currentDateTime()){
            auto consumed=t["consumed"].toArray();if(!consumed.contains(key))consumed.append(key);t["consumed"]=consumed;
        }
    }
    for(auto it=values.begin();it!=values.end();++it) item[it.key()]=it.value();
    if(values.contains("due"))item["revision"]=item["revision"].toInt()+1;
    overrides[key]=item; t["instances"]=overrides;
    // Instance changes must not invalidate unrelated reminders in the series.
    if(!repo->save(t)) { message=repo->lastError(); return false; }
    if(!purgeExpired(QDateTime::currentDateTime()))return false;
    emit changed(); return true;
}
bool DDLDepartment::setDeleted(const QString &id,bool deleted) {
    if(!deleted){message="已永久删除的事务不能恢复";return false;}
    if(!repo->applyChanges({},{id})){message=repo->lastError();return false;}emit changed();return true;
}
bool DDLDepartment::removeOccurrence(const TaskOccurrence &o){
    if(o.repeat=="none")return setDeleted(o.taskId,true);
    auto t=task(o.taskId);if(t.isEmpty())return false;
    auto excluded=t["excluded"].toArray();if(!excluded.contains(o.key))excluded.append(o.key);t["excluded"]=excluded;
    auto instances=t["instances"].toObject();instances.remove(o.key);t["instances"]=instances;
    if(!repo->save(t)){message=repo->lastError();return false;}emit changed();return true;
}
bool DDLDepartment::purgeExpired(const QDateTime &now){
    QList<QJsonObject> updates;QStringList removed;
    for(auto t:repo->all()){
        if(t["deleted"].toBool()){removed.append(t["id"].toString());continue;}
        const auto start=QDateTime::fromString(t["due"].toString(),Qt::ISODate);
        if(t["repeat"]=="none"){
            const auto o=occurrence(t,t["hasDue"].toBool()?start.toString(Qt::ISODate):"undated",t["hasDue"].toBool()?start:QDateTime());
            if(o.deleted||(o.done&&o.due.isValid()&&o.due<=now))removed.append(o.taskId);
            continue;
        }
        auto instances=t["instances"].toObject();auto excluded=t["excluded"].toArray();bool changed=false;
        for(auto it=instances.begin();it!=instances.end();){
            const auto date=QDate::fromString(it.key(),Qt::ISODate);
            const auto o=occurrence(t,it.key(),QDateTime(date,start.toTimeZone(taskZone(t)).time(),taskZone(t)));
            if(o.deleted||(o.done&&o.due.isValid()&&o.due<=now)){
                if(!excluded.contains(it.key()))excluded.append(it.key());it=instances.erase(it);changed=true;
            }else ++it;
        }
        if(changed){t["instances"]=instances;t["excluded"]=excluded;updates.append(t);}
    }
    if(updates.isEmpty()&&removed.isEmpty())return true;
    if(!repo->applyChanges(updates,removed)){message=repo->lastError();return false;}
    emit changed();return true;
}
bool DDLDepartment::setPaused(const QString &id,bool paused) {
    auto t=task(id);if(t.isEmpty())return false;t["paused"]=paused;
    if(!repo->save(t)){message=repo->lastError();return false;}emit changed();return true;
}
bool DDLDepartment::complete(const TaskOccurrence &o) {
    if(o.done){message="事务已完成";return false;}
    return setOccurrence(o.taskId,o.key,{{"done",true}});
}
bool DDLDepartment::snooze(const TaskOccurrence &o,int minutes) {
    if(o.done||!o.due.isValid()){message="只能稍后提醒有时间的未完成事务";return false;}
    return setOccurrence(o.taskId,o.key,{{"snooze",QDateTime::currentSecsSinceEpoch()+minutes*60},{"snoozeVersion",QUuid::createUuid().toString()}});
}
QList<TaskOccurrence> DDLDepartment::occurrences(const QDate &from,const QDate &to) const {
    QList<TaskOccurrence> result;
    for(const auto &t:repo->all()) {
        auto append=[&](const QString &key,QDateTime due) {
            const auto o=occurrence(t,key,due);if(!o.deleted)result.append(o);
        };
        if(!t["hasDue"].toBool()) { append("undated",{}); continue; }
        const auto start=QDateTime::fromString(t["due"].toString(),Qt::ISODate);
        if(!start.isValid()) continue;
        const auto repeat=t["repeat"].toString();
        if(repeat=="none") { append(start.toString(Qt::ISODate),start); continue; }
        const auto effective=taskZone(t);
        const auto local=start.toTimeZone(effective);
        QDate end=to;
        if(t["countLimited"].toBool())end=qMin(end,Recurrence::last(t));
        if(t.contains("until")) { auto d=QDate::fromString(t["until"].toString(),Qt::ISODate); if(d.isValid()&&d<end) end=d; }
        QSet<QString> generated;
        // Expand by one day for series whose timezone differs from the display timezone.
        for(auto day=qMax(from.addDays(-1),local.date());day<=end.addDays(1);day=day.addDays(1)) {
            if(!scheduled(t,day))continue;
            auto due=QDateTime(day,local.time(),effective);
            const auto key=day.toString(Qt::ISODate);generated.insert(key);append(key,due);
        }
        // A moved instance must remain visible/remindable even if its original date is outside this window.
        const auto overrides=t["instances"].toObject();
        for(auto it=overrides.begin();it!=overrides.end();++it){
            if(generated.contains(it.key()))continue;
            const auto original=QDate::fromString(it.key(),Qt::ISODate);
            const auto moved=QDateTime::fromString(it.value().toObject()["due"].toString(),Qt::ISODate);
            if(!scheduled(t,original))continue;
            if(moved.isValid()&&moved.toLocalTime().date()>=from&&moved.toLocalTime().date()<=to)append(it.key(),moved);
        }
    }
    // Only calendar/reminder ranges use this method; undated tasks are returned for callers to handle.
    result.erase(std::remove_if(result.begin(),result.end(),[&](const auto &o){return o.due.isValid()&&(o.due.toLocalTime().date()<from||o.due.toLocalTime().date()>to);}),result.end());
    std::stable_sort(result.begin(),result.end(),earlier);
    return result;
}
QList<TaskOccurrence> DDLDepartment::listOccurrences(const QDateTime &now,bool showCompleted) const {
    QList<TaskOccurrence> result;
    for(const auto &t:repo->all()){
        if(t["deleted"].toBool())continue;
        const auto start=QDateTime::fromString(t["due"].toString(),Qt::ISODate);
        if(t["repeat"]=="none"){
            const auto o=occurrence(t,t["hasDue"].toBool()?start.toString(Qt::ISODate):"undated",t["hasDue"].toBool()?start:QDateTime());
            if(!o.deleted&&(!o.done||showCompleted)&&!(o.done&&o.due.isValid()&&o.due<=now))result.append(o);
            continue;
        }
        if(!start.isValid())continue;
        if(t["repeat"]=="weekly"){bool valid=false;for(const auto &day:t["weekdays"].toArray())if(day.toInt()>=1&&day.toInt()<=7)valid=true;if(!valid)continue;}
        TaskOccurrence best;const auto zone=taskZone(t);const auto local=start.toTimeZone(zone);
        auto consider=[&](const TaskOccurrence &o){if(!o.deleted&&(!o.done||showCompleted)&&o.due.isValid()&&o.due>now&&(best.taskId.isEmpty()||earlier(o,best)))best=o;};
        // Moved instances can precede the next date generated by the regular schedule.
        const auto instances=t["instances"].toObject();
        for(auto it=instances.begin();it!=instances.end();++it){const auto date=QDate::fromString(it.key(),Qt::ISODate);if(scheduled(t,date))consider(occurrence(t,it.key(),QDateTime(date,local.time(),zone)));}
        auto until=QDate::fromString(t["until"].toString(),Qt::ISODate);
        if(t["countLimited"].toBool()){const auto end=Recurrence::last(t);if(!end.isValid())continue;until=until.isValid()?qMin(until,end):end;}
        for(auto date=qMax(local.date(),now.toTimeZone(zone).date());date.isValid()&&(!until.isValid()||date<=until);date=date.addDays(1)){
            const auto nominal=QDateTime(date,local.time(),zone);
            if(!best.taskId.isEmpty()&&nominal>best.due)break;
            if(!scheduled(t,date))continue;
            const auto o=occurrence(t,date.toString(Qt::ISODate),nominal);consider(o);
            if(!o.deleted&&(!o.done||showCompleted)&&o.due>now&&!instances.contains(o.key))break;
        }
        if(!best.taskId.isEmpty())result.append(best);
    }
    std::stable_sort(result.begin(),result.end(),earlier);return result;
}
void DDLDepartment::checkReminders(qint64 now) {
    const auto time=QDateTime::fromSecsSinceEpoch(now);
    if(!purgeExpired(time))return;
    const int hour=time.time().hour();
    if(quietStart!=quietEnd && (quietStart<quietEnd ? hour>=quietStart&&hour<quietEnd : hour>=quietStart||hour<quietEnd)) return;
    auto items=occurrences(time.date().addDays(-1),time.date().addDays(8));
    QSet<QString> known;for(const auto &o:items)known.insert(o.taskId+"/"+o.key);
    // A snoozed item may have an old due date outside the calendar/reminder expansion window.
    for(const auto &t:repo->all()){
        const auto states=t["instances"].toObject();const auto start=QDateTime::fromString(t["due"].toString(),Qt::ISODate);
        for(auto it=states.begin();it!=states.end();++it){
            if(it.value().toObject()["snooze"].toVariant().toLongLong()<=0||known.contains(t["id"].toString()+"/"+it.key()))continue;
            if(t["repeat"]!="none"&&!scheduled(t,QDate::fromString(it.key(),Qt::ISODate)))continue;
            const auto nominal=t["repeat"]=="none"?start:QDateTime(QDate::fromString(it.key(),Qt::ISODate),start.toTimeZone(taskZone(t)).time(),taskZone(t));
            items.append(occurrence(t,it.key(),nominal));
        }
    }
    QStringList notices;
    for(const auto &o:items) {
        if(o.done||o.deleted||o.paused||!o.due.isValid()) continue;
        auto t=task(o.taskId);
        const auto state=t["instances"].toObject()[o.key].toObject();
        QString suffix; qint64 trigger=0;
        if(o.snooze>0) {
            trigger=o.snooze;
            suffix="snooze-"+state["snoozeVersion"].toString();
        } else {
            if(!t["remind"].toBool(true)) continue;
            const qint64 due=o.due.toSecsSinceEpoch();
            if(due<=now) { trigger=due; suffix="due"; }
            else { trigger=due-t["advanceMinutes"].toInt(30)*60; suffix="advance"; }
        }
        if(trigger>now || trigger<now-86400) continue;
        const auto event=o.taskId+"/"+o.key+"/"+QString::number(t["revision"].toInt())+"/"+QString::number(state["revision"].toInt())+"/"+suffix;
        if(repo->recordReminder(event)) {
            notices.append(o.title);
            if(o.snooze>0) {
                if(o.due.toSecsSinceEpoch()<=now){
                    const auto dueEvent=o.taskId+"/"+o.key+"/"+QString::number(t["revision"].toInt())+"/"+QString::number(state["revision"].toInt())+"/due";
                    repo->recordReminder(dueEvent);
                }
                setOccurrence(o.taskId,o.key,{{"snooze",0}});
            }
        }
    }
    if(!notices.isEmpty()) emit reminder(notices.mid(0,8).join("\n")+(notices.size()>8?"\n……":""));
}

bool DDLDepartment::splitSeries(const QString &id,const QDate &from,QJsonObject replacement){
    message=TagRules::selectionError(replacement["tags"].toString());if(!message.isEmpty())return false;
    auto previous=task(id);
    if(previous.isEmpty()||replacement["title"].toString().trimmed().isEmpty()||
       !replacement["hasDue"].toBool()||!QDateTime::fromString(replacement["due"].toString(),Qt::ISODate).isValid()){
        message="循环事务标题、时间不可为空";return false;
    }
    if(!QStringList{"none","daily","weekly","monthly"}.contains(replacement["repeat"].toString())){message="循环规则无效";return false;}
    if(replacement["repeat"]=="weekly"&&replacement["weekdays"].toArray().isEmpty()){message="请选择每周日期";return false;}
    if(replacement.contains("remainingCount"))replacement["countLimit"]=replacement.take("remainingCount");
    replacement.remove("repeatLast");replacement.remove("consumed");replacement.remove("excluded");replacement.remove("instances");replacement.remove("until");
    message=Recurrence::validate(replacement);if(!message.isEmpty())return false;
    if(replacement["countLimited"].toBool())replacement["repeatLast"]=Recurrence::nth(replacement,replacement["countLimit"].toInt()).toString(Qt::ISODate);
    previous["until"]=from.addDays(-1).toString(Qt::ISODate);
    replacement["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);replacement["revision"]=1;
    // End date and new series commit atomically.
    if(!repo->saveMany({previous,replacement})){message=repo->lastError();return false;}
    emit changed();return true;
}
