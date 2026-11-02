#pragma once
#include "datatype.h"
#include <QWidget>
class QLabel;class QTextBrowser;class QComboBox;class QStackedWidget;class QPushButton;

class MailBodyReader:public QWidget {
    Q_OBJECT
public:
    explicit MailBodyReader(QWidget *parent=nullptr);
    void loadMail(const MailBrief &brief,const QString &htmlBody);
    void loadMail(const QString &htmlBody);
    void showLoading();
    void showError(const QString &message);
    QString plainText() const {return text;}
signals:
    void retryRequested();
private:
    void render();
    QLabel *subject,*sender,*recipient,*date,*stateText;
    QTextBrowser *browser;
    QComboBox *mode;
    QStackedWidget *bodyStack;
    QPushButton *retry;
    QString source,text,identity;
};
