#include "settinghomepage.h"

SettingHomePage::SettingHomePage(QWidget *parent)
    : QWidget{parent}
{
    // 信息初始化
    m_locationConfig = ConfigManager::getInstance()->getHomeweather();
    // ui初始化
    // 滚动区
    QVBoxLayout *generalLayout = new QVBoxLayout(this);
    generalLayout->setContentsMargins(0, 0, 0, 0);
    m_mainScroll = new QScrollArea(this);
    generalLayout->addWidget(m_mainScroll);
    QWidget *scrollContent = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(scrollContent);
    // 标题
    m_lblTitle = new QLabel("首页", this);
    contentLayout->addWidget(m_lblTitle);
    // 当前城市
    QHBoxLayout *currentCityLayout = new QHBoxLayout();
    m_lblLocationTest = new QLabel("当前城市：", this);
    m_lblLocationCity = new QLabel("--", this);
    currentCityLayout->addWidget(m_lblLocationTest);
    currentCityLayout->addWidget(m_lblLocationCity);
    contentLayout->addLayout(currentCityLayout);
    // 收藏城市列表
    QHBoxLayout *collectedLayout = new QHBoxLayout();
    m_lblCollectedTest = new QLabel("收藏的城市：");
    collectedLayout->addWidget(m_lblCollectedTest);
    QVBoxLayout *rightLayout = new QVBoxLayout();
    // 四个按钮
    QHBoxLayout *btnsLayout = new QHBoxLayout();
    m_upBtn = new QPushButton("up", this);
    m_downBtn = new QPushButton("down", this);
    m_selectBtn = new QPushButton("select", this);
    m_deleteBtn = new QPushButton("delete", this);
    btnsLayout->addWidget(m_upBtn);
    btnsLayout->addWidget(m_downBtn);
    btnsLayout->addWidget(m_selectBtn);
    btnsLayout->addWidget(m_deleteBtn);
    rightLayout->addLayout(btnsLayout);
    // 列表区
    QWidget *collectedContent = new QWidget(); // 创建内容载体
    m_contentLayout = new QVBoxLayout(collectedContent);
    m_contentLayout->setAlignment(Qt::AlignTop);
    // 组装
    rightLayout->addWidget(collectedContent);
    collectedLayout->addLayout(rightLayout);
    contentLayout->addLayout(collectedLayout);
    m_clickedIndex = -1;
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
    contentLayout->addLayout(addLayout);
    // 收尾
    m_mainScroll->setWidget(scrollContent);
    m_mainScroll->setWidgetResizable(true);

    // 刷新UI
    resetUI();

    // 链接
    // up
    connect(m_upBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedIndex == 0 || m_clickedIndex == -1){
            qDebug() << "无法上行";
            return;
        }
        int rawIndex = m_clickedIndex;
        int tarIndex = m_clickedIndex - 1;
        m_clickedIndex -= 1;
        auto tmp = m_locationConfig.savedCities[rawIndex];
        m_locationConfig.savedCities[rawIndex] = m_locationConfig.savedCities[tarIndex];
        m_locationConfig.savedCities[tarIndex] = tmp;
        if (m_locationConfig.currentCityIndex == rawIndex) m_locationConfig.currentCityIndex = tarIndex;
        else if (m_locationConfig.currentCityIndex == tarIndex) m_locationConfig.currentCityIndex = rawIndex;
        ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    });
    // down
    connect(m_downBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedIndex == m_locationConfig.savedCities.length() - 1 || m_clickedIndex == -1){
            qDebug() << "无法下行";
            return;
        }
        int rawIndex = m_clickedIndex;
        int tarIndex = m_clickedIndex + 1;
        m_clickedIndex += 1;
        auto tmp = m_locationConfig.savedCities[rawIndex];
        m_locationConfig.savedCities[rawIndex] = m_locationConfig.savedCities[tarIndex];
        m_locationConfig.savedCities[tarIndex] = tmp;
        if (m_locationConfig.currentCityIndex == rawIndex) m_locationConfig.currentCityIndex = tarIndex;
        else if (m_locationConfig.currentCityIndex == tarIndex) m_locationConfig.currentCityIndex = rawIndex;
        ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    });
    // select
    connect(m_selectBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedIndex == -1){
            qDebug() << "无法选择";
            return;
        }
        else if(m_clickedIndex == m_locationConfig.currentCityIndex){
            qDebug() << "无需改变";
            return;
        }
        m_locationConfig.currentCityIndex = m_clickedIndex;
        ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    });
    // select
    connect(m_deleteBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedIndex == -1){
            qDebug() << "无法删除";
            return;
        }
        if (m_clickedIndex < m_locationConfig.currentCityIndex){
            m_locationConfig.currentCityIndex -= 1;
        }
        else if(m_clickedIndex == m_locationConfig.currentCityIndex){
            m_locationConfig.currentCityIndex = -1;
        }
        m_locationConfig.savedCities.remove(m_clickedIndex);
        m_clickedIndex = -1;
        ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    });
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
    if (m_locationConfig.savedCities.isEmpty()) return;
    // 对齐当前城市
    if (m_locationConfig.currentCityIndex != -1){
        m_lblLocationCity->setText(m_locationConfig.savedCities[m_locationConfig.currentCityIndex].name);
    }
    else{
        m_lblLocationCity->setText("无");
    }
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
    // 删除旧按钮组
    if (QButtonGroup *oldGroup = this->findChild<QButtonGroup*>("cityGroup")) {
        delete oldGroup;
    }
    QButtonGroup *cityBtnGroup = new QButtonGroup(this);
    cityBtnGroup->setObjectName("cityGroup");
    cityBtnGroup->setExclusive(true);
    for (int i = 0; i < m_locationConfig.savedCities.length(); i++) {
        const weatherCity &city = m_locationConfig.savedCities[i];
        QString text = QString("%1(%2)").arg(city.name).arg(city.code);
        QPushButton *cityBtn = new QPushButton(text);
        cityBtn->setCheckable(true);
        if (m_clickedIndex == -1 && i == m_locationConfig.currentCityIndex) {
            cityBtn->setChecked(true);
            m_clickedIndex = i;
        }
        else if(i == m_clickedIndex){
            cityBtn->setChecked(true);
        }
        m_contentLayout->addWidget(cityBtn);
        cityBtnGroup->addButton(cityBtn, i);
    }
    connect(cityBtnGroup, &QButtonGroup::idClicked, this, [=](int id){
        qDebug() << "用户切换了当前城市，索引为：" << id;
        m_clickedIndex = id;
    });
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

