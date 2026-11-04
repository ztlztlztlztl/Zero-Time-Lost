#include "configmanager.h"
#include "tagrules.h"

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
    delete m_settingEngine;
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
        m_mailBasical = MailBasicalConfig();
        m_deadlineBasical = DeadlineBasicalConfig();m_noteTags.clear();
        return;
    }
    QJsonObject homeObj = settingObj["home"].toObject();
    QJsonObject weatherObj = homeObj["weather"].toObject();
    m_homeWeather = HomeWeatherConfig(weatherObj);
    if(m_homeWeather.currentCityIndex<0||m_homeWeather.currentCityIndex>=m_homeWeather.savedCities.size())m_homeWeather.currentCityIndex=-1;
    QJsonObject mailObj = settingObj["mail"].toObject();
    m_mailBasical = MailBasicalConfig(mailObj);
    m_mailBasical.normalize();
    QJsonObject deadlineObj = settingObj["deadline"].toObject();
    m_deadlineBasical = DeadlineBasicalConfig(deadlineObj);
    m_noteTags.clear();for(auto v:settingObj["noteTags"].toArray())m_noteTags.append(v.toString());

}

void ConfigManager::saveSetting(){
    qDebug() << "[ConfigManager] 保存 Setting 配置。";
    QJsonObject settingObj;
    // home
    QJsonObject homeObj;
    homeObj["weather"] = m_homeWeather.toJson(); // weather
    settingObj["home"] = homeObj; // home
    settingObj["mail"] = m_mailBasical.toJson(); // mail
    settingObj["deadline"] = m_deadlineBasical.toJson();
    QJsonArray noteTags;for(const auto &tag:m_noteTags)noteTags.append(tag);settingObj["noteTags"]=noteTags;
    // 打包保存
    m_settingEngine->lazyWrite(settingObj);
}

// weather
const HomeWeatherConfig& ConfigManager::getHomeweather() const {
    return m_homeWeather;
}

void ConfigManager::setHomeweather(const HomeWeatherConfig& newconfig){
    m_homeWeather = newconfig;
    if(m_homeWeather.currentCityIndex<0||m_homeWeather.currentCityIndex>=m_homeWeather.savedCities.size())m_homeWeather.currentCityIndex=-1;
    saveSetting();
    emit homeWeatherChanged();
}

// mail
const MailBasicalConfig& ConfigManager::getMailBasical() const{
    return m_mailBasical;

}
void ConfigManager::setMailBasical(const MailBasicalConfig& newconfig){
    m_mailBasical = newconfig;
    m_mailBasical.normalize();
    saveSetting();
    emit mailBasicalChanged();
}

// deadline
const DeadlineBasicalConfig& ConfigManager::getDeadlineBasical() const{
    return m_deadlineBasical;

}
void ConfigManager::setDeadlineBasical(const DeadlineBasicalConfig& newconfig){
    m_deadlineBasical = newconfig;
    saveSetting();
    emit deadlineChanged();
}

void ConfigManager::setCommonTags(const QStringList &tags){
    auto config=m_deadlineBasical;config.commonTags=TagRules::normalize(tags);setDeadlineBasical(config);
}
void ConfigManager::setNoteTags(const QStringList &tags){
    m_noteTags=TagRules::normalize(tags);saveSetting();emit noteTagsChanged();
}
