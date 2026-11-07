#pragma once
#include "cryptotool.h"
#include "paths.h"
#include <QSaveFile>
#include <QSettings>
#include <QtNetwork>
struct CloudProfile {
    QString host, token;
    QByteArray ca;
    int port = 443;
    bool enabled = true;
    static CloudProfile current() {
        QSettings s;
        CloudProfile p;
        p.host = s.value("cloud/host").toString();
        p.port = s.value("cloud/port", 443).toInt();
        p.token = CryptoTool::unprotect(s.value("cloud/token").toString());
        p.enabled = s.value("cloud/enabled", false).toBool();
        const auto path = s.value("cloud/ca").toString();
        if (!path.isEmpty()) {
            QFile f(QFileInfo(path).isRelative() ? Paths::data(path) : path);
            if (f.open(QIODevice::ReadOnly))
                p.ca = f.readAll();
            else
                p.ca = "invalid certificate";
        }
        return p;
    }
    QUrl url(QString *error, bool fixedPort = false) const {
        error->clear();
        auto raw = host.trimmed();
        QHostAddress address;
        if (address.setAddress(raw) && address.protocol() == QAbstractSocket::IPv6Protocol)
            raw = "[" + raw + "]";
        QUrl result(raw.contains("://") ? raw : "https://" + raw);
        if (!result.isValid() || result.scheme() != "https" || result.host().isEmpty() ||
            !result.userInfo().isEmpty() || result.port() != -1 || result.hasQuery() ||
            result.hasFragment() || (!result.path().isEmpty() && result.path() != "/") || port < 1 ||
            port > 65535 || fixedPort && port != 443) {
            *error = "请填写服务器 IP 或域名；HTTPS 端口固定为 443，地址不能包含路径或端口";
            return {};
        }
        result.setPort(port);
        return result;
    }
    QNetworkRequest request(const QString &path, QString *error, int timeout = 180000) const {
        auto base = url(error);
        if (base.isEmpty())
            return {};
        if (token.trimmed().isEmpty() || token.size() > 1024) {
            *error = "请填写有效访问令牌";
            return {};
        }
        const auto certs = QSslCertificate::fromData(ca);
        if (!ca.isEmpty() && certs.isEmpty()) {
            *error = "云端证书无效或不存在";
            return {};
        }
        QUrl relative(path);
        base.setPath(relative.path());
        base.setQuery(relative.query());
        QNetworkRequest req(base);
        req.setTransferTimeout(timeout);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Authorization", "Bearer " + token.trimmed().toUtf8());
        if (!ca.isEmpty()) {
            auto ssl = req.sslConfiguration();
            auto trusted = ssl.caCertificates();
            trusted.append(certs);
            ssl.setCaCertificates(trusted);
            req.setSslConfiguration(ssl);
        }
        return req;
    }
    bool apply(QString *error) const {
        const auto base = url(error, true);
        if (base.isEmpty())
            return false;
        request("/health", error);
        if (!error->isEmpty())
            return false;
        QString protectedToken;
        try {
            QSettings s;
            protectedToken = s.value("cloud/token").toString();
            if (CryptoTool::unprotect(protectedToken) != token.trimmed())
                protectedToken = CryptoTool::protect(token.trimmed());
        } catch (...) {
            try {
                protectedToken = CryptoTool::protect(token.trimmed());
            } catch (...) {
                *error = "令牌保护失败";
                return false;
            }
        }
        if (protectedToken.isEmpty()) {
            *error = "令牌保护失败";
            return false;
        }
        QString caPath;
        if (!ca.isEmpty()) {
            caPath = "cloud/ca-" +
                     QString::fromLatin1(QCryptographicHash::hash(ca, QCryptographicHash::Sha256).toHex()) +
                     ".crt";
            QDir().mkpath(Paths::data("cloud"));
            QSaveFile f(Paths::data(caPath));
            if (!f.open(QIODevice::WriteOnly) || f.write(ca) != ca.size() || !f.commit()) {
                *error = "证书保存失败";
                return false;
            }
        }
        QSettings settings;
        QMap<QString, QVariant> previous;
        for (auto name : {"host", "port", "token", "ca", "enabled"})
            previous[name] = settings.value("cloud/" + QString(name));
        settings.setValue("cloud/host", base.host());
        settings.setValue("cloud/port", 443);
        settings.setValue("cloud/token", protectedToken);
        settings.setValue("cloud/ca", caPath);
        settings.setValue("cloud/enabled", enabled);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            for (auto it = previous.begin(); it != previous.end(); ++it)
                settings.setValue("cloud/" + it.key(), it.value());
            *error = "连接设置保存失败，原配置已保留";
            return false;
        }
        return true;
    }
    static QString healthText(const QJsonObject &health) {
        QStringList failures;
        const auto jobs = health["jobs"].toObject();
        for (auto it = jobs.begin(); it != jobs.end(); ++it)
            if (!it.value().toObject()["error"].toString().isEmpty())
                failures << it.key() + "：" + it.value().toObject()["error"].toString();
        return failures.isEmpty() ? "HTTPS 与令牌验证成功，后台任务无已知错误"
                                  : "HTTPS 与令牌验证成功，部分后台任务异常：\n" + failures.join("\n");
    }
};
