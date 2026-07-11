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
    qDebug() << "[CacheManager] 构造函数被调用。";
}





CacheManager::~CacheManager()
{
    qDebug() << "[CacheManager] 析构函数被调用。";
}

void CacheManager::initialize()
{
    qInfo() << "[CacheManager] 开始自检与构建。";
    checkAndCreateDirectories();
    qInfo() << "[CacheManager] 自检完毕，读取缓存。";
    loadFromFile();
    qInfo() << "[CacheManager] 缓存读取完毕。";
}

void CacheManager::saveCache(){
    qDebug() << "[CacheManager] 触发保存。";
    saveToFile();
}

void CacheManager::checkAndCreateDirectories(){
    QString exeDirPath = QCoreApplication::applicationDirPath();
    QString cacheDirPath = exeDirPath + "/cache";
    m_cacheDirPath = cacheDirPath;

    // 检查文件夹是否存在
    qInfo() << "[CacheManager] 自检缓存目录。";
    QDir dir(cacheDirPath);
    if (!dir.exists()) {
        qDebug() << "[CacheManager] 缓存目录不存在，尝试创建：" << cacheDirPath;
        if (dir.mkpath(cacheDirPath)) {
            qDebug() << "[CacheManager] 缓存目录创建成功。";
        } else {
            qWarning() << "[CacheManager] 缓存目录创建失败！";
            return;
        }
    }
    else{
        qDebug() << "[CacheManager] 缓存目录" << cacheDirPath << "已存在，无需重复创建。";
    }

    // 检查文件是否存在
    qInfo() << "[CacheManager] 自检缓存文件。";
    QStringList jsons = {"/weather.json", "/quote.json"};
    QList<QString> inits = {"[]", "{}"};
    for (int i = 0; i < jsons.length(); i++) {
        QString filePath = cacheDirPath + jsons[i];
        QFile file(filePath);
        if (!file.exists()) {
            qDebug() << "[CacheManager] 缓存文件文件不存在，尝试创建：" << jsons[i];
            if (file.open(QIODevice::WriteOnly)) {
                qDebug() << "[CacheManager] 缓存文件创建成功！" << jsons[i];
                file.write(inits[i].toUtf8());
                file.close();
            } else {
                qWarning() << "[CacheManager] 缓存文件创建失败！" << jsons[i];
            }
        } else {
            qDebug() << "[CacheManager] 缓存文件" << jsons[i] << "已存在，无需重复创建。";
        }
    }
}

void CacheManager::loadFromFile(){
    qDebug() << "[CacheManager] 加载缓存：" << m_cacheDirPath;

    // weather部分
    QFile fileWeather(m_cacheDirPath + "/weather.json");
    if (!fileWeather.open(QIODevice::ReadOnly)) {
        qWarning() << "[CacheManager] 无法打开文件进行读取：/weather.json";
        return;
    }
    // 防文件损坏
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(fileWeather.readAll(), &error);
    fileWeather.close();
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[CacheManager] JSON 格式损坏：" << error.errorString();
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
    // quote 部分
    QFile fileQuote(m_cacheDirPath + "/quote.json");
    if (!fileQuote.open(QIODevice::ReadOnly)) {
        qWarning() << "[CacheManager] 无法打开文件进行读取：/quote.json";
        return;
    }
    QJsonDocument docQuote = QJsonDocument::fromJson(fileQuote.readAll(), &error);
    fileQuote.close();
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[CacheManager] JSON 格式损坏：" << error.errorString();
        // 信息留空
        return;
    }
    QJsonObject quoteObject = docQuote.object();
    quoteObject = quoteObject["quote"].toObject();
    m_quoteCache.content = quoteObject["content"].toString("愿你千帆阅尽，归来仍是少年");
    m_quoteCache.timestamp = quoteObject["timestamp"].toVariant().toLongLong(0);
}

void CacheManager::saveToFile(){
    qDebug() << "[CacheManager] 保存缓存：" << m_cacheDirPath;
    // weather部分
    QJsonArray weatherArray;
    for (const CityWeatherInfo &info : m_weatherCaches) {
        weatherArray.append(info.toJson());
    }
    // 打包保存
    QJsonDocument doc(weatherArray);
    QFile fileWeather(m_cacheDirPath + "/weather.json");
    if (fileWeather.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // 只读与清空
        fileWeather.write(doc.toJson()); // tojson()自带换行
        fileWeather.close();
    } else {
        qWarning() << "[CacheManager] /weather.json 保存失败！";
    }
    // quote 部分
    QJsonObject quoteObject;
    quoteObject["quote"] = m_quoteCache.toJson();
    QJsonDocument docquote(quoteObject);
    QFile fileQuote(m_cacheDirPath + "/quote.json");
    if (fileQuote.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // 只读与清空
        fileQuote.write(docquote.toJson()); // tojson()自带换行
        fileQuote.close();
    } else {
        qWarning() << "[CacheManager] /quote.json 保存失败！";
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

QuoteInfo CacheManager::getQuoteInfo() const {
    return m_quoteCache;
}

void CacheManager::updateQuoteCache(const QuoteInfo& newQuoteCache){
    m_quoteCache = newQuoteCache;
    saveToFile();
    emit quoteCacheUpdated(newQuoteCache.content);
}













