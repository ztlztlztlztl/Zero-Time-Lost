#ifndef MAILWIDGET_H
#define MAILWIDGET_H

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
    QSplitter *m_splitter;









};

#endif // MAILWIDGET_H
