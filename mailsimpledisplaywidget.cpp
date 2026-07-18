#include "mailsimpledisplaywidget.h"

MailSimpleDisplayWidget::MailSimpleDisplayWidget(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[MailSimpleDisplayWidget] 启动";
    // 滚动区
    QVBoxLayout *generalLayout = new QVBoxLayout(this);
    generalLayout->setContentsMargins(0, 0, 0, 0);
    m_mailScrollArea = new QScrollArea(this);
    generalLayout->addWidget(m_mailScrollArea);
    QWidget *scrollContent = new QWidget();
    m_contentLyt = new QVBoxLayout(scrollContent);
    m_generalTitle = new QLabel("邮件简报");
    m_contentLyt->addWidget(m_generalTitle);
    m_forceRefreshBtn = new QPushButton("拉取新邮件");
    m_contentLyt->addWidget(m_forceRefreshBtn);
    m_localMailBtn = new QPushButton("更多邮件");
    m_contentLyt->addWidget(m_localMailBtn);
    m_contentLyt->addStretch();




    // 收尾
    m_mailScrollArea->setWidget(scrollContent);
    m_mailScrollArea->setWidgetResizable(true);

    // 本地邮件初始化
    int initNumber = ConfigManager::getInstance()->getMailBasical().initialNumber;
    QList<MailBrief> initList = MailManager::getInstance()->initMailList(initNumber);
    for (const MailBrief& mail: initList){
        addMailBrief(2, mail);
    }

    connect(m_forceRefreshBtn, &QPushButton::clicked, this, [=](){
        emit refreshRequest();
        m_forceRefreshBtn->setDisabled(true);
        m_forceRefreshBtn->setText("轮询中。。。");
    });
    connect(m_localMailBtn, &QPushButton::clicked, this, [=](){
        int startNumber = m_contentLyt->count() - 3;
        int number = ConfigManager::getInstance()->getMailBasical().earlierNumber;
        QList<MailBrief> initList = MailManager::getInstance()->moreMailList(startNumber, initNumber);
        for (const MailBrief& mail: initList){
            addMailBrief(m_contentLyt->count() - 1, mail);
        }
    });
}

void MailSimpleDisplayWidget::addMailBrief(int location, const MailBrief& newMailBrief){
    QString name = MailAccountManager::getInstance()->getAddressName(newMailBrief.ownerEmail);
    MailSimpleCard *newMail = new MailSimpleCard(newMailBrief, name, this);
    m_contentLyt->insertWidget(location, newMail);
    connect(newMail, &MailSimpleCard::readRequest, this, [=](){
        qDebug() << "[MailSimpleDisplayWidget] 阅读" << name << " " << newMailBrief.subject;
        MailAccount acc = MailAccountManager::getInstance()->getMailAcc(newMailBrief.ownerEmail);
        emit readRequest(acc, newMailBrief);
    });
    connect(newMail, &MailSimpleCard::checkRequest, this, [=](){
        qDebug() << "[MailSimpleDisplayWidget] 已读" << name << " " << newMailBrief.subject;
        MailManager::getInstance()->checkMail(newMailBrief.ownerEmail, newMailBrief.uid);
    });
    connect(newMail, &MailSimpleCard::deleteRequest, this, [=](){
        qDebug() << "[MailSimpleDisplayWidget] 删除" << name << " " << newMailBrief.subject;
        MailManager::getInstance()->deleteMail(newMailBrief.ownerEmail, newMailBrief.uid);
        newMail->deleteLater();
    });
}

void MailSimpleDisplayWidget::enablePollingBtn(){
    m_forceRefreshBtn->setDisabled(false);
    m_forceRefreshBtn->setText("拉取新邮件");
}



