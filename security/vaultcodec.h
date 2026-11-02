#pragma once
#include <QJsonArray>
#include <QJsonObject>
namespace VaultCodec {
bool ready(const QJsonObject &vault);
QJsonObject create(const QString &password,const QJsonArray &entries,QByteArray *master,QByteArray *privateKey);
QJsonObject seal(const QJsonObject &vault,const QJsonObject &entry);
QJsonObject unseal(const QJsonObject &vault,const QJsonObject &record,const QByteArray &privateKey);
bool open(const QJsonObject &vault,const QString &password,QJsonArray *entries,QByteArray *master,QByteArray *privateKey,QString *error);
QJsonObject rekey(QJsonObject vault,const QString &password,const QByteArray &privateKey,QByteArray *master);
}
