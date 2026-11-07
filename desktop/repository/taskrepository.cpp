#include "taskrepository.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
TaskRepository::TaskRepository(const QString &path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    connection = "tasks-" + QUuid::createUuid().toString();
    db = QSqlDatabase::addDatabase("QSQLITE", connection);
    db.setDatabaseName(path);
    if (!db.open()) {
        error = db.lastError().text();
        return;
    }
    QSqlQuery q(db);
    for (const auto &sql : {"PRAGMA foreign_keys=ON", "PRAGMA journal_mode=WAL", "PRAGMA busy_timeout=3000",
                            "CREATE TABLE IF NOT EXISTS tasks(id TEXT PRIMARY KEY,payload TEXT NOT NULL)",
                            "CREATE TABLE IF NOT EXISTS reminder_events(event_key TEXT PRIMARY KEY)",
                            "PRAGMA user_version=2"}) {
        if (!q.exec(sql)) {
            error = q.lastError().text();
            return;
        }
    }
    if (q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='reminders'") && q.next()) {
        q.finish();
        if (!q.exec("INSERT OR IGNORE INTO reminder_events SELECT event_key FROM reminders") ||
            !q.exec("DROP TABLE reminders"))
            error = q.lastError().text();
    }
    if (!error.isEmpty())
        return;
    if (!q.exec("PRAGMA table_info(reminder_events)")) {
        error = q.lastError().text();
        return;
    }
    bool timestamp = false;
    while (q.next())
        if (q.value("name") == "delivered_at")
            timestamp = true;
    q.finish();
    if (!timestamp &&
        !q.exec("ALTER TABLE reminder_events ADD COLUMN delivered_at INTEGER NOT NULL DEFAULT 0")) {
        error = q.lastError().text();
        return;
    }
    initialized = true;
}
TaskRepository::~TaskRepository() {
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection);
}
QList<QJsonObject> TaskRepository::all() const {
    if (snapshotValid)
        return snapshot;
    QList<QJsonObject> result;
    QSqlQuery q(db);
    if (!q.exec("SELECT payload FROM tasks")) {
        readFailed = true;
        error = q.lastError().text();
        return {};
    }
    while (q.next()) {
        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(q.value(0).toByteArray(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject() ||
            doc.object()["id"].toString().isEmpty()) {
            readFailed = true;
            error = "事务记录损坏";
            return {};
        }
        result.append(doc.object());
    }
    if (q.lastError().isValid()) {
        readFailed = true;
        error = q.lastError().text();
        return {};
    }
    snapshot = result;
    byId.clear();
    for (const auto &t : result)
        byId.insert(t["id"].toString(), t);
    snapshotValid = true;
    readFailed = false;
    error.clear();
    return result;
}
QJsonObject TaskRepository::find(const QString &id) const {
    if (snapshotValid)
        return readFailed ? QJsonObject() : byId.value(id);
    QSqlQuery query(db);
    query.prepare("SELECT payload FROM tasks WHERE id=?");
    query.addBindValue(id);
    if (!query.exec()) {
        readFailed = true;
        error = query.lastError().text();
        return {};
    }
    if (!query.next()) {
        if (query.lastError().isValid()) {
            readFailed = true;
            error = query.lastError().text();
        }
        return {};
    }
    QJsonParseError parse;
    auto doc = QJsonDocument::fromJson(query.value(0).toByteArray(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject() || doc.object()["id"] != id) {
        readFailed = true;
        error = "事务记录损坏";
        return {};
    }
    return doc.object();
}
bool TaskRepository::save(const QJsonObject &task) {
    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO tasks(id,payload) VALUES(?,?) ON CONFLICT(id) DO UPDATE SET payload=excluded.payload");
    q.addBindValue(task["id"].toString());
    q.addBindValue(QJsonDocument(task).toJson(QJsonDocument::Compact));
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    snapshotValid = false;
    changedIds[task["id"].toString()] = ++generation;
    if (!readFailed)
        error.clear();
    return true;
}
bool TaskRepository::recordReminder(const QString &key) {
    QSqlQuery q(db);
    q.prepare("INSERT OR IGNORE INTO reminder_events(event_key,delivered_at) VALUES(?,?)");
    q.addBindValue(key);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    if (!readFailed)
        error.clear();
    return q.numRowsAffected() == 1;
}
bool TaskRepository::probe() {
    if (!initialized || !db.isOpen())
        return false;
    QSqlQuery q(db);
    if (!q.exec("PRAGMA quick_check") || !q.next() || q.value(0).toString() != "ok") {
        readFailed = true;
        error = "事务数据库检查失败";
        return false;
    }
    snapshotValid = false;
    all();
    return ready();
}
bool TaskRepository::pruneReminderReceipts() {
    // Only receipts for absent tasks, older than the undo window, are eligible.
    const auto tasks = all();
    if (!ready())
        return false;
    QStringList roots;
    for (const auto &t : tasks) {
        roots << t["id"].toString();
        for (auto name : {"identityRoot", "identity"})
            if (!t[name].toString().isEmpty())
                roots << t[name].toString();
        const auto instances = t["instances"].toObject();
        for (auto v : instances)
            if (!v.toObject()["identity"].toString().isEmpty())
                roots << v.toObject()["identity"].toString();
    }
    QSqlQuery q(db);
    q.prepare("SELECT event_key FROM reminder_events WHERE delivered_at>0 AND delivered_at<?");
    q.addBindValue(QDateTime::currentSecsSinceEpoch() - 90LL * 86400);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    QStringList expired;
    while (q.next()) {
        const auto key = q.value(0).toString();
        bool live = false;
        for (const auto &root : roots)
            if (key.startsWith(root + "/")) {
                live = true;
                break;
            }
        if (!live)
            expired << key;
    }
    q.finish();
    if (!db.transaction())
        return false;
    for (const auto &key : expired) {
        q.prepare("DELETE FROM reminder_events WHERE event_key=?");
        q.addBindValue(key);
        if (!q.exec()) {
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

bool TaskRepository::saveMany(const QList<QJsonObject> &tasks) { return applyChanges(tasks, {}); }
bool TaskRepository::applyChanges(const QList<QJsonObject> &tasks, const QStringList &removedIds) {
    snapshotValid = false;
    if (!db.transaction()) {
        error = db.lastError().text();
        return false;
    }
    for (const auto &task : tasks)
        if (!save(task)) {
            db.rollback();
            return false;
        }
    for (const auto &id : removedIds) {
        QSqlQuery q(db);
        q.prepare("DELETE FROM tasks WHERE id=?");
        q.addBindValue(id);
        if (!q.exec()) {
            error = q.lastError().text();
            db.rollback();
            return false;
        }
        changedIds[id] = ++generation;
        // Keep delivery receipts through the short undo window: restoring a task must not resend it.
    }
    if (!db.commit()) {
        error = db.lastError().text();
        db.rollback();
        return false;
    }
    if (!readFailed)
        error.clear();
    return true;
}
