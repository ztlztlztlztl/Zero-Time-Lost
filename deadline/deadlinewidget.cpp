#include "deadlinewidget.h"

DeadlineWidget::DeadlineWidget(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[DeadlineWidget] 启动。";
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    QLabel *test = new QLabel("this is deadlineWidget.", this);
    mainLayout->addWidget(test);




}
