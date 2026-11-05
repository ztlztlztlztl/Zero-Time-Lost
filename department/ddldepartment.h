#pragma once
#include "taskrepository.h"
#include <QObject>
#include <QDateTime>
#include <QJsonArray>
#include <QTimer>
#include "calendarrules.h"
struct TaskDaySummary {
    int today=0,tomorrow=0,overdue=0;
    bool available=true;
};
class DDLDepartment : public QObject {
    Q_OBJECT
public:
    explicit DDLDepartment(TaskRepository *repository,QObject *parent=nullptr);
    QList<QJsonObject> templates() const { return repo->all(); }
    QList<TaskOccurrence> occurrences(const QDate &from,const QDate &to) const;
    QList<TaskOccurrence> listOccurrences(const QDateTime &now,bool showCompleted) const;
    TaskDaySummary daySummary(const QDate &today) const;
    QStringList labelsToday(const QDateTime &now) const;
    QStringList currentIntervals(const QDateTime &now) const;
    static QString validateType(const QJsonObject &task);
    QJsonObject task(const QString &id) const;
    bool save(QJsonObject task);
    bool splitSeries(const QString &id,const QDate &from,QJsonObject replacement);
    bool setOccurrence(const QString &id,const QString &key,const QJsonObject &values);
    bool setDeleted(const QString &id,bool deleted);
    bool removeOccurrence(const TaskOccurrence &item);
    bool purgeExpired(const QDateTime &now);
    bool setPaused(const QString &id,bool paused);
    bool complete(const TaskOccurrence &item);
    bool snooze(const TaskOccurrence &item,int minutes=10);
    bool update(const QString &id,QJsonObject values);
    int suffixRemaining(const QString &id,const QDate &from) const;
    TaskOccurrence resolve(const QString &id,const QString &key) const;
    void resyncTime();
    void refreshFromSync(){undoEntries.clear();scheduleBoundary();emit changed();emit temporalChanged();}
    bool undo();
    QString undoText() const;
    void checkReminders(qint64 now);
    QString error() const { return message; }
    QString previewSplit(const QString &id,const QDate &from,QJsonObject replacement);
    void setQuiet(int start,int end) { quietStart=start; quietEnd=end; }
signals:
    void changed();
    void temporalChanged();
    void reminder(const QString &title);
private:
    struct UndoEntry {QList<QJsonObject> before,after;QStringList removed;QString text;qint64 expires=0;};
    QList<UndoEntry> undoEntries;
    QList<QJsonObject> prepareSplit(const QString &id,const QDate &from,QJsonObject replacement);
    qint64 lastCreatedAt=0;
    void remember(const QList<QJsonObject> &before,const QList<QJsonObject> &after,const QStringList &removed,const QString &text);
    TaskRepository *repo;
    QString message;
    int quietStart=23,quietEnd=7;
    void scheduleBoundary();
    QTimer *boundaryTimer;
};
