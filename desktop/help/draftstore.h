#pragma once
#include "paths.h"
#include "cryptotool.h"
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QSaveFile>
#include <QFile>
#include <QDir>
#include <QHash>
// Drafts, including newly typed secrets, are never written as plaintext.
namespace DraftStore {
inline QString path(const QString &key){
    const auto dir=Paths::data("cache/drafts");QDir().mkpath(dir);
    return dir+"/"+QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(),QCryptographicHash::Sha256).toHex());
}
inline QHash<QString,QByteArray> &memory(){static QHash<QString,QByteArray> drafts;return drafts;}
inline QJsonObject read(const QString &key){
    QFile f(path(key));auto bytes=memory().value(key);if(bytes.isEmpty()&&f.open(QIODevice::ReadOnly))bytes=f.readAll();
    try{return QJsonDocument::fromJson(CryptoTool::unprotect(QString::fromUtf8(bytes)).toUtf8()).object();}catch(...){return {};}
}
inline bool save(const QString &key,const QJsonObject &draft){
    try{const auto bytes=CryptoTool::protect(QString::fromUtf8(QJsonDocument(draft).toJson(QJsonDocument::Compact))).toUtf8();
        memory()[key]=bytes;QSaveFile f(path(key));return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit();}catch(...){return false;}
}
inline void clear(const QString &key){memory().remove(key);QFile::remove(path(key));}
inline bool flush(){bool ok=true;for(auto it=memory().cbegin();it!=memory().cend();++it){QSaveFile f(path(it.key()));const auto &bytes=it.value();if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())ok=false;}return ok;}
}
