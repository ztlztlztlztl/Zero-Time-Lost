#ifndef HELPJSONIOENGINE_H
#define HELPJSONIOENGINE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

class HelpJsonIOEngine : public QObject
{
    Q_OBJECT
public:
    // 绑定文件路径，并设置防抖时间
    explicit HelpJsonIOEngine(const QString &folderPath, const QString &filename,
        int delayMs = 500, QObject *parent = nullptr);
    ~HelpJsonIOEngine();
    // 读取接口
    QJsonObject read();
    // 懒写入接口
    void lazyWrite(const QJsonObject &jsonObj);
    // 强写入接口
    void forceWrite();
    bool flush(){forceWrite();return m_readValid&&!m_isDirty;}
    bool readable() const {return m_readValid;}
    bool dirty() const {return m_isDirty;}
    QString error() const {return m_error;}
    QString filePath() const {return m_filePath;}
    bool recover(bool reset,QString *error=nullptr);
    // 结束接口
    void endWrite(const QJsonObject &jsonObj);
signals:
    void writeFailed(const QString &message);
    void writeRecovered(const QString &message);

private:
    QString m_folderPath; // 文件夹路径
    QString m_fileName; // 文件名称
    QString m_filePath; // 文件路径

    QTimer *m_saveTimer; // 倒计时秒表
    QJsonObject m_cacheBuffer; // 内存缓冲区
    bool m_isDirty;
    bool m_writeFailed=false;
    bool m_readValid=true;
    bool m_readAttempted=false;
    QString m_error;

    void writeToDisk(const QJsonObject &jsonObj);
    void ensureDirExists();   // 剥离出来的自动建文件夹小助手
};

#endif // HELPJSONIOENGINE_H
