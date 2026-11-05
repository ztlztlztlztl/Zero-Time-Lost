#pragma once
#include <QSqlDatabase>
#include <QJsonObject>
#include <QList>
#include <QStringList>
class TaskRepository {
public:
    explicit TaskRepository(const QString &path);
    ~TaskRepository();
    bool ready() const { return db.isOpen() && error.isEmpty(); }
    QString lastError() const { return error; }
    QList<QJsonObject> all() const;
    QJsonObject find(const QString &id) const;
    bool save(const QJsonObject &task);
    bool saveMany(const QList<QJsonObject> &tasks);
    bool applyChanges(const QList<QJsonObject> &tasks,const QStringList &removedIds);
    bool recordReminder(const QString &key);
private:
    QSqlDatabase db;
    QString connection;
    mutable QString error;
    mutable QList<QJsonObject> snapshot;
    mutable bool snapshotValid=false;
};
