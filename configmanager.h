#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "datatype.h"

#include <QObject>
#include <QString>
#include <QVariant>
#include <QList>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

struct HomeWeatherConfig{
    int currentCityIndex;
    QList<weatherCity> savedCities;

    QJsonObject toJson(){
        QJsonObject homeWeatherObect;
        homeWeatherObect["currentCityIndex"] = QString::number(currentCityIndex);
        QJsonArray cityArray;
        for (const auto& city : savedCities) {
            QJsonObject cityObj;
            cityObj["name"] = city.name;
            cityObj["code"] = city.code;
            cityArray.append(cityObj);
        }
        homeWeatherObect["savedCities"] = cityArray;
        return homeWeatherObect;
    }
};



class ConfigManager : public QObject
{
    Q_OBJECT
public:
    // 单例模式
    static ConfigManager* getInstance();
    void initialize();
    // 保存函数
    void saveConfig();
    // 获取信息
    // weather
    const HomeWeatherConfig& getHomeweather() const;
    void setHomeweather(const HomeWeatherConfig& newconfig);

signals:
    void homeWeatherChanged();

private slots:




private:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 文件 I/O 与自愈层
    QString m_configDirPath;
    void checkAndCreateDirectories(); // 检查 config 文件夹
    void loadFromFile();
    void saveToFile();

    // 配置储存
    HomeWeatherConfig m_homeWeather;



};

#endif // CONFIGMANAGER_H
