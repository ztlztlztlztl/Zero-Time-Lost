#include "cloudconnection.h"
#include "cryptotool.h"
#include "paths.h"
#include <QFileInfo>
#include <QCoreApplication>
#include <QSettings>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QFile>
#include <QCryptographicHash>

CloudConnection *CloudConnection::instance(){static CloudConnection object;return &object;}
void CloudConnection::notifySettingsChanged(){
    ++generation;
    // Qt's pooled HTTP connection retains its original TLS trust configuration.
    // Swap managers before aborting old requests so callbacks cannot reuse that pool.
    auto previous=network;network=new QNetworkAccessManager(this);
    const auto pending=activeReplies;
    for(auto reply:pending)reply->abort();
    previous->deleteLater();emit settingsChanged();
}
bool CloudConnection::enabled(){return QSettings().value("cloud/enabled",false).toBool();}
QString CloudConnection::identity(){
    QSettings s;return s.value("cloud/host").toString()+":"+s.value("cloud/port",443).toString()+":"+s.value("cloud/token").toString()+":"+s.value("cloud/ca").toString()+":"+s.value("cloud/enabled").toString();
}
void CloudConnection::request(const QString &path,const QJsonObject &body,Callback callback,bool post){
    QSettings s;QString host=s.value("cloud/host").toString().trimmed();
    if(!host.contains("://"))host="https://"+host;
    QUrl base(host);
    auto fail=[callback](const QString &message){QMetaObject::invokeMethod(instance(),[callback,message]{callback({},message);},Qt::QueuedConnection);};
    if(!base.isValid()||base.scheme()!="https"||base.host().isEmpty()||!base.userInfo().isEmpty()||base.hasQuery()||base.hasFragment()||(base.path()!=""&&base.path()!="/")){
        fail("请填写 HTTPS 服务器 IP 或域名，端口在独立输入框填写");return;
    }
    base.setPort(s.value("cloud/port",443).toInt());
    const QUrl relative(path);base.setPath(relative.path());base.setQuery(relative.query());
    const auto token=CryptoTool::unprotect(s.value("cloud/token").toString());
    if(token.isEmpty()){fail("请先填写云端访问令牌");return;}
    QNetworkRequest req(base);req.setTransferTimeout(180000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");req.setRawHeader("Authorization","Bearer "+token.toUtf8());
    const auto ca=s.value("cloud/ca").toString();
    if(!ca.isEmpty()){
        const auto certs=QSslCertificate::fromPath(QFileInfo(ca).isRelative()?Paths::data(ca):ca);
        if(certs.isEmpty()){fail("云端证书文件无效或不存在，请重新选择");return;}
        auto ssl=req.sslConfiguration();auto trusted=ssl.caCertificates();trusted.append(certs);ssl.setCaCertificates(trusted);req.setSslConfiguration(ssl);
    }
    const auto expected=identity();const auto expectedGeneration=generation;
    auto reply=post?network->post(req,QJsonDocument(body).toJson(QJsonDocument::Compact)):network->get(req);
    activeReplies.insert(reply);
    connect(reply,&QNetworkReply::finished,this,[this,reply,callback,expected,expectedGeneration]{
        activeReplies.remove(reply);
        reply->deleteLater();
        if(expected!=identity()||expectedGeneration!=generation){callback({},"云端配置已改变，本次响应未应用");return;}
        const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error()!=QNetworkReply::NoError||http!=200){
            QString message;
            if(http==401)message="云端令牌无效，请检查设置";
            else if(http==409)message="云端数据版本或游标异常，请核查服务器备份恢复情况";
            else if(reply->error()==QNetworkReply::SslHandshakeFailedError)message="云端 HTTPS 验证失败，请检查 IP、系统时间及证书文件";
            else message="云端连接失败（HTTP "+QString::number(http)+"），本地修改已保留";
            callback({},message);return;
        }
        QJsonParseError error;auto doc=QJsonDocument::fromJson(reply->readAll(),&error);
        if(error.error!=QJsonParseError::NoError||!doc.isObject()){callback({},"云端响应格式异常");return;}
        callback(doc.object(),{});
    });
}
