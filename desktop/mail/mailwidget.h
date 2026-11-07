#pragma once
#include "mailgetservice.h"
#include "mailbodyreader.h"
#include "mailsimpledisplaywidget.h"
#include <QWidget>
class QStackedWidget;class QLabel;class QPushButton;class QGridLayout;
class MailWidget:public QWidget {
    Q_OBJECT
public:
    explicit MailWidget(MailGetService *department,QWidget *parent=nullptr);
    void showUnread();
signals:
    void taskDraft(const QString &title,const QString &text);
    void noteDraft(const QString &title,const QString &text);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void requestBody();
    void arrangeToolbar();
    MailGetService *m_mailGetService;
    MailSimpleDisplayWidget *m_mailSimpleDisplay;
    MailBodyReader *m_mailBodyReader;
    QStackedWidget *pages;
    QGridLayout *toolbar;
    QWidget *draftActions,*mailActions;
    QPushButton *backButton,*starButton,*deleteButton,*taskButton,*noteButton;
    QLabel *syncStatus;
    MailBrief selected;
    QString bodyError,temporaryBody;
    bool temporaryBodyAvailable=false;
};
