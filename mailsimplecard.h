#ifndef MAILSIMPLECARD_H
#define MAILSIMPLECARD_H

#include "datatype.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDateTime>
#include <QEvent>
#include <QMouseEvent>

class MailSimpleCard : public QWidget
{
    Q_OBJECT
public:
    explicit MailSimpleCard(const MailBrief& mailBrief, QString name, QWidget *parent = nullptr);

signals:
    void readRequest();
    void checkRequest();
    void deleteRequest();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;


private:
    QLabel *m_lblAccount;
    QPushButton *m_checkBtn;
    QPushButton *m_deleteBtn;
    QLabel *m_lblSender;
    QLabel *m_lblSubject;
    QLabel *m_lblTime;











};

#endif // MAILSIMPLECARD_H
