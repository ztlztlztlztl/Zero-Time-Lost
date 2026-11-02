#pragma once
#include "paths.h"
#include <QMessageLogContext>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
inline void ztlMessageOutput(QtMsgType type,const QMessageLogContext &,const QString &msg){
    static QMutex mutex;QMutexLocker lock(&mutex);
    const auto path=Paths::data("logs/ztl_running.log");
    if(QFileInfo(path).size()>5*1024*1024){QFile::remove(path+".old");QFile::rename(path,path+".old");}
    QFile file(path);
    if(file.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text))
        file.write((QDateTime::currentDateTime().toString(Qt::ISODate)+" ["+QString::number(type)+"] "+msg+"\n").toUtf8());
    if(type==QtFatalMsg){
        QFile crash(Paths::data("crash/fatal-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz")+".log"));
        if(crash.open(QIODevice::WriteOnly|QIODevice::Text))crash.write(msg.toUtf8());
        abort();
    }
}
