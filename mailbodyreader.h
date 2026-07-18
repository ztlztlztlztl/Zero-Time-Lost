#ifndef MAILBODYREADER_H
#define MAILBODYREADER_H

#include "datatype.h"

#include <QWidget>
#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>

class MailBodyReader : public QWidget
{
    Q_OBJECT
public:
    explicit MailBodyReader(QWidget *parent = nullptr);

    void loadMail(const MailBrief &mailBrief, const QString &htmlBody);
    void loadMail(const QString &htmlBody);

signals:



private:
    QLabel *m_lblSubject;
    QLabel *m_lblAddress;
    QLabel *m_lblSender;
    QLabel *m_lblDate;
    QTextBrowser *m_textBrowser;


};

#endif // MAILBODYREADER_H
