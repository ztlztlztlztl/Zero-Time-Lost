#include "cloudsync.h"
#include "cloudoutbox.h"
#include "calendardto.h"
#include "cloudconnection.h"
#include "configmanager.h"
#include "ddldepartment.h"
#include "filebatch.h"
#include "mailaccountmanager.h"
#include "mailmanager.h"
#include "notedepartment.h"
#include "paths.h"
#include "tagrules.h"
#include "taskrepository.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QUuid>
#include <QtWidgets>

namespace {
bool validState(const QJsonObject &value) {
    for (auto field : {"shadow", "pending", "conflicts", "journal"})
        if (value.contains(field) && !value[field].isNull() && !value[field].isObject()) return false;
    if (value.contains("resolved") && !value["resolved"].isNull() && !value["resolved"].isArray()) return false;
    if (value.contains("dataset") && !value["dataset"].isNull() && !value["dataset"].isString()) return false;
    if (value.contains("cursor") && !value["cursor"].isNull() && (!value["cursor"].isDouble() || value["cursor"].toDouble() != double(value["cursor"].toInteger()) || value["cursor"].toInteger() < 0)) return false;
    for (auto field : {"shadow", "pending", "conflicts"}) {
        const auto entries = value[field].toObject();
        for (auto it=entries.begin();it!=entries.end();++it)
            if (!it.value().isObject()) return false;
    }
    return true;
}
QString keyOf(const QJsonObject &r) { return r["kind"].toString() + ":" + r["id"].toString(); }
using CloudOutbox::same;
QJsonObject record(const QString &kind, const QString &id, const QJsonObject &payload) {
    return {{"kind", kind}, {"id", id}, {"payload", payload}, {"deleted", false}};
}
QStringList strings(QJsonValue v) {
    QStringList list;
    for (auto x : v.toArray())
        list.append(x.toString());
    return list;
}
} // namespace
CloudSync::CloudSync(TaskRepository *tasks, DDLDepartment *ddl, NoteDepartment *notes, QObject *parent)
    : QObject(parent), tasks(tasks), ddl(ddl), notes(notes), path(Paths::data("cloud/state.json")) {
    QFile file(path);
    if (file.exists()) {
        QJsonParseError error;
        if (!file.open(QIODevice::ReadOnly)) {
            healthy = false;
            stateReadFailed = true;
        } else {
            auto doc = QJsonDocument::fromJson(file.readAll(), &error);
            healthy = doc.isObject() && error.error == QJsonParseError::NoError && validState(doc.object());
            state = doc.object();
        }
        file.close();
    }
    stateCorrupt = !healthy && !stateReadFailed;
    if (healthy && !state["journal"].toObject().isEmpty())
        healthy = replay();
    debounce.setSingleShot(true);
    debounce.setInterval(800);
    connect(&debounce, &QTimer::timeout, this, &CloudSync::sync);
    auto changed = [this] { markDirty("settings"); };
    connect(ddl, &DDLDepartment::changed, this, [this] { markDirty("calendar"); });
    connect(notes, &NoteDepartment::changed, this, [this] {
        markDirty("note");
        markDirty("order");
    });
    auto config = ConfigManager::getInstance();
    connect(config, &ConfigManager::deadlineChanged, this, changed);
    connect(config, &ConfigManager::noteTagsChanged, this, changed);
    connect(config, &ConfigManager::homeWeatherChanged, this, changed);
    connect(config, &ConfigManager::mailBasicalChanged, this, changed);
    connect(MailAccountManager::getInstance(), &MailAccountManager::mailAccountChanged, this, changed);
    connect(MailManager::getInstance(), &MailManager::flagsChanged, this,
            [this](const QString &address, const QString &uid) {
                const auto id =
                    QString::fromLatin1(QCryptographicHash::hash(address.toUtf8() + '\0' + uid.toUtf8(),
                                                                 QCryptographicHash::Sha256)
                                            .toHex());
                dirtyMail[id] = {address, uid};
                markDirty("mailflag", id);
            });
    auto connection = CloudConnection::instance();
    connect(connection, &CloudConnection::syncRequested, this, &CloudSync::sync);
    connect(connection, &CloudConnection::conflictsRequested, this, &CloudSync::showConflicts);
    connect(connection, &CloudConnection::reconnectRequested, this, [this] {
        if (prepareReconnect())
            debounce.start();
    });
    connect(connection, &CloudConnection::settingsChanged, this, [this] { debounce.start(); });
    connect(this, &CloudSync::status, connection, &CloudConnection::status);
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState s) {
        if (s == Qt::ApplicationActive)
            debounce.start();
    });
    periodic.setInterval(30000);
    connect(&periodic, &QTimer::timeout, this, &CloudSync::sync);
    periodic.start();
    debounce.start();
}
void CloudSync::markDirty(const QString &kind, const QString &id) {
    if (kind != "mailflag" || id == "*")
        invalidKinds.insert(kind);
    if (applying)
        return;
    dirtyObjects[kind + ":" + id] = true;
    QDir().mkpath(Paths::data("cloud"));
    QSaveFile file(Paths::data("cloud/dirty.json"));
    const auto bytes = QJsonDocument(dirtyObjects).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        healthy = false;
        emit status("本地变更登记失败，暂停同步并保留业务原件");
    }
    if (CloudConnection::enabled())
        debounce.start();
}
QJsonObject CloudSync::snapshot() const {
    // QSettings credentials can change without a ConfigManager notification.
    // Check this small value before reusing the shared-settings snapshot.
    const auto weatherKey = QSettings().value("weatherKey").toString();
    if (snapshotCache.value("settings:shared").toObject()["payload"].toObject()["weatherKey"].toString() != weatherKey)
        invalidKinds.insert("settings");
    if (taskRevision != tasks->revision())
        invalidKinds.insert("calendar");
    if (noteRevision != notes->cloudRevision()) {
        invalidKinds.insert("note");
        invalidKinds.insert("order");
    }
    readable = tasks->ready() && notes->cloudReady() && MailAccountManager::getInstance()->cloudReady();
    for (auto owner : QList<QObject *>{ConfigManager::getInstance(), MailAccountManager::getInstance()})
        for (auto engine : owner->findChildren<HelpJsonIOEngine *>())
            readable = readable && engine->readable();
    QJsonObject result = snapshotCache;
    auto kindKeys = snapshotCache.isEmpty() ? QMap<QString, QSet<QString>>() : snapshotKeys;
    auto publicIds = snapshotCache.isEmpty() ? QSet<QString>() : publicNoteIds;
    QSet<QString> changed;
    auto remove = [&](const QString &kind, const QString &id) {
        const auto key = kind + ":" + id;
        if (result.contains(key)) {
            changed.insert(key);
            result.remove(key);
        }
        kindKeys[kind].remove(key);
        if (kind == "note")
            publicIds.remove(id);
    };
    auto add = [&](const QJsonObject &r) {
        const auto key = keyOf(r), kind = r["kind"].toString();
        if (snapshotCache.value(key) != r) {
            changed.insert(key);
            result[key] = r;
        }
        if (!kindKeys.value(kind).contains(key))
            kindKeys[kind].insert(key);
        if (kind == "note" && !publicIds.contains(r["id"].toString()))
            publicIds.insert(r["id"].toString());
    };
    if (invalidKinds.contains("calendar")) {
        if (snapshotCache.isEmpty())
            for (const auto &task : tasks->all())
                add(record("calendar", task["id"].toString(), CalendarDTO::canonicalEmptyPrefix(task)));
        else
            for (const auto &id : tasks->changesSince(taskRevision)) {
                const auto task = tasks->find(id);
                if (task.isEmpty())
                    remove("calendar", id);
                else
                    add(record("calendar", id, CalendarDTO::canonicalEmptyPrefix(task)));
            }
    }
    if (invalidKinds.contains("note") && snapshotCache.isEmpty())
        for (auto value : notes->cloudEntries()) {
            auto e = value.toObject();
            add(record("note", e["id"].toString(), e));
        }
    if (invalidKinds.contains("note") && !snapshotCache.isEmpty())
        for (const auto &id : notes->cloudChangesSince(noteRevision)) {
            const auto e = notes->cloudEntry(id);
            if (e.isEmpty())
                remove("note", id);
            else
                add(record("note", id, e));
        }
    if (invalidKinds.contains("order"))
        for (int kind : {1, 2}) {
            QJsonArray ids;
            for (const auto &id : notes->order(kind))
                if (publicIds.contains(id) || notes->entry(id).isEmpty())
                    ids.append(id);
            add(record("order", QString::number(kind), {{"ids", ids}}));
        }
    bool mailReady = true;
    const auto flagsList =
        invalidKinds.contains("mailflag") ? MailManager::getInstance()->cloudFlags(&mailReady) : QJsonArray();
    auto missingFlags = kindKeys.value("mailflag");
    readable = readable && tasks->ready() && mailReady;
    for (auto value : flagsList) {
        auto flags = value.toObject();
        const auto identity = flags["address"].toString().toUtf8() + '\0' + flags["uid"].toString().toUtf8();
        const auto id = QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
        missingFlags.remove("mailflag:" + id);
        add(record("mailflag", id, flags));
    }
    if (invalidKinds.contains("mailflag"))
        for (const auto &key : missingFlags)
            remove("mailflag", key.mid(9));
    if (!invalidKinds.contains("mailflag"))
        for (auto it = dirtyMail.begin(); it != dirtyMail.end(); ++it) {
            bool ok = false;
            const auto payload =
                MailManager::getInstance()->cloudFlag(it.value().first, it.value().second, &ok);
            readable = readable && ok;
            if (!payload.isEmpty())
                add(record("mailflag", it.key(), payload));
            else
                remove("mailflag", it.key());
        }
    if (invalidKinds.contains("settings")) {
        const auto c = ConfigManager::getInstance();
        auto weather = c->getHomeweather();
        auto mail = c->getMailBasical();
        auto deadline = c->getDeadlineBasical();
        QMap<QString, QJsonObject> accountMap;
        for (const auto &a : MailAccountManager::getInstance()->getMailAccounts())
            accountMap[a.address] = {{"name", a.name},
                                     {"address", a.address},
                                     {"code", a.code},
                                     {"server", a.server},
                                     {"port", a.port}};
        QJsonArray accounts;
        for (const auto &a : accountMap)
            accounts.append(a);
        add(record("settings", "shared",
                   {{"weather", weather.toJson()},
                    {"mail", mail.toJson()},
                    {"calendar", deadline.toJson()},
                    {"noteTags", QJsonArray::fromStringList(c->noteTags())},
                    {"weatherKey", weatherKey},
                    {"accounts", accounts}}));
    }
    if (readable) {
        snapshotCache = result;
        snapshotKeys = kindKeys;
        publicNoteIds = publicIds;
        captureKeys.unite(changed);
        taskRevision = tasks->revision();
        noteRevision = notes->cloudRevision();
        invalidKinds.clear();
        dirtyMail.clear();
    }
    return result;
}
bool CloudSync::save() {
    const auto resolved = state["resolved"].toArray();
    for (auto item : resolved) {
        const auto bytes = QJsonDocument(item.toObject()).toJson();
        const auto folder = Paths::data("cloud/resolved");
        QDir().mkpath(folder);
        QSaveFile archive(
            folder + "/" +
            QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) +
            ".json");
        if (!archive.open(QIODevice::WriteOnly) || archive.write(bytes) != bytes.size() ||
            !archive.commit()) {
            healthy = false;
            emit status("冲突归档保存失败，原状态保留");
            return false;
        }
    }
    if (!resolved.isEmpty())
        state.remove("resolved");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        healthy = false;
        emit status("云端同步状态无法保存，已暂停网络写入");
        return false;
    }
    auto bytes = QJsonDocument(state).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        healthy = false;
        emit status("云端同步状态保存失败，请检查磁盘空间");
        return false;
    }
    return true;
}
bool CloudSync::recover() {
    if (stateCorrupt)
        return false;
    if (stateReadFailed) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (!doc.isObject() || error.error != QJsonParseError::NoError || !validState(doc.object())) {
            stateCorrupt = true;
            return false;
        }
        state = doc.object();
        stateReadFailed = false;
    }
    if (healthy)
        return true;
    if (!tasks->probe())
        return false;
    for (auto owner : QList<QObject *>{ConfigManager::getInstance(), MailAccountManager::getInstance()})
        for (auto engine : owner->findChildren<HelpJsonIOEngine *>())
            if (!engine->readable() || !engine->flush())
                return false;
    snapshotCache = {};
    fullCapture = true;
    invalidKinds = {"calendar", "note", "order", "mailflag", "settings"};
    snapshot();
    if (!readable)
        return false;
    if (!save())
        return false;
    healthy = true;
    if (!state["journal"].toObject().isEmpty())
        healthy = replay();
    return healthy;
}
bool CloudSync::flush() {
    if (!healthy && !recover())
        return false;
    capture();
    return healthy && save();
}
void CloudSync::capture() {
    if (!healthy || applying || !CloudConnection::enabled())
        return;
    for (auto owner : QList<QObject *>{ConfigManager::getInstance(), MailAccountManager::getInstance()})
        for (auto engine : owner->findChildren<HelpJsonIOEngine *>())
            if (!engine->flush()) {
                healthy = false;
                emit status("业务设置未能保存，已暂停同步以保留修改");
                return;
            }
    const auto local = snapshot(), shadow = state["shadow"].toObject(),
               conflicts = state["conflicts"].toObject();
    auto pending = state["pending"].toObject();
    if (!readable) {
        healthy = false;
        emit status("本地业务数据或邮箱凭据无法读取，已暂停同步；请检查配置恢复或重新填写邮箱授权码");
        return;
    }
    auto keys = captureKeys;
    if (fullCapture) {
        for (auto it = local.begin(); it != local.end(); ++it)
            keys.insert(it.key());
        for (auto it = shadow.begin(); it != shadow.end(); ++it)
            keys.insert(it.key());
    }
    lastCaptureCount = keys.size();
    // Old empty-prefix requests fail validation before the server can accept
    // them. Repair only those invalid payloads, with a fresh operation ID;
    // keep valid in-flight operations immutable.
    if (fullCapture) for (auto it=pending.begin();it!=pending.end();++it) {
        auto request=it.value().toObject();
        if(request["kind"]!="calendar" || request["deleted"].toBool())continue;
        const auto before=request["payload"].toObject();
        const auto after=CalendarDTO::canonicalEmptyPrefix(before);
        if(before!=after && CalendarDTO::validate(after).isEmpty()) {
            request["payload"]=after;
            request["op"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
            it.value()=request;
        }
    }
    pending = CloudOutbox::collect(local, shadow, pending, conflicts, keys);
    if (pending != state["pending"].toObject()) {
        state["pending"] = pending;
        if (!save())
            return;
    }
    fullCapture = false;
    captureKeys.clear();
    if (!dirtyObjects.isEmpty()) {
        dirtyObjects = {};
        QFile::remove(Paths::data("cloud/dirty.json"));
    }
}
bool CloudSync::apply(const QJsonObject &r) {
    QScopedValueRollback<bool> guard(applying, true);
    const auto kind = r["kind"].toString(), id = r["id"].toString();
    const auto p = r["payload"].toObject();
    const bool deleted = r["deleted"].toBool();
    invalidKinds.insert(kind);
    if (kind == "calendar") {
        if (!deleted) {
            if (p["id"] != id)
                return false;
            if (!CalendarDTO::validate(p).isEmpty())
                return false;
        }
        if (!tasks->applyChanges(deleted ? QList<QJsonObject>() : QList<QJsonObject>{p},
                                 deleted ? QStringList{id} : QStringList()))
            return false;
        ddl->refreshFromSync();
        return true;
    }
    if (kind == "note")
        return notes->applyCloudEntry(id, p, deleted);
    if (kind == "mailflag" && !deleted)
        return MailManager::getInstance()->applyCloudFlags(p);
    if (kind == "order") {
        const int category = id.toInt();
        if (category != 1 && category != 2)
            return false;
        return notes->applyCloudOrder(category, strings(p["ids"]));
    }
    if (kind == "settings" && id == "shared" && !deleted) {
        // Validate and protect every credential before modifying files or live objects.
        if (!p["weather"].isObject() || !p["mail"].isObject() || !p["calendar"].isObject() ||
            !p["accounts"].isArray() || !p["noteTags"].isArray() || !p["weatherKey"].isString())
            return false;
        const auto weather = p["weather"].toObject();
        if (!weather["savedCities"].isArray())
            return false;
        QSet<QString> cities;
        for (auto v : weather["savedCities"].toArray()) {
            auto city = v.toObject();
            const auto code = city["code"].toString();
            if (city["name"].toString().trimmed().isEmpty() ||
                !QRegularExpression("^[0-9]{6}$").match(code).hasMatch() || cities.contains(code))
                return false;
            cities.insert(code);
        }
        for (auto tag : p["noteTags"].toArray())
            if (!tag.isString() || !TagRules::error(tag.toString()).isEmpty())
                return false;
        for (auto tag : p["calendar"].toObject()["commonTags"].toArray())
            if (!tag.isString() || !TagRules::error(tag.toString()).isEmpty())
                return false;
        const auto mail = p["mail"].toObject();
        for (const auto &rule : QList<QPair<QString, QPair<int, int>>>{
                 {"initialNumber", {1, 1000}}, {"earlierNumber", {1, 100}}, {"storageLimit", {1, 100000}}})
            if (mail.contains(rule.first)) {
                auto v = mail[rule.first];
                if (!v.isDouble() || v.toDouble() != v.toInt() || v.toInt() < rule.second.first ||
                    v.toInt() > rule.second.second)
                    return false;
            }
        if (mail.contains("pollingInterval") &&
            (!mail["pollingInterval"].isDouble() ||
             mail["pollingInterval"].toDouble() != mail["pollingInterval"].toInt()))
            return false;
        if (!weather["currentCityIndex"].isDouble() ||
            weather["currentCityIndex"].toDouble() != weather["currentCityIndex"].toInt() ||
            weather["currentCityIndex"].toInt() < -1 ||
            weather["currentCityIndex"].toInt() >= qMax(1, int(cities.size())))
            return false;
        if (p["calendar"].toObject().contains("commonTags") &&
            !p["calendar"].toObject()["commonTags"].isArray())
            return false;
        const int interval = mail["pollingInterval"].toInt(5);
        if (interval != -1 && (interval < 1 || interval > 1440))
            return false;
        QJsonArray protectedAccounts;
        QSet<QString> keep;
        try {
            for (auto v : p["accounts"].toArray()) {
                const auto value = v.toObject();
                MailAccount a(value);
                bool portOk = false;
                const int port = a.port.toInt(&portOk);
                if (a.address.trimmed().isEmpty() || a.server.trimmed().isEmpty() || a.code.isEmpty() ||
                    !portOk || port < 1 || port > 65535 || keep.contains(a.address))
                    return false;
                keep.insert(a.address);
                protectedAccounts.append(a.toJson());
            }
        } catch (...) {
            return false;
        }
        auto c = ConfigManager::getInstance();
        auto accounts = MailAccountManager::getInstance();
        for (auto owner : QList<QObject *>{c, accounts})
            for (auto engine : owner->findChildren<HelpJsonIOEngine *>())
                if (!engine->flush())
                    return false;
        QSettings settings;
        settings.sync();
        if (settings.status() != QSettings::NoError)
            return false;
        QTemporaryDir stage(Paths::data("tmp/shared-XXXXXX"));
        if (!stage.isValid())
            return false;
        const auto ini = stage.filePath("candidate.ini");
        if (QFile::exists(settings.fileName()) && !QFile::copy(settings.fileName(), ini))
            return false;
        {
            QSettings candidate(ini, QSettings::IniFormat);
            candidate.setValue("weatherKey", p["weatherKey"].toString());
            candidate.sync();
            if (candidate.status() != QSettings::NoError)
                return false;
        }
        QFile file(ini);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        QMap<QString, QByteArray> files{
            {settings.fileName(), file.readAll()},
            {Paths::data("config/settings.json"),
             QJsonDocument(QJsonObject{{"home", QJsonObject{{"weather", p["weather"]}}},
                                       {"mail", p["mail"]},
                                       {"deadline", p["calendar"]},
                                       {"noteTags", p["noteTags"]}})
                 .toJson()},
            {Paths::data("config/mails.json"),
             QJsonDocument(QJsonObject{{"accounts", protectedAccounts}}).toJson()}};
        if (!FileBatch::commit(files))
            return false;
        {
            QSignalBlocker b1(c), b2(accounts);
            settings.sync();
            c->reload();
            accounts->reload();
        }
        emit c->homeWeatherChanged();
        emit c->mailBasicalChanged();
        emit c->deadlineChanged();
        emit c->noteTagsChanged();
        emit accounts->mailAccountChanged();
        return true;
    }
    return false;
}
bool CloudSync::replay() {
    const auto journal = state["journal"].toObject();
    if (journal.isEmpty())
        return true;
    const auto r = journal["record"].toObject();
    const auto key = keyOf(r);
    const auto local = snapshot()[key].toObject();
    if (!readable)
        return false;
    // If a failed apply was followed by new edits, preserve the edits as a conflict.
    auto conflicts = state["conflicts"].toObject();
    if (!same(local, journal["before"].toObject()) && !same(local, r))
        conflicts[key] = QJsonObject{{"remote", r}, {"local", local}};
    else if (!apply(r))
        return false;
    auto shadow = state["shadow"].toObject();
    shadow[key] = r;
    state["shadow"] = shadow;
    state["conflicts"] = conflicts;
    state["cursor"] = journal["cursor"];
    state.remove("journal");
    return save();
}
void CloudSync::sync() {
    if (busy || resolving || !CloudConnection::enabled())
        return;
    if (!healthy && !recover()) {
        emit status("云端同步已暂停：本地同步状态损坏或写入失败，请检查 data/cloud");
        return;
    }
    capture();
    if (!healthy)
        return;
    busy = true;
    emit status("正在同步云端数据…");
    CloudConnection::instance()->request(
        "/health", {}, [this, guard = QPointer<CloudSync>(this)](const QJsonObject &r, const QString &error) {
            if (!guard)
                return;
            if (!error.isEmpty()) {
                finish(error);
                return;
            }
            if (r["protocol"] != 1 || r["dataset"].toString().isEmpty()) {
                finish("服务器协议不兼容");
                return;
            }
            const auto dataset = state["dataset"].toString();
            if (!dataset.isEmpty() && dataset != r["dataset"].toString()) {
                finish("服务器数据集已改变，请核查地址或备份恢复；本地修改保留");
                return;
            }
            qint64 known = state["cursor"].toInteger();
            const auto shadow = state["shadow"].toObject();
            for (auto value : shadow)
                known = qMax(known, value.toObject()["rev"].toInteger());
            if (r["head"].toInteger() < known) {
                finish("服务器修订号倒退，请核查备份恢复；已暂停上传");
                return;
            }
            state["dataset"] = r["dataset"];
            if (!save()) {
                finish("状态保存失败");
                return;
            }
            next();
        });
}
bool CloudSync::prepareReconnect() {
    if (busy || resolving || (!healthy && !recover())) {
        emit status("请等待同步结束并处理本地状态错误后再重新协商");
        return false;
    }
    snapshotCache = {};
    invalidKinds = {"calendar", "note", "order", "mailflag", "settings"};
    fullCapture = true;
    capture();
    if (!healthy)
        return false;
    const auto local = snapshot();
    if (!readable) {
        emit status("本地数据无法读取，未重置同步状态");
        return false;
    }
    const auto name = Paths::data("backups/cloud-sync-before-reconnect-" +
                                  QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json");
    QSaveFile archive(name);
    if (!archive.open(QIODevice::WriteOnly)) {
        emit status("无法备份本地同步状态，已取消");
        return false;
    }
    const auto bytes = QJsonDocument(QJsonObject{{"state", state}, {"local", local}}).toJson();
    if (archive.write(bytes) != bytes.size() || !archive.commit()) {
        emit status("同步状态备份失败，已取消");
        return false;
    }
    auto pending = state["pending"].toObject();
    const auto conflicts = state["conflicts"].toObject();
    for (auto it = conflicts.begin(); it != conflicts.end(); ++it) {
        if (local.contains(it.key()))
            continue;
        auto r = it.value().toObject()["remote"].toObject();
        r["deleted"] = true;
        r["payload"] = QJsonObject{};
        pending[it.key()] = r;
    }
    for (auto it = pending.begin(); it != pending.end(); ++it) {
        auto r = it.value().toObject();
        r["base"] = 0;
        r["op"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        r.remove("rev");
        it.value() = r;
    }
    state = QJsonObject{{"pending", pending}};
    fullCapture = true;
    if (!save())
        return false;
    capture();
    emit status("本地状态已备份，将重新协商同步；不同版本保留为冲突供选择");
    return healthy;
}
void CloudSync::next() {
    if (!CloudConnection::enabled() || !healthy) {
        finish(healthy ? QString() : "同步状态保存失败");
        return;
    }
    capture();
    if (!healthy) {
        finish("状态保存失败");
        return;
    }
    const auto pending = state["pending"].toObject();
    if (pending.isEmpty()) {
        pull();
        return;
    }
    const auto key = pending.begin().key();
    const auto request = pending[key].toObject();
    CloudConnection::instance()->request(
        "/v1/mutate", request,
        [this, key, request, guard = QPointer<CloudSync>(this)](const QJsonObject &r, const QString &error) {
            if (!guard)
                return;
            if (!error.isEmpty()) {
                finish(error);
                return;
            }
            if (r["ok"].toBool()) {
                const auto accepted = r["record"].toObject();
                if (keyOf(accepted) != key || !same(accepted, request)) {
                    finish("服务端修改确认无效");
                    return;
                }
                auto shadow = state["shadow"].toObject();
                shadow[key] = accepted;
                state["shadow"] = shadow;
            } else {
                if (!r["conflict"].isObject()) {
                    finish("服务端冲突响应无效");
                    return;
                }
                if (same(request, r["conflict"].toObject())) {
                    auto shadow = state["shadow"].toObject();
                    shadow[key] = r["conflict"];
                    state["shadow"] = shadow;
                } else {
                    auto conflicts = state["conflicts"].toObject();
                    conflicts[key] = QJsonObject{{"remote", r["conflict"]}, {"local", snapshot()[key]}};
                    state["conflicts"] = conflicts;
                }
            }
            auto pending = state["pending"].toObject();
            pending.remove(key);
            state["pending"] = pending;
            // Edits made while this immutable operation was in flight need a new operation.
            captureKeys.insert(key);
            if (!save()) {
                finish("状态保存失败");
                return;
            }
            if (key == "settings:shared" && same(state["shadow"].toObject()[key].toObject(), request))
                emit sharedSettingsUploaded();
            next();
        },
        true);
}
void CloudSync::pull() {
    const auto cursor = state["cursor"].toInteger();
    CloudConnection::instance()->request(
        "/v1/changes?cursor=" + QString::number(cursor), {},
        [this, cursor, guard = QPointer<CloudSync>(this)](const QJsonObject &r, const QString &error) {
            if (!guard)
                return;
            if (!error.isEmpty()) {
                finish(error);
                return;
            }
            if (r["dataset"] != state["dataset"] || !r["records"].isArray() ||
                r["cursor"].toInteger() < cursor) {
                finish("服务端增量响应无效");
                return;
            }
            capture();
            if (!healthy) {
                finish("状态保存失败");
                return;
            }
            for (auto value : r["records"].toArray()) {
                const auto remote = value.toObject();
                const auto key = keyOf(remote);
                auto shadow = state["shadow"].toObject();
                if (remote["rev"].toInteger() <= shadow[key].toObject()["rev"].toInteger())
                    continue;
                auto conflicts = state["conflicts"].toObject();
                auto pending = state["pending"].toObject();
                if (conflicts.contains(key) ||
                    (pending.contains(key) && !same(pending[key].toObject(), remote))) {
                    conflicts[key] = QJsonObject{{"remote", remote}, {"local", snapshot()[key]}};
                    pending.remove(key);
                    state["pending"] = pending;
                    state["conflicts"] = conflicts;
                    if (!save()) {
                        finish("状态保存失败");
                        return;
                    }
                    continue;
                }
                pending.remove(key);
                state["pending"] = pending;
                state["journal"] =
                    QJsonObject{{"record", remote}, {"before", snapshot()[key]}, {"cursor", state["cursor"]}};
                if (!save() || !replay()) {
                    healthy = false;
                    finish("云端数据写入失败，应用日志已保留");
                    return;
                }
            }
            state["cursor"] = r["cursor"];
            if (!save()) {
                finish("状态保存失败");
                return;
            }
            if (r["more"].toBool())
                pull();
            else
                finish();
        });
}
void CloudSync::finish(const QString &error) {
    busy = false;
    if (!error.isEmpty()) {
        emit status(error);
        return;
    }
    const int count = state["conflicts"].toObject().size();
    emit status(count ? QString("有 %1 项同步冲突，请在设置中处理").arg(count)
                      : "云端同步完成 " + QDateTime::currentDateTime().toString("HH:mm:ss"));
    emit synced();
}
bool CloudSync::resolveConflict(const QString &key, bool keepLocal) {
    auto conflicts = state["conflicts"].toObject();
    if (!conflicts.contains(key))
        return false;
    const auto remote = conflicts[key].toObject()["remote"].toObject();
    const auto local = snapshot()[key].toObject();
    auto archive = state["resolved"].toArray();
    archive.append(QJsonObject{{"key", key},
                               {"local", local},
                               {"remote", remote},
                               {"chosen", keepLocal ? "local" : "remote"},
                               {"time", QDateTime::currentDateTime().toString(Qt::ISODate)}});
    state["resolved"] = archive;
    if (!save())
        return false;
    if (!keepLocal) {
        state["journal"] = QJsonObject{{"record", remote}, {"before", local}, {"cursor", state["cursor"]}};
        if (!save() || !replay()) {
            healthy = false;
            emit status("冲突版本应用失败，日志已保留");
            return false;
        }
    } else {
        auto shadow = state["shadow"].toObject();
        shadow[key] = remote;
        state["shadow"] = shadow;
    }
    conflicts.remove(key);
    state["conflicts"] = conflicts;
    captureKeys.insert(key);
    return save();
}
void CloudSync::showConflicts() {
    if (busy) {
        emit status("同步中，请稍后处理冲突");
        return;
    }
    if (!healthy) {
        emit status("请先处理同步状态保存错误");
        return;
    }
    QScopedValueRollback<bool> resolvingGuard(resolving, true);
    auto conflicts = state["conflicts"].toObject();
    if (conflicts.isEmpty()) {
        emit status("没有待处理的同步冲突");
        return;
    }
    for (const auto &key : conflicts.keys()) {
        const auto conflict = conflicts[key].toObject(), remote = conflict["remote"].toObject();
        const auto local = snapshot()[key].toObject();
        QMessageBox dialog(QMessageBox::Question, "选择同步版本",
                           key + "\n本机和云端有不同修改。请选择保留的版本；双方内容会留在同步历史中。",
                           QMessageBox::NoButton, QApplication::activeWindow());
        auto localButton = dialog.addButton("保留本机", QMessageBox::AcceptRole);
        auto remoteButton = dialog.addButton("使用云端", QMessageBox::DestructiveRole);
        dialog.addButton("稍后处理", QMessageBox::RejectRole);
        auto visible = [](QJsonObject value) {
            auto p = value["payload"].toObject();
            if (value["kind"] == "settings") {
                p.remove("accounts");
                p.remove("weatherKey");
                value["payload"] = p;
            }
            return QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Indented));
        };
        dialog.setDetailedText("本机：\n" + visible(local) + "\n云端：\n" + visible(remote));
        dialog.exec();
        if (dialog.clickedButton() != localButton && dialog.clickedButton() != remoteButton)
            break;
        if (!resolveConflict(key, dialog.clickedButton() == localButton))
            return;
    }
    capture();
    debounce.start();
}
