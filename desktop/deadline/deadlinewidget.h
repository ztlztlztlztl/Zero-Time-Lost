#pragma once
#include "ddldepartment.h"
#include "advancedfilter.h"
#include <QWidget>
#include <atomic>
#include <memory>
namespace CalendarQuery {class Session;}
class QLineEdit;class QTabBar;class QCheckBox;class QStackedWidget;class QComboBox;class QLabel;class QPushButton;class DeadlineCalendarWidget;class DeadlineAgendaWidget;
class DeadlineWidget:public QWidget {
    Q_OBJECT
public:
    explicit DeadlineWidget(DDLDepartment *department,QWidget *parent=nullptr);
    ~DeadlineWidget() override;
    void addDraft(const QString &title,const QString &note);
    void refresh();
    void showDay(const QDate &date);
    void showItem(const QString &identity,const QDate &date);
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
    AdvancedFilter filter;
    QPushButton *filterButton;
    void toggleFilter();
    QTabBar *tabs;
    DeadlineAgendaWidget *agenda;
    QCheckBox *showCompleted;
    QStackedWidget *pages;
    DeadlineCalendarWidget *calendar;
    QLabel *heading;
    QDate anchor=QDate::currentDate();
    bool focusedDayInstances=false,pendingFocus=false;
    bool refreshing=false;
    QList<TaskOccurrence> visible;
    int requested=200;
    quint64 queryVersion=0;
    std::shared_ptr<std::atomic_bool> queryCancel;
    QString queryKey;
    std::shared_ptr<CalendarQuery::Session> querySession;
    QList<QJsonObject> querySnapshot;
    QString queryMinute;
    bool followingToday=true;
    QDate lastToday=QDate::currentDate();
    void applyPage(const QList<TaskOccurrence> &items,bool more,bool filtered,bool final=true);
};
