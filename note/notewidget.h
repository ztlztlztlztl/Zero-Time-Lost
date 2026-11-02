#pragma once
#include "notedepartment.h"
#include "notecard.h"
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
    NoteDepartment *dept;
    QLineEdit *search;
    QTabBar *tabs;
    QComboBox *tagFilter;
    QScrollArea *scroll;
    QWidget *canvas;
    QLabel *empty,*notice;
    QWidget *emptyGroup;
    QPushButton *add,*emptyAction,*clearFilters;
    QHash<QString,NoteCard*> cards;
    QList<NoteCard*> ordered;
    int activeTab=0;
    QString activeSearch,activeTag;
    bool refreshing=false;
};
