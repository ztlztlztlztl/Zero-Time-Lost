#include "settinghomepage.h"

SettingHomePage::SettingHomePage(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[SettingHomePage] 启动。";
    // 信息初始化
    m_locationConfig = ConfigManager::getInstance()->getHomeweather();
    m_themeConfig = ConfigManager::getInstance()->getHomeQuote();
    // ui初始化
    // 滚动区
    QVBoxLayout *generalLayout = new QVBoxLayout(this);
    generalLayout->setContentsMargins(0, 0, 0, 0);
    m_mainScroll = new QScrollArea(this);
    m_mainScroll->setStyleSheet(ZTL::Setting::homepage::scrollBarStyle);
    generalLayout->addWidget(m_mainScroll);
    QWidget *scrollContent = new QWidget();
    scrollContent->setObjectName("homeBackground");
    scrollContent->setStyleSheet(ZTL::Setting::homepage::backgroundStyle);
    QVBoxLayout *contentLayout = new QVBoxLayout(scrollContent);
    // 标题
    m_lblTitle = new QLabel("--首页--", this);
    m_lblTitle->setStyleSheet(ZTL::Setting::homepage::firstTitleStyle);
    contentLayout->addWidget(m_lblTitle);
    // 天气部分
    // 标题
    m_lblWeatherTitle = new QLabel("-天气-", this);
    m_lblWeatherTitle->setStyleSheet(ZTL::Setting::homepage::secondTitleStyle);
    contentLayout->addWidget(m_lblWeatherTitle);
    // 当前城市
    QHBoxLayout *currentCityLayout = new QHBoxLayout();
    m_lblLocationTest = new QLabel("当前城市", this);
    m_lblLocationTest->setMaximumWidth(110);
    m_lblLocationTest->setMinimumWidth(110);
    m_lblLocationTest->setStyleSheet(ZTL::Setting::homepage::thirdTitleStyle);
    m_lblLocationCity = new QLabel("--", this);
    m_lblLocationCity->setStyleSheet(ZTL::Setting::homepage::simpleContentStyle);
    currentCityLayout->addWidget(m_lblLocationTest, 0);
    currentCityLayout->addWidget(m_lblLocationCity, 1);
    contentLayout->addLayout(currentCityLayout);
    // 收藏城市列表
    QHBoxLayout *collectedCityLayout = new QHBoxLayout();
    m_lblCollectedLocationTest = new QLabel("收藏的城市");
    m_lblCollectedLocationTest->setAlignment(Qt::AlignTop);
    m_lblCollectedLocationTest->setMaximumWidth(110);
    m_lblCollectedLocationTest->setMinimumWidth(110);
    m_lblCollectedLocationTest->setStyleSheet(ZTL::Setting::homepage::thirdTitleStyle);
    collectedCityLayout->addWidget(m_lblCollectedLocationTest);
    QVBoxLayout *rightCityLayout = new QVBoxLayout();
    // 四个按钮
    QHBoxLayout *btnsCityLayout = new QHBoxLayout();
    m_upLocationBtn = new QPushButton("up", this);
    m_downLocationBtn = new QPushButton("down", this);
    m_selectLocationBtn = new QPushButton("select", this);
    m_deleteLocationBtn = new QPushButton("delete", this);
    btnsCityLayout->addWidget(m_upLocationBtn);
    btnsCityLayout->addWidget(m_downLocationBtn);
    btnsCityLayout->addWidget(m_selectLocationBtn);
    btnsCityLayout->addWidget(m_deleteLocationBtn);
    m_upLocationBtn->setStyleSheet(ZTL::Setting::homepage::firstInFourButtonStyle);
    m_downLocationBtn->setStyleSheet(ZTL::Setting::homepage::secondInFourButtonStyle);
    m_selectLocationBtn->setStyleSheet(ZTL::Setting::homepage::thirdInFourButtonStyle);
    m_deleteLocationBtn->setStyleSheet(ZTL::Setting::homepage::forthInFourButtonStyle);
    rightCityLayout->addLayout(btnsCityLayout);
    // 列表区
    QWidget *collectedCityContent = new QWidget(); // 创建内容载体
    m_contentLocationLayout = new QVBoxLayout(collectedCityContent);
    m_contentLocationLayout->setAlignment(Qt::AlignTop);
    // 组装
    rightCityLayout->addWidget(collectedCityContent);
    collectedCityLayout->addLayout(rightCityLayout);
    contentLayout->addLayout(collectedCityLayout);
    m_clickedLocationIndex = -1;
    // 添加城市
    QHBoxLayout *addCityLayout = new QHBoxLayout();
    m_lblAddLocationTest = new QLabel("添加新城市：", this);
    m_lblAddLocationTest->setStyleSheet(ZTL::Setting::homepage::thirdTitleStyle);
    m_addCityInput = new QLineEdit(this);
    m_addCityInput->setPlaceholderText("城市名");
    m_addCityInput->setFixedHeight(32);
    m_addCityInput->setStyleSheet(ZTL::Setting::homepage::cityInputStyle);
    m_addCodeInput = new QLineEdit(this);
    m_addCodeInput->setPlaceholderText("城市编码");
    m_addCodeInput->setFixedHeight(32);
    m_addCodeInput->setStyleSheet(ZTL::Setting::homepage::codeInputStyle);
    QRegularExpression re("\\d{6}$");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(re, this);
    m_addCodeInput->setValidator(validator);
    m_addLocationSubmit = new QPushButton("提交", this);
    m_addLocationSubmit->setFixedWidth(100);
    m_addLocationSubmit->setFixedHeight(36);
    m_addLocationSubmit->setStyleSheet(ZTL::Setting::homepage::citySubmitStyle);
    addCityLayout->addWidget(m_lblAddLocationTest);
    addCityLayout->addWidget(m_addCityInput);
    addCityLayout->addWidget(m_addCodeInput);
    addCityLayout->addWidget(m_addLocationSubmit);
    contentLayout->addLayout(addCityLayout);
    // 每日一言部分
    // 标题
    m_lblQuoteTitle = new QLabel("-每日一言-", this);
    m_lblQuoteTitle->setStyleSheet(ZTL::Setting::homepage::secondTitleStyle);
    contentLayout->addWidget(m_lblQuoteTitle);
    // 当前主题
    QHBoxLayout *currentThemeLayout = new QHBoxLayout();
    m_lblThemeTest = new QLabel("当前主题：", this);
    m_lblThemeTest->setMaximumWidth(110);
    m_lblThemeTest->setMinimumWidth(110);
    m_lblThemeTest->setStyleSheet(ZTL::Setting::homepage::thirdTitleStyle);
    m_lblThemeContent = new QLabel("--", this);
    m_lblThemeContent->setStyleSheet(ZTL::Setting::homepage::simpleContentStyle);
    currentThemeLayout->addWidget(m_lblThemeTest, 0);
    currentThemeLayout->addWidget(m_lblThemeContent, 1);
    contentLayout->addLayout(currentThemeLayout);
    // 收藏主题列表
    QHBoxLayout *collectedThemeLayout = new QHBoxLayout();
    m_lblCollectedThemeTest = new QLabel("收藏的主题：");
    m_lblCollectedThemeTest->setAlignment(Qt::AlignTop);
    m_lblCollectedThemeTest->setMaximumWidth(110);
    m_lblCollectedThemeTest->setMinimumWidth(110);
    m_lblCollectedThemeTest->setStyleSheet(ZTL::Setting::homepage::thirdTitleStyle);
    collectedThemeLayout->addWidget(m_lblCollectedThemeTest);
    QVBoxLayout *rightThemeLayout = new QVBoxLayout();
    // 四个按钮
    QHBoxLayout *btnsThemeLayout = new QHBoxLayout();
    m_upThemeBtn = new QPushButton("up", this);
    m_downThemeBtn = new QPushButton("down", this);
    m_selectThemeBtn = new QPushButton("select", this);
    m_deleteThemeBtn = new QPushButton("delete", this);
    btnsThemeLayout->addWidget(m_upThemeBtn);
    btnsThemeLayout->addWidget(m_downThemeBtn);
    btnsThemeLayout->addWidget(m_selectThemeBtn);
    btnsThemeLayout->addWidget(m_deleteThemeBtn);
    m_upThemeBtn->setStyleSheet(ZTL::Setting::homepage::firstInFourButtonStyle);
    m_downThemeBtn->setStyleSheet(ZTL::Setting::homepage::secondInFourButtonStyle);
    m_selectThemeBtn->setStyleSheet(ZTL::Setting::homepage::thirdInFourButtonStyle);
    m_deleteThemeBtn->setStyleSheet(ZTL::Setting::homepage::forthInFourButtonStyle);
    rightThemeLayout->addLayout(btnsThemeLayout);
    // 列表区
    QWidget *collectedThemeContent = new QWidget(); // 创建内容载体
    m_contentThemeLayout = new QVBoxLayout(collectedThemeContent);
    m_contentThemeLayout->setAlignment(Qt::AlignTop);
    // 组装
    rightThemeLayout->addWidget(collectedThemeContent);
    collectedThemeLayout->addLayout(rightThemeLayout);
    contentLayout->addLayout(collectedThemeLayout);
    m_clickedThemeIndex = -1;
    // 添加主题
    QHBoxLayout *addThemeLayout = new QHBoxLayout();
    m_lblAddThemeTest = new QLabel("添加新主题：", this);
    m_lblAddThemeTest->setMaximumWidth(110);
    m_lblAddThemeTest->setMinimumWidth(110);
    m_lblAddThemeTest->setStyleSheet(ZTL::Setting::homepage::thirdTitleStyle);
    m_addThemeComboBox = new QComboBox(this);
    m_addThemeComboBox->addItems(ZTL::Setting::homepage::tags);
    m_addThemeComboBox->setPlaceholderText("选择要添加的主题");
    m_addThemeComboBox->setView(new QListView(this));
    m_addThemeComboBox->setFixedHeight(32);
    m_addThemeComboBox->setStyleSheet(ZTL::Setting::homepage::themeComboStyle);
    m_addThemeComboBox->setCurrentIndex(-1);
    m_addThemeSubmit = new QPushButton("提交", this);
    m_addThemeSubmit->setFixedWidth(100);
    m_addThemeSubmit->setFixedHeight(36);
    m_addThemeSubmit->setStyleSheet(ZTL::Setting::homepage::themeSubmitStyle);
    addThemeLayout->addWidget(m_lblAddThemeTest);
    addThemeLayout->addWidget(m_addThemeComboBox);
    addThemeLayout->addWidget(m_addThemeSubmit);
    contentLayout->addLayout(addThemeLayout);
    // 收尾
    m_mainScroll->setWidget(scrollContent);
    m_mainScroll->setWidgetResizable(true);

    // 刷新UI
    resetUI();

    // 链接
    // up
    connect(m_upLocationBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedLocationIndex == 0 || m_clickedLocationIndex == -1){
            qDebug() << "[SettingHomePage] 当前选定城市无法上行。";
            return;
        }
        int rawIndex = m_clickedLocationIndex;
        int tarIndex = m_clickedLocationIndex - 1;
        m_clickedLocationIndex -= 1;
        auto tmp = m_locationConfig.savedCities[rawIndex];
        m_locationConfig.savedCities[rawIndex] = m_locationConfig.savedCities[tarIndex];
        m_locationConfig.savedCities[tarIndex] = tmp;
        if (m_locationConfig.currentCityIndex == rawIndex) m_locationConfig.currentCityIndex = tarIndex;
        else if (m_locationConfig.currentCityIndex == tarIndex) m_locationConfig.currentCityIndex = rawIndex;
        ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    });
    connect(m_upThemeBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedThemeIndex == 0 || m_clickedThemeIndex == -1){
            qDebug() << "[SettingHomePage] 当前选定主题无法上行。";
            return;
        }
        int rawIndex = m_clickedThemeIndex;
        int tarIndex = m_clickedThemeIndex - 1;
        m_clickedThemeIndex -= 1;
        auto tmp = m_themeConfig.savedThemes[rawIndex];
        m_themeConfig.savedThemes[rawIndex] = m_themeConfig.savedThemes[tarIndex];
        m_themeConfig.savedThemes[tarIndex] = tmp;
        if (m_themeConfig.currentThemeIndex == rawIndex) m_themeConfig.currentThemeIndex = tarIndex;
        else if (m_themeConfig.currentThemeIndex == tarIndex) m_themeConfig.currentThemeIndex = rawIndex;
        ConfigManager::getInstance()->setHomeQuote(m_themeConfig);
    });
    // down
    connect(m_downLocationBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedLocationIndex == m_locationConfig.savedCities.length() - 1 || m_clickedLocationIndex == -1){
            qDebug() << "[SettingHomePage] 当前选定城市无法下行。";
            return;
        }
        int rawIndex = m_clickedLocationIndex;
        int tarIndex = m_clickedLocationIndex + 1;
        m_clickedLocationIndex += 1;
        auto tmp = m_locationConfig.savedCities[rawIndex];
        m_locationConfig.savedCities[rawIndex] = m_locationConfig.savedCities[tarIndex];
        m_locationConfig.savedCities[tarIndex] = tmp;
        if (m_locationConfig.currentCityIndex == rawIndex) m_locationConfig.currentCityIndex = tarIndex;
        else if (m_locationConfig.currentCityIndex == tarIndex) m_locationConfig.currentCityIndex = rawIndex;
        ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    });
    connect(m_downThemeBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedThemeIndex == m_themeConfig.savedThemes.length() - 1 || m_clickedThemeIndex == -1){
            qDebug() << "[SettingHomePage] 当前选定主题无法下行。";
            return;
        }
        int rawIndex = m_clickedThemeIndex;
        int tarIndex = m_clickedThemeIndex + 1;
        m_clickedThemeIndex += 1;
        auto tmp = m_themeConfig.savedThemes[rawIndex];
        m_themeConfig.savedThemes[rawIndex] = m_themeConfig.savedThemes[tarIndex];
        m_themeConfig.savedThemes[tarIndex] = tmp;
        if (m_themeConfig.currentThemeIndex == rawIndex) m_themeConfig.currentThemeIndex = tarIndex;
        else if (m_themeConfig.currentThemeIndex == tarIndex) m_themeConfig.currentThemeIndex = rawIndex;
        ConfigManager::getInstance()->setHomeQuote(m_themeConfig);
    });
    // select
    connect(m_selectLocationBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedLocationIndex == -1){
            qDebug() << "[SettingHomePage] 当前选定城市无法选择作为配置。";
            return;
        }
        else if(m_clickedLocationIndex == m_locationConfig.currentCityIndex){
            qDebug() << "[SettingHomePage] 相同，无需改变当前城市配置。";
            return;
        }
        m_locationConfig.currentCityIndex = m_clickedLocationIndex;
        ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    });
    connect(m_selectThemeBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedThemeIndex == -1){
            qDebug() << "[SettingHomePage] 当前选定主题无法选择作为配置。";
            return;
        }
        else if(m_clickedThemeIndex == m_themeConfig.currentThemeIndex){
            qDebug() << "[SettingHomePage] 相同，无需改变当前主题配置。";
            return;
        }
        m_themeConfig.currentThemeIndex = m_clickedThemeIndex;
        ConfigManager::getInstance()->setHomeQuote(m_themeConfig);
    });
    // delete
    connect(m_deleteLocationBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedLocationIndex == -1){
            qDebug() << "[SettingHomePage] 未选择，不可执行删除";
            return;
        }
        if (m_clickedLocationIndex < m_locationConfig.currentCityIndex){
            m_locationConfig.currentCityIndex -= 1;
        }
        else if(m_clickedLocationIndex == m_locationConfig.currentCityIndex){
            m_locationConfig.currentCityIndex = -1;
        }
        m_locationConfig.savedCities.remove(m_clickedLocationIndex);
        m_clickedLocationIndex = -1;
        ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    });
    connect(m_deleteThemeBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedThemeIndex == -1){
            qDebug() << "[SettingHomePage] 未选择，不可执行删除";
            return;
        }
        if (m_clickedThemeIndex < m_themeConfig.currentThemeIndex){
            m_themeConfig.currentThemeIndex -= 1;
        }
        else if(m_clickedThemeIndex == m_themeConfig.currentThemeIndex){
            m_themeConfig.currentThemeIndex = -1;
        }
        m_themeConfig.savedThemes.remove(m_clickedThemeIndex);
        m_clickedThemeIndex = -1;
        ConfigManager::getInstance()->setHomeQuote(m_themeConfig);
    });
    connect(m_addLocationSubmit, &QPushButton::clicked, this, [=](){
        qDebug() << "[SettingHomePage] location submit被点击";
        addCity();
    });
    connect(m_addThemeSubmit, &QPushButton::clicked, this, [=](){
        qDebug() << "[SettingHomePage] theme submit被点击";
        addTheme();
    });
    connect(this, &SettingHomePage::weatherSaveRequest, this, [=](){
        qDebug() << "[SettingHomePage] 保存weather配置";
        ConfigManager::getInstance()->setHomeweather(m_locationConfig);
    });
    connect(ConfigManager::getInstance(), &ConfigManager::homeWeatherChanged, this, [=](){
        qDebug() << "[SettingHomePage] weather配置改变，需要刷新。";
        resetUI();
    });
    connect(ConfigManager::getInstance(), &ConfigManager::homeQuoteChanged, this, [=](){
        qDebug() << "[SettingHomePage] quote配置改变，需要刷新。";
        resetUI();
    });
}

