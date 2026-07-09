#include "homeweatherservice.h"

homeWeatherService::homeWeatherService(QObject *parent)
    : QObject{parent}
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &homeWeatherService::updateWeather);
    m_timer->start(ZTL::Home::Service::timeInterval);

    connect(ConfigManager::getInstance(), &ConfigManager::homeWeatherChanged, this, [=](){
        updateWeather();
    });

}


void homeWeatherService::updateWeather(){
    int index = ConfigManager::getInstance()->getHomeweather().currentCityIndex;
    if (index == -1){
        emit updateWeatherRequest("无", "-273.0", "--");
        return;
    }
    QString code = ConfigManager::getInstance()->getHomeweather().savedCities[index].code;
    CityWeatherInfo info = CacheManager::getInstance()->getCityWeatherInfo(code);
    qint64 currentSec = QDateTime::currentSecsSinceEpoch();
    if (currentSec - info.timestamp > 1195) {
        if (!useWeatherAPI(code)){
            emit updateWeatherRequest("无", "-273.0", "--");
            return;
        }
    }
    info = CacheManager::getInstance()->getCityWeatherInfo(code);
    emit updateWeatherRequest(
        ConfigManager::getInstance()->getHomeweather().savedCities[index].name
        , info.temperature, info.weather);
    return;





}



bool homeWeatherService::useWeatherAPI(QString code){
    // 准备访问条件
    QDir scriptdir("./scripts");
    if (!scriptdir.exists()) {
        qDebug() << "script文件夹不存在";
        return false;
    }
    QFile scriptfile("./scripts/getWeather.py");
    if (!scriptfile.exists()) {
        qDebug() << "script文件不存在";
        return false;
    }
    qDebug() << "都存在";
    // 启动进程
    QProcess *process = new QProcess(this);
    QString program = "python";
    QStringList arguments;
    arguments << "-u";
    arguments << "./scripts/getWeather.py";
    arguments << "--code" << code;
    arguments << "--key" << ZTL::Home::Service::gaodeKey;
    // 链接信号
    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray output = process->readAllStandardOutput();
        QString jsonStr = QString::fromLocal8Bit(output);
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError) {
            qWarning() << "JSON 解析失败，错误原因：" << error.errorString();
            return;
        }
        QJsonObject jsonObj = doc.object();
        QString status = jsonObj.value("status").toString();
        if (status == "0") {
            qWarning() << "返回状态为错误";
            return;
        }
        QJsonObject rawObj = jsonObj["lives"].toArray()[0].toObject();
        CityWeatherInfo info;
        info.cityCode = code;
        info.weather = rawObj["weather"].toString();
        info.temperature = rawObj["temperature_float"].toString();
        info.timestamp = QDateTime::currentSecsSinceEpoch();
        CacheManager::getInstance()->updateWeatherCache(info);
    });
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        qDebug() << "脚本错误：" << QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
    });
    connect(process, &QProcess::finished, process, &QProcess::deleteLater);
    process->start(program, arguments);
    return true;
}