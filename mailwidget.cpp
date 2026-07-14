#include "mailwidget.h"

MailWidget::MailWidget(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[MailWidget] 启动。";
    m_mailGetService = new MailGetService(this);

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


}
