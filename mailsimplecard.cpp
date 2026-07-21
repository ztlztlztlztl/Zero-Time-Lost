#include "mailsimplecard.h"

MailSimpleCard::MailSimpleCard(const MailBrief& mailBrief, QString name, QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *mainLyt = new QVBoxLayout(this);
    m_lblAccount = new QLabel(name);
    m_lblAccount->setStyleSheet(ZTL::Mail::list::LblAccountStyle);
    m_lblSender = new QLabel(QString("·sender: %1").arg(mailBrief.sender));
    m_lblSender->setStyleSheet(ZTL::Mail::list::LblInfoStyle);
    m_lblSubject = new ElidedLabel();
    m_lblSubject->setFullText(QString("·subject: %1").arg(mailBrief.subject));
    m_lblSubject->setStyleSheet(ZTL::Mail::list::LblInfoStyle);
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(mailBrief.timestamp);
    m_lblTime = new QLabel(QString("·time: %1").arg(dateTime.toString("yyyy-MM-dd hh:mm:ss")));
    m_lblTime->setStyleSheet(ZTL::Mail::list::LblInfoStyle);
    m_checkBtn = new QPushButton("check");
    m_checkBtn->setStyleSheet(ZTL::Mail::list::CheckBtnStyle);
    m_deleteBtn = new QPushButton("delete");
    m_deleteBtn->setStyleSheet(ZTL::Mail::list::DeleteBtnStyle);
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










