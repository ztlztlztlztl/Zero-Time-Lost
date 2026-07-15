#ifndef MAILGETSERVICE_H
#define MAILGETSERVICE_H

#include "configmanager.h"
#include "mailaccountmanager.h"
#include "datatype.h"



#include <QObject>
#include <QProcess>
#include <QtConcurrent>
#include <QFutureWatcher>

class MailGetService : public QObject
{
    Q_OBJECT
public:
    explicit MailGetService(QObject *parent = nullptr);
    void mailPolling();

signals:
    void pollingOver();
    void newMailBrief(const MailBrief& newMailbrief);

private:


    bool useGetMail(const MailAccount& acc);
    QFutureWatcher<bool> *m_watcher; // 线程观察器




};

#endif // MAILGETSERVICE_H
