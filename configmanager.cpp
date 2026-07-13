#include "configmanager.h"

ConfigManager* ConfigManager::getInstance()
{
    // 单例的标准写法
    static ConfigManager instance;
    return &instance; // 返回它的指针
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(nullptr) // 单例的父指针为 nullptr
{
    qDebug() << "[ConfigManager] 构造函数被调用。";
}

ConfigManager::~ConfigManager()
{
    qDebug() << "[ConfigManager] 析构函数被调用。";
}

void ConfigManager::initialize()
{
    qInfo() << "[ConfigManager] 开始自检与读取。";
    m_settingEngine = new HelpJsonIOEngine("./config", "settings.json", 1000, this);
    loadFromFile();
    qInfo() << "[ConfigManager] 配置读取完毕。";
}

void ConfigManager::loadFromFile(){
    qDebug() << "[ConfigManager] 加载配置：./config";
    // setting部分
    QJsonObject settingObj = m_settingEngine->read();
    if (settingObj.isEmpty()) {
        qWarning() << "[ConfigManager] ./settings.json JSON 格式损坏或空。";
        m_homeWeather = HomeWeatherConfig();
        m_homeQuote = HomeQuoteConfig();
        m_mailBasical = MailBasicalConfig();
        return;
    }
    QJsonObject homeObj = settingObj["home"].toObject();
    QJsonObject weatherObj = homeObj["weather"].toObject();
    m_homeWeather = HomeWeatherConfig(weatherObj);
    QJsonObject quoteObj = homeObj["quote"].toObject();
    m_homeQuote = HomeQuoteConfig(quoteObj);
    QJsonObject mailObj = settingObj["mail"].toObject();
    m_mailBasical = MailBasicalConfig(mailObj);

}

void ConfigManager::saveSetting(){
    qDebug() << "[ConfigManager] 保存 Setting 配置。";
    QJsonObject settingObj;
    // home
    QJsonObject homeObj;
    homeObj["weather"] = m_homeWeather.toJson(); // weather
    homeObj["quote"] = m_homeQuote.toJson(); // quote
    settingObj["home"] = homeObj;
    settingObj["mail"] = m_mailBasical.toJson(); // mail
    // 打包保存
    m_settingEngine->lazyWrite(settingObj);
}

// weather
const HomeWeatherConfig& ConfigManager::getHomeweather() const {
    return m_homeWeather;
}

void ConfigManager::setHomeweather(const HomeWeatherConfig& newconfig){
    m_homeWeather = newconfig;
    saveSetting();
    emit homeWeatherChanged();
}

// quote
const HomeQuoteConfig& ConfigManager::getHomeQuote() const{
    return m_homeQuote;
}

void ConfigManager::setHomeQuote(const HomeQuoteConfig& newconfig){
    m_homeQuote = newconfig;
    saveSetting();
    emit homeQuoteChanged();
}

// mail
const MailBasicalConfig& ConfigManager::getMailBasical() const{
    return m_mailBasical;

}
void ConfigManager::setMailBasical(const MailBasicalConfig& newconfig){
    m_mailBasical = newconfig;
    saveSetting();
    emit mailBasicalChanged();
}









