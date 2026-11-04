#pragma once
#include "helpsqliteioengine.h"
#include "datatype.h"
#include "advancedfilter.h"
#include <QObject>
#include <QJsonObject>
class MailManager : public QObject {
    Q_OBJECT
public:
    static MailManager *getInstance();
    void initialize();
    QList<MailBrief> query(const QString &text,const QString &account,int state,int limit,int offset=0);
    QList<MailBrief> queryFiltered(const AdvancedFilter &filter,int state,int limit,int offset=0);
    qint64 unreadCount() const;
    void checkMail(QString address,QString uid);
    void addMail(const MailBrief &mail);
    void deleteMail(const QString &address,const QString &uid);
    bool restoreMail(const QString &address,const QString &uid,const QJsonObject &message);
    bool isDeleted(const QString &address,const QString &uid) const;
    void setStar(const QString &address,const QString &uid,bool starred);
    QString getMailBody(const QString &address,const QString &uid);
    void updateMailBody(const QString &address,const QString &uid,const QString &body);
    QJsonObject syncState(const QString &address);
    bool saveBatch(const QString &address,const QJsonObject &data,bool updateSync=true);
    QStringList pendingSeen(const QString &address);
    bool acknowledgeSeen(const QString &address,const QStringList &ids);
    qint64 oldestUid(const QString &address,const QString &validity);
    void clearUp();
signals:
    void changed();
private:
    explicit MailManager(QObject *parent=nullptr):QObject(parent) {}
    HelpSQLiteIOEngine *m_mailEngine=nullptr;
};
