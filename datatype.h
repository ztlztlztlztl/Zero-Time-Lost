#ifndef DATATYPE_H
#define DATATYPE_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QVariant>
#include <QSqlQuery>

struct weatherCity{
    QString name;
    QString code;
};


struct MailBrief {
    QString ownerEmail; // 归属账号
    QString uid; // 在这个账号下的uid

    QString sender; // 发件人
    QString subject; // 邮件主题
    qint64 timestamp; // 收件时间
    bool isRead; // 本地阅读状态

    MailBrief() : timestamp(0), isRead(false){}

    MailBrief(const QJsonObject &json, const QString &owner) {
        ownerEmail = owner;
        uid = json.value("id").toString();
        sender = json.value("from").toString();
        subject = json.value("subject").toString();
        timestamp = json.value("date").toVariant().toLongLong();
        isRead = false;
    }

    MailBrief(const QSqlQuery &query) {
        ownerEmail = query.value("owner_email").toString();
        uid = query.value("uid").toString();
        sender = query.value("sender").toString();
        subject = query.value("subject").toString();
        timestamp = query.value("timestamp").toLongLong();
        isRead = query.value("isRead").toBool();
    }

    QVariantList toBindValues() const {
        return QVariantList()
        << ownerEmail
        << uid
        << sender
        << subject
        << timestamp
        << (isRead ? 1 : 0); // bool 转 int
    }
};

struct DeadlineEntity {
    int id; // ID
    QString title; // 标题
    QString describe; // 详细备注
    qint64 dueTimestamp; // 截止时间戳 (秒级),最小间隔为分
    QList<QString> tags; // DDL的tag
    bool isDone; // 是否已完成
    bool isIgnore; // 是否置灰

    DeadlineEntity() : id(0), dueTimestamp(0), isDone(false), isIgnore(false) {}

    DeadlineEntity(const QSqlQuery &query) {
        id = query.value("id").toInt();
        title = query.value("title").toString();
        describe = query.value("describe").toString();
        dueTimestamp = query.value("due_timestamp").toLongLong();
        tags = query.value("tags").toString().split("|", Qt::SkipEmptyParts);
        isDone = query.value("is_done").toBool();
        isIgnore = query.value("is_ignore").toBool();
    }

    QVariantList toInsertBindValues() const {
        return QVariantList()
        << title
        << describe
        << dueTimestamp
        << tags.join("|")
        << (isDone ? 1 : 0)
        << (isIgnore ? 1 : 0);
    }

    QVariantList toUpdateBindValues() const {
        QVariantList values = toInsertBindValues();
        values << id;
        return values;
    }
};
































#endif // DATATYPE_H
