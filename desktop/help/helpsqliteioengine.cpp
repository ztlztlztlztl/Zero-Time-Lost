#include <QLoggingCategory>
Q_LOGGING_CATEGORY(ztlStorageLog,"ztl.storage")
#include "helpsqliteioengine.h"
#include "paths.h"

HelpSQLiteIOEngine::HelpSQLiteIOEngine(const QString &folderPath,
    const QString &dbName, const QString &connectionName,
    QObject *parent): QObject(parent){
    // 变量
    m_folderPath = QDir::isAbsolutePath(folderPath) ? folderPath : Paths::data(folderPath);
    m_dbPath = m_folderPath + "/" + dbName;
    m_connectionName = connectionName;

    ensureDirExists();

    // 挂载数据库连接
    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
        qCDebug(ztlStorageLog) << "[HelpSQLiteIOEngine] 复用已存在的数据库连接：" << m_connectionName;
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
        qCDebug(ztlStorageLog) << "[HelpSQLiteIOEngine] 创建全新的数据库连接：" << m_connectionName;
    }
    m_db.setDatabaseName(m_dbPath);


}


HelpSQLiteIOEngine::~HelpSQLiteIOEngine()
{
    if (m_db.isOpen()) {
        m_db.close();
    }

    // 清空本地对象引用
    m_db = QSqlDatabase();

    // 从 Qt 的全局连接池中离开
    QSqlDatabase::removeDatabase(m_connectionName);

    qCDebug(ztlStorageLog) << "[HelpSQLiteIOEngine] 数据库连接已安全释放。";
}



bool HelpSQLiteIOEngine::createTable(const QString &sqlString)
{
    // 确保文件夹存在，且数据库已连上
    ensureDirExists();
    if (!openDatabase()) {
        qWarning() << "[HelpSQLiteIOEngine] 数据库未就绪。";
        return false;
    }

    // 创建Query建表
    QSqlQuery query(m_db);
    if (!query.exec(sqlString)) {
        failure(query.lastError().text());
        qWarning() << "[HelpSQLiteIOEngine] 建表失败！";
        return false;
    }

    qCDebug(ztlStorageLog) << "[HelpSQLiteIOEngine] 建表指令执行成功。";
    return true;
}

bool HelpSQLiteIOEngine::executeNonQuery(const QString &sqlString, const QVariantList &bindValues)
{
    ensureDirExists();
    if (!openDatabase()) {
        qWarning() << "[HelpSQLiteIOEngine] 数据库未连接。";
        return false;
    }
    QSqlQuery query(m_db);

    // 预编译 SQL，防注入
    query.prepare(sqlString);
    for (int i = 0; i < bindValues.size(); ++i) {
        query.addBindValue(bindValues.at(i));
    }

    // 执行
    if (!query.exec()) {
        failure(query.lastError().text());
        qWarning() << "[HelpSQLiteIOEngine] 执行 " << sqlString << " 失败:" << query.lastError().text();
        return false;
    }
    m_error.clear();return true;
}

QSqlQuery HelpSQLiteIOEngine::executeQuery(const QString &sqlString, const QVariantList &bindValues)
{
    ensureDirExists();
    if (!openDatabase()) {
        qWarning() << "[HelpSQLiteIOEngine] 数据库未打开！";
        return QSqlQuery();
    }
    QSqlQuery query(m_db);
    query.prepare(sqlString);
    for (int i = 0; i < bindValues.size(); ++i) {
        query.addBindValue(bindValues.at(i));
    }
    if (!query.exec()) {
        failure(query.lastError().text());
        qWarning() << "[HelpSQLiteIOEngine] 查询执行失败！";
    }
    return query;
}



bool HelpSQLiteIOEngine::beginTransaction()
{
    if (!openDatabase()) {
        return false;
    }
    if (!m_db.driver()->hasFeature(QSqlDriver::Transactions)) {
        qWarning() << "[HelpSQLiteIOEngine] 当前数据库驱动不支持事务操作！" << m_connectionName;
        return false;
    }
    if(!m_db.transaction()){failure(m_db.lastError().text());return false;}return true;
}

bool HelpSQLiteIOEngine::commitTransaction()
{
    if (!m_db.commit()) {
        failure(m_db.lastError().text());
        qWarning() << "[HelpSQLiteIOEngine] 事务提交失败：" << m_db.lastError().text();
        return false;
    }
    return true;
}

bool HelpSQLiteIOEngine::rollbackTransaction()
{
    if (!m_db.rollback()) {
        qWarning() << "[HelpSqliteIOEngine] 事务回滚失败：" << m_db.lastError().text();
        return false;
    }
    return true;
}



void HelpSQLiteIOEngine::ensureDirExists(){if(!QDir(m_folderPath).exists()&&!QDir().mkpath(m_folderPath))failure("无法创建邮件数据库目录");}

bool HelpSQLiteIOEngine::openDatabase()
{
    // 检查是否已经包含了这个名字的连接
    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
        m_db.setDatabaseName(m_dbPath); // 路径
    }
    // 打开db文件
    if (!m_db.isOpen() && !m_db.open()) {
        failure(m_db.lastError().text());
        qWarning() << "[HelpSQLiteIOEngine] 无法打开数据库:" << m_dbPath << "，错误详情:" << m_db.lastError().text();
        return false;
    }

    return true;
}


















