#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "datatype.h"
#include "helpjsonioengine.h"

#include <QObject>
#include <QString>
#include <QVariant>
#include <QList>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

struct HomeWeatherConfig{
    int currentCityIndex;
    QList<weatherCity> savedCities;

    HomeWeatherConfig(){
        currentCityIndex = 0;
        savedCities = {{"北京海淀", "110108"}};
    }

    HomeWeatherConfig(const QJsonObject &homeWeatherObect){
        currentCityIndex = homeWeatherObect["currentCityIndex"].toInt(0);
        QJsonArray cityArray = homeWeatherObect["savedCities"].toArray();
        for (const auto& city : cityArray) {
            QJsonObject _city = city.toObject();
            weatherCity __city =
                {_city["name"].toString("北京海淀"), _city["code"].toString("110108")};
            savedCities.append(__city);
        }
    }

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

    HomeQuoteConfig(){
        currentThemeIndex = 0;
        savedThemes = {"无"};
    }

    HomeQuoteConfig(const QJsonObject &homeWeatherObect){
        currentThemeIndex = homeWeatherObect["currentThemeIndex"].toInt(0);
        QJsonArray themeArray = homeWeatherObect["savedThemes"].toArray();
        for (const auto& theme : themeArray) {
            QString _theme = theme.toString();
            savedThemes.append(_theme);
        }
    }

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
    void loadFromFile();

    void saveSetting();



    // 配置储存
    HomeWeatherConfig m_homeWeather;
    HomeQuoteConfig m_homeQuote;

    // 对应的IO引擎
    HelpJsonIOEngine *m_homeSettingEngine;




};

#endif // CONFIGMANAGER_H
