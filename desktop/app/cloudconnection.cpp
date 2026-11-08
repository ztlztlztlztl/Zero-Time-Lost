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
#include <QHostAddress>

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
bool CloudConnection::enabled(){return !QCoreApplication::instance()->property("ztlSetupActive").toBool()&&QSettings().value("cloud/enabled",false).toBool();}
QString CloudConnection::identity(){
    QSettings s;return s.value("cloud/host").toString()+":"+s.value("cloud/port",443).toString()+":"+s.value("cloud/token").toString()+":"+s.value("cloud/ca").toString()+":"+s.value("cloud/enabled").toString();
}
void CloudConnection::request(const QString &path,const QJsonObject &body,Callback callback,bool post){
    QString error;QNetworkRequest req;try{req=CloudProfile::current().request(path,&error);}catch(...){error="无法读取云端凭据";}
    if(!error.isEmpty()){QMetaObject::invokeMethod(this,[callback,error]{callback({},error);},Qt::QueuedConnection);return;}
    const auto expected=identity();const auto expectedGeneration=generation;
    auto reply=post?network->post(req,QJsonDocument(body).toJson(QJsonDocument::Compact)):network->get(req);
    activeReplies.insert(reply);
    connect(reply,&QNetworkReply::finished,this,[this,reply,callback,expected,expectedGeneration]{
        activeReplies.remove(reply);
        reply->deleteLater();
        if(expected!=identity()||expectedGeneration!=generation){callback({},"云端配置已改变，本次响应未应用");return;}
        const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->readAll();
        if(reply->error()!=QNetworkReply::NoError||http!=200){
            QString message;
            if(http==401)message="云端令牌无效，请检查设置";
            else if(http==409)message="云端数据版本或游标异常，请核查服务器备份恢复情况";
            else if(http==422){
                const auto details=QJsonDocument::fromJson(bytes).object()["detail"].toArray();
                const auto reason=details.isEmpty()?QString():details.first().toObject()["msg"].toString().left(240);
                message="云端数据校验失败（HTTP 422）"+(reason.isEmpty()?QString():"："+reason)+"；本地修改已保留";
            }
            else if(reply->error()==QNetworkReply::SslHandshakeFailedError)message="云端 HTTPS 验证失败，请检查 IP、系统时间及证书文件";
            else message="云端连接失败（HTTP "+QString::number(http)+"），本地修改已保留";
            callback({},message);return;
        }
        QJsonParseError error;auto doc=QJsonDocument::fromJson(bytes,&error);
        if(error.error!=QJsonParseError::NoError||!doc.isObject()){callback({},"云端响应格式异常");return;}
        callback(doc.object(),{});
    });
}

void CloudConnection::testProfile(const CloudProfile &profile,Callback callback){
    QString error;auto request=profile.request("/health",&error,20000);
    if(!error.isEmpty()){QMetaObject::invokeMethod(this,[callback,error]{callback({},error);},Qt::QueuedConnection);return;}
    auto tester=new QNetworkAccessManager(this);auto reply=tester->get(request);
    connect(reply,&QNetworkReply::finished,this,[tester,reply,callback]{
        const auto doc=QJsonDocument::fromJson(reply->readAll());const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString error;if(reply->error()!=QNetworkReply::NoError||http!=200)error=http==401?"访问令牌无效":"HTTPS 连接验证失败，请检查地址、证书及 443 放行状态";
        else if(!doc.isObject()||doc.object()["protocol"]!=1)error="服务器协议不兼容";
        callback(doc.object(),error);reply->deleteLater();tester->deleteLater();
    });
}
