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
    QString dateSource="header";
    QString dateText() const {return dateSource=="unknown"?QString("时间未知"):QDateTime::fromSecsSinceEpoch(timestamp).toString("yyyy-MM-dd HH:mm:ss")+(dateSource=="internal"?"（收件服务器时间）":dateSource=="received"?"（首次收取时间，原发信时间未知）":"");}
    qint64 timestamp; // 收件时间
    bool isRead; // 本地阅读状态
    bool starred = false;

    MailBrief() : timestamp(0), isRead(false){}

    MailBrief(const QJsonObject &json, const QString &owner) {
        ownerEmail = owner;
        uid = json.value("id").toString();
        sender = json.value("from").toString();
        subject = json.value("subject").toString();
        timestamp = json.value("date").toVariant().toLongLong();
        dateSource=json.value("dateSource").toString("header");
        isRead = false;
    }

    MailBrief(const QSqlQuery &query) {
        ownerEmail = query.value("owner_email").toString();
        uid = query.value("uid").toString();
        sender = query.value("sender").toString();
        subject = query.value("subject").toString();
        timestamp = query.value("timestamp").toLongLong();
        dateSource=query.value("date_source").toString();if(dateSource.isEmpty())dateSource="header";
        isRead = query.value("isRead").toBool();
        starred = query.value("starred").toBool();
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
