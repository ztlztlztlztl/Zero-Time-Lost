#include "deadlinewidget.h"

DeadlineWidget::DeadlineWidget(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[DeadlineWidget] 启动。";
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_leftStack = new QStackedWidget();
    QLabel *leftHolder = new QLabel("this is console");
    m_leftStack->addWidget(leftHolder);
    QLabel *rightHolder = new QLabel("this is timemap");
    m_splitter->addWidget(m_leftStack);
    m_splitter->addWidget(rightHolder);
    m_splitter->setSizes(QList<int>() << 100 << 300);
    mainLayout->addWidget(m_splitter);





}
