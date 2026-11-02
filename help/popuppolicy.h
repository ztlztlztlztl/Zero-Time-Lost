#pragma once
#include <QtWidgets>
#include <functional>
// Only clicks inside the owning application dismiss an ordinary editor.
// App deactivation leaves it open; owned menus and modal prompts stay interactive.
class PopupPolicy:public QObject {
public:
    PopupPolicy(QDialog *dialog,std::function<void()> dismiss)
        :QObject(dialog),dialog(dialog),dismiss(std::move(dismiss)) {qApp->installEventFilter(this);}
    ~PopupPolicy() override {qApp->removeEventFilter(this);}
    bool eventFilter(QObject *object,QEvent *event) override {
        if(!dialog->isVisible())return false;
        auto widget=qobject_cast<QWidget*>(object);
        auto owner=dialog->parentWidget();
        if(!widget||!owner)return false;
        for(auto p=widget;p;p=p->parentWidget())if(p==dialog)return false;
        // A page switch or close-to-tray must also finish the nested editor loop.
        if(event->type()==QEvent::Hide||event->type()==QEvent::Close){
            for(auto p=owner;p;p=p->parentWidget())if(widget==p){dismiss();return false;}
        }
        if(event->type()!=QEvent::MouseButtonPress||widget->window()!=owner->window()
            ||QApplication::activeModalWidget()||pending)return false;
        pending=true;
        QTimer::singleShot(0,this,[this]{pending=false;if(dialog->isVisible())dismiss();});
        return false;
    }
    static int exec(QDialog &dialog){
        QEventLoop loop;
        QObject::connect(&dialog,&QDialog::finished,&loop,&QEventLoop::quit);
        auto policy=new PopupPolicy(&dialog,[&dialog]{dialog.setProperty("outsideDismiss",true);dialog.reject();});
        dialog.setWindowModality(Qt::NonModal);dialog.show();loop.exec();delete policy;
        return dialog.result();
    }
private:
    QDialog *dialog;
    std::function<void()> dismiss;
    bool pending=false;
};
