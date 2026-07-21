#include "deadlinewidget.h"

DeadlineWidget::DeadlineWidget(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[DeadlineWidget] 启动。";
    QHBoxLayout *mainLyt = new QHBoxLayout(this);
    test = new QLabel("hello,ztl.This is DDL page.");
    mainLyt->addWidget(test);
}
