#include "notedepartment.h"
#include "cryptotool.h"
#include "vaultcodec.h"
#include <QJsonDocument>
#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QTimer>
#include <QUuid>
#include <QFile>
#include <QDateTime>
#include <QTimeZone>
#include <QSet>
#include <QTemporaryDir>
#include <stdexcept>
#include <QScopedValueRollback>
#include "bookmarktarget.h"
namespace {
QJsonObject currentNote(QJsonObject entry){
    entry.remove("category");
    if(entry["kind"].toInt()==0){
        QStringList parts;
        if(!entry["account"].toString().isEmpty())parts.append("账号："+entry["account"].toString());
        if(!entry["secret"].toString().isEmpty())parts.append("秘密："+entry["secret"].toString());
        if(!entry["body"].toString().isEmpty())parts.append(entry["body"].toString());
        entry["body"]=parts.join("\n");entry.remove("account");entry.remove("secret");entry.remove("tags");
    }
    return entry;
}
QJsonObject migrateNote(QJsonObject e,int index,bool *truncated=nullptr){
    e=currentNote(e);
    if(e["createdAt"].toString().isEmpty())e["createdAt"]=QDateTime::fromMSecsSinceEpoch(946684800000LL+index,QTimeZone::UTC).toString(Qt::ISODateWithMs);
    if(e["kind"].toInt()!=0){
        auto tags=e["tags"].toString().split(',',Qt::SkipEmptyParts);for(auto &tag:tags)tag=tag.trimmed();tags.removeAll("");tags.removeDuplicates();
        if(tags.size()>3){if(truncated)*truncated=true;tags=tags.mid(0,3);}e["tags"]=tags.join(',');
        if(e["kind"].toInt()==1&&!e.contains("targetType")){e["targetType"]="website";e["target"]=e["body"].toString().trimmed();e.remove("body");}
    }
    return e;
}

}
NoteDepartment::NoteDepartment(VaultRepository *repository,QObject *parent):QObject(parent),repo(repository),publicRepo(repository->path()+".notes.json") {
    if(!publicRepo.ready())message=publicRepo.error();
    else{
        QJsonArray migrated;bool truncated=false;int index=0;
        for(auto value:publicRepo.entries()){
            auto e=migrateNote(value.toObject(),index++,&truncated);
            if(e["kind"].toInt()!=0&&e["deleted"].toBool())continue;
            if(e["kind"].toInt()==0)e=QJsonObject{{"id",e["id"]},{"kind",0},{"title",e["title"]},{"createdAt",e["createdAt"]},{"deleted",e["deleted"]}};
            migrated.append(e);
        }
        if(migrated!=publicRepo.entries()&&!publicRepo.save(migrated,publicRepo.indexed()))message=publicRepo.error();
        if(truncated)migrationMessage="旧纸条和书签的标签已保留前 3 个；秘密不再保留标签。";
    }
}

