#include "mailwidget.h"

MailWidget::MailWidget(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[MailWidget] 启动。";
    // service 创建
    m_mailGetService = new MailGetService(this);
    // ui初始化
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_mailSimpleDisplay = new MailSimpleDisplayWidget();
    QLabel *test2 = new QLabel("右页", m_splitter);
    m_splitter->addWidget(m_mailSimpleDisplay);
    m_splitter->addWidget(test2);
    m_splitter->setSizes(QList<int>() << 100 << 300);
    mainLayout->addWidget(m_splitter);




    connect(m_mailSimpleDisplay, &MailSimpleDisplayWidget::refreshRequest, this, [=](){
        m_mailGetService->mailPolling();
    });
    connect(m_mailGetService, &MailGetService::newMailBrief, this, [=](const MailBrief& newMailBrief){
        qDebug() << "[MailWidget] 添加新邮件简报。";
        MailManager::getInstance()->addMail(newMailBrief);
        m_mailSimpleDisplay->addNewMailBrief(newMailBrief);
    });
    connect(m_mailGetService, &MailGetService::pollingOver, this, [=](){
        m_mailSimpleDisplay->enablePollingBtn();
    });


}
