#pragma once
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSqlDatabase>
#include <QStringList>
class TaskRepository {
  public:
    explicit TaskRepository(const QString &path);
    ~TaskRepository();
    bool ready() const { return db.isOpen() && initialized && !readFailed; }
    bool probe();
    quint64 revision() const { return generation; }
    QStringList changesSince(quint64 since) const {
        QStringList ids;
        for (auto it = changedIds.begin(); it != changedIds.end(); ++it)
            if (it.value() > since)
                ids.append(it.key());
        return ids;
    }
    QString lastError() const { return error; }
    QList<QJsonObject> all() const;
    QJsonObject find(const QString &id) const;
    bool save(const QJsonObject &task);
    bool saveMany(const QList<QJsonObject> &tasks);
    bool applyChanges(const QList<QJsonObject> &tasks, const QStringList &removedIds);
    bool recordReminder(const QString &key);
    bool pruneReminderReceipts();

  private:
    QSqlDatabase db;
    QString connection;
    mutable QString error;
    quint64 generation = 0;
    QHash<QString, quint64> changedIds;
    bool initialized = false;
    mutable bool readFailed = false;
    mutable QList<QJsonObject> snapshot;
    mutable QHash<QString, QJsonObject> byId;
    mutable bool snapshotValid = false;
};
