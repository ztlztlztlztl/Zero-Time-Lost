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
        homeWeatherObect["currentCityIndex"] = currentCityIndex;
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

struct HomeQuoteConfig{
    int currentThemeIndex;
    QList<QString> savedThemes;

    QJsonObject toJson(){
        QJsonObject homeQuoteObect;
        homeQuoteObect["currentThemeIndex"] = currentThemeIndex;
        QJsonArray ThemeArray;
        for (const QString& theme : savedThemes) {
            ThemeArray.append(theme);
        }
        homeQuoteObect["savedThemes"] = ThemeArray;
        return homeQuoteObect;
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
    // quote
    const HomeQuoteConfig& getHomeQuote() const;
    void setHomeQuote(const HomeQuoteConfig& newconfig);


signals:
    void homeWeatherChanged();
    void homeQuoteChanged();

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
    HomeQuoteConfig m_homeQuote;




};

#endif // CONFIGMANAGER_H
