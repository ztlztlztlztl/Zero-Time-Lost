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
    m_readingPart = new QStackedWidget();
    m_lblReadingText = new QLabel("等待选择邮件阅读");
    m_mailBodyReader = new MailBodyReader(this);
    m_readingPart->addWidget(m_lblReadingText);
    m_readingPart->addWidget(m_mailBodyReader);
    m_splitter->addWidget(m_mailSimpleDisplay);
    m_splitter->addWidget(m_readingPart);
    m_splitter->setSizes(QList<int>() << 100 << 200);
    mainLayout->addWidget(m_splitter);




    connect(m_mailSimpleDisplay, &MailSimpleDisplayWidget::refreshRequest, this, [=](){
        m_mailGetService->mailPolling();
    });
    connect(m_mailGetService, &MailGetService::newMailBrief, this, [=](const MailBrief& newMailBrief){
        qDebug() << "[MailWidget] 添加新邮件简报。";
        MailManager::getInstance()->addMail(newMailBrief);
        m_mailSimpleDisplay->addMailBrief(2, newMailBrief);
    });
    connect(m_mailGetService, &MailGetService::pollingOver, this, [=](){
        m_mailSimpleDisplay->enablePollingBtn();
    });
    connect(m_mailSimpleDisplay, &MailSimpleDisplayWidget::readRequest, this,
            [=](const MailAccount& acc, const MailBrief& brf){
        QString body = MailManager::getInstance()->getMailBody(brf.ownerEmail, brf.uid);
        if (!body.isEmpty()){
            m_mailBodyReader->loadMail(brf, body);
            m_readingPart->setCurrentIndex(1);
        }
        else {
            m_lblReadingText->setText("正在获取邮件正文，请稍等...");
            m_readingPart->setCurrentIndex(0);
            m_mailSimpleDisplay->blockSignals(true);
            m_mailBodyReader->loadMail(brf, "");
            m_mailGetService->mailBody(acc, brf.uid);
        }
    });
    connect(m_mailGetService, &MailGetService::newMailBody, this,
            [=](const QString &address, const QString &uid, const QString &body){
        MailManager::getInstance()->updateMailBody(address, uid, body);
    });
    connect(m_mailGetService, &MailGetService::getBodyOver, this,
            [=](const QString &address, const QString &uid){
                QString body = MailManager::getInstance()->getMailBody(address, uid);
                if (!body.isEmpty()){
                    m_mailBodyReader->loadMail(body);
                    m_readingPart->setCurrentIndex(1);
                }
                else {
                    m_lblReadingText->setText("获取邮件正文失败或正文为空。");
                    m_readingPart->setCurrentIndex(0);
                }
                m_mailSimpleDisplay->blockSignals(false);
            });


}
