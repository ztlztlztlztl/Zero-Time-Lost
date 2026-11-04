#pragma once
#include "calendarrules.h"
#include <atomic>
#include <memory>
#include <queue>

struct CalendarPage {QList<TaskOccurrence> items;bool more=false;};
namespace CalendarQuery {
inline CalendarPage page(const QList<QJsonObject> &snapshot,const AdvancedFilter &filter,const QDate &from,const QDate &to,int limit,
                         const QDateTime &now,bool completed,const std::shared_ptr<std::atomic_bool> &cancel={}){
    struct Stream {QJsonObject task;QDate next,end;};
    struct Node {TaskOccurrence item;int stream=-1;};
    struct Compare {bool operator()(const Node &a,const Node &b) const{return CalendarRules::earlier(b.item,a.item);}};
    QList<Stream> streams;std::priority_queue<Node,std::vector<Node>,Compare> heap;CalendarPage result;
    auto stopped=[&]{return cancel&&cancel->load();};
    auto accepts=[&](const TaskOccurrence &o){return !o.isLabel()&&CalendarRules::agendaVisible(o,now,completed)&&CalendarRules::matches(o,filter)&&(!o.due.isValid()||(o.date()>=from&&o.date()<=to));};
    auto advance=[&](int index){auto &stream=streams[index];const auto &t=stream.task;
        while(stream.next.isValid()&&stream.next<=stream.end&&!stopped()){
            const int n=Recurrence::ordinal(t,stream.next.addDays(-1))+1;const auto date=Recurrence::nth(t,n);
            if(!date.isValid()||date>stream.end){stream.next={};break;}stream.next=date.addDays(1);
            if(!Recurrence::scheduled(t,date)||t["instances"].toObject().contains(date.toString(Qt::ISODate)))continue;
            const auto o=CalendarRules::occurrence(t,date.toString(Qt::ISODate),QDateTime(date,Recurrence::start(t).time(),Recurrence::zone(t)));
            if(accepts(o)){heap.push({o,index});return;}
        }
    };
    for(const auto &t:snapshot){if(stopped())return {};if(t["deleted"].toBool())continue;
        const auto nominal=QDateTime::fromString(t["due"].toString(),Qt::ISODate);
        if(t["type"]=="label"){
            const auto o=CalendarRules::occurrence(t,nominal.toString(Qt::ISODate),nominal);
            if(CalendarRules::agendaVisible(o,now,true))result.items.append(o);continue;
        }
        if(t["paused"].toBool())continue;
        if(t["repeat"]=="none"){
            const auto o=CalendarRules::occurrence(t,t["hasDue"].toBool()?nominal.toString(Qt::ISODate):"undated",t["hasDue"].toBool()?nominal:QDateTime());
            if(accepts(o))heap.push({o,-1});continue;
        }
        if(!nominal.isValid())continue;
        const auto states=t["instances"].toObject();
        for(auto it=states.begin();it!=states.end();++it){const auto day=QDate::fromString(it.key(),Qt::ISODate);if(!Recurrence::scheduled(t,day))continue;
            const auto o=CalendarRules::occurrence(t,it.key(),QDateTime(day,Recurrence::start(t).time(),Recurrence::zone(t)));if(accepts(o))heap.push({o,-1});}
        auto base=CalendarRules::occurrence(t,QString(),nominal);
        // Per-instance exceptions above are still tested when the template itself does not match.
        if(!filter.matchesCalendarType(base.type)||!filter.matchesTitle(base.title)||!filter.matchesTags(base.tags)||(filter.starred>=0&&((base.priority>0)!=(filter.starred==1))))continue;
        auto end=to.addDays(1);if(t["countLimited"].toBool())end=qMin(end,Recurrence::last(t));
        streams.append({t,qMax(from.addDays(-1),Recurrence::start(t).date()),end});advance(streams.size()-1);
    }
    int count=0;
    while(!heap.empty()&&!stopped()){
        if(count>=limit){result.more=true;break;}auto node=heap.top();heap.pop();result.items.append(node.item);++count;
        if(node.stream>=0)advance(node.stream);
    }
    std::stable_sort(result.items.begin(),result.items.end(),CalendarRules::earlier);return result;
}
}
