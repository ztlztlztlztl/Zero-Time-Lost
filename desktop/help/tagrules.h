#pragma once
#include <QString>
#include <QStringList>
namespace TagRules {
inline int weight(const QString &text){
    int n=0;for(auto cp:text.toUcs4())n+=cp<128?1:2;return n;
}
inline QString error(const QString &text){
    if(text.trimmed().isEmpty())return "标签不能为空";
    for(auto cp:text.toUcs4()){
        const auto category=QChar::category(cp);
        if(cp==','||cp==0xff0c||QChar::isSpace(cp)&&cp!=' '||
           category==QChar::Other_Control||category==QChar::Other_Format)
            return "标签不能包含逗号、换行或控制字符";
    }
    if(weight(text.trimmed())>10)return "标签长度超过 10（中文 2，英文 1）";
    return {};
}
inline QString selectionError(const QString &value){
    if(value.isEmpty())return {};
    const auto tags=value.split(',',Qt::KeepEmptyParts);
    if(tags.size()>3)return "最多选择三个标签";
    for(const auto &tag:tags){auto issue=error(tag);if(!issue.isEmpty())return issue;}
    return {};
}
inline QStringList normalize(const QStringList &values){
    QStringList result;for(const auto &v:values){auto t=v.trimmed();if(error(v).isEmpty()&&!result.contains(t))result.append(t);}return result;
}
}
