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







#endif // DATATYPE_H
