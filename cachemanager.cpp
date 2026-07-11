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
    qInfo() << "[CacheManager] 开始自检与读取。";
    m_scriptEngine = new HelpJsonIOEngine("./cache", "script.json", 1000, this);
    loadFromFile();
    qInfo() << "[CacheManager] 缓存读取完毕。";
}

void CacheManager::loadFromFile(){
    qDebug() << "[CacheManager] 加载缓存：";
    // script部分
    QJsonObject scriptObj = m_scriptEngine->read();
    if (scriptObj.isEmpty()) {
        qWarning() << "[CacheManager] ./script.json JSON 格式损坏或空。";
        m_weatherCaches = QHash<QString, CityWeatherInfo>();
        m_quoteCache = QuoteInfo();
        return;
    }
    QJsonArray weatherArray = scriptObj["weather"].toArray();
    for (int i = 0; i < weatherArray.size(); ++i) {
        QJsonObject cityObj = weatherArray[i].toObject();
        qint64 currentSec = QDateTime::currentSecsSinceEpoch();
        if (currentSec - cityObj.value("timestamp").toVariant().toLongLong(0) > 1200) continue;
        CityWeatherInfo item = CityWeatherInfo(cityObj);
        m_weatherCaches[item.cityCode] = item;
    }
    QJsonObject quoteObject = scriptObj["quote"].toObject();
    m_quoteCache = QuoteInfo(quoteObject);
}

void CacheManager::saveScript(){
    qDebug() << "[CacheManager] 保存缓存：";
    QJsonObject scriptObj;
    QJsonArray weatherArray;
    for (const CityWeatherInfo &info : m_weatherCaches) {
        weatherArray.append(info.toJson());
    }
    scriptObj["weather"] = weatherArray;
    scriptObj["quote"] = m_quoteCache.toJson();
    m_scriptEngine->lazyWrite(scriptObj);

}

CityWeatherInfo CacheManager::getCityWeatherInfo(QString code) const {
    return m_weatherCaches.value(code);
}

void CacheManager::updateWeatherCache(const CityWeatherInfo& newWeatherCache){
    m_weatherCaches[newWeatherCache.cityCode] = newWeatherCache;
    saveScript();
    emit weatherCacheUpdated(newWeatherCache.cityCode);
}

QuoteInfo CacheManager::getQuoteInfo() const {
    return m_quoteCache;
}

void CacheManager::updateQuoteCache(const QuoteInfo& newQuoteCache){
    m_quoteCache = newQuoteCache;
    saveScript();
    emit quoteCacheUpdated(newQuoteCache.content);
}













