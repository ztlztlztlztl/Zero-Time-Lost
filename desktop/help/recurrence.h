#pragma once
#include <QtCore>
#include <algorithm>
// A finite series allocates the first N nominal dates. Completing/removing an
// occurrence consumes its slot; it never creates a replacement at the tail.
namespace Recurrence {
inline QTimeZone zone(const QJsonObject &t){const QTimeZone z(t["zone"].toString().toUtf8());return z.isValid()?z:QTimeZone::systemTimeZone();}
inline QDateTime start(const QJsonObject &t){return QDateTime::fromString(t["due"].toString(),Qt::ISODate).toTimeZone(zone(t));}
inline QJsonObject templateAt(const QJsonObject &task,const QDate &day){
    auto effective=task;
    for(const auto &entry:task["templateHistory"].toArray()){const auto h=entry.toObject(),fields=h["fields"].toObject();if(day<QDate::fromString(h["from"].toString(),Qt::ISODate))continue;
        const auto originalEnd=QDateTime(day,QDateTime::fromString(fields["due"].toString(),Qt::ISODate).toTimeZone(zone(task)).time(),zone(task));
        if(originalEnd>QDateTime::fromString(h["until"].toString(),Qt::ISODate))continue;
        for(auto it=fields.begin();it!=fields.end();++it)effective[it.key()]=it.value();break;
    }return effective;
}
inline QDateTime endAt(const QJsonObject &task,const QDate &day){
    const auto t=templateAt(task,day);auto end=QDateTime(day,start(t).time(),zone(t));
    if(t["type"]=="interval"){
        const auto base=QDateTime::fromString(t["start"].toString(),Qt::ISODate).toTimeZone(zone(t));
        const auto begin=QDateTime(day,base.time(),zone(t),QDateTime::TransitionResolution::PreferBefore);
        if(begin.time()!=base.time()||end.time()!=start(t).time()||begin>=end)end=begin.addSecs(base.time().secsTo(start(t).time()));
    }return end;
}
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
        auto before=[&](const QDate &value){
            const qint64 years=value.year()-1;const int months=value.month()-1;const qint64 leaps=years/4-years/100+years/400;qint64 total=0;
            for(int d:chosen){
                if(d<=28)total+=years*12+months;
                else if(d==29)total+=years*11+leaps+months-(months>=2&&!QDate::isLeapYear(value.year())?1:0);
                else if(d==30)total+=years*11+months-(months>=2?1:0);
                else{total+=years*7;for(int m:{1,3,5,7,8,10,12})if(m<value.month())++total;}
                if(d<value.day()&&QDate(value.year(),value.month(),d).isValid())++total;
            }return total;
        };
        count=int(before(date)-before(first));if(chosen.contains(date.day()))++count;
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
    if(rule=="monthly"){
        auto candidate=t;candidate.remove("until");qint64 low=first.toJulianDay(),high=QDate(9999,12,31).toJulianDay();
        if(ordinal(candidate,QDate::fromJulianDay(high))<n)return {};
        while(low<high){const auto middle=low+(high-low)/2;if(ordinal(candidate,QDate::fromJulianDay(middle))>=n)high=middle;else low=middle+1;}
        return QDate::fromJulianDay(low);
    }
    return {};
}
inline QDate nextDate(const QJsonObject &t,QDate from){
    from=qMax(from,start(t).date());if(!from.isValid())return {};
    if(t["repeat"]=="daily")return from;
    const auto selected=days(t);if(selected.isEmpty())return {};
    if(t["repeat"]=="weekly"){for(int i=0;i<7;++i){const auto date=from.addDays(i);if(selected.contains(date.dayOfWeek()))return date;}}
    if(t["repeat"]=="monthly")for(int i=0;i<3;++i){const auto month=QDate(from.year(),from.month(),1).addMonths(i);for(int d:selected){QDate date(month.year(),month.month(),d);if(date.isValid()&&date>=from)return date;}}
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
    QDate passed=local.date();if(endAt(t,passed)>now)passed=passed.addDays(-1);
    passed=qMin(passed,last(t));int used=qMin(limit,ordinal(t,passed));
    QSet<QString> actions;for(const auto &key:t["excluded"].toArray())actions.insert(key.toString());
    for(const auto &key:t["consumed"].toArray())actions.insert(key.toString());
    const auto instances=t["instances"].toObject();
    for(auto it=instances.begin();it!=instances.end();++it){
        const auto date=QDate::fromString(it.key(),Qt::ISODate);if(!scheduled(t,date))continue;
        const auto state=it.value().toObject();const auto nominal=endAt(t,date);
        const auto actual=state.contains("due")?QDateTime::fromString(state["due"].toString(),Qt::ISODate):nominal;
        const bool before=date<=passed,after=actual.isValid()&&actual<=now;
        if(before!=after)used+=after?1:-1;
        if(state["done"].toBool()||state["deleted"].toBool())actions.insert(it.key());
    }
    for(const auto &key:actions){
        const auto date=QDate::fromString(key,Qt::ISODate);if(!scheduled(t,date))continue;
        auto actual=endAt(t,date);const auto state=instances[key].toObject();
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
