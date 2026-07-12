#include "mailwidget.h"

MailWidget::MailWidget(QWidget *parent)
    : QWidget{parent}
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    m_splitter = new QSplitter(Qt::Horizontal, this);
    QLabel *test1 = new QLabel("左页", m_splitter);
    QLabel *test2 = new QLabel("右页", m_splitter);
    m_splitter->addWidget(test1);
    m_splitter->addWidget(test2);
    m_splitter->setSizes(QList<int>() << 100 << 300);
    mainLayout->addWidget(m_splitter);
}
