#include "helpsqliteioengine.h"

HelpSQLiteIOEngine::HelpSQLiteIOEngine(const QString &folderPath,
    const QString &dbName, const QString &connectionName,
    QObject *parent): QObject(parent){
    // 变量
    m_folderPath = folderPath;
    m_dbPath = folderPath + "/" + dbName;
    m_connectionName = connectionName;

    ensureDirExists();

    // 挂载数据库连接
    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
        qDebug() << "[HelpSQLiteIOEngine] 复用已存在的数据库连接：" << m_connectionName;
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
        qDebug() << "[HelpSQLiteIOEngine] 创建全新的数据库连接：" << m_connectionName;
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

    qDebug() << "[HelpSQLiteIOEngine] 数据库连接已安全释放。";
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
        qWarning() << "[HelpSQLiteIOEngine] 建表失败！";
        return false;
    }

    qDebug() << "[HelpSQLiteIOEngine] 建表指令执行成功。";
    return true;
}

bool HelpSQLiteIOEngine::executeNonQuery(const QString &sqlString, const QVariantList &bindValues)
{
    if (!openDatabase()) {
        qWarning() << "[HelpSQLiteIOEngine] 数据库未连接。";
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    // 预编译 SQL，防注入
    query.prepare(sqlString);
    for (int i = 0; i < bindValues.size(); ++i) {
        query.addBindValue(bindValues.at(i));
    }

    // 执行
    if (!query.exec()) {
        qWarning() << "[HelpSQLiteIOEngine] 执行 " << sqlString << " 失败:" << query.lastError().text();
        return false;
    }
    return true;
}

QSqlQuery HelpSQLiteIOEngine::executeQuery(const QString &sqlString, const QVariantList &bindValues)
{
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
    return m_db.transaction();
}

bool HelpSQLiteIOEngine::commitTransaction()
{
    if (!m_db.commit()) {
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



void HelpSQLiteIOEngine::ensureDirExists(){
    qDebug() << "[HelpSQLiteIOEnjine] 检查目录：" << m_folderPath;
    QDir dir(m_folderPath);
    if (!dir.exists()) {
        qDebug() << "[HelpSQLiteIOEnjine] 目录不存在，尝试创建：" << m_folderPath << "。";
        if (dir.mkpath(m_folderPath)) {
            qDebug() << "[HelpSQLiteIOEnjine] 目录创建成功。";
        } else {
            qWarning() << "[HelpSQLiteIOEnjine] 目录创建失败！";
            return;
        }
    }
    else{
        qDebug() << "[HelpSQLiteIOEnjine] 目录" << m_folderPath << "已存在，自检完成。";
    }
}

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
        qWarning() << "[HelpSQLiteIOEngine] 无法打开数据库:" << m_dbPath << "，错误详情:" << m_db.lastError().text();
        return false;
    }

    return true;
}


















