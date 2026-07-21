#ifndef DEADLINEMANAGER_H
#define DEADLINEMANAGER_H


#include "datatype.h"
#include "helpsqliteioengine.h"






#include <QObject>
#include <QDateTime>
#include <QDate>

class DeadlineManager : public QObject
{
    Q_OBJECT
public:
    // 单例模式
    static DeadlineManager* getInstance();
    void initialize();
    // 获取信息
    QList<DeadlineEntity> initFutureList(qint64 nowTimestamp);
    QList<DeadlineEntity> initPastList(qint64 nowTimestamp, int pastNumber);
    // 启动程序时获取初始DDL列表, 第一个是获取没有过期的，第二个是获取已过期的。

    bool checkDDL(int id); // 标记某DDL为已完成
    bool ignoreDDL(int id); // 标记某DDL为置灰
    bool unignoreDDL(int id); // 标记某DDL为否置灰
    int addDDL(const DeadlineEntity& DDLEntity); // 向本地添加DDL
    bool updateDDL(const DeadlineEntity& DDLEntity); // 向修改本地DDL
    bool deleteDDL(int id); // 删除某DDL

    bool clearLongLongAgo(qint64 saveDuration);





signals:

private slots:




private:
    explicit DeadlineManager(QObject *parent = nullptr);
    ~DeadlineManager();
    DeadlineManager(const DeadlineManager&) = delete;
    DeadlineManager& operator=(const DeadlineManager&) = delete;


    HelpSQLiteIOEngine *m_ddlEngine;
};

#endif // DEADLINEMANAGER_H
