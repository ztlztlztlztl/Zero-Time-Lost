#pragma once
#include <QJsonArray>
#include <QString>
class NoteRepository {
public:
    explicit NoteRepository(QString path);
    QJsonArray entries() const{return items;}
    bool indexed() const{return vaultIndexed;}
    bool save(const QJsonArray &entries,bool indexed);
    bool ready() const{return valid;}
    QString error() const{return message;}
private:
    QString file,message;
    QJsonArray items;
    bool valid=true,vaultIndexed=false;
};
