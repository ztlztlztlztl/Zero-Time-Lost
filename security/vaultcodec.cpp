#include "vaultcodec.h"
#include "cryptotool.h"
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QSet>
#include <QUuid>
#include <stdexcept>
namespace {
QString fingerprint(const QJsonObject &vault){return QString::fromLatin1(QCryptographicHash::hash(QByteArray::fromBase64(vault["publicKey"].toString().toLatin1()),QCryptographicHash::Sha256).toHex());}
void wrapPrivate(QJsonObject &vault,const QByteArray &privateKey,const QByteArray &master){
    auto bytes=QJsonDocument(QJsonObject{{"vaultId",vault["vaultId"]},{"fingerprint",fingerprint(vault)},{"key",QString::fromLatin1(privateKey.toBase64())}}).toJson(QJsonDocument::Compact);
    vault["private"]=CryptoTool::encrypt(bytes,master);CryptoTool::wipe(bytes);
}
}
bool VaultCodec::ready(const QJsonObject &vault){
    const auto wrapped=vault["private"].toObject();
    return vault["version"]==2&&vault["kdf"]=="PBKDF2-SHA256-600000"&&!vault["vaultId"].toString().isEmpty()
        &&vault["entries"].isArray()&&!vault["publicKey"].toString().isEmpty()
        &&QByteArray::fromBase64(vault["salt"].toString().toLatin1()).size()==16
        &&QByteArray::fromBase64(wrapped["nonce"].toString().toLatin1()).size()==12
        &&QByteArray::fromBase64(wrapped["tag"].toString().toLatin1()).size()==16&&!wrapped["cipher"].toString().isEmpty();
}
QJsonObject VaultCodec::seal(const QJsonObject &vault,const QJsonObject &entry){
    if(vault["publicKey"].toString().isEmpty()||vault["vaultId"].toString().isEmpty())throw std::runtime_error("missing public key");
    auto key=CryptoTool::random(32);auto payload=entry;payload["vaultId"]=vault["vaultId"];
    auto plain=QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QJsonObject result;
    try {
        result={{"id",entry["id"]},{"title",entry["title"]},{"createdAt",entry["createdAt"]},{"payload",CryptoTool::encrypt(plain,key)},
                {"wrappedKey",QString::fromLatin1(CryptoTool::rsaEncrypt(QByteArray::fromBase64(vault["publicKey"].toString().toLatin1()),key,vault["vaultId"].toString().toUtf8()).toBase64())}};
    }catch(...){CryptoTool::wipe(key);CryptoTool::wipe(plain);throw;}
    CryptoTool::wipe(key);CryptoTool::wipe(plain);return result;
}
QJsonObject VaultCodec::create(const QString &password,const QJsonArray &entries,QByteArray *master,QByteArray *privateKey){
    QByteArray pub;CryptoTool::rsaKeyPair(pub,*privateKey);auto salt=CryptoTool::random(16);*master=CryptoTool::derive(password,salt);
    QJsonObject vault{{"version",2},{"kdf","PBKDF2-SHA256-600000"},{"vaultId",QUuid::createUuid().toString(QUuid::WithoutBraces)},
                      {"publicKey",QString::fromLatin1(pub.toBase64())},{"salt",QString::fromLatin1(salt.toBase64())}};
    wrapPrivate(vault,*privateKey,*master);QJsonArray sealed;for(auto e:entries)sealed.append(seal(vault,e.toObject()));vault["entries"]=sealed;return vault;
}
QJsonObject VaultCodec::unseal(const QJsonObject &vault,const QJsonObject &record,const QByteArray &privateKey){
    QByteArray key,bytes;
    try {
        key=CryptoTool::rsaDecrypt(privateKey,QByteArray::fromBase64(record["wrappedKey"].toString().toLatin1()),vault["vaultId"].toString().toUtf8());
        bool ok=false;bytes=CryptoTool::decrypt(record["payload"].toObject(),key,&ok);CryptoTool::wipe(key);
        const auto doc=QJsonDocument::fromJson(bytes);CryptoTool::wipe(bytes);auto entry=doc.object();
        if(!ok||!doc.isObject()||entry["vaultId"]!=vault["vaultId"]||entry["kind"]!=0)throw std::runtime_error("payload");
        for(auto field:{"id","title","createdAt"})if(entry[field]!=record[field])throw std::runtime_error("metadata");
        entry.remove("vaultId");return entry;
    }catch(...){CryptoTool::wipe(key);CryptoTool::wipe(bytes);throw;}
}
bool VaultCodec::open(const QJsonObject &vault,const QString &password,QJsonArray *entries,QByteArray *master,QByteArray *privateKey,QString *error){
    *entries={};CryptoTool::wipe(*master);CryptoTool::wipe(*privateKey);
    try {
        if(!ready(vault))throw std::runtime_error("format");
        const auto salt=QByteArray::fromBase64(vault["salt"].toString().toLatin1());if(salt.size()!=16)throw std::runtime_error("salt");
        *master=CryptoTool::derive(password,salt);bool ok=false;auto plain=CryptoTool::decrypt(vault["private"].toObject(),*master,&ok);auto info=QJsonDocument::fromJson(plain).object();CryptoTool::wipe(plain);
        if(!ok||info["vaultId"]!=vault["vaultId"]||info["fingerprint"]!=fingerprint(vault))throw std::runtime_error("password");
        *privateKey=QByteArray::fromBase64(info["key"].toString().toLatin1());info={};
        if(CryptoTool::rsaPublicKey(*privateKey)!=QByteArray::fromBase64(vault["publicKey"].toString().toLatin1()))throw std::runtime_error("key mismatch");
        QJsonArray decoded;QSet<QString> ids;
        for(auto value:vault["entries"].toArray()){
            const auto record=value.toObject();const auto id=record["id"].toString();if(id.isEmpty()||ids.contains(id))throw std::runtime_error("duplicate");ids.insert(id);
            decoded.append(unseal(vault,record,*privateKey));
        }
        *entries=decoded;error->clear();return true;
    }catch(...){CryptoTool::wipe(*master);CryptoTool::wipe(*privateKey);*entries={};*error="密码错误或密库损坏，未修改原文件";return false;}
}
QJsonObject VaultCodec::rekey(QJsonObject vault,const QString &password,const QByteArray &privateKey,QByteArray *master){
    if(!ready(vault)||CryptoTool::rsaPublicKey(privateKey)!=QByteArray::fromBase64(vault["publicKey"].toString().toLatin1()))throw std::runtime_error("vault changed");
    const auto salt=CryptoTool::random(16);*master=CryptoTool::derive(password,salt);vault["salt"]=QString::fromLatin1(salt.toBase64());wrapPrivate(vault,privateKey,*master);return vault;
}
