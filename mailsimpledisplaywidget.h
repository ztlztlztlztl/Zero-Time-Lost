#ifndef MAILSIMPLEDISPLAYWIDGET_H
#define MAILSIMPLEDISPLAYWIDGET_H

#include "datatype.h"
#include "configmanager.h"
#include "mailmanager.h"
#include "mailaccountmanager.h"

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>

class MailSimpleDisplayWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MailSimpleDisplayWidget(QWidget *parent = nullptr);
    void addNewMailBrief(const MailBrief& newMailBrief);
    void enablePollingBtn();

signals:
    void refreshRequest();




private:
    // ui们
    QScrollArea *m_mailScrollArea;
    QVBoxLayout *m_contentLyt;
    QLabel *m_generalTitle;
    QPushButton *m_forceRefreshBtn;
    QPushButton *m_localMailBtn;














};

#endif // MAILSIMPLEDISPLAYWIDGET_H
