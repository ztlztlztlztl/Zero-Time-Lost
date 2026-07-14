#ifndef MAILSIMPLEDISPLAYWIDGET_H
#define MAILSIMPLEDISPLAYWIDGET_H

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

signals:
    void refreshRequest();




private:
    // ui们
    QScrollArea *m_mailScrollArea;
    QLabel *m_generalTitle;
    QPushButton *m_forceRefreshBtn;
    QPushButton *m_localMailBtn;














};

#endif // MAILSIMPLEDISPLAYWIDGET_H
