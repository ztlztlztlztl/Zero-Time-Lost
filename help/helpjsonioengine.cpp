#include "helpjsonioengine.h"
#include "paths.h"
#include <QSaveFile>
HelpJsonIOEngine::HelpJsonIOEngine(const QString &folder,const QString &name,int delay,QObject *parent):QObject(parent),
    m_folderPath(QDir::isAbsolutePath(folder)?folder:Paths::data(folder)),m_fileName(name),m_isDirty(false) {
    m_filePath=m_folderPath+"/"+name;ensureDirExists();m_saveTimer=new QTimer(this);m_saveTimer->setSingleShot(true);m_saveTimer->setInterval(delay);
    connect(m_saveTimer,&QTimer::timeout,this,&HelpJsonIOEngine::forceWrite);
}
HelpJsonIOEngine::~HelpJsonIOEngine(){forceWrite();}
QJsonObject HelpJsonIOEngine::read(){
    QFile f(m_filePath);if(!f.open(QIODevice::ReadOnly))return {};
    QJsonParseError e;auto doc=QJsonDocument::fromJson(f.readAll(),&e);
    if(e.error!=QJsonParseError::NoError){qWarning()<<"Invalid configuration"<<m_fileName;return {};}return doc.object();
}
void HelpJsonIOEngine::lazyWrite(const QJsonObject &obj){m_cacheBuffer=obj;m_isDirty=true;m_saveTimer->start();}
void HelpJsonIOEngine::forceWrite(){if(m_isDirty){m_saveTimer->stop();writeToDisk(m_cacheBuffer);}}
void HelpJsonIOEngine::endWrite(const QJsonObject &obj){m_cacheBuffer=obj;m_isDirty=true;forceWrite();}
void HelpJsonIOEngine::writeToDisk(const QJsonObject &obj){
    ensureDirExists();QSaveFile f(m_filePath);auto bytes=QJsonDocument(obj).toJson();
    if(f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit()){
        m_isDirty=false;m_cacheBuffer={};
        if(m_writeFailed){m_writeFailed=false;emit writeRecovered(m_fileName+" 已保存");}
    }else {
        qWarning()<<"Configuration write failed"<<m_fileName;m_saveTimer->start(5000);
        if(!m_writeFailed){m_writeFailed=true;emit writeFailed(m_fileName+" 保存失败，修改已保留在内存中；请检查数据目录权限或磁盘空间。程序将在 5 秒后重试，退出前请等待保存成功。");}
    }
}
void HelpJsonIOEngine::ensureDirExists(){QDir().mkpath(m_folderPath);}




