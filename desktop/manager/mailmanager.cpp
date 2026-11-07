#include "mailmanager.h"
#include "configmanager.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
MailManager *MailManager::getInstance() {
    static MailManager instance;
    return &instance;
}
bool MailManager::initialize() {
    if (m_mailEngine&&initialized)
        return true;
    delete m_mailEngine;m_mailEngine=nullptr;initializationError.clear();initializing=true;
    connect(ConfigManager::getInstance(), &ConfigManager::mailBasicalChanged, this, &MailManager::pruneBodies,
            Qt::UniqueConnection);
    m_mailEngine = new HelpSQLiteIOEngine("./database", "mail.db", "ZTL_Mail_Conn", this);
    connect(m_mailEngine, &HelpSQLiteIOEngine::failed, this,
            [this](const QString &e) {if(initializing)initializationError=e;emit storageError("邮件存储失败：" + e); });
    if(!m_mailEngine->beginTransaction()){initializing=false;return false;}
    m_mailEngine->createTable(
        "CREATE TABLE IF NOT EXISTS mails(owner_email TEXT,uid TEXT,sender TEXT,subject TEXT,subject_fold "
        "TEXT,timestamp INTEGER,isRead INTEGER,body TEXT,hidden INTEGER DEFAULT 0,starred INTEGER DEFAULT "
        "0,PRIMARY KEY(owner_email,uid))");
    m_mailEngine->createTable(
        "CREATE TABLE IF NOT EXISTS pending_seen(address TEXT,uid TEXT,PRIMARY KEY(address,uid))");
    m_mailEngine->createTable("CREATE TABLE IF NOT EXISTS cloud_flags(address TEXT,uid TEXT,payload "
                              "TEXT,PRIMARY KEY(address,uid))");
    // Existing databases predate hidden/starred columns.
    auto columns = m_mailEngine->executeQuery("PRAGMA table_info(mails)");
    QStringList names;
    while (columns.next())
        names << columns.value("name").toString();
    columns.finish();
    if (!names.contains("date_source"))
        m_mailEngine->executeNonQuery("ALTER TABLE mails ADD COLUMN date_source TEXT DEFAULT 'header'");
    for (const auto &field : {"body_loaded", "body_accessed"})
        if (!names.contains(field))
            m_mailEngine->executeNonQuery(
                QString("ALTER TABLE mails ADD COLUMN %1 INTEGER DEFAULT 0").arg(field));
    m_mailEngine->executeNonQuery("UPDATE mails SET body_loaded=1 WHERE body!=''");
    m_mailEngine->createTable(
        "CREATE TABLE IF NOT EXISTS mail_retries(address TEXT PRIMARY KEY,validity TEXT,payload TEXT)");
    if (!names.contains("hidden"))
        m_mailEngine->executeNonQuery("ALTER TABLE mails ADD COLUMN hidden INTEGER DEFAULT 0");
    if (!names.contains("starred"))
        m_mailEngine->executeNonQuery("ALTER TABLE mails ADD COLUMN starred INTEGER DEFAULT 0");
    if (!names.contains("subject_fold")) {
        m_mailEngine->executeNonQuery("ALTER TABLE mails ADD COLUMN subject_fold TEXT");
        auto rows = m_mailEngine->executeQuery("SELECT owner_email,uid,subject FROM mails");
        QList<QVariantList> folds;
        while (rows.next())
            folds.append({rows.value(2).toString().toCaseFolded(), rows.value(0), rows.value(1)});
        rows.finish();
        for (const auto &values : folds)
            m_mailEngine->executeNonQuery("UPDATE mails SET subject_fold=? WHERE owner_email=? AND uid=?",
                                          values);
    }
    m_mailEngine->executeNonQuery("PRAGMA secure_delete=ON");
    // Old trash entries kept full messages. Apply the same title-only rule to them.
    m_mailEngine->executeNonQuery(
        "UPDATE mails SET sender='',timestamp=0,isRead=0,body='',starred=0 WHERE hidden=1");
    // Keep legacy sequence-number identities offline rather than treating them as server UIDs.
    m_mailEngine->executeNonQuery("UPDATE mails SET uid='legacy:'||uid WHERE instr(uid,':')=0");
    m_mailEngine->createTable(
        "CREATE TABLE IF NOT EXISTS sync_state(address TEXT PRIMARY KEY,validity TEXT,cursor INTEGER)");
    m_mailEngine->executeNonQuery(
        "CREATE INDEX IF NOT EXISTS mails_date ON mails(timestamp DESC,owner_email,uid)");
    initialized=initializationError.isEmpty();
    if(initialized)initialized=m_mailEngine->commitTransaction();
    if(!initialized)m_mailEngine->rollbackTransaction();
    initializing=false;return initialized;
}
QList<MailBrief> MailManager::query(const QString &text, const QString &account, int state, int limit,
                                    int offset) {
    if(!initialize()){lastQueryOk=false;return {};}
    QString sql = "SELECT * FROM mails WHERE hidden=?";
    QVariantList params{state == 3 ? 1 : 0};
    if (!account.isEmpty()) {
        sql += " AND owner_email=?";
        params << account;
    }
    if (state == 1)
        sql += " AND isRead=0";
    if (state == 2)
        sql += " AND starred=1";
    if (!text.isEmpty()) {
        sql += " AND (instr(lower(subject),lower(?))>0 OR instr(lower(sender),lower(?))>0 OR "
               "instr(lower(body),lower(?))>0)";
        params << text << text << text;
    }
    sql += " ORDER BY timestamp DESC,owner_email,uid DESC LIMIT ? OFFSET ?";
    params << qBound(1, limit, 1000) << qMax(0, offset);
    QList<MailBrief> result;
    auto q = m_mailEngine->executeQuery(sql, params);
    while (q.next())
        result.append(MailBrief(q));
    lastQueryOk = q.isActive() && !q.lastError().isValid();
    return result;
}
qint64 MailManager::unreadCount() const {
    if (!ready())
        return -1;
    auto query = m_mailEngine->executeQuery("SELECT COUNT(*) FROM mails WHERE hidden=0 AND isRead=0");
    return query.next() ? query.value(0).toLongLong() : -1;
}
bool MailManager::checkMail(QString address, QString uid) {
    if (!initialize()) return false;
    const bool ok = m_mailEngine->executeNonQuery(
        "UPDATE mails SET isRead=1 WHERE owner_email=? AND uid=? AND hidden=0", {address, uid});
    if (ok) {
        emit flagsChanged(address, uid);
        emit changed();
    }
    return ok;
}
bool MailManager::addMail(const MailBrief &m) {
    if (!initialize()) return false;
    auto params = m.toBindValues();
    params << QString("") << m.subject.toCaseFolded() << m.dateSource;
    const bool ok = m_mailEngine->executeNonQuery("INSERT OR IGNORE INTO "
                                         "mails(owner_email,uid,sender,subject,timestamp,isRead,body,subject_"
                                         "fold,date_source) VALUES(?,?,?,?,?,?,?,?,?)",
                                         params);
    if (ok) { emit flagsChanged(m.ownerEmail, m.uid); emit changed(); }
    return ok;
}
bool MailManager::deleteMail(const QString &a, const QString &u) {
    if (!initialize()) return false;
    const bool ok = m_mailEngine->executeNonQuery(
        "UPDATE mails SET hidden=1,sender='',timestamp=0,isRead=0,body='',starred=0,body_loaded=0 WHERE "
        "owner_email=? AND uid=?",
        {a, u});
    if (ok) {
        emit flagsChanged(a, u);
        emit changed();
    }
    return ok;
}
bool MailManager::isDeleted(const QString &a, const QString &u) const {
    if (!ready()) return false;
    auto q = m_mailEngine->executeQuery("SELECT hidden FROM mails WHERE owner_email=? AND uid=?", {a, u});
    return q.next() && q.value(0).toBool();
}
bool MailManager::restoreMail(const QString &a, const QString &u, const QJsonObject &message) {
    if (message["id"].toString() != u || !message["from"].isString() || !message["subject"].isString() ||
        !message["date"].isDouble() || !isDeleted(a, u))
        return false;
    const bool ok = m_mailEngine->executeNonQuery(
        "UPDATE mails SET "
        "hidden=0,sender=?,subject=?,subject_fold=?,timestamp=?,date_source=?,isRead=0,starred=0,body='' "
        "WHERE owner_email=? AND uid=? AND hidden=1",
        {message["from"].toString(), message["subject"].toString(),
         message["subject"].toString().toCaseFolded(), message["date"].toInteger(),
         message["dateSource"].toString("header"), a, u});
    if (ok) {
        emit flagsChanged(a, u);
        emit changed();
    }
    return ok;
}
bool MailManager::setStar(const QString &a, const QString &u, bool star) {
    if (!initialize()) return false;
    const bool ok = m_mailEngine->executeNonQuery(
        "UPDATE mails SET starred=? WHERE owner_email=? AND uid=? AND hidden=0", {star, a, u});
    if (ok) {
        emit flagsChanged(a, u);
        emit changed();
        if (!star)
            pruneBodies();
    }
    return ok;
}
QString MailManager::error() const { return !initializationError.isEmpty()?initializationError:m_mailEngine ? m_mailEngine->error() : "邮件数据库未初始化"; }
QString MailManager::getMailBody(const QString &a, const QString &u) {
    if (!initialize()) return {};
    auto q = m_mailEngine->executeQuery("SELECT body FROM mails WHERE owner_email=? AND uid=?", {a, u});
    auto result = q.next() ? q.value(0).toString() : QString();
    q.finish();
    m_mailEngine->executeNonQuery("UPDATE mails SET body_accessed=? WHERE owner_email=? AND uid=?",
                                  {QDateTime::currentMSecsSinceEpoch(), a, u});
    return result;
}
bool MailManager::bodyLoaded(const QString &a, const QString &u) {
    if (!initialize()) return false;
    auto q =
        m_mailEngine->executeQuery("SELECT body_loaded FROM mails WHERE owner_email=? AND uid=?", {a, u});
    return q.next() && q.value(0).toBool();
}
void MailManager::pinBody(const QString &a, const QString &u) {
    pinnedAddress = a;
    pinnedUid = u;
    pruneBodies();
}
bool MailManager::updateMailBody(const QString &a, const QString &u, const QString &body) {
    if (!initialize()) return false;
    const bool ok = m_mailEngine->executeNonQuery(
        "UPDATE mails SET body=?,body_loaded=1,body_accessed=? WHERE owner_email=? AND uid=? AND hidden=0",
        {body, QDateTime::currentMSecsSinceEpoch(), a, u});
    return ok && pruneBodies();
}
bool MailManager::pruneBodies() {
    if (!initialize())
        return false;
    const int limit = qMax(1, ConfigManager::getInstance()->getMailBasical().storageLimit);
    int count = 0;
    qint64 bytes = 0;
    QList<QVariantList> discard;
    auto q =
        m_mailEngine->executeQuery("SELECT owner_email,uid,length(CAST(body AS BLOB)) FROM mails WHERE "
                                   "body_loaded=1 AND starred=0 ORDER BY body_accessed DESC,owner_email,uid");
    while (q.next()) {
        const auto a = q.value(0).toString(), u = q.value(1).toString();
        const bool pinned = a == pinnedAddress && u == pinnedUid;
        if (pinned)
            continue;
        const auto size = q.value(2).toLongLong();
        if (count >= limit || bytes + size > 256LL * 1024 * 1024)
            discard.append({a, u});
        else {
            ++count;
            bytes += size;
        }
    }
    if (!q.isActive() || q.lastError().isValid())
        return false;
    q.finish();
    for (const auto &ids : discard)
        if (!m_mailEngine->executeNonQuery(
                "UPDATE mails SET body='',body_loaded=0 WHERE owner_email=? AND uid=?", ids))
            return false;
    return true;
}
QJsonObject MailManager::syncState(const QString &a) {
    if (!initialize()) return {};
    auto q = m_mailEngine->executeQuery("SELECT validity,cursor FROM sync_state WHERE address=?", {a});
    if (!q.next())
        return {};
    QJsonObject state{{"validity", q.value(0).toString()}, {"cursor", q.value(1).toLongLong()}};
    q.finish();
    auto retry = m_mailEngine->executeQuery("SELECT payload FROM mail_retries WHERE address=? AND validity=?",
                                            {a, state["validity"].toString()});
    if (retry.next())
        state["retryUids"] = QJsonDocument::fromJson(retry.value(0).toByteArray()).array();
    return state;
}
bool MailManager::saveBatch(const QString &address, const QJsonObject &data, bool updateSync) {
    if(!initialize())return false;
    if (!m_mailEngine->beginTransaction())
        return false;
    bool ok = true;
    for (auto v : data["messages"].toArray()) {
        MailBrief m(v.toObject(), address);
        auto params = m.toBindValues();
        params << QString("") << m.subject.toCaseFolded() << m.dateSource;
        ok = ok && m_mailEngine->executeNonQuery(
                       "INSERT OR IGNORE INTO pending_seen(address,uid) VALUES(?,?)", {address, m.uid});
        ok = ok && m_mailEngine->executeNonQuery("INSERT OR IGNORE INTO "
                                                 "mails(owner_email,uid,sender,subject,timestamp,isRead,body,"
                                                 "subject_fold,date_source) VALUES(?,?,?,?,?,?,?,?,?)",
                                                 params);
    }
    if (updateSync)
        ok = ok && m_mailEngine->executeNonQuery(
                       "INSERT INTO sync_state(address,validity,cursor) VALUES(?,?,?) ON CONFLICT(address) "
                       "DO UPDATE SET validity=excluded.validity,cursor=excluded.cursor",
                       {address, data["validity"].toString(), data["cursor"].toVariant().toLongLong()});
    for (auto v : data["messages"].toArray()) {
        const auto uid = v.toObject()["id"].toString();
        auto q = m_mailEngine->executeQuery("SELECT payload FROM cloud_flags WHERE address=? AND uid=?",
                                            {address, uid});
        QJsonObject flags;
        if (q.next())
            flags = QJsonDocument::fromJson(q.value(0).toByteArray()).object();
        q.finish();
        if (!flags.isEmpty())
            ok = ok && applyCloudFlags(flags, false);
    }
    ok = ok && m_mailEngine->executeNonQuery(
                   "INSERT OR REPLACE INTO mail_retries VALUES(?,?,?)",
                   {address, data["validity"].toString(),
                    QJsonDocument(data["retryUids"].toArray()).toJson(QJsonDocument::Compact)});
    if (!ok) {
        m_mailEngine->rollbackTransaction();
        return false;
    }
    if (!m_mailEngine->commitTransaction()) {
        m_mailEngine->rollbackTransaction();
        return false;
    }
    pruneBodies();
    for (auto v : data["messages"].toArray())
        emit flagsChanged(address, v.toObject()["id"].toString());
    emit changed();
    return true;
}
void MailManager::clearUp() {
    delete m_mailEngine;
    m_mailEngine = nullptr;
    initialized=false;
}

