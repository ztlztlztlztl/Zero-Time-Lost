#pragma once
#include <QtWidgets>
// Editors are modeless in phase 1; confirmations remain modal.
inline QWidget *activeTestDialog(){
    if(auto modal=QApplication::activeModalWidget())return modal;
    if(auto dialog=qobject_cast<QDialog*>(QApplication::activeWindow()))if(dialog->isVisible())return dialog;
    for(auto widget:QApplication::topLevelWidgets())if(qobject_cast<QDialog*>(widget)&&widget->isVisible())return widget;
    return nullptr;
}
