#include "homewidget.h"

HomeWidget::HomeWidget(QWidget *parent) : QWidget(parent)
{
    // 创建网格布局
    QGridLayout *mainGrid = new QGridLayout(this);
    mainGrid->setContentsMargins(0, 0, 0, 0);
    mainGrid->setSpacing(0);
    // 实例化背景
    m_background = new HomeBackgroundShader(this);
    mainGrid->addWidget(m_background, 0, 0);
    // 实例化上层
    // 创建一个无外观 Widget
    QWidget *uiLayerWidget = new QWidget(this);
    uiLayerWidget->setStyleSheet("background: transparent;");
    // 在上层创建垂直布局
    QVBoxLayout *uiVerticalLayout = new QVBoxLayout(uiLayerWidget);
    uiVerticalLayout->setContentsMargins(0, 40, 0, 0);
    uiVerticalLayout->setSpacing(0);
    // 实例化三段式信息板
    m_infoWidget = new HomeInfoWidget(this);
    uiVerticalLayout->addWidget(m_infoWidget);
    // 在信息组件下方加入一个弹簧
    QSpacerItem *vSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    uiVerticalLayout->addSpacerItem(vSpacer);
    // 把上层加入第 0 行 0 列！
    mainGrid->addWidget(uiLayerWidget, 0, 0);
    // 服务器们
    // m_weatherService
    m_weatherService = new homeWeatherService(this);

    connect(m_weatherService, &homeWeatherService::updateWeatherRequest, this,
            [=](QString name, QString temp, QString account){
        m_infoWidget->updateWeather(name, temp, account);
    });

    connect(CacheManager::getInstance(), &CacheManager::weatherCacheUpdated, this,
            [=](QString code){
                auto info = CacheManager::getInstance()->getCityWeatherInfo(code);
                m_infoWidget->updateWeather("", info.temperature, info.weather);
            });

    m_weatherService->updateWeather();
}

HomeWidget::~HomeWidget()
{}
