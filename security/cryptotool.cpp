#include "cryptotool.h"
#include <stdexcept>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#endif
namespace {
#ifdef Q_OS_WIN
void check(NTSTATUS status) { if (status < 0) throw std::runtime_error("Windows cryptography failed"); }
struct Algorithm {
    BCRYPT_ALG_HANDLE h = nullptr;
    Algorithm(LPCWSTR name, ULONG flags = 0) { check(BCryptOpenAlgorithmProvider(&h, name, nullptr, flags)); }
    ~Algorithm() { if(h) BCryptCloseAlgorithmProvider(h,0); }
};
struct Key {
    BCRYPT_KEY_HANDLE h = nullptr;
    ~Key() { if(h) BCryptDestroyKey(h); }
};
#endif
}
void CryptoTool::wipe(QByteArray &b) {
    b.detach();
    volatile char *p = b.data();
    for (qsizetype i=0;i<b.size();++i) p[i]=0;
    b.clear();
}
QByteArray CryptoTool::random(int bytes) {
#ifdef Q_OS_WIN
    QByteArray result(bytes,0);
    check(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(result.data()), bytes, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    return result;
#else
    throw std::runtime_error("Encryption currently requires Windows");
#endif
}
QByteArray CryptoTool::derive(const QString &password, const QByteArray &salt) {
#ifdef Q_OS_WIN
    Algorithm alg(BCRYPT_SHA256_ALGORITHM, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    QByteArray key(32,0), pass=password.toUtf8();
    const auto status=BCryptDeriveKeyPBKDF2(alg.h, reinterpret_cast<PUCHAR>(pass.data()), pass.size(),
        reinterpret_cast<PUCHAR>(const_cast<char*>(salt.constData())), salt.size(), 600000,
        reinterpret_cast<PUCHAR>(key.data()),key.size(),0);
    wipe(pass); check(status); return key;
#else
    throw std::runtime_error("Encryption currently requires Windows");
#endif
}
QJsonObject CryptoTool::encrypt(const QByteArray &plain, const QByteArray &key) {
#ifdef Q_OS_WIN
    if(key.size()!=32) throw std::runtime_error("Invalid encryption key");
    Algorithm alg(BCRYPT_AES_ALGORITHM);
    check(BCryptSetProperty(alg.h, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)), sizeof(BCRYPT_CHAIN_MODE_GCM),0));
    Key handle;
    check(BCryptGenerateSymmetricKey(alg.h,&handle.h,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(key.constData())),key.size(),0));
    auto nonce=random(12); QByteArray tag(16,0), cipher(plain.size(),0);
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info; BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce=reinterpret_cast<PUCHAR>(nonce.data()); info.cbNonce=nonce.size();
    info.pbTag=reinterpret_cast<PUCHAR>(tag.data()); info.cbTag=tag.size();
    ULONG count=0;
    check(BCryptEncrypt(handle.h,reinterpret_cast<PUCHAR>(const_cast<char*>(plain.constData())),plain.size(),&info,nullptr,0,
                        reinterpret_cast<PUCHAR>(cipher.data()),cipher.size(),&count,0));
    return {{"nonce",QString::fromLatin1(nonce.toBase64())},{"tag",QString::fromLatin1(tag.toBase64())},{"cipher",QString::fromLatin1(cipher.toBase64())}};
#else
    throw std::runtime_error("Encryption currently requires Windows");
#endif
}
QByteArray CryptoTool::decrypt(const QJsonObject &obj,const QByteArray &key,bool *ok) {
    *ok=false;
#ifdef Q_OS_WIN
    auto nonce=QByteArray::fromBase64(obj["nonce"].toString().toLatin1()), tag=QByteArray::fromBase64(obj["tag"].toString().toLatin1()),
         cipher=QByteArray::fromBase64(obj["cipher"].toString().toLatin1());
    if(nonce.size()!=12 || tag.size()!=16 || key.size()!=32 || cipher.size()>32*1024*1024) return {};
    Algorithm alg(BCRYPT_AES_ALGORITHM);
    check(BCryptSetProperty(alg.h,BCRYPT_CHAINING_MODE,reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),sizeof(BCRYPT_CHAIN_MODE_GCM),0));
    Key handle;
    check(BCryptGenerateSymmetricKey(alg.h,&handle.h,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(key.constData())),key.size(),0));
    QByteArray plain(cipher.size(),0); ULONG count=0;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info; BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce=reinterpret_cast<PUCHAR>(nonce.data()); info.cbNonce=nonce.size(); info.pbTag=reinterpret_cast<PUCHAR>(tag.data()); info.cbTag=tag.size();
    const auto status=BCryptDecrypt(handle.h,reinterpret_cast<PUCHAR>(cipher.data()),cipher.size(),&info,nullptr,0,
                                   reinterpret_cast<PUCHAR>(plain.data()),plain.size(),&count,0);
    if(status<0) { wipe(plain); return {}; }
    *ok=true; return plain;
#else
    return {};
