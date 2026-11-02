#pragma once
#include "ddldepartment.h"
#include <QWidget>
class QLineEdit;class QTabBar;class QCheckBox;class QStackedWidget;class QComboBox;class QLabel;class QPushButton;class DeadlineCalendarWidget;class DeadlineAgendaWidget;
class DeadlineWidget:public QWidget {
    Q_OBJECT
public:
    explicit DeadlineWidget(DDLDepartment *department,QWidget *parent=nullptr);
    void addDraft(const QString &title,const QString &note);
    void refresh();
    void showDay(const QDate &date);
    void prepareLock(){emit locking();}
signals:
    void locking();
    void saveFailed(const QString &message);
private:
    void edit(const TaskOccurrence *item=nullptr);
    void showDetail(const TaskOccurrence &item);
    bool removeTask(const TaskOccurrence &item);
    void changeView(int index);
    void navigate(int direction);
    void updateHeading();
    DDLDepartment *dept;
    QLineEdit *search;
    QTabBar *tabs;
    DeadlineAgendaWidget *agenda;
    QCheckBox *showCompleted;
    QStackedWidget *pages;
    DeadlineCalendarWidget *calendar;
    QComboBox *calendarTag,*calendarStar;
    QLabel *heading;
    QPushButton *clearFilters;
    QDate anchor=QDate::currentDate();
    bool focusedDayInstances=false,pendingFocus=false;
    bool refreshing=false;
    QList<TaskOccurrence> visible;
};
