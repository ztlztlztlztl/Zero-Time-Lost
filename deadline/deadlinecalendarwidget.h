#pragma once
#include <QWidget>
#include <QDate>
class DDLDepartment;class QTableWidget;
class DeadlineCalendarWidget:public QWidget {
    Q_OBJECT
public:
    enum CellRole { DateRole=Qt::UserRole,TitlesRole,CountRole,MonthRole,DetailsRole };
    explicit DeadlineCalendarWidget(DDLDepartment *department,QWidget *parent=nullptr);
    void refresh();
    void setDate(const QDate &date);
    void setFilter(const QString &keyword,const QString &tag,int starred);
    void setMonthView(bool month);
    bool isMonthView() const {return monthView;}
    QDate date() const {return anchor;}
signals:
    void dateActivated(const QDate &date);
private:
    DDLDepartment *dept;
    QDate anchor=QDate::currentDate();
    QTableWidget *grid;
    QString keyword,tag;
    int starred=-1;
    bool monthView=true,scrollPending=true;
};
