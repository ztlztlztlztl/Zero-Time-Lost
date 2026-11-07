#pragma once
#include "uipalette.h"
#include <QtWidgets>
inline void addPageHeader(QVBoxLayout *layout,const QString &name){
    auto title=new QLabel(name);title->setObjectName("pageTitle");title->setTextFormat(Qt::PlainText);
    title->setStyleSheet(UiPalette::style("font-size:26px;font-weight:600;color:#eeeeee;background:transparent;"));
    layout->addWidget(title,0,Qt::AlignLeft);
    const auto accent=name=="Mail"?UiPalette::Mail:name=="Calendar"?UiPalette::Calendar:name=="Note"?UiPalette::Note:UiPalette::Settings;
    auto marks=new QWidget;marks->setFixedSize(104,3);marks->setAttribute(Qt::WA_TransparentForMouseEvents);
    marks->setStyleSheet(QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %1,stop:.70 %1,stop:.71 #101216,stop:.77 #101216,stop:.78 %2,stop:1 %2);").arg(accent.name(),UiPalette::Home.name()));
    auto heading=new QVBoxLayout;heading->setContentsMargins(0,0,0,2);heading->setSpacing(8);
    layout->removeWidget(title);heading->addWidget(title,0,Qt::AlignLeft);heading->addWidget(marks,0,Qt::AlignLeft);layout->addLayout(heading);
}