NoteDepartment::~NoteDepartment(){lock();}
bool NoteDepartment::creationReady() const {return repo->exists()&&VaultCodec::ready(repo->read());}
QJsonArray NoteDepartment::catalog() const {
    const auto vault=repo->exists()?repo->read():QJsonObject();if(vault["version"]!=2)return publicRepo.entries();
    QJsonArray result;for(auto v:publicRepo.entries())if(v.toObject()["kind"].toInt()!=0)result.append(v);
    for(auto value:vault["entries"].toArray()){const auto e=value.toObject();result.append(QJsonObject{{"id",e["id"]},{"kind",0},{"title",e["title"]},{"createdAt",e["createdAt"]}});}return result;
}
bool NoteDepartment::create(const QString &password){
    if(exists()){message="密库已存在，请验证主密码完成升级";return false;}
    if(password.size()<10){message="主密码至少 10 个字符";return false;}
    if(!publicRepo.ready()){message=publicRepo.error();return false;}
    QByteArray master,priv;
    try {
        const auto vault=VaultCodec::create(password,{},&master,&priv);
        if(!repo->write(vault)){message=repo->error();CryptoTool::wipe(master);CryptoTool::wipe(priv);return false;}
        key=master;privateKey=priv;sessionVault=vault;items={};message.clear();emit changed();return true;
    }catch(...){CryptoTool::wipe(master);CryptoTool::wipe(priv);message="无法初始化秘密加密";return false;}
}
bool NoteDepartment::migrateLegacy(const QJsonObject &env,const QString &password){
    QByteArray candidate,master,priv,plain;
    try {
        if(env["version"]!=1||env["kdf"]!="PBKDF2-SHA256-600000"){message="密库格式不兼容";return false;}
        const auto salt=QByteArray::fromBase64(env["salt"].toString().toLatin1());if(salt.size()!=16){message="密库格式损坏";return false;}
        candidate=CryptoTool::derive(password,salt);bool ok=false;plain=CryptoTool::decrypt(env,candidate,&ok);CryptoTool::wipe(candidate);
        auto doc=QJsonDocument::fromJson(plain);CryptoTool::wipe(plain);if(!ok||!doc.isArray()){message="密码错误或密库损坏";return false;}
        if(!publicRepo.ready()){message=publicRepo.error();return false;}
        QJsonArray ordinary,secrets;for(auto v:publicRepo.entries())if(v.toObject()["kind"].toInt()!=0)ordinary.append(v);
        bool split=false,truncated=false;int index=0;QSet<QString> ids;
        for(auto v:doc.array()){
            auto e=migrateNote(v.toObject(),index++,&truncated);if(e["deleted"].toBool())continue;
            const auto id=e["id"].toString();if(id.isEmpty()||ids.contains(id)||e["title"].toString().trimmed().isEmpty()){message="旧密库条目损坏，未升级";return false;}ids.insert(id);
            if(e["kind"].toInt()==0)secrets.append(e);
            else{split=true;bool found=false;for(auto old:ordinary)if(old.toObject()["id"]==e["id"])found=true;if(!found)ordinary.append(e);}
        }
        const auto upgraded=VaultCodec::create(password,secrets,&master,&priv);
        // Verify the new representation before replacing the source.
        QJsonArray check;QByteArray checkMaster,checkPrivate;QString failure;
        if(!VaultCodec::open(upgraded,password,&check,&checkMaster,&checkPrivate,&failure)||check!=secrets){CryptoTool::wipe(checkMaster);CryptoTool::wipe(checkPrivate);throw std::runtime_error("migration verification");}
        CryptoTool::wipe(checkMaster);CryptoTool::wipe(checkPrivate);
        if(split&&!QFile::exists(repo->path()+".before-split")&&!repo->backup(repo->path()+".before-split")){message=repo->error();CryptoTool::wipe(master);CryptoTool::wipe(priv);return false;}
        if(!publicRepo.save(ordinary,true)){message=publicRepo.error();CryptoTool::wipe(master);CryptoTool::wipe(priv);return false;}
        if(!repo->write(upgraded)){message=repo->error();CryptoTool::wipe(master);CryptoTool::wipe(priv);return false;}
        key=master;privateKey=priv;items=secrets;message.clear();if(truncated)migrationMessage="旧纸条和书签的标签已保留前 3 个；秘密不保留标签。";return true;
    }catch(...){CryptoTool::wipe(candidate);CryptoTool::wipe(plain);CryptoTool::wipe(master);CryptoTool::wipe(priv);message="旧密库升级失败，原加密文件未替换";return false;}
}
bool NoteDepartment::unlock(const QString &password){
    if(unlocked())return true;
    const auto vault=repo->read();
    bool ok=false;if(vault["version"]==1)ok=migrateLegacy(vault,password);
    else ok=VaultCodec::open(vault,password,&items,&key,&privateKey,&message);
    if(ok){sessionVault=vault["version"]==1?repo->read():vault;emit changed();}return ok;
}
void NoteDepartment::lock(bool clearClipboard){
    if(locking)return;QScopedValueRollback<bool> guard(locking,true);
    if(unlocked())emit aboutToLock();
    if(!pendingDrafts.isEmpty())flushDrafts();
    items={};CryptoTool::wipe(key);CryptoTool::wipe(privateKey);
    auto clipboard=QApplication::clipboard();
    if(clearClipboard&&!clipboardFingerprint.isEmpty()&&QCryptographicHash::hash(clipboard->text().toUtf8(),QCryptographicHash::Sha256)==clipboardFingerprint)clipboard->clear();
    if(clearClipboard)clipboardFingerprint.clear();emit locked();emit changed();
}
namespace {
void upsert(QJsonArray &array,const QJsonObject &entry){for(int i=0;i<array.size();++i)if(array[i].toObject()["id"]==entry["id"]){array[i]=entry;return;}array.append(entry);}
QJsonArray withoutId(const QJsonArray &array,const QString &id){QJsonArray result;for(auto value:array)if(value.toObject()["id"]!=id)result.append(value);return result;}
bool sameVault(const QJsonObject &a,const QJsonObject &b){return a["vaultId"]==b["vaultId"]&&a["publicKey"]==b["publicKey"]&&!a["vaultId"].toString().isEmpty();}
}
bool NoteDepartment::flushDrafts(){
    if(pendingDrafts.isEmpty())return true;
    auto vault=repo->read();
    if(!VaultCodec::ready(vault)){message="无法写入密库，恢复草稿仅保留在本次运行的加密内存中，退出后会丢失";return false;}
    auto drafts=vault["drafts"].toArray();
    for(const auto &pending:pendingDrafts){
        if(!sameVault(vault,pending)){message="密库身份发生变化，恢复草稿仅保留在旧密库的加密内存副本中；退出后会丢失，请恢复原密库文件后再保存";return false;}
        upsert(drafts,pending["sealed"].toObject());
    }
    vault["drafts"]=drafts;
    if(!repo->write(vault)){message="恢复草稿未能写入磁盘，仅保留在本次运行的加密内存中，退出后会丢失";return false;}
    sessionVault=vault;pendingDrafts.clear();message.clear();return true;
}
bool NoteDepartment::preserveDraft(const QJsonObject &entry){
    if(!unlocked()||entry["kind"]!=0||entry["id"].toString().isEmpty()){message="无法封存秘密草稿";return false;}
    auto vault=repo->read();
    if(!VaultCodec::ready(vault)||!sameVault(vault,sessionVault))vault=sessionVault;
    try {
        if(!VaultCodec::ready(vault)||CryptoTool::rsaPublicKey(privateKey)!=QByteArray::fromBase64(vault["publicKey"].toString().toLatin1()))throw std::runtime_error("vault changed");
        const QJsonObject draft{{"id",entry["id"]},{"kind",0},{"title","恢复草稿"},{"createdAt",entry["createdAt"]},
            {"purpose","recovery-draft"},{"body",QString::fromUtf8(QJsonDocument(entry).toJson(QJsonDocument::Compact))}};
        pendingDrafts[entry["id"].toString()]={{"vaultId",vault["vaultId"]},{"publicKey",vault["publicKey"]},{"sealed",VaultCodec::seal(vault,draft)}};
    }catch(...){message="秘密草稿加密失败，本轮输入未能封存；上次保存的内容保持不变";return false;}
    return flushDrafts();
}
QJsonObject NoteDepartment::recoveryDraft(const QString &id){
    message.clear();
    if(!unlocked())return {};
    auto vault=repo->read();if(!VaultCodec::ready(vault))vault=sessionVault;
    QJsonObject sealed;
    if(pendingDrafts.contains(id)){
        const auto pending=pendingDrafts.value(id);if(!sameVault(vault,pending)){message="恢复草稿属于其他密库";return {};}
        sealed=pending["sealed"].toObject();
    }else for(auto value:vault["drafts"].toArray())if(value.toObject()["id"]==id){sealed=value.toObject();break;}
    if(sealed.isEmpty())return {};
    try {
        const auto draft=VaultCodec::unseal(vault,sealed,privateKey);
        auto plain=draft["body"].toString().toUtf8();QJsonParseError error;const auto doc=QJsonDocument::fromJson(plain,&error);CryptoTool::wipe(plain);
        const auto entry=doc.object();
        if(draft["purpose"]!="recovery-draft"||error.error!=QJsonParseError::NoError||!doc.isObject()||entry["id"]!=id||entry["kind"]!=0)throw std::runtime_error("draft");
        return entry;
    }catch(...){message="恢复草稿损坏，已保留原文件；正式秘密内容保持不变";return {};}
}
QJsonObject NoteDepartment::entry(const QString &id) const {
    if(unlocked())for(auto e:items)if(e.toObject()["id"]==id)return e.toObject();
    for(auto e:catalog())if(e.toObject()["id"]==id)return e.toObject();return {};
}
bool NoteDepartment::save(QJsonObject e){
    message.clear();e=currentNote(e);const int kind=e["kind"].toInt(-1);
    if(kind<0||kind>2){message="无效条目类型";return false;}
    if(e["title"].toString().trimmed().isEmpty()){message="请输入标题";return false;}
    e["title"]=e["title"].toString().trimmed();
    if(kind!=0){
        auto tags=e["tags"].toString().split(',',Qt::SkipEmptyParts);for(auto &tag:tags)tag=tag.trimmed();tags.removeAll("");tags.removeDuplicates();if(tags.size()>3){message="最多选择三个标签";return false;}e["tags"]=tags.join(',');
        if(kind==1){if(!e.contains("targetType")){e["targetType"]="website";e["target"]=e["body"].toString().trimmed();}message=BookmarkTarget::validate(e);if(!message.isEmpty())return false;e.remove("body");}
    }
    if(e["id"].toString().isEmpty())e["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto previous=entry(e["id"].toString());if(!previous.isEmpty()&&previous["kind"]!=e["kind"]){message="请新建其他类别的条目";return false;}
    if(previous.isEmpty()){
        auto time=QDateTime::currentMSecsSinceEpoch();for(auto v:catalog())time=qMax(time,QDateTime::fromString(v.toObject()["createdAt"].toString(),Qt::ISODateWithMs).toMSecsSinceEpoch()+1);
        e["createdAt"]=QDateTime::fromMSecsSinceEpoch(time,QTimeZone::UTC).toString(Qt::ISODateWithMs);
    }else e["createdAt"]=previous["createdAt"];
    e.remove("deleted");
    if(kind==0){
        if(!creationReady()){message="请先在设置 → Note 中初始化或升级秘密加密";return false;}
        if(!previous.isEmpty()&&!unlocked()){message="修改已有秘密前，请验证主密码";return false;}
        try{
            auto vault=repo->read();if(unlocked()&&CryptoTool::rsaPublicKey(privateKey)!=QByteArray::fromBase64(vault["publicKey"].toString().toLatin1())){message="密库公钥发生变化，请关闭并重新验证";return false;}auto sealed=vault["entries"].toArray();upsert(sealed,VaultCodec::seal(vault,e));vault["entries"]=sealed;
            vault["drafts"]=withoutId(vault["drafts"].toArray(),e["id"].toString());
            if(!repo->write(vault)){message=repo->error();return false;}pendingDrafts.remove(e["id"].toString());sessionVault=vault;if(unlocked())upsert(items,e);
        }catch(...){message="秘密加密保存失败，原文件保持不变";return false;}
    }else{
        e.remove("secret");e.remove("account");auto next=publicRepo.entries();upsert(next,e);if(!publicRepo.save(next,publicRepo.indexed())){message=publicRepo.error();return false;}
    }
    emit changed();return true;
}
bool NoteDepartment::remove(const QString &id){
    const auto e=entry(id);if(e.isEmpty()){message="条目不存在";return false;}
    if(e["kind"].toInt()==0){
        if(!unlocked()){message="请先验证主密码";return false;}
        auto vault=repo->read();if(!VaultCodec::ready(vault)){message="密库格式损坏，无法删除";return false;}QJsonArray next;for(auto v:vault["entries"].toArray())if(v.toObject()["id"]!=id)next.append(v);vault["entries"]=next;
        vault["drafts"]=withoutId(vault["drafts"].toArray(),id);
        if(!repo->write(vault)){message=repo->error();return false;}pendingDrafts.remove(id);sessionVault=vault;items=withoutId(items,id);
    }else{
        QJsonArray next;for(auto v:publicRepo.entries())if(v.toObject()["id"]!=id)next.append(v);if(!publicRepo.save(next,publicRepo.indexed())){message=publicRepo.error();return false;}
    }
    message.clear();emit changed();return true;
}
bool NoteDepartment::backup(const QString &path){if(!flushDrafts())return false;if(!repo->backup(path)){message=repo->error();return false;}return true;}
bool NoteDepartment::restore(const QString &path,const QString &password){
    VaultRepository source(path);const auto env=source.read();QByteArray master,priv;QJsonArray records;QString failure;
    QJsonObject replacement=env;
    if(env["version"]==2){
        if(!VaultCodec::open(env,password,&records,&master,&priv,&message))return false;
    }else if(env["version"]==1){
        // Use an isolated repository so failed validation/migration cannot replace the live vault.
        QTemporaryDir scratch;if(!scratch.isValid()){message="无法建立备份验证目录";return false;}
        VaultRepository candidate(scratch.filePath("vault"));if(!candidate.write(env)){message=candidate.error();return false;}
        NoteDepartment validator(&candidate);if(!validator.unlock(password)){message=validator.error();return false;}
        replacement=candidate.read();if(!VaultCodec::open(replacement,password,&records,&master,&priv,&message))return false;
        auto ordinary=publicRepo.entries();for(auto v:validator.catalog())if(v.toObject()["kind"].toInt()!=0)upsert(ordinary,v.toObject());
        if(!publicRepo.save(ordinary,publicRepo.indexed())){CryptoTool::wipe(master);CryptoTool::wipe(priv);message=publicRepo.error();return false;}
    }else{message="备份格式不兼容";return false;}
    CryptoTool::wipe(master);CryptoTool::wipe(priv);
    lock();if(!flushDrafts())return false;
    if(repo->exists()&&!repo->backup(repo->path()+".before-restore")){message=repo->error();return false;}
    if(!repo->write(replacement)){message=repo->error();return false;}pendingDrafts.clear();sessionVault={};return unlock(password);
}
bool NoteDepartment::changePassword(const QString &password){
    if(!unlocked()||password.size()<10){message="先验证主密码，新密码至少 10 个字符";return false;}
    if(!flushDrafts())return false;
    QByteArray master;
    try{auto vault=VaultCodec::rekey(repo->read(),password,privateKey,&master);if(!repo->write(vault)){CryptoTool::wipe(master);message=repo->error();return false;}CryptoTool::wipe(key);key=master;sessionVault=vault;message.clear();return true;}
    catch(...){CryptoTool::wipe(master);message="密码修改失败";return false;}
}
void NoteDepartment::copySecret(const QString &text) {
    if(!unlocked()) return;
    QApplication::clipboard()->setText(text);clipboardFingerprint=QCryptographicHash::hash(text.toUtf8(),QCryptographicHash::Sha256);
    const auto expected=clipboardFingerprint;
    QTimer::singleShot(30000,this,[this,expected]{
        auto cb=QApplication::clipboard();
        if(clipboardFingerprint==expected&&QCryptographicHash::hash(cb->text().toUtf8(),QCryptographicHash::Sha256)==expected) cb->clear();
        if(clipboardFingerprint==expected) clipboardFingerprint.clear();
    });
}
