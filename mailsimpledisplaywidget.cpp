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
        addNewMailBrief(mail);
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
            addNewMailBrief(mail);
        }
    });
}

void MailSimpleDisplayWidget::addNewMailBrief(const MailBrief& newMailBrief){
    QString name = MailAccountManager::getInstance()->getAddressName(newMailBrief.ownerEmail);
    QString str = name + "|" + newMailBrief.sender + "|" + newMailBrief.uid;
    QLabel *newMail = new QLabel(str);
    m_contentLyt->insertWidget(2, newMail);
}

void MailSimpleDisplayWidget::enablePollingBtn(){
    m_forceRefreshBtn->setDisabled(false);
}



