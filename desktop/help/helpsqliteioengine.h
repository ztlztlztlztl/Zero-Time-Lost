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

    bool createTable(const QString &sqlString);

    bool executeNonQuery(const QString &sqlString, const QVariantList &bindValues = QVariantList());

    QSqlQuery executeQuery(const QString &sqlString, const QVariantList &bindValues = QVariantList());

    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    QString error() const {return m_error;}
signals:
    void failed(const QString &message);

private:
    QString m_folderPath;
    QString m_dbPath;
    QString m_connectionName; // 数据库连接的唯一标识

    QSqlDatabase m_db; // 核心数据库对象
    QString m_error;
    void failure(const QString &message){m_error=message;emit failed(message);}

    // 底层安全检查
    void ensureDirExists();
    bool openDatabase();
};

#endif // HELPSQLITEIOENGINE_H
