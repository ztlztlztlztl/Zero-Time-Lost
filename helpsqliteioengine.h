#ifndef HELPSQLITEIOENGINE_H
#define HELPSQLITEIOENGINE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantList>
#include <QString>
#include <QDebug>
#include <QDir>
#include <QSqlDriver>

class HelpSQLiteIOEngine : public QObject
{
    Q_OBJECT
public:
    explicit HelpSQLiteIOEngine(const QString &folderPath, const QString &dbName, const QString &connectionName = "DefaultConnection", QObject *parent = nullptr);

    // 💡 析构：安全关闭数据库连接
    ~HelpSQLiteIOEngine();

    // 💡 基础设施：自动建表接口（管家在初始化时调用）
    bool createTable(const QString &sqlString);

    // 💡 泛用写入/修改接口：支持防注入的参数绑定（INSERT/UPDATE/DELETE）
    bool executeNonQuery(const QString &sqlString, const QVariantList &bindValues = QVariantList());

    // 💡 泛用查询接口：返回查询结果集（SELECT）
    QSqlQuery executeQuery(const QString &sqlString, const QVariantList &bindValues = QVariantList());

    // 💡 事务控制：用于大批量写入时的极限性能榨取
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

private:
    QString m_folderPath;
    QString m_dbPath;
    QString m_connectionName; // 数据库连接的唯一标识

    QSqlDatabase m_db; // 核心数据库对象

    // 底层安全检查
    void ensureDirExists();
    bool openDatabase();
};

#endif // HELPSQLITEIOENGINE_H
