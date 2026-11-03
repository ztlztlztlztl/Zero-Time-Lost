#pragma once
#include <QtCore>
#include <algorithm>
// A finite series allocates the first N nominal dates. Completing/removing an
// occurrence consumes its slot; it never creates a replacement at the tail.
namespace Recurrence {
inline QTimeZone zone(const QJsonObject &t){const QTimeZone z(t["zone"].toString().toUtf8());return z.isValid()?z:QTimeZone::systemTimeZone();}
inline QDateTime start(const QJsonObject &t){return QDateTime::fromString(t["due"].toString(),Qt::ISODate).toTimeZone(zone(t));}
inline QList<int> days(const QJsonObject &t){
    QList<int> result;const bool weekly=t["repeat"]=="weekly";
    for(const auto &v:t[weekly?"weekdays":"monthdays"].toArray()){int n=v.toInt();if(n>=1&&n<=(weekly?7:31)&&!result.contains(n))result.append(n);}
    if(!weekly&&result.isEmpty())result.append(start(t).date().day());
    std::sort(result.begin(),result.end());return result;
}
inline bool raw(const QJsonObject &t,const QDate &date){
    if(!date.isValid()||date<start(t).date())return false;
    const auto until=QDate::fromString(t["until"].toString(),Qt::ISODate);
    if(until.isValid()&&date>until)return false;
    if(t["repeat"]=="weekly")return days(t).contains(date.dayOfWeek());
    if(t["repeat"]=="monthly")return days(t).contains(date.day());
    return t["repeat"]=="daily";
}
// Count nominal dates through date, including both endpoints.
inline int ordinal(const QJsonObject &t,QDate date){
    const auto first=start(t).date();if(!first.isValid()||date<first)return 0;
    const auto until=QDate::fromString(t["until"].toString(),Qt::ISODate);if(until.isValid())date=qMin(date,until);
    if(date<first)return 0;
    const auto rule=t["repeat"].toString();if(rule=="daily")return int(first.daysTo(date)+1);
    const auto chosen=days(t);int count=0;
    if(rule=="weekly"){
        const auto length=first.daysTo(date)+1;count=int(length/7)*chosen.size();
        for(int i=0;i<length%7;++i)if(chosen.contains(first.addDays(i).dayOfWeek()))++count;
    }else if(rule=="monthly"){
        for(auto month=QDate(first.year(),first.month(),1);month.isValid()&&month<=date;month=month.addMonths(1))
            for(int d:chosen){const QDate item(month.year(),month.month(),d);if(item.isValid()&&item>=first&&item<=date)++count;}
    }
    return count;
}
inline QDate nth(const QJsonObject &t,int n){
    if(n<1)return {};
    const auto first=start(t).date();if(!first.isValid())return {};
    const auto rule=t["repeat"].toString();
    if(rule=="daily")return first.addDays(n-1);
    const auto selected=days(t);if(selected.isEmpty())return {};
    if(rule=="weekly"){
        const auto week=first.addDays(1-first.dayOfWeek());
        for(int d:selected)if(week.addDays(d-1)>=first){if(--n==0)return week.addDays(d-1);}
        const auto base=week.addDays(7);return base.addDays(qint64((n-1)/selected.size())*7+selected[(n-1)%selected.size()]-1);
    }
    if(rule=="monthly")for(auto month=QDate(first.year(),first.month(),1);month.isValid()&&month.year()<=9999;month=month.addMonths(1))
        for(int d:selected){const QDate item(month.year(),month.month(),d);if(item.isValid()&&item>=first&&--n==0)return item;}
    return {};
}
inline QDate last(const QJsonObject &t){
    if(!t["countLimited"].toBool())return {};
    auto date=QDate::fromString(t["repeatLast"].toString(),Qt::ISODate);
    date=date.isValid()?date:nth(t,t["countLimit"].toInt());
    const auto until=QDate::fromString(t["until"].toString(),Qt::ISODate);return until.isValid()?qMin(date,until):date;
}
inline bool scheduled(const QJsonObject &t,const QDate &date){
    if(!raw(t,date))return false;
    if(!t["countLimited"].toBool())return true;
    const auto end=last(t);return end.isValid()&&date<=end;
}
inline int remaining(const QJsonObject &t,const QDateTime &now=QDateTime::currentDateTime()){
    if(!t["countLimited"].toBool()||t["repeat"]=="none")return 0;
    const int limit=qMin(t["countLimit"].toInt(),ordinal(t,last(t)));const auto initial=start(t),local=now.toTimeZone(zone(t));
    QDate passed=local.date();if(QDateTime(passed,initial.time(),zone(t))>now)passed=passed.addDays(-1);
    passed=qMin(passed,last(t));int used=qMin(limit,ordinal(t,passed));
    QSet<QString> actions;for(const auto &key:t["excluded"].toArray())actions.insert(key.toString());
    for(const auto &key:t["consumed"].toArray())actions.insert(key.toString());
    const auto instances=t["instances"].toObject();
    for(auto it=instances.begin();it!=instances.end();++it){
        const auto date=QDate::fromString(it.key(),Qt::ISODate);if(!scheduled(t,date))continue;
        const auto state=it.value().toObject();const auto nominal=QDateTime(date,initial.time(),zone(t));
        const auto actual=state.contains("due")?QDateTime::fromString(state["due"].toString(),Qt::ISODate):nominal;
        const bool before=date<=passed,after=actual.isValid()&&actual<=now;
        if(before!=after)used+=after?1:-1;
        if(state["done"].toBool()||state["deleted"].toBool())actions.insert(it.key());
    }
    for(const auto &key:actions){
        const auto date=QDate::fromString(key,Qt::ISODate);if(!scheduled(t,date))continue;
        auto actual=QDateTime(date,initial.time(),zone(t));const auto state=instances[key].toObject();
        if(state.contains("due"))actual=QDateTime::fromString(state["due"].toString(),Qt::ISODate);
        if(!actual.isValid()||actual>now)++used;
    }
    return qBound(0,limit-used,limit);
}
inline QString validate(const QJsonObject &t){
    if(t["repeat"]=="none")return {};
    if((t["repeat"]=="weekly"||t["repeat"]=="monthly")&&(days(t).isEmpty()||(t.contains("monthdays")&&t["repeat"]=="monthly"&&t["monthdays"].toArray().isEmpty())))return "请选择至少一个循环日期";
    if(t["countLimited"].toBool()&&(t["countLimit"].toInt()<1||t["countLimit"].toInt()>1000000||!nth(t,t["countLimit"].toInt()).isValid()||nth(t,t["countLimit"].toInt()).year()>9999))return "循环次数须为正数，且末次日期不能超过 9999 年";
    return {};
}
}
