#pragma once
#include "mailaccountmanager.h"
#include "datatype.h"
#include "processworker.h"
#include <QObject>
class MailGetService : public QObject {
    Q_OBJECT
public:
    explicit MailGetService(QObject *parent=nullptr);
    void mailPolling();
    void mailHistory();
    void mailBody(const MailAccount &account,const QString &id);
    void restoreMail(const QString &address,const QString &id);
    void stop(){worker.stop();busy=false;restoring.clear();}
signals:
    void pollingOver(bool haveNew);
    void newMailBody(const QString &address,const QString &id,const QString &body);
    void getBodyOver(const QString &address,const QString &id);
    void bodyFailed(const QString &address,const QString &id,const QString &message);
    void status(const QString &message);
    void restoreFinished(const QString &address,const QString &id,bool success);
private:
    void nextAccount();
    ProcessWorker worker;
    QList<MailAccount> accounts;
    int index=0;
    bool busy=false,found=false,historyMode=false;
    QStringList errors;
    QSet<QString> restoring;
};
