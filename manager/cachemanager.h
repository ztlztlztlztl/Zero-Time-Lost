#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include "helpjsonioengine.h"

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QHash>


struct CityWeatherInfo {
    QString cityCode; // 城市编码
    qint64 timestamp; // 天气信息的秒级时间戳
    QString weather; // 天气现象 (如 "晴")
    QString temperature; // 温度

    CityWeatherInfo(){
        cityCode = "000000";
        timestamp = 0;
        weather = "无·";
        temperature = "-273.0";
    }

    CityWeatherInfo(const QJsonObject &weatherObect){
        cityCode = weatherObect["cityCode"].toString();
        timestamp = weatherObect["timestamp"].toVariant().toLongLong();
        weather = weatherObect["weather"].toString();
        temperature = weatherObect["temperature"].toString();
    }

    QJsonObject toJson() const{
        QJsonObject weatherObect;
        weatherObect["cityCode"] = cityCode;
        weatherObect["timestamp"] = QString::number(timestamp);
        weatherObect["weather"] = weather;
        weatherObect["temperature"] = temperature;
        return weatherObect;
    }
};

struct QuoteInfo {
    QString content; // 句子
    qint64 timestamp; // 获取的时间戳

    QuoteInfo(){
        content = "愿你千帆阅尽，归来仍是少年。";
        timestamp = 0;
    }

    QuoteInfo(const QJsonObject &quoteObect) {
        content = quoteObect["content"].toString();
        timestamp = quoteObect["timestamp"].toVariant().toLongLong();
    }

    QJsonObject toJson() const{
        QJsonObject quoteObect;
        quoteObect["content"] = content;
        quoteObect["timestamp"] = QString::number(timestamp);
        return quoteObect;
    }
};

class CacheManager : public QObject
{
    Q_OBJECT
public:
    // 单例模式
    static CacheManager* getInstance();
    void initialize();
    // 获取信息
    // weather
    CityWeatherInfo getCityWeatherInfo(QString code) const;
    void updateWeatherCache(const CityWeatherInfo& newWeatherCache);
    // weather
    QuoteInfo getQuoteInfo() const;
    void updateQuoteCache(const QuoteInfo& newQuoteCache);


signals:
    void weatherCacheUpdated(QString code);
    void quoteCacheUpdated(QString content);

private slots:




private:
    explicit CacheManager(QObject *parent = nullptr);
    ~CacheManager();
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;

    // 文件 I/O 与自愈层
    void loadFromFile();

    void saveScript();

    // 配置储存
    QHash<QString, CityWeatherInfo> m_weatherCaches;
    QuoteInfo m_quoteCache;

    HelpJsonIOEngine *m_scriptEngine;




};

#endif // CACHEMANAGER_H
