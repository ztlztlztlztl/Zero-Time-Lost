#include "settingweatherpage.h"

SettingWeatherPage::SettingWeatherPage(QWidget *parent)
    : QWidget{parent}
{
    // 信息初始化
    m_locationConfig = ConfigManager::getInstance()->getHomeweather();
    // ui初始化
    // 标题
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    m_lblTitle = new QLabel("首页", this);
    mainLayout->addWidget(m_lblTitle);
    // 当前城市
    QHBoxLayout *currentCityLayout = new QHBoxLayout();
    m_lblLocationTest = new QLabel("当前城市：", this);
    m_lblLocationCity = new QLabel("--", this);
    currentCityLayout->addWidget(m_lblLocationTest);
    currentCityLayout->addWidget(m_lblLocationCity);
    mainLayout->addLayout(currentCityLayout);
    // 收藏城市列表
    QHBoxLayout *collectedLayout = new QHBoxLayout();
    m_lblCollectedTest = new QLabel("收藏的城市：");
    collectedLayout->addWidget(m_lblCollectedTest);
    m_collectedScroll = new QScrollArea(this); // 创建一个滚动区域
    m_collectedScroll->setWidgetResizable(true);
    QWidget *collectedContent = new QWidget(); // 创建内容载体
    QVBoxLayout *contentLayout = new QVBoxLayout(collectedContent);
    contentLayout->setAlignment(Qt::AlignTop);
    //
    for (weatherCity city: m_locationConfig.savedCities){
        QLabel *test = new QLabel(QString("%1  %2").arg(city.name).arg(city.code), this);
        contentLayout->addWidget(test);
    }
    // 组装
    m_collectedScroll->setWidget(collectedContent);
    collectedLayout->addWidget(m_collectedScroll);
    mainLayout->addLayout(collectedLayout);
    // 信息对齐
    m_lblLocationCity->setText(m_locationConfig.savedCities[m_locationConfig.currentCityIndex].name);

}
