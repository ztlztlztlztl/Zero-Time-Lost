#ifndef HOMEQUOTESERVICE_H
#define HOMEQUOTESERVICE_H

#include "cachemanager.h"
#include "configmanager.h"

#include <QObject>
#include <QDateTime>
#include <QDate>
#include <QDebug>
#include <QProcess>
#include <Qtimer>

class HomeQuoteService : public QObject
{
    Q_OBJECT
public:
    explicit HomeQuoteService(QObject *parent = nullptr);

    void updateQuote();

signals:
    void updateQuoteRequest(QString content);




private:
    bool useQuoteAPI();

    QTimer *m_timer;  // 🚀 计时器指针
    QDate m_lastRecordDate;


};

#endif // HOMEQUOTESERVICE_H
