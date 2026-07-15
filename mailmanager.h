#ifndef MAILMANAGER_H
#define MAILMANAGER_H

#include "helpsqliteioengine.h"
#include "datatype.h"

#include <QObject>












class MailManager : public QObject
{
    Q_OBJECT
public:
    // 单例模式
    static MailManager* getInstance();
    void initialize();
    // 获取信息
    QList<MailBrief> initMailList(int number); // 启动程序时获取初始邮件列表
    QList<MailBrief> moreMailList(int startNumber, int number); // 加载本地更多邮件时获取邮件列表
    void checkMail(QString address, QString uid); // 标记某封邮件为已读
    void addMail(const MailBrief& newMail); // 向本地添加一封邮件




signals:

private slots:




private:
    explicit MailManager(QObject *parent = nullptr);
    ~MailManager();
    MailManager(const MailManager&) = delete;
    MailManager& operator=(const MailManager&) = delete;



    HelpSQLiteIOEngine *m_mailEngine;
};

#endif // MAILMANAGER_H
