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
    qDebug() << "开始加载全局配置...";
    checkAndCreateDirectories();
}


void ConfigManager::checkAndCreateDirectories(){
    QString exeDirPath = QCoreApplication::applicationDirPath();
    QString configDirPath = exeDirPath + "/config";

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
    QStringList jsons = {"/setting.json"};
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









