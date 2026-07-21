#ifndef MAILWIDGET_H
#define MAILWIDGET_H

#include "mailgetservice.h"
#include "mailsimpledisplaywidget.h"
#include "mailmanager.h"
#include "mailbodyreader.h"
#include "constants.h"

#include <QWidget>
#include <QLabel>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>

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
    QStackedWidget *m_readingPart;
    QLabel *m_lblReadingText;
    MailBodyReader *m_mailBodyReader;



    // 信息们
    MailGetService *m_mailGetService;





};

#endif // MAILWIDGET_H
