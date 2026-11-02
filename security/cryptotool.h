#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
namespace CryptoTool {
QByteArray random(int bytes);
QByteArray derive(const QString &password, const QByteArray &salt);
QJsonObject encrypt(const QByteArray &plain, const QByteArray &key);
QByteArray decrypt(const QJsonObject &envelope, const QByteArray &key, bool *ok);
QString protect(const QString &secret);
QString unprotect(const QString &encrypted);
void wipe(QByteArray &bytes);
void rsaKeyPair(QByteArray &publicKey,QByteArray &privateKey);
QByteArray rsaPublicKey(const QByteArray &privateKey);
QByteArray rsaEncrypt(const QByteArray &publicKey,const QByteArray &plain,const QByteArray &label);
QByteArray rsaDecrypt(const QByteArray &privateKey,const QByteArray &cipher,const QByteArray &label);
}
