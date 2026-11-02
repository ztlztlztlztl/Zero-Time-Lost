#pragma once
#include "homeweatherservice.h"
class HomeDepartment:public QObject {
    Q_OBJECT
public:
    explicit HomeDepartment(QObject *parent=nullptr):QObject(parent),weather(this){
        connect(&weather,&HomeWeatherService::updateWeatherRequest,this,&HomeDepartment::weatherUpdated);
    }
    void refresh(bool force=false){weather.updateWeather(force);}
signals:
    void weatherUpdated(QString,QString,QString);
private:
    HomeWeatherService weather;
};
