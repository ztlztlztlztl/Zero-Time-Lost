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

struct HomeWeatherConfig{
    QString currentCityCode;
    QList<weatherCity> savedCities;
};




class ConfigManager : public QObject
{
    Q_OBJECT
public:
    // 单例模式
    static ConfigManager* getInstance();
    void initialize();

signals:


private:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 文件 I/O 与自愈层
    void checkAndCreateDirectories(); // 检查 config 文件夹
    void loadFromFile();
    void saveToFile();



};

#endif // CONFIGMANAGER_H
