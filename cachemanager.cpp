#include "cachemanager.h"

CacheManager* CacheManager::getInstance()
{
    // 单例的标准写法
    static CacheManager instance;
    return &instance; // 返回它的指针
}

CacheManager::CacheManager(QObject *parent)
    : QObject(nullptr) // 单例的父指针为 nullptr
{
    qDebug() << "CacheManager 构造函数被调用";
}





CacheManager::~CacheManager()
{
    qDebug() << "CacheManager 析构函数被调用";
}

void CacheManager::initialize()
{
    qDebug() << "自检路径";
    checkAndCreateDirectories();
    qDebug() << "读取配置";
    loadFromFile();
}

void CacheManager::saveCache(){
    qDebug() << "触发保存";
    saveToFile();
}

void CacheManager::checkAndCreateDirectories(){
    QString exeDirPath = QCoreApplication::applicationDirPath();
    QString cacheDirPath = exeDirPath + "/cache";
    m_cacheDirPath = cacheDirPath;

    // 检查文件夹是否存在
    QDir dir(cacheDirPath);
    if (!dir.exists()) {
        qDebug() << "检测到配置目录不存在，正在尝试创建：" << cacheDirPath;
        if (dir.mkpath(cacheDirPath)) {
            qDebug() << "配置目录创建成功！";
        } else {
            qWarning() << "警告：配置目录创建失败！";
            return;
        }
    }
    else{
        qDebug() << "配置路径" << cacheDirPath << "已存在，无需重复创建。";
    }

    // 检查文件是否存在

    QStringList jsons = {"/weather.json"};
    for (int i = 0; i < jsons.length(); i++) {
        QString filePath = cacheDirPath + jsons[i];
        QFile file(filePath);
        if (!file.exists()) {
            qDebug() << "检测到配置文件不存在，正在尝试创建：" << filePath;
            if (file.open(QIODevice::WriteOnly)) {
                qDebug() << "配置文件创建成功！" << jsons[i];
                file.write("[]");
                file.close();
            } else {
                qWarning() << "警告：配置文件创建失败！" << jsons[i];
            }
        } else {
            qDebug() << "配置文件" << jsons[i] << "已存在，无需重复创建。";
        }
    }
}

void CacheManager::loadFromFile(){
    qDebug() << "load 缓存 from" << m_cacheDirPath;

    // weather部分
    QFile file(m_cacheDirPath + "/weather.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件进行读取！";
        return;
    }
    // 防文件损坏
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "JSON 格式损坏：" << error.errorString();
        // 信息留空
        return;
    }

    QJsonArray weatherArray = doc.array();
    for (int i = 0; i < weatherArray.size(); ++i) {
        QJsonObject cityObj = weatherArray[i].toObject();
        qint64 currentSec = QDateTime::currentSecsSinceEpoch();
        if (currentSec - cityObj.value("timestamp").toVariant().toLongLong(0) > 1200) continue;
        CityWeatherInfo item;
        item.cityCode = cityObj.value("cityCode").toString("000000");
        item.timestamp = cityObj.value("timestamp").toVariant().toLongLong(0);
        item.weather = cityObj.value("weather").toString("无");
        item.temperature = cityObj.value("temperature").toString("-273.0");
        m_weatherCaches[item.cityCode] = item;
    }
}

void CacheManager::saveToFile(){
    qDebug() << "save 缓存 to" << m_cacheDirPath;
    // weather部分
    QJsonArray weatherArray;
    for (const CityWeatherInfo &info : m_weatherCaches) {
        weatherArray.append(info.toJson());
    }
    // 打包保存
    QJsonDocument doc(weatherArray);
    QFile file(m_cacheDirPath + "/weather.json");
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // 只读与清空
        file.write(doc.toJson()); // tojson()自带换行
        file.close();
    } else {
        qDebug() << "保存失败！";
    }
}

CityWeatherInfo CacheManager::getCityWeatherInfo(QString code) const {
    return m_weatherCaches.value(code);
}

void CacheManager::updateWeatherCache(const CityWeatherInfo& newWeatherCache){
    m_weatherCaches[newWeatherCache.cityCode] = newWeatherCache;
    saveToFile();
    emit weatherCacheUpdated(newWeatherCache.cityCode);
}















