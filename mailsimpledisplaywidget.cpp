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
    QVBoxLayout *contentLayout = new QVBoxLayout(scrollContent);
    m_generalTitle = new QLabel("邮件简报");
    contentLayout->addWidget(m_generalTitle);
    m_forceRefreshBtn = new QPushButton("拉取新邮件");
    contentLayout->addWidget(m_forceRefreshBtn);
    m_localMailBtn = new QPushButton("更多邮件");
    contentLayout->addWidget(m_localMailBtn);
    contentLayout->addStretch();




    // 收尾
    m_mailScrollArea->setWidget(scrollContent);
    m_mailScrollArea->setWidgetResizable(true);

    connect(m_forceRefreshBtn, &QPushButton::clicked, this, [=](){
        emit refreshRequest();
    });
}
