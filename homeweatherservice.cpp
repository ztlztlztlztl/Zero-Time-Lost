#include "homeweatherservice.h"

HomeWeatherService::HomeWeatherService(QObject *parent)
    : QObject{parent}
{
    qInfo() << "[HomeWeatherService] 启动。";
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &HomeWeatherService::updateWeather);
    m_timer->start(ZTL::Home::Service::timeInterval);

    connect(ConfigManager::getInstance(), &ConfigManager::homeWeatherChanged, this, [=](){
        updateWeather();
    });

}


void HomeWeatherService::updateWeather(){
    qInfo() << "[HomeWeatherService] 请求更新天气信息。";
    int index = ConfigManager::getInstance()->getHomeweather().currentCityIndex;
    if (index == -1){
        emit updateWeatherRequest("无", "-273.0", "--");
        return;
    }
    QString code = ConfigManager::getInstance()->getHomeweather().savedCities[index].code;
    CityWeatherInfo info = CacheManager::getInstance()->getCityWeatherInfo(code);
    qint64 currentSec = QDateTime::currentSecsSinceEpoch();
    if (currentSec - info.timestamp > 1195) {
        qInfo() << "[HomeWeatherService] 天气信息需更新。";
        if (!useWeatherAPI(code)){
            emit updateWeatherRequest("无", "-273.0", "--");
            return;
        }
    }
    else{
        qInfo() << "[HomeWeatherService] 天气信息不需更新";
        QString name = ConfigManager::getInstance()->getHomeweather().savedCities[
        ConfigManager::getInstance()->getHomeweather().currentCityIndex].name;
        emit updateWeatherRequest(name, info.temperature, info.weather);
    }
    return;





}



bool HomeWeatherService::useWeatherAPI(QString code){
    qInfo() << "[HomeWeatherService] 请求调用天气脚本。";
    QDir scriptdir("./scripts");
    if (!scriptdir.exists()) {
        qWarning() << "[HomeWeatherService] script文件夹不存在。";
        return false;
    }
    QFile scriptfile("./scripts/getWeather.py");
    if (!scriptfile.exists()) {
        qWarning() << "[HomeWeatherService] getWeather.py文件不存在。";
        return false;
    }
    qDebug() << "[HomeWeatherService] getWeather.py存在。";
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
            qWarning() << "[HomeWeatherService] JSON 解析失败，错误原因：" << error.errorString();
            return;
        }
        QJsonObject jsonObj = doc.object();
        QString status = jsonObj.value("status").toString();
        if (status == "0") {
            qWarning() << "[HomeWeatherService] 返回状态为错误。";
            return;
        }
        QJsonArray livesArray = jsonObj["lives"].toArray();
        if (livesArray.isEmpty()) {
            qWarning() << "[HomeWeatherService] 高德API未返回天气数据！";
            return;
        }
        QJsonObject rawObj = livesArray[0].toObject();
        CityWeatherInfo info;
        info.cityCode = code;
        info.weather = rawObj["weather"].toString();
        info.temperature = rawObj["temperature_float"].toString();
        info.timestamp = QDateTime::currentSecsSinceEpoch();
        CacheManager::getInstance()->updateWeatherCache(info);
    });
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        qInfo() << "[HomeWeatherService] 脚本错误：" << QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
    });
    connect(process, &QProcess::finished, process, [=](){
        process->deleteLater();
        qInfo() << "[HomeWeatherService] 天气脚本调用完毕。";
    });
    process->start(program, arguments);
    return true;
}