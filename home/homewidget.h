#pragma once
#include <QWidget>
#include <QDate>
#include "calendarrules.h"
class HomeDepartment;class HomeBackgroundShader;class HomeInfoWidget;class QLabel;
class HomeWidget:public QWidget {
    Q_OBJECT
public:
    explicit HomeWidget(HomeDepartment *department,QWidget *parent=nullptr);
    void setSummary(const QDate &date,int today,int tomorrow,int overdue,qint64 unread,bool available=true);
    void setCalendarContext(const QStringList &labels,const QStringList &intervals);
    void setCalendarItems(const QList<TaskOccurrence> &items);
signals:
    void calendarItemRequested(const QString &identity,const QDate &date);
    void ddlRequested(const QDate &date);
    void mailRequested();
    void summaryRefreshRequested();
protected:
    void showEvent(QShowEvent *event) override;
private:
    void updateWeather(const QString &name,const QString &temperature,const QString &description);
    void updateSummaryText();
    HomeBackgroundShader *background;
    HomeInfoWidget *info;
    QLabel *location,*temperature,*weather,*summary;
    QString hoveredLink;
    QDate summaryDate;
    int todayCount=0,tomorrowCount=0,overdueCount=0;
    qint64 unreadCount=-1;
    bool summaryAvailable=false;
};
