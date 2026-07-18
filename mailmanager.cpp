#include "mailmanager.h"


MailManager* MailManager::getInstance(){
    // 单例的标准写法
    static MailManager instance;
    return &instance;
}

void MailManager::initialize(){
    qInfo() << "[MailManager] 开始初始化。";
    // 注册 Qt 元对象，使得线程里可以emit出自定义的结构体
    qRegisterMetaType<MailBrief>("MailBrief");
    qRegisterMetaType<QList<MailBrief>>("QList<MailBrief>");

    // 实例化 SQLite Engine
    m_mailEngine = new HelpSQLiteIOEngine("./database", "mail.db", "ZTL_Mail_Conn", this);

    // 建表
    QString createTableSql = R"(
        CREATE TABLE IF NOT EXISTS mails (
            owner_email TEXT,
            uid TEXT,
            sender TEXT,
            subject TEXT,
            timestamp INTEGER,
            isRead INTEGER,
            body TEXT,
            PRIMARY KEY (owner_email, uid)
        )
    )";

    m_mailEngine->createTable(createTableSql);

    qInfo() << "[MailManager] 初始化完成。";
}

QList<MailBrief> MailManager::initMailList(int number){
    QList<MailBrief> mails;
    if (number <= 0) {
        return mails;
    }

    QString sql = R"(
                SELECT owner_email, uid, sender, subject, timestamp, isRead
                FROM mails
                ORDER BY timestamp ASC
                LIMIT ?
    )";
    QVariantList bindValues;
    bindValues << number;
    QSqlQuery query = m_mailEngine->executeQuery(sql, bindValues);
    while (query.next()) {
        mails.append(MailBrief(query));
    }
    return mails;
}

QList<MailBrief> MailManager::moreMailList(int startNumber, int number){
    QList<MailBrief> mails;
    if (number <= 0 || startNumber <= 0) {
        return mails;
    }

    QString sql = R"(
                SELECT owner_email, uid, sender, subject, timestamp, isRead
                FROM mails
                ORDER BY timestamp ASC
                LIMIT ?
                OFFSET ?
    )";
    QVariantList bindValues;
    bindValues << number << startNumber;
    QSqlQuery query = m_mailEngine->executeQuery(sql, bindValues);
    while (query.next()) {
        mails.append(MailBrief(query));
    }
    return mails;
}
void MailManager::checkMail(QString address, QString uid){

    QString sql = R"(
                UPDATE mails
                SET isRead = 1
                WHERE owner_email = ? AND uid = ?
    )";

    QVariantList params;
    params << address << uid;
    if (m_mailEngine->executeNonQuery(sql, params)) {

    } else {
        qWarning() << QString("[MailManager] 标记已读失败。");
    }



}
void MailManager::addMail(const MailBrief& newMail){
    // 使用 OR IGNORE
    QString sql = R"(
        INSERT OR IGNORE INTO mails
        (owner_email, uid, sender, subject, timestamp, isRead, body)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    QVariantList params = newMail.toBindValues();
    params.append("");
    bool isSuccess = m_mailEngine->executeNonQuery(sql, params);
    if (isSuccess) {
    } else {
        qWarning() << "[MailManager] 邮件入库失败 UID:" << newMail.uid;
    }
}

void MailManager::deleteMail(const QString &address, const QString &uid)
{
    QString sql = R"(
        DELETE FROM mails
        WHERE owner_email = ? AND uid = ?
    )";
    QVariantList params;
    params << address << uid;
    if (m_mailEngine->executeNonQuery(sql, params)) {
        qInfo() << "[MailManager] 邮件本地删除成功。账号:" << address << "UID:" << uid;
    } else {
        qWarning() << "[MailManager] 邮件本地删除失败！账号:" << address << "UID:" << uid;
    }
}


QString MailManager::getMailBody(const QString &address, const QString &uid)
{
    QString sql = R"(
        SELECT body
        FROM mails
        WHERE owner_email = ? AND uid = ?
    )";

    QVariantList params;
    params << address << uid;
    QSqlQuery query = m_mailEngine->executeQuery(sql, params);
    if (query.next()) {
        return query.value("body").toString();
    } else {
        qWarning() << "[MailManager] 获取正文失败，账号:" << address << "UID:" << uid;
        return QString();
    }
}

void MailManager::updateMailBody(const QString &address, const QString &uid, const QString &body)
{
    QString sql = R"(
        UPDATE mails
        SET body = ?
        WHERE owner_email = ? AND uid = ?
    )";

    QVariantList params;
    params << body << address << uid;
    if (m_mailEngine->executeNonQuery(sql, params)) {
        qDebug() << "[MailManager] 邮件本地正文保存成功！账号:" << address << "UID:" << uid;
    } else {
        qWarning() << "[MailManager] 邮件本地正文保存失败！账号:" << address << "UID:" << uid;
    }
}
















void MailManager::clearUp(){
    delete m_mailEngine;
    m_mailEngine = nullptr;
}












MailManager::MailManager(QObject *parent): QObject(parent){
    qDebug() << "[MailManager] 构造函数被调用。";
}
MailManager::~MailManager(){
    qDebug() << "[MailManager] 析构函数被调用。";
};










