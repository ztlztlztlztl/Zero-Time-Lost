#pragma once
#include <QtWidgets>
inline void addPageHeader(QVBoxLayout *layout,const QString &name){
    auto title=new QLabel(name);title->setObjectName("pageTitle");title->setTextFormat(Qt::PlainText);
    title->setStyleSheet("font-size:26px;font-weight:600;color:#eeeeee;background:transparent;");
    layout->addWidget(title,0,Qt::AlignLeft);
}
