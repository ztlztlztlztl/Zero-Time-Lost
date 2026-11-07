#pragma once
#include "cloudprofile.h"
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslCertificate>
#include <QUrl>
namespace ConnectionProfile {
inline QJsonObject read(const QString &path, QString *error) {
    auto fail = [&](const QString &s) {
        *error = s;
        return QJsonObject{};
    };
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 1024 * 1024)
        return fail("连接文件无法读取或过大");
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject())
        return fail("连接文件格式无效");
    const auto p = doc.object();
    if (p["format"] != 1 || p["protocol"] != 1 || p["port"] != 443)
        return fail("连接文件版本、协议或端口不兼容");
    for (auto it = p.begin(); it != p.end(); ++it)
        if (!QStringList{"format", "protocol", "host", "port", "token", "ca"}.contains(it.key()))
            return fail("连接文件包含不支持的字段");
    CloudProfile candidate;candidate.host=p["host"].toString();candidate.port=443;candidate.token=p["token"].toString();candidate.ca=p["ca"].toString().toUtf8();
    if(candidate.ca.size()>128*1024)return fail("连接文件中的 CA 过大");
    candidate.url(error,true);if(!error->isEmpty())return {};
    candidate.request("/health",error);if(!error->isEmpty())return {};
    return p;
}
} // namespace ConnectionProfile
