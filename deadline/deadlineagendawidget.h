#pragma once
#include <QScrollArea>
#include <QMap>
#include "ddldepartment.h"

class DeadlineAgendaWidget:public QScrollArea {
    Q_OBJECT
public:
    explicit DeadlineAgendaWidget(QWidget *parent=nullptr);
    void setOccurrences(const QList<TaskOccurrence> &items,const QDate &anchor,bool filtered);
    void focusDate(const QDate &date);
signals:
    void detailRequested(const TaskOccurrence &item);
    void completeRequested(const TaskOccurrence &item);
private:
    QMap<QDate,QWidget*> dateAnchors;
};
