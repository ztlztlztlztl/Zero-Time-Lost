#pragma once
#include <QWidget>
#include <QDate>
#include "advancedfilter.h"
#include "ddldepartment.h"
class DDLDepartment;class QTableWidget;
class DeadlineCalendarWidget:public QWidget {
    Q_OBJECT
public:
    enum CellRole { DateRole=Qt::UserRole,TitlesRole,CountRole,MonthRole,DetailsRole,EntriesRole,LabelsRole,RowStartRole,RowEndRole,HighlightStartRole,HighlightEndRole,HighlightTopRole,HighlightBottomRole,NowLineRole };
    explicit DeadlineCalendarWidget(DDLDepartment *department,QWidget *parent=nullptr);
    void refresh();
    void setDate(const QDate &date);
    void setFilter(const AdvancedFilter &value);
    void setFilter(const QString &keyword,const QString &tag,int starred);
    void setMonthView(bool month);
    bool isMonthView() const {return monthView;}
    QDate date() const {return anchor;}
signals:
    void createRequested(const QDate &date,int minute,const QString &type);
    void dateActivated(const QDate &date);
    void detailRequested(const TaskOccurrence &item);
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    void activateCell(const QModelIndex &index,const QPoint &position);
    void showItems(const QList<int> &indexes,const QPoint &position);
    void highlight(const QString &identity);
    QList<TaskOccurrence> displayed;
    QVector<QPair<int,int>> timeRows;
    QString selectedIdentity,hoverIdentity;
    DDLDepartment *dept;
    QDate anchor=QDate::currentDate();
    QTableWidget *grid;
    AdvancedFilter filter;
    bool monthView=true,scrollPending=true;
};
