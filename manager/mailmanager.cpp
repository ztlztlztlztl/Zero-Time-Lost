#include "mailmanager.h"
#include <QJsonArray>
#include "configmanager.h"
MailManager *MailManager::getInstance() {static MailManager instance;return &instance;}
void MailManager::initialize() {
    if(m_mailEngine)return;
    m_mailEngine=new HelpSQLiteIOEngine("./database","mail.db","ZTL_Mail_Conn",this);
    m_mailEngine->createTable("CREATE TABLE IF NOT EXISTS mails(owner_email TEXT,uid TEXT,sender TEXT,subject TEXT,subject_fold TEXT,timestamp INTEGER,isRead INTEGER,body TEXT,hidden INTEGER DEFAULT 0,starred INTEGER DEFAULT 0,PRIMARY KEY(owner_email,uid))");
    m_mailEngine->createTable("CREATE TABLE IF NOT EXISTS pending_seen(address TEXT,uid TEXT,PRIMARY KEY(address,uid))");
    // Existing databases predate hidden/starred columns.
    auto columns=m_mailEngine->executeQuery("PRAGMA table_info(mails)");
    QStringList names;while(columns.next()) names<<columns.value("name").toString();columns.finish();
    if(!names.contains("hidden"))m_mailEngine->executeNonQuery("ALTER TABLE mails ADD COLUMN hidden INTEGER DEFAULT 0");
    if(!names.contains("starred"))m_mailEngine->executeNonQuery("ALTER TABLE mails ADD COLUMN starred INTEGER DEFAULT 0");
    if(!names.contains("subject_fold")){
        m_mailEngine->executeNonQuery("ALTER TABLE mails ADD COLUMN subject_fold TEXT");
        auto rows=m_mailEngine->executeQuery("SELECT owner_email,uid,subject FROM mails");
        QList<QVariantList> folds;while(rows.next())folds.append({rows.value(2).toString().toCaseFolded(),rows.value(0),rows.value(1)});rows.finish();
        for(const auto &values:folds)m_mailEngine->executeNonQuery("UPDATE mails SET subject_fold=? WHERE owner_email=? AND uid=?",values);
    }
    m_mailEngine->executeNonQuery("PRAGMA secure_delete=ON");
    // Old trash entries kept full messages. Apply the same title-only rule to them.
    m_mailEngine->executeNonQuery("UPDATE mails SET sender='',timestamp=0,isRead=0,body='',starred=0 WHERE hidden=1");
    // Keep legacy sequence-number identities offline rather than treating them as server UIDs.
    m_mailEngine->executeNonQuery("UPDATE mails SET uid='legacy:'||uid WHERE instr(uid,':')=0");
    m_mailEngine->createTable("CREATE TABLE IF NOT EXISTS sync_state(address TEXT PRIMARY KEY,validity TEXT,cursor INTEGER)");
    m_mailEngine->executeNonQuery("CREATE INDEX IF NOT EXISTS mails_date ON mails(timestamp DESC,owner_email,uid)");
}
QList<MailBrief> MailManager::query(const QString &text,const QString &account,int state,int limit,int offset) {
    QString sql="SELECT * FROM mails WHERE hidden=?";
    QVariantList params{state==3?1:0};
    if(!account.isEmpty()){sql+=" AND owner_email=?";params<<account;}
    if(state==1)sql+=" AND isRead=0";
    if(state==2)sql+=" AND starred=1";
    if(!text.isEmpty()){sql+=" AND (instr(lower(subject),lower(?))>0 OR instr(lower(sender),lower(?))>0 OR instr(lower(body),lower(?))>0)";params<<text<<text<<text;}
    sql+=" ORDER BY timestamp DESC,owner_email,uid DESC LIMIT ? OFFSET ?";params<<qBound(1,limit,1000)<<qMax(0,offset);
    QList<MailBrief> result;auto q=m_mailEngine->executeQuery(sql,params);while(q.next())result.append(MailBrief(q));return result;
}
qint64 MailManager::unreadCount() const {
    if(!m_mailEngine)return -1;
    auto query=m_mailEngine->executeQuery("SELECT COUNT(*) FROM mails WHERE hidden=0 AND isRead=0");
    return query.next()?query.value(0).toLongLong():-1;
}
void MailManager::checkMail(QString address,QString uid){if(m_mailEngine->executeNonQuery("UPDATE mails SET isRead=1 WHERE owner_email=? AND uid=? AND hidden=0",{address,uid}))emit changed();}
void MailManager::addMail(const MailBrief &m) {auto params=m.toBindValues();params<<QString("")<<m.subject.toCaseFolded();m_mailEngine->executeNonQuery("INSERT OR IGNORE INTO mails(owner_email,uid,sender,subject,timestamp,isRead,body,subject_fold) VALUES(?,?,?,?,?,?,?,?)",params);}
void MailManager::deleteMail(const QString &a,const QString &u){if(m_mailEngine->executeNonQuery("UPDATE mails SET hidden=1,sender='',timestamp=0,isRead=0,body='',starred=0 WHERE owner_email=? AND uid=?",{a,u}))emit changed();}
bool MailManager::isDeleted(const QString &a,const QString &u) const{auto q=m_mailEngine->executeQuery("SELECT hidden FROM mails WHERE owner_email=? AND uid=?",{a,u});return q.next()&&q.value(0).toBool();}
bool MailManager::restoreMail(const QString &a,const QString &u,const QJsonObject &message){
    if(message["id"].toString()!=u||!message["from"].isString()||!message["subject"].isString()||!message["date"].isDouble()||!isDeleted(a,u))return false;
    const bool ok=m_mailEngine->executeNonQuery("UPDATE mails SET hidden=0,sender=?,subject=?,subject_fold=?,timestamp=?,isRead=0,starred=0,body='' WHERE owner_email=? AND uid=? AND hidden=1",
        {message["from"].toString(),message["subject"].toString(),message["subject"].toString().toCaseFolded(),message["date"].toInteger(),a,u});
    if(ok)emit changed();return ok;
}
void MailManager::setStar(const QString &a,const QString &u,bool star){if(m_mailEngine->executeNonQuery("UPDATE mails SET starred=? WHERE owner_email=? AND uid=? AND hidden=0",{star,a,u}))emit changed();}
QString MailManager::getMailBody(const QString &a,const QString &u){auto q=m_mailEngine->executeQuery("SELECT body FROM mails WHERE owner_email=? AND uid=?",{a,u});return q.next()?q.value(0).toString():QString();}
void MailManager::updateMailBody(const QString &a,const QString &u,const QString &body){m_mailEngine->executeNonQuery("UPDATE mails SET body=? WHERE owner_email=? AND uid=? AND hidden=0",{body,a,u});}
QJsonObject MailManager::syncState(const QString &a){auto q=m_mailEngine->executeQuery("SELECT validity,cursor FROM sync_state WHERE address=?",{a});if(!q.next())return {};return {{"validity",q.value(0).toString()},{"cursor",q.value(1).toLongLong()}};}
bool MailManager::saveBatch(const QString &address,const QJsonObject &data,bool updateSync){
    if(!m_mailEngine->beginTransaction())return false;
    bool ok=true;
    for(auto v:data["messages"].toArray()){
        MailBrief m(v.toObject(),address);auto params=m.toBindValues();params<<QString("")<<m.subject.toCaseFolded();
        ok=ok&&m_mailEngine->executeNonQuery("INSERT OR IGNORE INTO pending_seen(address,uid) VALUES(?,?)",{address,m.uid});
        ok=ok&&m_mailEngine->executeNonQuery("INSERT OR IGNORE INTO mails(owner_email,uid,sender,subject,timestamp,isRead,body,subject_fold) VALUES(?,?,?,?,?,?,?,?)",params);
    }
    if(updateSync)ok=ok&&m_mailEngine->executeNonQuery("INSERT INTO sync_state(address,validity,cursor) VALUES(?,?,?) ON CONFLICT(address) DO UPDATE SET validity=excluded.validity,cursor=excluded.cursor",
                                     {address,data["validity"].toString(),data["cursor"].toVariant().toLongLong()});
    if(!ok){m_mailEngine->rollbackTransaction();return false;}
    if(!m_mailEngine->commitTransaction()){m_mailEngine->rollbackTransaction();return false;}
    // Retention applies to re-downloadable bodies, never identity/tombstones or sources.
    const int limit=qMax(1,ConfigManager::getInstance()->getMailBasical().storageLimit);
    m_mailEngine->executeNonQuery("UPDATE mails SET body='' WHERE starred=0 AND (owner_email,uid) NOT IN (SELECT owner_email,uid FROM mails ORDER BY timestamp DESC LIMIT ?)",{limit});
    emit changed();return true;
}
void MailManager::clearUp(){delete m_mailEngine;m_mailEngine=nullptr;}

