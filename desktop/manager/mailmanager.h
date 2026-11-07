#pragma once
#include "advancedfilter.h"
#include "datatype.h"
#include "helpsqliteioengine.h"
#include <QJsonObject>
#include <QObject>
class MailManager : public QObject {
    Q_OBJECT
  public:
    static MailManager *getInstance();
    bool initialize();
    bool ready() const {return initialized;}
    QList<MailBrief> query(const QString &text, const QString &account, int state, int limit, int offset = 0);
    QList<MailBrief> queryFiltered(const AdvancedFilter &filter, int state, int limit, int offset = 0);
    qint64 unreadCount() const;
    bool checkMail(QString address, QString uid);
    bool addMail(const MailBrief &mail);
    bool deleteMail(const QString &address, const QString &uid);
    bool restoreMail(const QString &address, const QString &uid, const QJsonObject &message);
    bool isDeleted(const QString &address, const QString &uid) const;
    bool setStar(const QString &address, const QString &uid, bool starred);
    QString getMailBody(const QString &address, const QString &uid);
    bool updateMailBody(const QString &address, const QString &uid, const QString &body);
    bool bodyLoaded(const QString &address, const QString &uid);
    void pinBody(const QString &address = {}, const QString &uid = {});
    bool pruneBodies();
    QString error() const;
    bool querySucceeded() const { return lastQueryOk; }
    QJsonObject syncState(const QString &address);
    bool saveBatch(const QString &address, const QJsonObject &data, bool updateSync = true);
    QStringList pendingSeen(const QString &address);
    bool acknowledgeSeen(const QString &address, const QStringList &ids);
    qint64 oldestUid(const QString &address, const QString &validity);
    void clearUp();
    QJsonArray cloudFlags(bool *ok = nullptr) const;
    QJsonObject cloudFlag(const QString &address, const QString &uid, bool *ok = nullptr) const;
    bool applyCloudFlags(const QJsonObject &flags, bool notify = true);
  signals:
    void changed();
    void flagsChanged(const QString &address, const QString &uid);
    void storageError(const QString &message);

  private:
    explicit MailManager(QObject *parent = nullptr) : QObject(parent) {}
    HelpSQLiteIOEngine *m_mailEngine = nullptr;
    bool initializing=false,initialized=false;
    QString initializationError;
    QString pinnedAddress, pinnedUid;
    bool lastQueryOk = true;
};
