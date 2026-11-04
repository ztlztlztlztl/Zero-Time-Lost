#include "ddldepartment.h"
#include "tagrules.h"
#include "recurrence.h"
#include "calendarquery.h"
#include <QUuid>
#include <QTimeZone>
#include <algorithm>
#include <QSet>
#include <QTimer>
namespace {
QTimeZone taskZone(const QJsonObject &t){const QTimeZone zone(t["zone"].toString().toUtf8());return zone.isValid()?zone:QTimeZone::systemTimeZone();}
bool scheduled(const QJsonObject &t,const QDate &date){
    return Recurrence::scheduled(t,date);
}
using CalendarRules::occurrence;
using CalendarRules::earlier;
}
DDLDepartment::DDLDepartment(TaskRepository *repository,QObject *parent):QObject(parent),repo(repository) {
    for(const auto &t:repo->all())lastCreatedAt=qMax(lastCreatedAt,t["createdAt"].toVariant().toLongLong());
    boundaryTimer=new QTimer(this);boundaryTimer->setSingleShot(true);boundaryTimer->setTimerType(Qt::PreciseTimer);
    connect(boundaryTimer,&QTimer::timeout,this,[this]{emit temporalChanged();scheduleBoundary();});
    connect(this,&DDLDepartment::changed,this,&DDLDepartment::scheduleBoundary);scheduleBoundary();
}
void DDLDepartment::scheduleBoundary(){
    const auto now=QDateTime::currentDateTime();auto next=QDateTime(now.date().addDays(1),QTime(0,0));
    for(const auto &o:occurrences(now.date(),now.date().addDays(1)))for(const auto &time:{o.start,o.due})if(time.isValid()&&time>now&&time<next)next=time;
    boundaryTimer->start(int(qBound<qint64>(1LL,now.msecsTo(next)+1,86400001LL)));
}
QStringList DDLDepartment::labelsToday(const QDateTime &now) const {
    QStringList result;for(const auto &o:occurrences(now.date(),now.date()))if(o.isLabel())result.append(o.title);return result;
}
QStringList DDLDepartment::currentIntervals(const QDateTime &now) const {
    QStringList result;for(const auto &o:occurrences(now.date(),now.date()))if(o.isInterval()&&!o.paused&&o.issue.isEmpty()&&o.start<=now&&now<o.due)result.append(o.title);return result;
}
QString DDLDepartment::validateType(const QJsonObject &t){
    const auto type=t["type"].toString("ddl");
    if(!QStringList{"ddl","label","interval"}.contains(type))return "事务类别无效";
    if(type=="label"&&(!t["hasDue"].toBool()||t["repeat"]!="none"))return "全天标签需要日期且不能循环";
    if(type=="interval"){
        const auto start=QDateTime::fromString(t["start"].toString(),Qt::ISODate),end=QDateTime::fromString(t["due"].toString(),Qt::ISODate);
        if(!t["hasDue"].toBool()||!start.isValid()||!end.isValid()||start>=end||start.toLocalTime().date()!=end.toLocalTime().date())return "区间事务必须在同一天，且开始时间早于结束时间";
    }
    return {};
}
TaskDaySummary DDLDepartment::daySummary(const QDate &today) const {
    TaskDaySummary result;result.available=repo->ready()&&today.isValid();if(!result.available)return result;
    for(const auto &o:occurrences(today,today.addDays(1))){
        if(!o.isDDL()||o.done||o.deleted||o.paused||!o.due.isValid())continue;
        const auto date=o.due.toLocalTime().date();
        if(date==today)++result.today;else if(date==today.addDays(1))++result.tomorrow;
    }
    // Past recurring instances are omitted from the existing agenda. Count the
    // outstanding single tasks retained there, without expanding years of history.
    for(const auto &t:repo->all()){
        if(t["type"].toString("ddl")!="ddl"||t["repeat"]!="none"||!t["hasDue"].toBool())continue;
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
    if(!t.contains("createdAt")){lastCreatedAt=qMax(lastCreatedAt+1,QDateTime::currentMSecsSinceEpoch());t["createdAt"]=lastCreatedAt;}
    if(!t.contains("zone"))t["zone"]=QString::fromUtf8(QTimeZone::systemTimeZoneId());
    if(t["type"]=="label")t["labelDate"]=QDateTime::fromString(t["due"].toString(),Qt::ISODate).date().toString(Qt::ISODate);
    message=validateType(t);if(!message.isEmpty())return false;
    if(t["type"]=="label"){t["note"]="";t["tags"]="";t["priority"]=0;}
    if(t["type"].toString("ddl")!="ddl"){t["remind"]=false;}

    message=TagRules::selectionError(t["tags"].toString());if(!message.isEmpty())return false;
    if(t["title"].toString().trimmed().isEmpty()) { message="请输入标题"; return false; }
    if(t["hasDue"].toBool()&&!QDateTime::fromString(t["due"].toString(),Qt::ISODate).isValid()){message="时间无效";return false;}
    if(!QStringList{"none","daily","weekly","monthly"}.contains(t["repeat"].toString())){message="循环规则无效";return false;}
    if(t["id"].toString().isEmpty()) t["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto previous=task(t["id"].toString());
    if(t["repeat"]=="none")t["identity"]=previous["identity"].toString(t["id"].toString()+"/single");
    if(!previous.isEmpty()&&previous["type"].toString("ddl")!=t["type"].toString("ddl")){message="编辑时不能更改事务类别";return false;}
    if(!t.contains("excluded")&&previous.contains("excluded"))t["excluded"]=previous["excluded"];
    if(!previous.isEmpty()&&previous["repeat"]=="none"&&t["repeat"]=="none"){
        const auto oldKey=previous["hasDue"].toBool()?QDateTime::fromString(previous["due"].toString(),Qt::ISODate).toString(Qt::ISODate):QString("undated");
        const auto newKey=t["hasDue"].toBool()?QDateTime::fromString(t["due"].toString(),Qt::ISODate).toString(Qt::ISODate):QString("undated");
        if(oldKey!=newKey){
            auto instances=t.contains("instances")?t["instances"].toObject():previous["instances"].toObject();
            if(instances.contains(oldKey)){auto state=instances.take(oldKey).toObject();state.remove("due");instances[newKey]=state;t["instances"]=instances;}
        }
    }
    bool timingChanged=previous.isEmpty();for(const auto &field:{"due","start","repeat","weekdays","monthdays","remind","advanceMinutes"})if(t.value(field)!=previous.value(field))timingChanged=true;
    t["revision"]=previous["revision"].toInt()+(timingChanged?1:0);
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
    auto t=task(id); if(t.isEmpty()) return false;if(t["repeat"]!="none"&&!Recurrence::scheduled(t,QDate::fromString(key,Qt::ISODate))){message="本次已不在原循环中，请重新打开";return false;}const auto originalDue=resolve(id,key).due;
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
    if(t["type"]=="interval"){
        auto effective=t;auto merged=overrides;merged[key]=item;effective["instances"]=merged;
        const auto date=QDate::fromString(key,Qt::ISODate);const auto nominal=date.isValid()?QDateTime(date,Recurrence::start(t).time(),taskZone(t)):QDateTime::fromString(t["due"].toString(),Qt::ISODate);
        const auto updated=occurrence(effective,key,nominal);effective["start"]=updated.start.toString(Qt::ISODate);effective["due"]=updated.due.toString(Qt::ISODate);
        message=validateType(effective);if(!message.isEmpty())return false;
    }
    if(t["type"].toString("ddl")!="ddl"&&(values.contains("done")||values.contains("snooze"))){message="此类别没有完成或提醒操作";return false;}
    if(values.contains("due")&&QDateTime::fromString(values["due"].toString(),Qt::ISODate)!=originalDue)item["revision"]=item["revision"].toInt()+1;
    overrides[key]=item; t["instances"]=overrides;
    // Instance changes must not invalidate unrelated reminders in the series.
    if(!repo->save(t)) { message=repo->lastError(); return false; }
    if(!purgeExpired(QDateTime::currentDateTime()))return false;
    emit changed(); return true;
}
bool DDLDepartment::setDeleted(const QString &id,bool deleted) {
    if(!deleted){message="已永久删除的事务不能恢复";return false;}
    const auto before=task(id);if(before.isEmpty()){message="事务已不存在";return false;}
    if(!repo->applyChanges({},{id})){message=repo->lastError();return false;}remember({before},{},{id},"删除循环或事务");emit changed();return true;
}
bool DDLDepartment::removeOccurrence(const TaskOccurrence &o){
    if(o.repeat=="none")return setDeleted(o.taskId,true);
    auto t=task(o.taskId);if(t.isEmpty())return false;const auto before=t;
    auto excluded=t["excluded"].toArray();if(!excluded.contains(o.key))excluded.append(o.key);t["excluded"]=excluded;
    auto instances=t["instances"].toObject();instances.remove(o.key);t["instances"]=instances;
    if(!repo->save(t)){message=repo->lastError();return false;}remember({before},{t},{},"删除本次事务");emit changed();return true;
}
bool DDLDepartment::purgeExpired(const QDateTime &now){
    Q_UNUSED(now); // Completed records are retained for calendar history.
    QList<QJsonObject> updates;QStringList removed;
    for(auto t:repo->all()){
        if(t["deleted"].toBool()){removed.append(t["id"].toString());continue;}
        const auto start=QDateTime::fromString(t["due"].toString(),Qt::ISODate);
        if(t["repeat"]=="none"){
            const auto o=occurrence(t,t["hasDue"].toBool()?start.toString(Qt::ISODate):"undated",t["hasDue"].toBool()?start:QDateTime());
            if(o.deleted)removed.append(o.taskId);
            continue;
        }
        auto instances=t["instances"].toObject();auto excluded=t["excluded"].toArray();bool changed=false;
        for(auto it=instances.begin();it!=instances.end();){
            const auto date=QDate::fromString(it.key(),Qt::ISODate);
            const auto o=occurrence(t,it.key(),QDateTime(date,start.toTimeZone(taskZone(t)).time(),taskZone(t)));
            if(o.deleted){
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
    auto t=task(id);if(t.isEmpty())return false;if(t["paused"].toBool()==paused)return true;
    auto windows=t["pauseWindows"].toArray();if(paused)windows.append(QJsonObject{{"from",QDateTime::currentSecsSinceEpoch()},{"to",0}});else if(!windows.isEmpty()){auto window=windows.last().toObject();window["to"]=QDateTime::currentSecsSinceEpoch();windows[windows.size()-1]=window;}t["pauseWindows"]=windows;t["paused"]=paused;
    if(!repo->save(t)){message=repo->lastError();return false;}emit changed();return true;
}
bool DDLDepartment::complete(const TaskOccurrence &o) {
    if(!o.isDDL()||o.done||resolve(o.taskId,o.key).done){message="只能完成未完成的 DDL";return false;}
    const auto before=task(o.taskId);
    if(!setOccurrence(o.taskId,o.key,{{"done",true}}))return false;
    remember({before},{task(o.taskId)},{},"完成事务");return true;
}
bool DDLDepartment::snooze(const TaskOccurrence &o,int minutes) {
    if(!o.isDDL()||o.done||!o.due.isValid()){message="只能稍后提醒有时间的未完成事务";return false;}
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
        if(t["deleted"].toBool()||t["paused"].toBool())continue;
        const auto start=QDateTime::fromString(t["due"].toString(),Qt::ISODate);
        if(t["repeat"]=="none"){
            const auto o=occurrence(t,t["hasDue"].toBool()?start.toString(Qt::ISODate):"undated",t["hasDue"].toBool()?start:QDateTime());
            if(CalendarRules::agendaVisible(o,now,showCompleted))result.append(o);
            continue;
        }
        if(!start.isValid())continue;
        if(t["repeat"]=="weekly"){bool valid=false;for(const auto &day:t["weekdays"].toArray())if(day.toInt()>=1&&day.toInt()<=7)valid=true;if(!valid)continue;}
        TaskOccurrence best;QSet<QString> ongoing;const auto zone=taskZone(t);const auto local=start.toTimeZone(zone);
        auto consider=[&](const TaskOccurrence &o){
            if(!CalendarRules::agendaVisible(o,now,showCompleted)||!o.due.isValid()||o.due<=now)return;
            if(o.isInterval()&&o.start<=now){if(!ongoing.contains(o.identity())){result.append(o);ongoing.insert(o.identity());}return;}
            if(best.taskId.isEmpty()||earlier(o,best))best=o;
        };
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
            if(!o.deleted&&(!o.done||showCompleted)&&o.due>now&&!instances.contains(o.key)&&(!o.isInterval()||o.start>now))break;
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
        if(!o.isDDL()||o.done||o.deleted||o.paused||!o.due.isValid()) continue;
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
        bool suppressed=false;for(const auto &entry:t["pauseWindows"].toArray()){const auto window=entry.toObject();const auto from=window["from"].toVariant().toLongLong(),to=window["to"].toVariant().toLongLong();if(trigger>=from&&(!to||trigger<to)){suppressed=true;break;}}if(suppressed)continue;
        const auto event=o.identity()+"/"+QString::number(t["revision"].toInt())+"/"+QString::number(state["revision"].toInt())+"/"+suffix;
        if(repo->recordReminder(event)) {
            notices.append(o.title);
            if(o.snooze>0) {
                if(o.due.toSecsSinceEpoch()<=now){
                    const auto dueEvent=o.identity()+"/"+QString::number(t["revision"].toInt())+"/"+QString::number(state["revision"].toInt())+"/due";
                    repo->recordReminder(dueEvent);
                }
                setOccurrence(o.taskId,o.key,{{"snooze",0}});
            }
        }
    }
    if(!notices.isEmpty()) emit reminder(notices.mid(0,8).join("\n")+(notices.size()>8?"\n……":""));
}

TaskOccurrence DDLDepartment::resolve(const QString &id,const QString &key) const {
    const auto t=task(id);if(t.isEmpty()||(t["repeat"]!="none"&&!Recurrence::scheduled(t,QDate::fromString(key,Qt::ISODate))))return {};
    const auto nominal=t["repeat"]=="none"?QDateTime::fromString(t["due"].toString(),Qt::ISODate):QDateTime(QDate::fromString(key,Qt::ISODate),Recurrence::start(t).time(),taskZone(t));
    return occurrence(t,key,nominal);
}
bool DDLDepartment::update(const QString &id,QJsonObject values){
    const auto original=task(id);if(original.isEmpty()){message="原事务已不存在；输入已保留，未新增事务";return false;}
    auto merged=original;
    for(const auto &field:{"type","title","note","tags","start","due","hasDue","zone","repeat","weekdays","monthdays","countLimited","remainingCount","priority","remind","advanceMinutes"})
        if(values.contains(field))merged[field]=values[field];
    merged["id"]=id;return save(merged);
}
int DDLDepartment::suffixRemaining(const QString &id,const QDate &from) const {
    auto t=task(id);const int total=Recurrence::remaining(t);t["until"]=from.addDays(-1).toString(Qt::ISODate);
    return qMax(0,total-Recurrence::remaining(t));
}
void DDLDepartment::resyncTime(){scheduleBoundary();emit temporalChanged();}
void DDLDepartment::remember(const QList<QJsonObject> &before,const QList<QJsonObject> &after,const QStringList &removed,const QString &text){
    const auto now=QDateTime::currentMSecsSinceEpoch();
    while(!undoEntries.isEmpty()&&(undoEntries.first().expires<now||undoEntries.size()>=10))undoEntries.removeFirst();
    undoEntries.append({before,after,removed,text,now+10000});
}
QString DDLDepartment::undoText() const {
    return !undoEntries.isEmpty()&&undoEntries.last().expires>=QDateTime::currentMSecsSinceEpoch()?undoEntries.last().text:QString();
}
bool DDLDepartment::undo(){
    if(undoText().isEmpty()){message="撤销时间已过";return false;}
    const auto entry=undoEntries.last();
    for(const auto &id:entry.removed)if(!task(id).isEmpty()){message="事务已发生新变化，不能覆盖";return false;}
    // Revert only fields changed by this action. Preserve later, unrelated edits.
    QList<QJsonObject> restored;
    for(const auto &before:entry.before){
        const auto id=before["id"].toString();auto current=task(id);QJsonObject after;
        for(const auto &t:entry.after)if(t["id"]==id)after=t;
        if(after.isEmpty()){restored.append(before);continue;}
        if(current.isEmpty()){message="事务已删除，不能撤销完成";return false;}
        for(const auto &field:{"until","repeat","countLimit","identityRoot"})if(current.value(field)!=after.value(field)){message="循环已重新安排，不能撤销此前的操作";return false;}
        auto state=current["instances"].toObject();const auto oldStates=before["instances"].toObject(),newStates=after["instances"].toObject();
        QSet<QString> keys;for(auto it=oldStates.begin();it!=oldStates.end();++it)keys.insert(it.key());for(auto it=newStates.begin();it!=newStates.end();++it)keys.insert(it.key());
        for(const auto &key:keys){const auto old=oldStates[key].toObject(),next=newStates[key].toObject();auto actual=state[key].toObject();
            QSet<QString> fields;for(auto it=old.begin();it!=old.end();++it)fields.insert(it.key());for(auto it=next.begin();it!=next.end();++it)fields.insert(it.key());
            for(const auto &field:fields)if(old[field]!=next[field]){if(actual.value(field)!=next.value(field)){message="相关状态已修改，无法撤销";return false;}if(old.contains(field))actual[field]=old[field];else actual.remove(field);}
            if(actual.isEmpty())state.remove(key);else state[key]=actual;
        }
        current["instances"]=state;
        if(before["excluded"]!=after["excluded"]){auto actual=current["excluded"].toArray();for(const auto &v:after["excluded"].toArray())if(!before["excluded"].toArray().contains(v)){for(int i=actual.size()-1;i>=0;--i)if(actual[i]==v)actual.removeAt(i);}current["excluded"]=actual;}
        restored.append(current);
    }
    if(!repo->saveMany(restored)){message=repo->lastError();return false;}
    undoEntries.removeLast();emit changed();return true;
}
QList<QJsonObject> DDLDepartment::prepareSplit(const QString &id,const QDate &from,QJsonObject replacement){
    auto previous=task(id);if(previous.isEmpty()||!from.isValid()||!Recurrence::scheduled(previous,from)){message="原循环或本次已不存在";return {};}
    message=TagRules::selectionError(replacement["tags"].toString());if(!message.isEmpty())return {};
    message=validateType(replacement);if(!message.isEmpty())return {};
    if(previous["type"].toString("ddl")!=replacement["type"].toString("ddl")){message="编辑时不能更改事务类别";return {};}
    if(replacement["title"].toString().trimmed().isEmpty()||!replacement["hasDue"].toBool()||!QDateTime::fromString(replacement["due"].toString(),Qt::ISODate).isValid()){message="标题和有效时间不可为空";return {};}
    if(!QStringList{"none","daily","weekly","monthly"}.contains(replacement["repeat"].toString())){message="循环规则无效";return {};}
    if(replacement["repeat"]=="weekly"&&Recurrence::days(replacement).isEmpty()){message="请选择至少一个星期";return {};}
    const auto selected=resolve(id,from.toString(Qt::ISODate));
    const auto proposed=QDateTime::fromString(replacement["due"].toString(),Qt::ISODate);
    // Values unchanged from a single-instance exception must not become the series template.
    const QJsonObject selectedFields{{"title",selected.title},{"note",selected.note},{"tags",selected.tags},{"priority",selected.priority}};
    for(auto it=selectedFields.begin();it!=selectedFields.end();++it)if(replacement[it.key()]==it.value())replacement[it.key()]=previous[it.key()];
    // Split membership follows the nominal sequence, even when the selected occurrence was moved.
    const auto zone=taskZone(previous);const auto localProposed=proposed.toTimeZone(zone),localSelected=selected.due.toTimeZone(zone);
    if(localProposed.date()==localSelected.date()){
        const auto endTime=localProposed.time()==localSelected.time()?Recurrence::start(previous).time():localProposed.time();
        replacement["due"]=QDateTime(from,endTime,zone).toString(Qt::ISODate);
        if(selected.isInterval()){
            const auto proposedStart=QDateTime::fromString(replacement["start"].toString(),Qt::ISODate).toTimeZone(zone);
            const auto baseStart=QDateTime::fromString(previous["start"].toString(),Qt::ISODate).toTimeZone(zone);
            replacement["start"]=QDateTime(from,proposedStart.time()==selected.start.toTimeZone(zone).time()?baseStart.time():proposedStart.time(),zone).toString(Qt::ISODate);
        }
    }
    message=validateType(replacement);if(!message.isEmpty())return {};
    const bool sameDates=previous["repeat"]==replacement["repeat"]&&(previous["repeat"]=="daily"||Recurrence::days(previous)==Recurrence::days(replacement))&&
        QDateTime::fromString(replacement["due"].toString(),Qt::ISODate).date()==from;
    const int defaultRemaining=suffixRemaining(id,from);
    int requested=replacement.value("remainingCount").toInt(defaultRemaining);
    if(replacement.contains("_remainingEdited")&&!replacement["_remainingEdited"].toBool())requested=defaultRemaining;
    if(requested<0||requested>1000000){message="剩余次数超出有效范围";return {};}
    const bool changedCount=replacement.contains("_remainingEdited")?replacement["_remainingEdited"].toBool():requested!=defaultRemaining;
    const int allocation=Recurrence::ordinal(previous,Recurrence::last(previous))-Recurrence::ordinal(previous,from.addDays(-1));
    replacement.remove("_remainingEdited");replacement.remove("remainingCount");
    replacement["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);const bool deliveryChanged=previous["repeat"]!=replacement["repeat"]||Recurrence::start(previous).time()!=Recurrence::start(replacement).time()||previous["remind"]!=replacement["remind"]||previous["advanceMinutes"]!=replacement["advanceMinutes"];
    replacement["revision"]=previous["revision"].toInt()+(deliveryChanged?1:0);replacement["createdAt"]=previous["createdAt"];
    replacement["originSeries"]=previous["originSeries"].toString(id);replacement["paused"]=previous["paused"];replacement["pauseWindows"]=previous["pauseWindows"];
    replacement.remove("templateHistory");if(!sameDates)replacement.remove("identityRoot");
    replacement.remove("repeatLast");replacement.remove("until");replacement.remove("consumed");replacement.remove("excluded");replacement.remove("instances");
    QList<QJsonObject> updates;QJsonObject states;QJsonArray excluded,consumed;int preservedRemaining=0;
    const auto oldStates=previous["instances"].toObject();
    QSet<QString> exceptions;for(auto it=oldStates.begin();it!=oldStates.end();++it)exceptions.insert(it.key());for(const auto &v:previous["excluded"].toArray())exceptions.insert(v.toString());for(const auto &v:previous["consumed"].toArray())exceptions.insert(v.toString());
    for(const auto &key:exceptions){const auto date=QDate::fromString(key,Qt::ISODate);if(date<from||!Recurrence::scheduled(previous,date))continue;
        auto state=oldStates[key].toObject();const auto old=occurrence(previous,key,QDateTime(date,Recurrence::start(previous).time(),taskZone(previous)));
        if(sameDates&&!changedCount){state["identity"]=old.identity();
            if(old.done||old.due<=QDateTime::currentDateTime()){state["priority"]=old.priority;state["title"]=old.title;state["note"]=old.note;state["tags"]=old.tags;state["due"]=old.due.toString(Qt::ISODate);if(old.isInterval())state["start"]=old.start.toString(Qt::ISODate);}
            states[key]=state;if(previous["excluded"].toArray().contains(key))excluded.append(key);if(previous["consumed"].toArray().contains(key))consumed.append(key);
        }else if(!old.deleted){
            if(Recurrence::raw(replacement,date)&&!excluded.contains(key))excluded.append(key);
            const auto actualKey=old.due.toTimeZone(taskZone(replacement)).date().toString(Qt::ISODate);if(Recurrence::raw(replacement,old.due.toTimeZone(taskZone(replacement)).date())&&!excluded.contains(actualKey))excluded.append(actualKey);
            auto retained=previous;retained["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);retained["originSeries"]=previous["originSeries"].toString(id);retained["repeat"]="none";retained["countLimited"]=false;
            retained["title"]=old.title;retained["note"]=old.note;retained["tags"]=old.tags;retained["due"]=old.due.toString(Qt::ISODate);retained["start"]=old.start.toString(Qt::ISODate);retained.remove("until");retained.remove("excluded");retained.remove("consumed");
            state["identity"]=old.identity();state.remove("snooze");state.remove("snoozeVersion");retained["instances"]=QJsonObject{{old.due.toString(Qt::ISODate),state}};updates.append(retained);
            if(!old.done&&old.due>QDateTime::currentDateTime()&&!previous["consumed"].toArray().contains(key))++preservedRemaining;
        }else if(Recurrence::raw(replacement,date)&&!excluded.contains(key))excluded.append(key);
    }
    if(sameDates){auto history=previous["templateHistory"].toArray();QJsonObject fields;for(const auto &field:{"title","note","tags","priority","due","start","zone"})fields[field]=previous[field];history.append(QJsonObject{{"from",from.toString(Qt::ISODate)},{"until",QDateTime::currentDateTime().toString(Qt::ISODate)},{"fields",fields}});replacement["templateHistory"]=history;}
    if(sameDates)replacement["identityRoot"]=previous["identityRoot"].toString(id);
    if(requested<preservedRemaining){message=QString("有 %1 项尚未消耗的单次修改需要保留，请先单独处理，或增加剩余次数").arg(preservedRemaining);return {};}
    replacement["instances"]=states;replacement["excluded"]=excluded;replacement["consumed"]=consumed;
    if(replacement["repeat"]!="none"&&!previous["countLimited"].toBool()&&!replacement["countLimited"].toBool()&&requested==0){
        replacement["countLimited"]=false;
    }else if(replacement["repeat"]!="none"){
        replacement["countLimited"]=true;
        if(sameDates&&!changedCount)replacement["countLimit"]=qMax(1,allocation);
        else {
            const int target=qMax(0,requested-preservedRemaining);
            if(target==0){replacement["countLimit"]=1;replacement["excluded"]=QJsonArray{QDateTime::fromString(replacement["due"].toString(),Qt::ISODate).date().toString(Qt::ISODate)};}
            else {int low=target,high=qMin(1000000,target+exceptions.size()*2+Recurrence::ordinal(replacement,QDate::currentDate())+1);
                while(low<high){const int mid=low+(high-low)/2;replacement["countLimit"]=mid;replacement["repeatLast"]=Recurrence::nth(replacement,mid).toString(Qt::ISODate);if(Recurrence::remaining(replacement)>=target)high=mid;else low=mid+1;}
                replacement["countLimit"]=low;replacement.remove("repeatLast");
            }
        }
        message=Recurrence::validate(replacement);if(!message.isEmpty())return {};
        replacement["repeatLast"]=Recurrence::nth(replacement,replacement["countLimit"].toInt()).toString(Qt::ISODate);
    }
    previous["until"]=from.addDays(-1).toString(Qt::ISODate);updates.prepend(previous);updates.append(replacement);
    return updates;
}

bool DDLDepartment::splitSeries(const QString &id,const QDate &from,QJsonObject replacement){
    const auto updates=prepareSplit(id,from,replacement);if(updates.isEmpty())return false;
    if(!repo->saveMany(updates)){message=repo->lastError();return false;}emit changed();return true;
}
QString DDLDepartment::previewSplit(const QString &id,const QDate &from,QJsonObject replacement){
    auto updates=prepareSplit(id,from,replacement);if(updates.isEmpty())return {};updates.removeFirst();
    int left=0,exceptions=0;QDate last;const auto now=QDateTime::currentDateTime();
    for(const auto &t:updates){const auto due=QDateTime::fromString(t["due"].toString(),Qt::ISODate);if(t["repeat"]=="none"){++exceptions;const auto o=CalendarRules::occurrence(t,due.toString(Qt::ISODate),due);if(!o.done&&!o.deleted&&o.due>now)++left;last=qMax(last,o.date());}else {left+=Recurrence::remaining(t);last=qMax(last,Recurrence::last(t));exceptions+=t["instances"].toObject().size();}}
    AdvancedFilter f;const auto first=CalendarQuery::page(updates,f,now.date(),QDate(9999,12,31),1,now,true);
    return QString("后续尚未消耗 %1 次\n首个有效日期：%2\n末次安排日期：%3\n保留 %4 项单次状态或修改\n\n已完成、删除和单次修改将保留，单次修改优先于批量模板。确定保存？").arg(left).arg(first.items.isEmpty()?QString("无"):first.items.first().date().toString(Qt::ISODate)).arg(last.isValid()?last.toString(Qt::ISODate):QString("不限")).arg(exceptions);
}
