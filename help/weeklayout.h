#pragma once
#include <QVector>
#include <QtGlobal>

namespace WeekLayout {
struct Metrics {
    int lineHeight=22;
    int gap=3;
    int padding=7;
};

inline int rowHeight(int count,bool compressed,const Metrics &metrics){
    return qMax(compressed?36:86,2*metrics.padding+count*metrics.lineHeight+qMax(0,count-1)*metrics.gap+4);
}

// Assign slots to the complete, chronologically sorted row before hiding filtered nodes.
inline QVector<int> textTops(int count,const Metrics &metrics){
    QVector<int> tops;tops.reserve(count);
    for(int i=0;i<count;++i)tops.append(metrics.padding+i*(metrics.lineHeight+metrics.gap));
    return tops;
}

inline int insertionY(const QVector<int> &sortedMinutes,int minute,const Metrics &metrics){
    int index=0;while(index<sortedMinutes.size()&&sortedMinutes[index]<minute)++index;
    return metrics.padding+index*(metrics.lineHeight+metrics.gap)-metrics.gap/2-1;
}
}
