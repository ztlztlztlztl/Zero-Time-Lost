#pragma once
#include <QWidget>
#include <QDateTime>
#include "calendarrules.h"
class QLabel;class QTimer;
class HomeInfoWidget:public QWidget {
    Q_OBJECT
public:
    explicit HomeInfoWidget(QWidget *parent=nullptr);
    void refreshDateTime(const QDateTime &now=QDateTime::currentDateTime());
    void setFlowTime(qreal seconds);
    void setCalendarContext(const QStringList &labels,const QStringList &intervals);
    void setCalendarItems(const QList<TaskOccurrence> &items);
signals:
    void localTimeChanged(const QTime &time);
    void dayChanged();
    void itemRequested(const QString &identity,const QDate &date);
    void calendarItemsChanged();
protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
private:
    QLabel *date,*clock,*labels,*intervals;
    QTimer *timer;
    QDate displayedDay;
    QList<TaskOccurrence> calendarItems;
};