#endif
}
QString CryptoTool::protect(const QString &secret) {
    if(secret.isEmpty()) return {};
#ifdef Q_OS_WIN
    auto bytes=secret.toUtf8(); DATA_BLOB in{DWORD(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},out{};
    if(!CryptProtectData(&in,L"ZTL account",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out)) { wipe(bytes); throw std::runtime_error("Credential protection failed"); }
    QByteArray result(reinterpret_cast<char*>(out.pbData),out.cbData);
    SecureZeroMemory(out.pbData,out.cbData); LocalFree(out.pbData); wipe(bytes);
    return QString::fromLatin1(result.toBase64());
#else
    throw std::runtime_error("Credential protection requires Windows");
#endif
}
QString CryptoTool::unprotect(const QString &encrypted) {
    if(encrypted.isEmpty()) return {};
#ifdef Q_OS_WIN
    auto bytes=QByteArray::fromBase64(encrypted.toLatin1()); DATA_BLOB in{DWORD(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},out{};
    if(!CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out)) return {};
    QString result=QString::fromUtf8(reinterpret_cast<char*>(out.pbData),out.cbData);
    SecureZeroMemory(out.pbData,out.cbData); LocalFree(out.pbData); return result;
#else
    return {};
#endif
}

#ifdef Q_OS_WIN
namespace {
QByteArray exportRsa(BCRYPT_KEY_HANDLE key,LPCWSTR type){
    ULONG size=0;check(BCryptExportKey(key,nullptr,type,nullptr,0,&size,0));
    QByteArray result(size,0);check(BCryptExportKey(key,nullptr,type,reinterpret_cast<PUCHAR>(result.data()),result.size(),&size,0));return result;
}
void importRsa(Algorithm &algorithm,Key &key,const QByteArray &blob,LPCWSTR type){
    if(blob.isEmpty()||blob.size()>16384)throw std::runtime_error("Invalid RSA key");
    check(BCryptImportKeyPair(algorithm.h,nullptr,type,&key.h,reinterpret_cast<PUCHAR>(const_cast<char*>(blob.constData())),blob.size(),0));
}
}
#endif
void CryptoTool::rsaKeyPair(QByteArray &publicKey,QByteArray &privateKey){
#ifdef Q_OS_WIN
    Algorithm alg(BCRYPT_RSA_ALGORITHM);Key key;check(BCryptGenerateKeyPair(alg.h,&key.h,3072,0));check(BCryptFinalizeKeyPair(key.h,0));
    publicKey=exportRsa(key.h,BCRYPT_RSAPUBLIC_BLOB);privateKey=exportRsa(key.h,BCRYPT_RSAFULLPRIVATE_BLOB);
#else
    throw std::runtime_error("RSA requires Windows");
#endif
}
QByteArray CryptoTool::rsaPublicKey(const QByteArray &privateKey){
#ifdef Q_OS_WIN
    Algorithm alg(BCRYPT_RSA_ALGORITHM);Key key;importRsa(alg,key,privateKey,BCRYPT_RSAFULLPRIVATE_BLOB);return exportRsa(key.h,BCRYPT_RSAPUBLIC_BLOB);
#else
    throw std::runtime_error("RSA requires Windows");
#endif
}
QByteArray CryptoTool::rsaEncrypt(const QByteArray &publicKey,const QByteArray &plain,const QByteArray &label){
#ifdef Q_OS_WIN
    Algorithm alg(BCRYPT_RSA_ALGORITHM);Key key;importRsa(alg,key,publicKey,BCRYPT_RSAPUBLIC_BLOB);
    BCRYPT_OAEP_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM,reinterpret_cast<PUCHAR>(const_cast<char*>(label.constData())),ULONG(label.size())};ULONG size=0;
    auto input=reinterpret_cast<PUCHAR>(const_cast<char*>(plain.constData()));
    check(BCryptEncrypt(key.h,input,plain.size(),&padding,nullptr,0,nullptr,0,&size,BCRYPT_PAD_OAEP));QByteArray result(size,0);
    check(BCryptEncrypt(key.h,input,plain.size(),&padding,nullptr,0,reinterpret_cast<PUCHAR>(result.data()),result.size(),&size,BCRYPT_PAD_OAEP));return result;
#else
    throw std::runtime_error("RSA requires Windows");
#endif
}
QByteArray CryptoTool::rsaDecrypt(const QByteArray &privateKey,const QByteArray &cipher,const QByteArray &label){
#ifdef Q_OS_WIN
    Algorithm alg(BCRYPT_RSA_ALGORITHM);Key key;importRsa(alg,key,privateKey,BCRYPT_RSAFULLPRIVATE_BLOB);
    BCRYPT_OAEP_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM,reinterpret_cast<PUCHAR>(const_cast<char*>(label.constData())),ULONG(label.size())};ULONG size=0;
    auto input=reinterpret_cast<PUCHAR>(const_cast<char*>(cipher.constData()));
    check(BCryptDecrypt(key.h,input,cipher.size(),&padding,nullptr,0,nullptr,0,&size,BCRYPT_PAD_OAEP));QByteArray result(size,0);
    const auto status=BCryptDecrypt(key.h,input,cipher.size(),&padding,nullptr,0,reinterpret_cast<PUCHAR>(result.data()),result.size(),&size,BCRYPT_PAD_OAEP);
    if(status<0){wipe(result);check(status);}result.resize(size);return result;
#else
    throw std::runtime_error("RSA requires Windows");
#endif
}
