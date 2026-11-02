#pragma once
#include "datatype.h"
#include "mailaccountmanager.h"
#include <QWidget>
#include <QSet>
class QTreeWidget;class QTabBar;class QPushButton;class QLineEdit;class QComboBox;class QLabel;
class MailSimpleDisplayWidget:public QWidget{
    Q_OBJECT
public:
    explicit MailSimpleDisplayWidget(QWidget *parent=nullptr);
    void enablePollingBtn();
    void reload();
    void showUnread();
    void restoreFinished(const QString &address,const QString &id,bool success);
signals:
    void refreshRequest();
    void historyRequest();
    void readRequest(const MailAccount &,const MailBrief &);
    void restoreRequest(const QString &address,const QString &id);
private:
    QTreeWidget *rows;
    QTabBar *tabs;
    QPushButton *refresh,*more;
    QLineEdit *search;
    QComboBox *account;
    QLabel *empty;
    QList<MailBrief> visible;
    int loaded=10;
    QSet<QString> pendingRestores;
};