qint64 MailManager::oldestUid(const QString &address, const QString &validity) {
    if (!initialize()) return 0;
    auto q = m_mailEngine->executeQuery("SELECT min(CAST(substr(uid,instr(uid,':')+1) AS INTEGER)) FROM "
                                        "mails WHERE owner_email=? AND uid LIKE ?",
                                        {address, validity + ":%"});
    return q.next() ? q.value(0).toLongLong() : 0;
}

QStringList MailManager::pendingSeen(const QString &address) {
    if (!initialize()) return {};
    QStringList ids;
    auto q = m_mailEngine->executeQuery("SELECT uid FROM pending_seen WHERE address=? ORDER BY uid LIMIT 100",
                                        {address});
    while (q.next())
        ids.append(q.value(0).toString());
    return ids;
}

QJsonArray MailManager::cloudFlags(bool *ok) const {
    if (ok)
        *ok = false;
    QJsonArray result;
    if (!ready())
        return result;
    auto q =
        m_mailEngine->executeQuery("SELECT owner_email,uid,isRead,starred,hidden,subject,sender,timestamp "
                                   "FROM mails WHERE uid NOT LIKE 'legacy:%'");
    while (q.next())
        result.append(QJsonObject{{"address", q.value(0).toString()},
                                  {"uid", q.value(1).toString()},
                                  {"read", q.value(2).toBool()},
                                  {"starred", q.value(3).toBool()},
                                  {"hidden", q.value(4).toBool()},
                                  {"subject", q.value(5).toString()},
                                  {"sender", q.value(6).toString()},
                                  {"timestamp", q.value(7).toLongLong()}});
    if (!q.isActive() || q.lastError().isValid())
        return {};
    q.finish();
    auto pending =
        m_mailEngine->executeQuery("SELECT payload FROM cloud_flags WHERE NOT EXISTS (SELECT 1 FROM mails "
                                   "WHERE owner_email=address AND mails.uid=cloud_flags.uid)");
    while (pending.next())
        result.append(QJsonDocument::fromJson(pending.value(0).toByteArray()).object());
    if (!pending.isActive() || pending.lastError().isValid())
        return {};
    if (ok)
        *ok = true;
    return result;
}
bool MailManager::applyCloudFlags(const QJsonObject &flags, bool notify) {
    const auto address = flags["address"].toString(), uid = flags["uid"].toString();
    if (address.isEmpty() || uid.isEmpty() || !initialize())
        return false;
    if (notify && !m_mailEngine->beginTransaction())
        return false;
    bool ok =
        m_mailEngine->executeNonQuery("INSERT OR REPLACE INTO cloud_flags VALUES(?,?,?)",
                                      {address, uid, QJsonDocument(flags).toJson(QJsonDocument::Compact)});
    if (flags["hidden"].toBool())
        ok = ok && m_mailEngine->executeNonQuery(
                       "UPDATE mails SET hidden=1,sender='',timestamp=0,isRead=0,body='',starred=0 WHERE "
                       "owner_email=? AND uid=?",
                       {address, uid});
    else
        ok = ok &&
             m_mailEngine->executeNonQuery(
                 "UPDATE mails SET hidden=0,isRead=?,starred=?,subject=?,subject_fold=?,sender=?,timestamp=? "
                 "WHERE owner_email=? AND uid=?",
                 {flags["read"].toBool(), flags["starred"].toBool(), flags["subject"].toString(),
                  flags["subject"].toString().toCaseFolded(), flags["sender"].toString(),
                  flags["timestamp"].toInteger(), address, uid});
    ok = ok && m_mailEngine->executeNonQuery("DELETE FROM cloud_flags WHERE address=? AND uid=? AND "
                                             "EXISTS(SELECT 1 FROM mails WHERE owner_email=? AND uid=?)",
                                             {address, uid, address, uid});
    if (notify) {
        if (ok)
            ok = m_mailEngine->commitTransaction();
        if (!ok)
            m_mailEngine->rollbackTransaction();
        if (ok)
            emit changed();
    }
    return ok;
}
bool MailManager::acknowledgeSeen(const QString &address, const QStringList &ids) {
    if (!initialize()) return false;
    if (!m_mailEngine->beginTransaction())
        return false;
    for (const auto &id : ids)
        if (!m_mailEngine->executeNonQuery("DELETE FROM pending_seen WHERE address=? AND uid=?",
                                           {address, id})) {
            m_mailEngine->rollbackTransaction();
            return false;
        }
    if (m_mailEngine->commitTransaction()) return true;
    m_mailEngine->rollbackTransaction();
    return false;
}

