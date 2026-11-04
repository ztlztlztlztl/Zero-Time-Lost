#pragma once
#include "notedepartment.h"
#include "notecard.h"
#include "advancedfilter.h"
#include <QtWidgets>
class NoteWidget:public QWidget {
    Q_OBJECT
public:
    explicit NoteWidget(NoteDepartment *department,QWidget *parent=nullptr);
    bool ensureUnlocked();
    bool prepareLeave();
    void addDraft(const QString &title,const QString &body);
    void manage(const QString &action);
signals:
    void saveFailed(const QString &message);
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
    void showEvent(QShowEvent *event) override;
private:
    void refresh();
    bool filterCards();
    void relayout();
    void edit(QJsonObject entry={});
    void secretEditor(QJsonObject entry={});
    void expand(NoteCard *card);
    bool moveCard(const QString &source,const QString &target);
    QPointer<NoteCard> dragCandidate;
    QPoint dragPress;
    QTimer longPress;
    QFrame *dropMarker=nullptr;
    void startCardDrag();
    NoteCard *dropTarget(const QPoint &point) const;
    NoteDepartment *dept;
    AdvancedFilter filter,appliedFilter;
    QPushButton *filterButton;
    void toggleFilter();
    void openFilter();
    QTabBar *tabs;
    QScrollArea *scroll;
    QWidget *canvas;
    QLabel *empty,*notice;
    QWidget *emptyGroup;
    QPushButton *add,*emptyAction;
    QHash<QString,NoteCard*> cards;
    QList<NoteCard*> ordered;
    int activeTab=0;
    bool refreshing=false;
};
