#include "settinghomepage.h"

SettingHomePage::SettingHomePage(QWidget *parent)
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
    m_contentLayout = new QVBoxLayout(collectedContent);
    m_contentLayout->setAlignment(Qt::AlignTop);// 组装
    m_collectedScroll->setWidget(collectedContent);
    collectedLayout->addWidget(m_collectedScroll);
    mainLayout->addLayout(collectedLayout);
    // 添加城市
    QHBoxLayout *addLayout = new QHBoxLayout();
    m_lblAddTest = new QLabel("添加新城市：", this);
    m_addCityInput = new QLineEdit(this);
    m_addCityInput->setPlaceholderText("城市名");
    m_addCodeInput = new QLineEdit(this);
    m_addCodeInput->setPlaceholderText("城市编码");
    QRegularExpression re("\\d{6}$");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(re, this);
    m_addCodeInput->setValidator(validator);
    m_addSubmit = new QPushButton("提交", this);
    addLayout->addWidget(m_lblAddTest);
    addLayout->addWidget(m_addCityInput);
    addLayout->addWidget(m_addCodeInput);
    addLayout->addWidget(m_addSubmit);
    mainLayout->addLayout(addLayout);

    // 刷新UI
    resetUI();

    // 链接
    connect(m_addSubmit, &QPushButton::clicked, this, [=](){
        qDebug() << "submit被点击";
        addCity();
    });
    connect(this, &SettingHomePage::weatherSaveRequest, this, [=](){
        qDebug() << "保存weather配置";
        ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    });
    connect(ConfigManager::getInstance(), &ConfigManager::homeWeatherChanged, this, [=](){
        qDebug() << "weather配置改变";
        resetUI();
    });
}

void SettingHomePage::resetUI(){
    qDebug() << "刷新home页的UI";
    m_lblLocationCity->setText(m_locationConfig.savedCities[m_locationConfig.currentCityIndex].name);
    // 刷新城市列表
    if (m_contentLayout) {
        while (m_contentLayout->count() > 0) {
            QLayoutItem *item = m_contentLayout->takeAt(0);
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
    }
    for (int i = 0;i < m_locationConfig.savedCities.length(); i++) {
        weatherCity &city = m_locationConfig.savedCities[i];
        QHBoxLayout *cityLayout = new QHBoxLayout();
        QLabel *cityName = new QLabel(city.name);
        cityName->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        QLabel *cityCode = new QLabel(city.code);
        cityCode->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QPushButton *upBtn = new QPushButton("up");
        upBtn->setProperty("rowIndex", i);
        QPushButton *downBtn = new QPushButton("down");
        QPushButton *selectBtn = new QPushButton("select");
        QPushButton *deleteBtn = new QPushButton("delete");
        cityLayout->addWidget(cityName);
        cityLayout->addWidget(cityCode);
        cityLayout->addWidget(upBtn);
        cityLayout->addWidget(downBtn);
        cityLayout->addWidget(selectBtn);
        cityLayout->addWidget(deleteBtn);
        m_contentLayout->addLayout(cityLayout);
    }
}

bool SettingHomePage::addCity(){
    QString iname = m_addCityInput->text().trimmed();
    QString icode = m_addCodeInput->text().trimmed();
    if (iname.isEmpty() || icode.isEmpty() || icode.length() < 6){
        return false;
    }
    for (const weatherCity &city : m_locationConfig.savedCities) {
        if (city.code == icode || city.name == iname) return false;
    }
    m_locationConfig.savedCities.append({iname, icode});
    ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    m_addCityInput->clear();
    m_addCodeInput->clear();
    return true;
}

