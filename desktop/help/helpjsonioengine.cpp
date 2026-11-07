#include "helpjsonioengine.h"
#include "paths.h"
#include <QSaveFile>
#include <QDateTime>
HelpJsonIOEngine::HelpJsonIOEngine(const QString &folder,const QString &name,int delay,QObject *parent):QObject(parent),
    m_folderPath(QDir::isAbsolutePath(folder)?folder:Paths::data(folder)),m_fileName(name),m_isDirty(false) {
    m_filePath=m_folderPath+"/"+name;ensureDirExists();m_saveTimer=new QTimer(this);m_saveTimer->setSingleShot(true);m_saveTimer->setInterval(delay);
    connect(m_saveTimer,&QTimer::timeout,this,&HelpJsonIOEngine::forceWrite);
}
HelpJsonIOEngine::~HelpJsonIOEngine(){forceWrite();}
QJsonObject HelpJsonIOEngine::read(){
    m_readAttempted=true;
    QFile f(m_filePath);if(!f.open(QIODevice::ReadOnly)){m_readValid=!f.exists();m_error=m_readValid?QString():m_fileName+" 无法读取，原文件已保护";return {};}
    if(f.size()>32*1024*1024){m_readValid=false;m_error=m_fileName+" 超过 32 MiB，原文件已保护";return {};}
    QJsonParseError e;auto doc=QJsonDocument::fromJson(f.readAll(),&e);
    m_readValid=e.error==QJsonParseError::NoError&&doc.isObject();
    if(!m_readValid){m_error=m_fileName+" 格式损坏，原文件已保护；请恢复备份或明确重建";qWarning()<<"Invalid configuration"<<m_fileName;return {};}
    m_error.clear();return doc.object();
}
void HelpJsonIOEngine::lazyWrite(const QJsonObject &obj){m_cacheBuffer=obj;m_isDirty=true;m_saveTimer->start();}
void HelpJsonIOEngine::forceWrite(){if(m_isDirty){m_saveTimer->stop();writeToDisk(m_cacheBuffer);}}
void HelpJsonIOEngine::endWrite(const QJsonObject &obj){m_cacheBuffer=obj;m_isDirty=true;forceWrite();}
void HelpJsonIOEngine::writeToDisk(const QJsonObject &obj){
    if(QFile::exists(Paths::data("config/shared-commit.pending"))){m_error="共享设置提交尚未恢复，暂缓其他配置写入";m_saveTimer->start(5000);return;}
    if(!m_readAttempted&&QFileInfo(m_filePath).isFile())read();
    if(!m_readValid){if(!m_writeFailed){m_writeFailed=true;emit writeFailed(m_error);}return;}
    ensureDirExists();QSaveFile f(m_filePath);auto bytes=QJsonDocument(obj).toJson();
    if(f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit()){
        m_isDirty=false;m_cacheBuffer={};m_readValid=true;m_error.clear();
        if(m_writeFailed){m_writeFailed=false;emit writeRecovered(m_fileName+" 已保存");}
    }else {
        m_error=m_fileName+" 保存失败";qWarning()<<"Configuration write failed"<<m_fileName;m_saveTimer->start(5000);
        if(!m_writeFailed){m_writeFailed=true;emit writeFailed(m_fileName+" 保存失败，修改已保留在内存中；请检查数据目录权限或磁盘空间。程序将在 5 秒后重试，退出前请等待保存成功。");}
    }
}
bool HelpJsonIOEngine::recover(bool reset,QString *error){
    auto fail=[&](const QString &message){m_error=message;if(error)*error=message;return false;};
    if(reset){
        const auto backup=m_filePath+".damaged-"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
        if(QFile::exists(m_filePath)&&!QFile::copy(m_filePath,backup))return fail("无法备份损坏原件，未重建配置");
        // The caller explicitly chose reset; retain unsaved edits if any.
        m_readValid=true;m_readAttempted=true;endWrite(m_isDirty?m_cacheBuffer:QJsonObject());
        return flush();
    }
    const bool protectedRead=!m_readValid;read();if(!m_readValid)return fail(m_error);
    if(protectedRead){m_cacheBuffer={};m_isDirty=false;m_saveTimer->stop();m_writeFailed=false;return true;}
    if(m_isDirty)forceWrite();return flush();
}
void HelpJsonIOEngine::ensureDirExists(){QDir().mkpath(m_folderPath);}
