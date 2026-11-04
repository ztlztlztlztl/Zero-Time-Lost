#pragma once
#include <QScrollArea>
#include <QMap>
#include <QPointer>
#include "ddldepartment.h"

class DeadlineAgendaWidget:public QScrollArea {
    Q_OBJECT
public:
    explicit DeadlineAgendaWidget(QWidget *parent=nullptr);
    void setOccurrences(const QList<TaskOccurrence> &items,const QDate &anchor,bool filtered);
    void focusDate(const QDate &date);
    void selectItem(const QString &identity);
    void setMoreAvailable(bool more){externalMore=more;}
    void setSelectionScope(QWidget *scope){selectionScope=scope;}
signals:
    void moreRequested();
    void detailRequested(const TaskOccurrence &item);
    void completeRequested(const TaskOccurrence &item);
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    void render();
    void updateHighlight();
    QList<TaskOccurrence> allItems;
    QDate selectedAnchor;
    QString selectedIdentity,hoverIdentity;
    QPointer<QWidget> selectionScope;
    bool filtered=false;
    int shown=200;
    bool externalMore=false;
    QMap<QDate,QWidget*> dateAnchors;
};
