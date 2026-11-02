#pragma once
#include "taskrepository.h"
#include <QObject>
#include <QDateTime>
#include <QJsonArray>
struct TaskOccurrence {
    QString taskId, key, title, note, tags, repeat;
    QDateTime due;
    bool done=false, deleted=false, paused=false;
    int priority=0;
    qint64 snooze=0;
};
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
    void checkReminders(qint64 now);
    QString error() const { return message; }
    void setQuiet(int start,int end) { quietStart=start; quietEnd=end; }
signals:
    void changed();
    void reminder(const QString &title);
private:
    TaskRepository *repo;
    QString message;
    int quietStart=23,quietEnd=7;
};
