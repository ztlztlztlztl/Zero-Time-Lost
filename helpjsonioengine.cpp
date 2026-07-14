#include "helpjsonioengine.h"

HelpJsonIOEngine::HelpJsonIOEngine(const QString &folderPath,
    const QString &filename, int delayMs, QObject *parent)
    : QObject{parent}
{
    m_folderPath = folderPath;
    m_fileName = filename;
    m_filePath = m_folderPath + "/" + m_fileName;
    m_delayMs = delayMs;

    qInfo() << "[HelpJsonIOEngine] 初始化自检：" << m_filePath;
    ensureDirExists();

    m_saveTimer = new QTimer(this);
    m_saveTimer->setInterval(delayMs);
    m_cacheBuffer = QJsonObject();
    m_isDirty = false;

    connect(m_saveTimer, &QTimer::timeout, this, &HelpJsonIOEngine::onSaveTimerTimeout);

}

HelpJsonIOEngine::~HelpJsonIOEngine(){
    forceWrite();
}

QJsonObject HelpJsonIOEngine::read(){
    qDebug() << "[HelpJsonIOEngine] 加载：" << m_fileName;
    ensureDirExists();
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[HelpJsonIOEngine] 无法打开文件：" << m_fileName;
        return QJsonObject();
    }
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[HelpJsonIOEngine] " << m_fileName << " 格式损坏：" << error.errorString();
        return QJsonObject();
    }
    return doc.object();
}

void HelpJsonIOEngine::lazyWrite(const QJsonObject &jsonObj){
    m_cacheBuffer = jsonObj;
    m_isDirty = true;
    m_saveTimer->start();

}

void HelpJsonIOEngine::forceWrite(){
    if (m_isDirty) {
        m_saveTimer->stop();
        writeToDisk(m_cacheBuffer);
    }
}

void HelpJsonIOEngine::onSaveTimerTimeout(){
    forceWrite();
}

void HelpJsonIOEngine::writeToDisk(const QJsonObject &jsonObj){
    qDebug() << "[HelpJsonIOEngine] 写入：" << m_filePath;
    ensureDirExists();
    QJsonDocument doc(jsonObj);
    QFile file(m_filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {// 只读与清空
        file.write(doc.toJson()); // tojson()自带换行
        file.close();
        m_isDirty = false;
        qDebug() << "[HelpJsonIOEngine] 更新成功：" << m_fileName;
    } else {
        qWarning() << "[HelpJsonIOEngine] 更新失败：" << m_fileName;
    }
    m_cacheBuffer = QJsonObject();
}

void HelpJsonIOEngine::ensureDirExists(){
    qDebug() << "[HelpJsonIOEngine] 检查目录：" << m_folderPath;
    QDir dir(m_folderPath);
    if (!dir.exists()) {
        qDebug() << "[HelpJsonIOEngine] 目录不存在，尝试创建：" << m_folderPath << "。";
        if (dir.mkpath(m_folderPath)) {
            qDebug() << "[HelpJsonIOEngine] 目录创建成功。";
        } else {
            qWarning() << "[HelpJsonIOEngine] 目录创建失败！";
            return;
        }
    }
    else{
        qDebug() << "[HelpJsonIOEngine] 目录" << m_folderPath << "已存在，自检完成。";
    }
    qDebug() << "[HelpJsonIOEngine] 检查文件：" << m_filePath;
    QFile file(m_filePath);
    if (!file.exists()) {
        qDebug() << "[HelpJsonIOEngine] 文件不存在，尝试创建：" << m_filePath;
        if (file.open(QIODevice::WriteOnly)) {
            qDebug() << "[HelpJsonIOEngine] 文件创建成功。" << m_filePath;
            file.write(QString("{}").toUtf8());
            file.close();
        } else {
            qWarning() << "[HelpJsonIOEngine] 文件创建失败！" << m_filePath;
        }
    } else {
        qDebug() << "[HelpJsonIOEngine] 文件" << m_filePath << "已存在，无需重复创建。";
    }

}




