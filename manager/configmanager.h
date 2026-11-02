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



struct MailBasicalConfig{
    int pollingInterval;
    int initialNumber;
    int earlierNumber;
    int storageLimit;

    MailBasicalConfig(){
        pollingInterval = 5;
        initialNumber = 10;
        earlierNumber = 1;
        storageLimit = 100;
    }

    MailBasicalConfig(const QJsonObject &mailBasicalObect){
        pollingInterval = mailBasicalObect["pollingInterval"].toInt(5);
        initialNumber = mailBasicalObect["initialNumber"].toInt(10);
        earlierNumber = mailBasicalObect["earlierNumber"].toInt(1);
        storageLimit = mailBasicalObect["storageLimit"].toInt(100);
    }

    void normalize(){
        if(pollingInterval!=-1)pollingInterval=qBound(1,pollingInterval,1440);
        initialNumber=qBound(1,initialNumber,1000);
        earlierNumber=qBound(1,earlierNumber,100);
        storageLimit=qBound(1,storageLimit,100000);
    }

    QJsonObject toJson(){
        QJsonObject mailBasicalObect;
        mailBasicalObect["pollingInterval"] = pollingInterval;
        mailBasicalObect["initialNumber"] = initialNumber;
        mailBasicalObect["earlierNumber"] = earlierNumber;
        mailBasicalObect["storageLimit"] = storageLimit;
        return mailBasicalObect;
    }

};


struct DeadlineBasicalConfig{
    int initialPastNumber;
    qint64 saveDuration; // 秒级
    qint64 remindTime;
    QList<QString> commonTags;


    DeadlineBasicalConfig(){
        initialPastNumber = 10;
        saveDuration = 2592000;
        remindTime = 172800;
        commonTags = {};
    }

    DeadlineBasicalConfig(const QJsonObject &deadlineBasicalObect){
        initialPastNumber = deadlineBasicalObect["initialPastNumber"].toInt(10);
        saveDuration = deadlineBasicalObect["saveDuration"].toVariant().toLongLong();
        if (saveDuration <= 0) saveDuration = 2592000;
        remindTime = deadlineBasicalObect["remindTime"].toVariant().toLongLong();
        if (remindTime != -1 && remindTime <= 0) remindTime = 172800;
        QJsonArray tagArray = deadlineBasicalObect["commonTags"].toArray();
        for (const auto& tag : tagArray) {
            QString _tag = tag.toString();
            commonTags.append(_tag);
        }
    }

    QJsonObject toJson(){
        QJsonObject deadlineBasicalObect;
        deadlineBasicalObect["initialPastNumber"] = initialPastNumber;
        deadlineBasicalObect["saveDuration"] = QString::number(saveDuration);
        deadlineBasicalObect["remindTime"] = QString::number(remindTime);
        QJsonArray tagArray;
        for (const QString& tag : commonTags) {
            tagArray.append(tag);
        }
        deadlineBasicalObect["commonTags"] = tagArray;
        return deadlineBasicalObect;
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
    // mail
    const MailBasicalConfig& getMailBasical() const;
    void setMailBasical(const MailBasicalConfig& newconfig);
    // deadline
    const DeadlineBasicalConfig& getDeadlineBasical() const;
    void setDeadlineBasical(const DeadlineBasicalConfig& Deadline);
    QStringList commonTags() const { return m_deadlineBasical.commonTags; }
    QStringList noteTags() const { return m_noteTags; }
    void setNoteTags(const QStringList &tags);
    void setCommonTags(const QStringList &tags);


signals:
    void homeWeatherChanged();
    void mailBasicalChanged();
    void deadlineChanged();
    void noteTagsChanged();

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
    MailBasicalConfig m_mailBasical;
    DeadlineBasicalConfig m_deadlineBasical;
    QStringList m_noteTags;

    // 对应的IO引擎
    HelpJsonIOEngine *m_settingEngine=nullptr;



};

#endif // CONFIGMANAGER_H
