#include "mailsimplecard.h"

MailSimpleCard::MailSimpleCard(const MailBrief& mailBrief, QString name, QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *mainLyt = new QVBoxLayout(this);
    m_lblAccount = new QLabel(name);
    m_lblSender = new QLabel(mailBrief.sender);
    m_lblSubject = new QLabel(mailBrief.subject);
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(mailBrief.timestamp);
    m_lblTime = new QLabel(dateTime.toString("yyyy-MM-dd hh:mm:ss"));
    m_checkBtn = new QPushButton("check");
    m_deleteBtn = new QPushButton("delete");
    QHBoxLayout *lastLineLyt = new QHBoxLayout();
    if (!mailBrief.isRead) lastLineLyt->addWidget(m_checkBtn);
    lastLineLyt->addWidget(m_deleteBtn);
    mainLyt->addWidget(m_lblAccount);
    mainLyt->addWidget(m_lblSender);
    mainLyt->addWidget(m_lblSubject);
    mainLyt->addWidget(m_lblTime);
    mainLyt->addLayout(lastLineLyt);


    connect(m_checkBtn, &QPushButton::clicked, this, [=](){
        m_checkBtn->setText("checked");
        m_checkBtn->setDisabled(true);
        emit checkRequest();
    });
    connect(m_deleteBtn, &QPushButton::clicked, this, [=](){
        emit deleteRequest();
    });




}

void MailSimpleCard::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_checkBtn->setText("checked");
        m_checkBtn->setDisabled(true);
        emit checkRequest();
        emit readRequest();
        event->accept();
    }
}










