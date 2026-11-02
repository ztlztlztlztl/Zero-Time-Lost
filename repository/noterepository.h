#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QString>
class NoteRepository {
public:
    explicit NoteRepository(QString path);
    QJsonArray entries() const{return items;}
    bool indexed() const{return vaultIndexed;}
    QStringList order(int kind) const;
    bool saveOrder(int kind,const QStringList &ids);
    bool save(const QJsonArray &entries,bool indexed);
    bool ready() const{return valid;}
    QString error() const{return message;}
private:
    QString file,message;
    QJsonArray items;
    QJsonObject orders;
    bool valid=true,vaultIndexed=false;
};