QList<MailBrief> MailManager::queryFiltered(const AdvancedFilter &filter, int state, int limit, int offset) {
    if(!initialize()){lastQueryOk=false;return {};}
    QString sql = "SELECT * FROM mails WHERE hidden=?";
    QVariantList params{state == 3 ? 1 : 0};
    if (state == 1)
        sql += " AND isRead=0";
    if (state == 2)
        sql += " AND starred=1";
    for (const auto &word : filter.words()) {
        sql += " AND instr(subject_fold,?)>0";
        params << word.toCaseFolded();
    }
    if (!filter.accounts.isEmpty()) {
        QStringList placeholders;
        for (const auto &address : filter.accounts) {
            placeholders.append("?");
            params << address;
        }
        sql += " AND owner_email IN (" + placeholders.join(',') + ")";
    }
    if (filter.read >= 0) {
        sql += " AND isRead=?";
        params << filter.read;
    }
    if (filter.starred >= 0) {
        sql += " AND starred=?";
        params << filter.starred;
    }
    if (filter.limitDates) {
        sql += " AND timestamp>=? AND timestamp<?";
        params << filter.from.startOfDay().toSecsSinceEpoch()
               << filter.to.addDays(1).startOfDay().toSecsSinceEpoch();
    }
    sql += " ORDER BY timestamp DESC,owner_email,uid DESC LIMIT ? OFFSET ?";
    params << qBound(1, limit, 1000) << qMax(0, offset);
    QList<MailBrief> result;
    auto q = m_mailEngine->executeQuery(sql, params);
    while (q.next())
        result.append(MailBrief(q));
    lastQueryOk = q.isActive() && !q.lastError().isValid();
    return result;
}

QJsonObject MailManager::cloudFlag(const QString &address, const QString &uid, bool *ok) const {
    if (ok)
        *ok = false;
    if (!ready())
        return {};
    auto q = m_mailEngine->executeQuery(
        "SELECT isRead,starred,hidden,subject,sender,timestamp FROM mails WHERE owner_email=? AND uid=?",
        {address, uid});
    if (!q.isActive() || q.lastError().isValid())
        return {};
    if (q.next()) {
        if (ok)
            *ok = true;
        return {{"address", address},
                {"uid", uid},
                {"read", q.value(0).toBool()},
                {"starred", q.value(1).toBool()},
                {"hidden", q.value(2).toBool()},
                {"subject", q.value(3).toString()},
                {"sender", q.value(4).toString()},
                {"timestamp", q.value(5).toLongLong()}};
    }
    q.finish();
    auto pending = m_mailEngine->executeQuery("SELECT payload FROM cloud_flags WHERE address=? AND uid=?",
                                              {address, uid});
    const bool found = pending.next();
    if (ok)
        *ok = pending.isActive() && !pending.lastError().isValid();
    return found ? QJsonDocument::fromJson(pending.value(0).toByteArray()).object() : QJsonObject();
}