void SettingHomePage::resetUI(){
    qInfo() << "[SettingHomePage] 刷新setting页的home页的UI。";
    if (m_locationConfig.savedCities.isEmpty() && m_themeConfig.savedThemes.isEmpty()) return;
    // 城市部分
    // 对齐当前城市
    if (m_locationConfig.currentCityIndex != -1){
        m_lblLocationCity->setText(m_locationConfig.savedCities[m_locationConfig.currentCityIndex].name);
    }
    else{
        m_lblLocationCity->setText("无");
    }
    // 刷新城市列表
    if (m_contentLocationLayout) {
        while (m_contentLocationLayout->count() > 0) {
            QLayoutItem *item = m_contentLocationLayout->takeAt(0);
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
    }
    // 删除旧按钮组
    if (QButtonGroup *oldCityGroup = this->findChild<QButtonGroup*>("cityGroup")) {
        delete oldCityGroup;
    }
    QButtonGroup *cityBtnGroup = new QButtonGroup(this);
    cityBtnGroup->setObjectName("cityGroup");
    cityBtnGroup->setExclusive(true);
    for (int i = 0; i < m_locationConfig.savedCities.length(); i++) {
        const weatherCity &city = m_locationConfig.savedCities[i];
        QString text = QString("%1(%2)").arg(city.name).arg(city.code);
        QPushButton *cityBtn = new QPushButton(text);
        cityBtn->setStyleSheet(ZTL::Setting::homepage::listButtonStyle);
        cityBtn->setCheckable(true);
        if (m_clickedLocationIndex == -1 && i == m_locationConfig.currentCityIndex) {
            cityBtn->setChecked(true);
            m_clickedLocationIndex = i;
        }
        else if(i == m_clickedLocationIndex){
            cityBtn->setChecked(true);
        }
        m_contentLocationLayout->addWidget(cityBtn);
        cityBtnGroup->addButton(cityBtn, i);
    }
    connect(cityBtnGroup, &QButtonGroup::idClicked, this, [=](int id){
        qDebug() << "[SettingHomePage] 切换了当前点击的城市，索引为：" << id;
        m_clickedLocationIndex = id;
    });
    // 每日一言部分
    if (m_themeConfig.currentThemeIndex != -1){
        m_lblThemeContent->setText(m_themeConfig.savedThemes[m_themeConfig.currentThemeIndex]);
    }
    else{
        m_lblThemeContent->setText("无");
    }
    if (m_contentThemeLayout) {
        while (m_contentThemeLayout->count() > 0) {
            QLayoutItem *item = m_contentThemeLayout->takeAt(0);
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
    }
    if (QButtonGroup *oldThemeGroup = this->findChild<QButtonGroup*>("themeGroup")) {
        delete oldThemeGroup;
    }
    QButtonGroup *themeBtnGroup = new QButtonGroup(this);
    themeBtnGroup->setObjectName("themeGroup");
    themeBtnGroup->setExclusive(true);
    for (int i = 0; i < m_themeConfig.savedThemes.length(); i++) {
        QString theme = m_themeConfig.savedThemes[i];
        QPushButton *themeBtn = new QPushButton(theme);
        themeBtn->setStyleSheet(ZTL::Setting::homepage::listButtonStyle);
        themeBtn->setCheckable(true);
        if (m_clickedThemeIndex == -1 && i == m_themeConfig.currentThemeIndex) {
            themeBtn->setChecked(true);
            m_clickedThemeIndex = i;
        }
        else if(i == m_clickedThemeIndex){
            themeBtn->setChecked(true);
        }
        m_contentThemeLayout->addWidget(themeBtn);
        themeBtnGroup->addButton(themeBtn, i);
    }
    connect(themeBtnGroup, &QButtonGroup::idClicked, this, [=](int id){
        qDebug() << "[SettingHomePage] 切换了当前点击的主题，索引为：" << id;
        m_clickedThemeIndex = id;
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

bool SettingHomePage::addTheme(){
    QString theme = m_addThemeComboBox->currentText();
    if (m_addThemeComboBox->currentIndex() == -1){
        return false;
    }
    for (QString _theme : m_themeConfig.savedThemes) {
        if (theme == _theme) return false;
    }
    m_themeConfig.savedThemes.append(theme);
    ConfigManager::getInstance()->setHomeQuote(m_themeConfig);
    m_addThemeComboBox->setCurrentIndex(-1);
    return true;
}

