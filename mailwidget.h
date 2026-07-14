#ifndef MAILWIDGET_H
#define MAILWIDGET_H

#include "mailgetservice.h"
#include "mailsimpledisplaywidget.h"

#include <QWidget>
#include <QLabel>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>

class MailWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MailWidget(QWidget *parent = nullptr);

signals:



private:
    // ui们
    QSplitter *m_splitter;
    MailSimpleDisplayWidget *m_mailSimpleDisplay;



    // 信息们
    MailGetService *m_mailGetService;





};

#endif // MAILWIDGET_H
