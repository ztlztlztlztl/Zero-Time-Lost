#ifndef DEADLINEWIDGET_H
#define DEADLINEWIDGET_H



#include "deadlineaddwidget.h"
#include "deadlinemanager.h"
#include "deadlinetimemapview.h"

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

    QLabel *m_waittingPage;
    DeadlineAddWidget *m_deadlineAddWidget;

    DeadlineTimeMapView *m_mapView;

    int m_action;
    // 0 search, 1 add, 2 modyfy





};

#endif // DEADLINEWIDGET_H
