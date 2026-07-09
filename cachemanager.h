#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QList>
#include <QHash>


struct CityWeatherInfo {
    QString cityCode; // 城市编码
    qint64 timestamp; // 天气信息的秒级时间戳
    QString weather; // 天气现象 (如 "晴")
    QString temperature; // 温度

    CityWeatherInfo(){
        cityCode = "";
        timestamp = 0;
        weather = "";
        temperature = "";
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

class CacheManager : public QObject
{
    Q_OBJECT
public:
    // 单例模式
    static CacheManager* getInstance();
    void initialize();
    // 保存函数
    void saveCache();
    // 获取信息
    // weather
    CityWeatherInfo getCityWeatherInfo(QString code) const;
    void updateWeatherCache(const CityWeatherInfo& newWeatherCache);

signals:
    void weatherCacheUpdated(QString code);

private slots:




private:
    explicit CacheManager(QObject *parent = nullptr);
    ~CacheManager();
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;

    // 文件 I/O 与自愈层
    QString m_cacheDirPath;
    void checkAndCreateDirectories(); // 检查 config 文件夹
    void loadFromFile();
    void saveToFile();

    // 配置储存
    QHash<QString, CityWeatherInfo> m_weatherCaches;



};

#endif // CACHEMANAGER_H
