#include "taskrepository.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>
#include <QDir>
#include <QFileInfo>
TaskRepository::TaskRepository(const QString &path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    connection="tasks-"+QUuid::createUuid().toString();
    db=QSqlDatabase::addDatabase("QSQLITE",connection); db.setDatabaseName(path);
    if(!db.open()) { error=db.lastError().text(); return; }
    QSqlQuery q(db);
    for(const auto &sql : {"PRAGMA foreign_keys=ON","PRAGMA journal_mode=WAL","PRAGMA busy_timeout=3000",
        "CREATE TABLE IF NOT EXISTS tasks(id TEXT PRIMARY KEY,payload TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS reminder_events(event_key TEXT PRIMARY KEY)",
        "PRAGMA user_version=2"}) {
        if(!q.exec(sql)) { error=q.lastError().text(); return; }
    }
    if(q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='reminders'")&&q.next()){
        q.finish();
        if(!q.exec("INSERT OR IGNORE INTO reminder_events SELECT event_key FROM reminders")||!q.exec("DROP TABLE reminders"))error=q.lastError().text();
    }
}
TaskRepository::~TaskRepository() { db.close(); db=QSqlDatabase(); QSqlDatabase::removeDatabase(connection); }
QList<QJsonObject> TaskRepository::all() const {
    if(snapshotValid)return snapshot;
    QList<QJsonObject> result; QSqlQuery q(db);
    if(!q.exec("SELECT payload FROM tasks")){error=q.lastError().text();return {};}
    while(q.next()){
        QJsonParseError parseError;const auto doc=QJsonDocument::fromJson(q.value(0).toByteArray(),&parseError);
        if(parseError.error!=QJsonParseError::NoError||!doc.isObject()||doc.object()["id"].toString().isEmpty()){error="事务记录损坏";return {};}
        result.append(doc.object());
    }
    if(q.lastError().isValid()){error=q.lastError().text();return {};}
    snapshot=result;snapshotValid=true;return result;
}
QJsonObject TaskRepository::find(const QString &id) const {
    if(snapshotValid){for(const auto &t:snapshot)if(t["id"]==id)return t;return {};}
    QSqlQuery query(db);
    query.prepare("SELECT payload FROM tasks WHERE id=?");
    query.addBindValue(id);
    return query.exec()&&query.next()?QJsonDocument::fromJson(query.value(0).toByteArray()).object():QJsonObject();
}
bool TaskRepository::save(const QJsonObject &task) {
    QSqlQuery q(db); q.prepare("INSERT INTO tasks(id,payload) VALUES(?,?) ON CONFLICT(id) DO UPDATE SET payload=excluded.payload");
    q.addBindValue(task["id"].toString()); q.addBindValue(QJsonDocument(task).toJson(QJsonDocument::Compact));
    if(!q.exec()) { error=q.lastError().text(); return false; } snapshotValid=false;return true;
}
bool TaskRepository::recordReminder(const QString &key){
    QSqlQuery q(db);q.prepare("INSERT OR IGNORE INTO reminder_events(event_key) VALUES(?)");q.addBindValue(key);
    return q.exec()&&q.numRowsAffected()==1;
}

bool TaskRepository::saveMany(const QList<QJsonObject> &tasks){
    return applyChanges(tasks,{});
}
bool TaskRepository::applyChanges(const QList<QJsonObject> &tasks,const QStringList &removedIds){
    snapshotValid=false;
    if(!db.transaction()){error=db.lastError().text();return false;}
    for(const auto &task:tasks)if(!save(task)){db.rollback();return false;}
    for(const auto &id:removedIds){
        QSqlQuery q(db);q.prepare("DELETE FROM tasks WHERE id=?");q.addBindValue(id);
        if(!q.exec()){error=q.lastError().text();db.rollback();return false;}
        // Keep delivery receipts through the short undo window: restoring a task must not resend it.
    }
    if(!db.commit()){error=db.lastError().text();db.rollback();return false;}return true;
}