qint64 MailManager::oldestUid(const QString &address,const QString &validity){
    auto q=m_mailEngine->executeQuery("SELECT min(CAST(substr(uid,instr(uid,':')+1) AS INTEGER)) FROM mails WHERE owner_email=? AND uid LIKE ?",{address,validity+":%"});
    return q.next()?q.value(0).toLongLong():0;
}











QStringList MailManager::pendingSeen(const QString &address){
    QStringList ids;auto q=m_mailEngine->executeQuery("SELECT uid FROM pending_seen WHERE address=? ORDER BY uid LIMIT 100",{address});while(q.next())ids.append(q.value(0).toString());return ids;
}
bool MailManager::acknowledgeSeen(const QString &address,const QStringList &ids){
    if(!m_mailEngine->beginTransaction())return false;
    for(const auto &id:ids)if(!m_mailEngine->executeNonQuery("DELETE FROM pending_seen WHERE address=? AND uid=?",{address,id})){m_mailEngine->rollbackTransaction();return false;}
    return m_mailEngine->commitTransaction();
}

QList<MailBrief> MailManager::queryFiltered(const AdvancedFilter &filter,int state,int limit,int offset){
    QString sql="SELECT * FROM mails WHERE hidden=?";
    QVariantList params{state==3?1:0};
    if(state==1)sql+=" AND isRead=0";
    if(state==2)sql+=" AND starred=1";
    for(const auto &word:filter.words()){sql+=" AND instr(subject_fold,?)>0";params<<word.toCaseFolded();}
    if(!filter.accounts.isEmpty()){
        QStringList placeholders;for(const auto &address:filter.accounts){placeholders.append("?");params<<address;}
        sql+=" AND owner_email IN ("+placeholders.join(',')+")";
    }
    if(filter.read>=0){sql+=" AND isRead=?";params<<filter.read;}
    if(filter.starred>=0){sql+=" AND starred=?";params<<filter.starred;}
    if(filter.limitDates){
        sql+=" AND timestamp>=? AND timestamp<?";
        params<<filter.from.startOfDay().toSecsSinceEpoch()<<filter.to.addDays(1).startOfDay().toSecsSinceEpoch();
    }
    sql+=" ORDER BY timestamp DESC,owner_email,uid DESC LIMIT ? OFFSET ?";
    params<<qBound(1,limit,1000)<<qMax(0,offset);
    QList<MailBrief> result;auto q=m_mailEngine->executeQuery(sql,params);while(q.next())result.append(MailBrief(q));return result;
}
