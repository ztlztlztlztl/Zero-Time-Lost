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
    qDebug() << "ConfigManager 构造函数被调用";
}





ConfigManager::~ConfigManager()
{
    qDebug() << "ConfigManager 析构函数被调用";
}

void ConfigManager::initialize()
{
    qDebug() << "自检路径";
    checkAndCreateDirectories();
    qDebug() << "读取配置";
    loadFromFile();
}


void ConfigManager::saveConfig(){
    qDebug() << "触发保存";
    saveToFile();
}

void ConfigManager::checkAndCreateDirectories(){
    QString exeDirPath = QCoreApplication::applicationDirPath();
    QString configDirPath = exeDirPath + "/config";
    m_configDirPath = configDirPath;

    // 检查文件夹是否存在
    QDir dir(configDirPath);
    if (!dir.exists()) {
        qDebug() << "检测到配置目录不存在，正在尝试创建：" << configDirPath;
        if (dir.mkpath(configDirPath)) {
            qDebug() << "配置目录创建成功！";
        } else {
            qWarning() << "警告：配置目录创建失败！";
            return;
        }
    }
    else{
        qDebug() << "配置路径" << configDirPath << "已存在，无需重复创建。";
    }

    // 检查文件是否存在
    QStringList jsons = {"/settings.json"};
    for (int i = 0; i < jsons.length(); i++) {
        QString filePath = configDirPath + jsons[i];
        QFile file(filePath);
        if (!file.exists()) {
            qDebug() << "检测到配置文件不存在，正在尝试创建：" << filePath;
            if (file.open(QIODevice::WriteOnly)) {
                qDebug() << "配置文件创建成功！" << jsons[i];
                file.write("{}"); // 写一个json的大括号
                file.close();
            } else {
                qWarning() << "警告：配置文件创建失败！" << jsons[i];
            }
        } else {
            qDebug() << "配置文件" << jsons[i] << "已存在，无需重复创建。";
        }
    }
}

void ConfigManager::loadFromFile(){
    qDebug() << "load 配置 from" << m_configDirPath;

    // setting部分
    QFile file(m_configDirPath + "/settings.json");
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
        m_homeWeather.currentCityCode = "110108";
        m_homeWeather.savedCities = {weatherCity{"北京海淀", "110108"}};
        return;
    }

    QJsonObject rootObj = doc.object();
    QJsonObject homeObj = rootObj["home"].toObject();
    QJsonObject weatherObj = homeObj["weather"].toObject();
    // toString 默认值
    m_homeWeather.currentCityCode = weatherObj.value("currentCityCode").toString("110108");
    m_homeWeather.savedCities.clear();
    QJsonArray cityArray = weatherObj.value("savedCities").toArray();
    for (int i = 0; i < cityArray.size(); ++i) {
        QJsonObject cityObj = cityArray[i].toObject();
        weatherCity item;
        item.name = cityObj.value("name").toString("未知城市");
        item.code = cityObj.value("code").toString("000000");
        m_homeWeather.savedCities.append(item);
    }
}

void ConfigManager::saveToFile(){
    qDebug() << "save 配置 to" << m_configDirPath;
    // setting部分
    // 总{}
    QJsonObject settingObj;
    // home{}
    QJsonObject homeObj;
    homeObj["weather"] = m_homeWeather.toJson(); // weather
    settingObj["home"] = homeObj;
    // 打包保存
    QJsonDocument doc(settingObj);
    QFile file(m_configDirPath + "/settings.json");
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // 只读与清空
        file.write(doc.toJson()); // tojson()自带换行
        file.close();
    } else {
        qDebug() << "保存失败！";
    }
}

const HomeWeatherConfig& ConfigManager::getHomeweather() const {
    return m_homeWeather;
}

void ConfigManager::setHomeweather(const HomeWeatherConfig& newconfig){
    m_homeWeather = newconfig;
    saveToFile();
    emit homeWeatherChanged();
}


