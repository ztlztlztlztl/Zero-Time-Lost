#include "homeweatherservice.h"
#include "configmanager.h"
#include "cachemanager.h"
#include <QNetworkReply>
#include <QUrlQuery>
#include <QSettings>
#include "cloudconnection.h"
#include <QPointer>
HomeWeatherService::HomeWeatherService(QObject *parent,const QUrl &url):QObject(parent),network(this),endpoint(url){
    connect(ConfigManager::getInstance(),&ConfigManager::homeWeatherChanged,this,[this]{updateWeather(true);});
    connect(CloudConnection::instance(),&CloudConnection::settingsChanged,this,[this]{++cloudGeneration;updateWeather(true);});
}
void HomeWeatherService::updateWeather(bool force){
    if(force&&pending){pending->disconnect(this);pending->abort();pending->deleteLater();pending.clear();active.clear();}
    const auto config=ConfigManager::getInstance()->getHomeweather();
    if(config.currentCityIndex<0||config.currentCityIndex>=config.savedCities.size()){emit updateWeatherRequest("无","--","未选择城市");return;}
    const auto city=config.savedCities[config.currentCityIndex];
    const auto cached=CacheManager::getInstance()->getCityWeatherInfo(city.code);
    if(!force&&QDateTime::currentSecsSinceEpoch()-cached.timestamp<1200){emit updateWeatherRequest(city.name,cached.temperature,cached.weather);return;}
    if(CloudConnection::enabled()){
        if(pending){pending->disconnect(this);pending->abort();pending->deleteLater();pending.clear();active.clear();}
        const auto generation=++cloudGeneration;QPointer<HomeWeatherService> guard(this);
        CloudConnection::instance()->request("/v1/weather/"+city.code+(force?"?refresh=true":""),{},[this,guard,city,generation](const QJsonObject &r,const QString &error){
            if(!guard||generation!=cloudGeneration)return;
            const auto config=ConfigManager::getInstance()->getHomeweather();
            if(config.currentCityIndex<0||config.currentCityIndex>=config.savedCities.size()||config.savedCities[config.currentCityIndex].code!=city.code)return;
            const auto failure=!error.isEmpty()?error:r["error"].toString();
            if(r["temperature"].isString()&&r["weather"].isString()){
                CityWeatherInfo info;info.cityCode=city.code;info.temperature=r["temperature"].toString();
                info.weather=r["weather"].toString()+" · "+QDateTime::fromSecsSinceEpoch(r["updated"].toInteger()).toString("MM-dd HH:mm");
                if(!failure.isEmpty())info.weather+="（缓存；"+failure+"）";
                info.timestamp=r["updated"].toInteger();CacheManager::getInstance()->updateWeatherCache(info);emit updateWeatherRequest(city.name,info.temperature,info.weather);
            }else{
                const auto old=CacheManager::getInstance()->getCityWeatherInfo(city.code);
                emit updateWeatherRequest(city.name,old.timestamp>0?old.temperature:"--",failure.isEmpty()?"云端暂无天气数据":failure);
            }
        });return;
    }
    const auto key=QSettings().value("weatherKey").toString().trimmed();
    if(key.isEmpty()){emit updateWeatherRequest(city.name,"--","未设置天气 Key，请在 Home 设置填写 Web 服务 Key");return;}
    if(pending){if(active==city.code)return;pending->disconnect(this);pending->abort();pending->deleteLater();pending.clear();}
    active=city.code;
    QUrl url(endpoint);QUrlQuery query;
    query.addQueryItem("city",city.code);query.addQueryItem("key",key);query.addQueryItem("extensions","base");url.setQuery(query);
    QNetworkRequest req(url);req.setTransferTimeout(15000);
    auto reply=network.get(req);pending=reply;
    connect(reply,&QNetworkReply::finished,this,[this,reply,city,key]{
        reply->deleteLater();if(pending==reply){pending.clear();active.clear();}
        const auto config=ConfigManager::getInstance()->getHomeweather();
        const bool current=config.currentCityIndex>=0&&config.currentCityIndex<config.savedCities.size()&&config.savedCities[config.currentCityIndex].code==city.code&&QSettings().value("weatherKey").toString().trimmed()==key;
        QJsonParseError parseError;const auto doc=QJsonDocument::fromJson(reply->readAll(),&parseError);const auto response=doc.object();
        const auto providerCode=response["infocode"].toString();
        auto fail=[&](const QString &message){
            // Do not log URLs, request bodies or keys.
            qWarning()<<"[Weather] request failed; network="<<int(reply->error())<<"http="<<reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()<<"provider="<<providerCode.toInt();
            if(current)emit updateWeatherRequest(city.name,"--",message);
        };
        if(reply->error()!=QNetworkReply::NoError){
            switch(reply->error()){
            case QNetworkReply::HostNotFoundError:fail("天气域名解析失败，请检查网络/DNS");break;
            case QNetworkReply::TimeoutError:case QNetworkReply::OperationCanceledError:fail("天气请求超时，请稍后重试");break;
            case QNetworkReply::SslHandshakeFailedError:fail("天气 TLS 连接失败，请检查系统时间/证书");break;
            default:fail("天气连接失败，请检查网络/代理");break;
            }
            return;
        }
        if(parseError.error!=QJsonParseError::NoError||!doc.isObject()){fail("天气响应格式异常，请稍后重试");return;}
        if(response["status"].toString()!="1"){
            if(providerCode=="10009")fail("Key 平台不匹配，请使用 Web 服务 Key");
            else if(providerCode=="10001")fail("天气 Key 无效，请检查 Home 设置");
            else if(providerCode=="10002")fail("天气服务未开通，请检查 Key 权限");
            else if(providerCode=="10003"||providerCode=="10004"||providerCode=="10010")fail("天气调用额度/频率受限，请稍后重试");
            else if(providerCode=="10005"||providerCode=="10006")fail("天气 Key 来源受限，请检查 IP/域名白名单");
            else fail("天气服务拒绝请求，请检查 Key 权限");
            return;
        }
        const auto rows=response["lives"].toArray();
        if(rows.isEmpty()){fail("暂无天气数据，请检查城市编码");return;}
        const auto row=rows[0].toObject();CityWeatherInfo info;info.cityCode=city.code;info.temperature=row["temperature"].toString();info.weather=row["weather"].toString();info.timestamp=QDateTime::currentSecsSinceEpoch();
        if(info.temperature.isEmpty()||info.weather.isEmpty()){fail("天气数据不完整，请稍后重试");return;}
        CacheManager::getInstance()->updateWeatherCache(info);if(current)emit updateWeatherRequest(city.name,info.temperature,info.weather);
    });
}
