#pragma once
#include <QtWidgets>
// Editors are modeless in phase 1; confirmations remain modal.
inline QWidget *activeTestDialog(){
    if(auto modal=QApplication::activeModalWidget())return modal;
    if(auto dialog=qobject_cast<QDialog*>(QApplication::activeWindow()))if(dialog->isVisible())return dialog;
    for(auto widget:QApplication::topLevelWidgets())if(qobject_cast<QDialog*>(widget)&&widget->isVisible())return widget;
    return nullptr;
}

inline bool applyTestFilter(QWidget &page,const char *buttonName,const QString &words={},const QString &tag={},int star=-1){
    auto button=page.findChild<QPushButton*>(buttonName);if(!button)return false;
    if(button->text()=="关闭筛选")button->click();
    bool applied=false;
    QTimer::singleShot(0,&page,[&]{
        auto dialog=page.findChild<QDialog*>("advancedFilter");if(!dialog)return;
        dialog->findChild<QLineEdit*>("filterKeywords")->setText(words);
        if(auto combo=dialog->findChild<QComboBox*>("filterStar"))combo->setCurrentIndex(combo->findData(star));
        if(!tag.isEmpty())if(auto list=dialog->findChild<QListWidget*>("ddlAvailableTags"))
            for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)==tag){list->itemClicked(list->item(i));break;}
        applied=true;dialog->findChild<QPushButton*>("filterApply")->click();
    });
    button->click();return applied;
}
