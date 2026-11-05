#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>
class QNetworkReply;
class HomeWeatherService:public QObject {
    Q_OBJECT
public:
    explicit HomeWeatherService(QObject *parent=nullptr,const QUrl &endpoint=QUrl("https://restapi.amap.com/v3/weather/weatherInfo"));
    void updateWeather(bool force=false);
signals:
    void updateWeatherRequest(QString name,QString temp,QString weather);
private:
    QNetworkAccessManager network;
    QUrl endpoint;
    QPointer<QNetworkReply> pending;
    QString active;
    quint64 cloudGeneration=0;
};
