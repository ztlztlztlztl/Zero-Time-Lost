#include "noterepository.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
NoteRepository::NoteRepository(QString path):file(std::move(path)){
    if(!QFileInfo::exists(file))return;
    QFile source(file);if(!source.open(QIODevice::ReadOnly)||source.size()>32*1024*1024){valid=false;message="无法读取 Note 列表";return;}
    QJsonParseError error;auto doc=QJsonDocument::fromJson(source.readAll(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject()||doc.object()["version"]!=1||!doc.object()["entries"].isArray()){
        valid=false;message="Note 列表损坏，请恢复备份";return;
    }
    orders=doc.object()["orders"].toObject();items=doc.object()["entries"].toArray();vaultIndexed=doc.object()["vaultIndexed"].toBool();reindex();
}
bool NoteRepository::save(const QJsonArray &entries,bool indexed){
    if(!valid)return false;
    QDir().mkpath(QFileInfo(file).absolutePath());QSaveFile target(file);
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"orders",orders},{"vaultIndexed",indexed},{"entries",entries}}).toJson(QJsonDocument::Compact);
    if(bytes.size()>32*1024*1024||!target.open(QIODevice::WriteOnly)||target.write(bytes)!=bytes.size()||!target.commit()){
        message="Note 列表保存失败";return false;
    }
    items=entries;vaultIndexed=indexed;reindex();message.clear();return true;
}

QStringList NoteRepository::order(int kind) const {
    QStringList ids;for(auto v:orders[QString::number(kind)].toArray())ids.append(v.toString());return ids;
}
bool NoteRepository::saveOrder(int kind,const QStringList &ids){
    const auto previous=orders;QJsonArray array;for(const auto &id:ids)array.append(id);orders[QString::number(kind)]=array;
    if(save(items,vaultIndexed))return true;orders=previous;return false;
}
