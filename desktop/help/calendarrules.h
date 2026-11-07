#pragma once
#include "recurrence.h"
#include "advancedfilter.h"

enum class CalendarKind {DDL,Label,Interval,Invalid};
struct TaskOccurrence {
    QString taskId,key,title,note,tags,repeat,type="ddl",stableId,issue;
    QDateTime due,start;
    qint64 createdAt=0,snooze=0;
    bool done=false,deleted=false,paused=false;
    int priority=0;
    CalendarKind kind() const {return type=="ddl"?CalendarKind::DDL:type=="label"?CalendarKind::Label:type=="interval"?CalendarKind::Interval:CalendarKind::Invalid;}
    bool isDDL() const {return kind()==CalendarKind::DDL;}
    bool isLabel() const {return kind()==CalendarKind::Label;}
    bool isInterval() const {return kind()==CalendarKind::Interval;}
    QString identity() const {return stableId.isEmpty()?taskId+"/"+key:stableId;}
    QDate date() const {return due.toLocalTime().date();}
    QString state(const QDateTime &now=QDateTime::currentDateTime()) const {
        return !issue.isEmpty()?issue:now<start?QString("未开始"):now<due?QString("进行中"):QString("已结束");
    }
};

namespace CalendarRules {
inline bool earlier(const TaskOccurrence &a,const TaskOccurrence &b){
    if(a.due.isValid()!=b.due.isValid())return a.due.isValid();
    if(a.due!=b.due)return a.due<b.due;
    if(a.isLabel()&&b.isLabel()&&a.createdAt!=b.createdAt)return a.createdAt<b.createdAt;
    return a.identity()<b.identity();
}
inline bool agendaVisible(const TaskOccurrence &o,const QDateTime &now,bool completed){
    return !o.deleted&&!o.paused&&(!o.isDDL()||(!o.done||completed)&&!(o.done&&o.due.isValid()&&o.due<=now))&&
        (!o.isInterval()||!o.issue.isEmpty()||o.due>now)&&(!o.isLabel()||o.date()>=now.date());
}
inline bool matches(const TaskOccurrence &o,const AdvancedFilter &f){
    return o.isLabel()||(f.matchesCalendarType(o.type)&&f.matchesTitle(o.title)&&f.matchesTags(o.tags)&&
        f.matchesDate(o.due)&&(f.starred<0||((o.priority>0)==(f.starred==1))));
}
inline TaskOccurrence occurrence(const QJsonObject &task,const QString &key,QDateTime nominal){
    const auto t=Recurrence::templateAt(task,QDate::fromString(key,Qt::ISODate));
    const auto v=t["instances"].toObject()[key].toObject();TaskOccurrence o;
    o.type=t["type"].toString("ddl");o.taskId=t["id"].toString();o.key=key;
    o.stableId=v["identity"].toString(t["repeat"]=="none"?t["identity"].toString():t["identityRoot"].toString().isEmpty()?QString():t["identityRoot"].toString()+"/"+key);
    o.title=v["title"].toString(t["title"].toString());o.note=v["note"].toString(t["note"].toString());
    o.tags=v["tags"].toString(t["tags"].toString());o.repeat=t["repeat"].toString();o.done=v["done"].toBool();
    o.deleted=t["deleted"].toBool()||v["deleted"].toBool()||t["excluded"].toArray().contains(key);o.paused=t["paused"].toBool();
    o.priority=v["priority"].toInt(t["priority"].toInt());o.createdAt=t["createdAt"].toVariant().toLongLong();o.snooze=v["snooze"].toVariant().toLongLong();
    o.due=v.contains("due")?QDateTime::fromString(v["due"].toString(),Qt::ISODate):t["repeat"]!="none"?Recurrence::endAt(task,QDate::fromString(key,Qt::ISODate)):nominal;
    if(o.isLabel()&&t.contains("labelDate"))o.due=QDateTime(QDate::fromString(t["labelDate"].toString(),Qt::ISODate),QTime(0,0));
    if(o.isInterval()){
        const auto zone=Recurrence::zone(t);const auto base=QDateTime::fromString(t["start"].toString(),Qt::ISODate).toTimeZone(zone);
        const auto end=Recurrence::start(t);const auto date=o.due.toTimeZone(zone).date();
        o.start=v.contains("start")?QDateTime::fromString(v["start"].toString(),Qt::ISODate):QDateTime(date,base.time(),zone,QDateTime::TransitionResolution::PreferBefore);
        if(!v.contains("start")&&!v.contains("due")){
            const int duration=base.time().secsTo(end.time());
            // Resolve the pair together across a DST gap, preserving its wall-clock duration.
            if(o.start.time()!=base.time()||o.due.time()!=end.time()||o.start>=o.due)
                o.due=o.start.addSecs(duration);
        }
        if(!o.start.isValid()||!o.due.isValid()||o.start>=o.due||o.start.toTimeZone(zone).date()!=o.due.toTimeZone(zone).date())o.issue="时间异常，请编辑修正";
    }
    return o;
}
}
