#include "deadlinemanager.h"

DeadlineManager* DeadlineManager::getInstance(){
    // 单例的标准写法
    static DeadlineManager instance;
    return &instance;
}


void DeadlineManager::initialize(){
    qInfo() << "[DeadlineManager] 开始初始化。";

    // 实例化 SQLite Engine
    m_ddlEngine = new HelpSQLiteIOEngine("./database", "deadline.db", "ZTL_Deadline_Conn", this);

    // 建表
    QString createTableSql = R"(
        CREATE TABLE IF NOT EXISTS deadlines (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT,
            describe TEXT,
            due_timestamp INTEGER,
            tags TEXT,
            is_done INTEGER,
            is_ignore INTEGER
        )
    )";

    m_ddlEngine->createTable(createTableSql);

    qInfo() << "[DeadlineManager] 初始化完成。";
}



QList<DeadlineEntity> DeadlineManager::initFutureList(qint64 nowTimestamp){
    QList<DeadlineEntity> ddls;

    QString sql = R"(
                SELECT title, describe, due_timestamp, tags, is_done, is_ignore, id
                FROM deadlines
                WHERE due_timestamp > ?
                ORDER BY due_timestamp ASC
    )";
    QVariantList bindValues;
    bindValues << nowTimestamp;
    QSqlQuery query = m_ddlEngine->executeQuery(sql, bindValues);
    while (query.next()) {
        ddls.append(DeadlineEntity(query));
    }
    return ddls;
}


QList<DeadlineEntity> DeadlineManager::initPastList(qint64 nowTimestamp, int pastNumber){
    QList<DeadlineEntity> ddls;
    if(pastNumber <= 0){
        return ddls;
    }

    QString sql = R"(
                SELECT title, describe, due_timestamp, tags, is_done, is_ignore, id
                FROM deadlines
                WHERE due_timestamp <= ?
                ORDER BY due_timestamp DESC
                LIMIT ?
    )";
    QVariantList bindValues;
    bindValues << nowTimestamp << pastNumber;
    QSqlQuery query = m_ddlEngine->executeQuery(sql, bindValues);
    while (query.next()) {
        ddls.append(DeadlineEntity(query));
    }
    return ddls;
}

bool DeadlineManager::checkDDL(int id){
    QString sql = R"(
                UPDATE deadlines
                SET is_done = 1
                WHERE id = ?
    )";

    QVariantList params;
    params << id;
    if (m_ddlEngine->executeNonQuery(sql, params)) {
        return true;
    } else {
        qWarning() << QString("[DeadlineManager] 标记已完成失败。");
        return false;
    }
}

bool DeadlineManager::ignoreDDL(int id){
    QString sql = R"(
                UPDATE deadlines
                SET is_ignore = 1
                WHERE id = ?
    )";

    QVariantList params;
    params << id;
    if (m_ddlEngine->executeNonQuery(sql, params)) {
        return true;
    } else {
        qWarning() << QString("[DeadlineManager] 标记置灰失败。");
        return false;
    }

}
bool DeadlineManager::unignoreDDL(int id){
    QString sql = R"(
                UPDATE deadlines
                SET is_ignore = 0
                WHERE id = ?
    )";

    QVariantList params;
    params << id;
    if (m_ddlEngine->executeNonQuery(sql, params)) {
        return true;
    } else {
        qWarning() << QString("[DeadlineManager] 标记否置灰失败。");
        return false;
    }

}

int DeadlineManager::addDDL(const DeadlineEntity& DDLEntity){
    QString sql = R"(
        INSERT INTO deadlines
        (title, describe, due_timestamp, tags, is_done, is_ignore)
        VALUES (?, ?, ?, ?, ?, ?)
    )";
    QVariantList params = DDLEntity.toInsertBindValues();

    QSqlQuery query = m_ddlEngine->executeQuery(sql, params); // 执行插入

    if (query.isActive()) {
        int generatedId = query.lastInsertId().toInt();
        qDebug() << "[DeadlineManager] 添加DDL成功，ID:" << generatedId;
        return generatedId;
    }
    return -1; // 插入失败
}

bool DeadlineManager::updateDDL(const DeadlineEntity& DDLEntity){
    QString sql = R"(
        UPDATE deadlines
        SET title = ?, describe = ?, due_timestamp = ?, tags = ?, is_done = ?, is_ignore = ?
        WHERE id = ?
    )";

    QVariantList params = DDLEntity.toUpdateBindValues();

    if (m_ddlEngine->executeNonQuery(sql, params)) {
        qInfo() << "[DeadlineManager] 修改 DDL 成功，ID:" << DDLEntity.id;
        return true;
    }

    qWarning() << "[DeadlineManager] 修改 DDL 失败！ID:" << DDLEntity.id;
    return false;
}

bool DeadlineManager::deleteDDL(int id){
    QString sql = R"(
        DELETE FROM deadlines
        WHERE id = ?
    )";

    QVariantList params;
    params << id;

    if (m_ddlEngine->executeNonQuery(sql, params)) {
        qInfo() << "[DeadlineManager] 删除 DDL 成功，ID:" << id;
        return true;
    }

    qWarning() << "[DeadlineManager] 删除 DDL 失败！ID:" << id;
    return false;

}


bool DeadlineManager::clearLongLongAgo(qint64 saveDuration){
    qint64 timestampSec = QDateTime::currentSecsSinceEpoch();


    QString sql = R"(
        DELETE FROM deadlines
        WHERE due_timestamp < ?
    )";

    QVariantList params;
    params << (timestampSec - saveDuration);

    if (m_ddlEngine->executeNonQuery(sql, params)) {
        qInfo() << "[DeadlineManager] 清除远古DDL成功。";
        return true;
    }

    qWarning() << "[DeadlineManager] 清除远古DDL失败！";
    return false;

}


