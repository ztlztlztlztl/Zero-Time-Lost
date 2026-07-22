#ifndef DEADLINEWIDGET_H
#define DEADLINEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QStackedWidget>

class DeadlineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeadlineWidget(QWidget *parent = nullptr);




signals:

private slots:




private:
    QSplitter *m_splitter;
    QStackedWidget *m_leftStack;



};

#endif // DEADLINEWIDGET_H
