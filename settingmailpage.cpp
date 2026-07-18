#include "settingmailpage.h"

SettingMailPage::SettingMailPage(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[SettingMailPage] 启动。";
    // 数据
    m_mailBasicalConfig = ConfigManager::getInstance()->getMailBasical();
    m_mailAccounts = MailAccountManager::getInstance()->getMailAccounts();
    // ui初始化
    // 滚动区
    QVBoxLayout *generalLayout = new QVBoxLayout(this);
    generalLayout->setContentsMargins(0, 0, 0, 0);
    m_mainScroll = new QScrollArea(this);
    generalLayout->addWidget(m_mainScroll);
    QWidget *scrollContent = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(scrollContent);
    // 标题
    m_lblTitle = new QLabel("--邮箱--", this);
    contentLayout->addWidget(m_lblTitle);
    // 基础配置部分
    m_lblBasicalConfigTitle = new QLabel("-基础配置-", this);
    contentLayout->addWidget(m_lblBasicalConfigTitle);
    // 轮询间隔
    QHBoxLayout *pollingIntervalLyt = new QHBoxLayout();
    m_lblPollingIntervalTest = new QLabel("轮询间隔", this);
    m_pollingIntervalCombo = new QComboBox(this);
    for (const auto &item : ZTL::Setting::mailpage::intervals){
        m_pollingIntervalCombo->addItem(item.first, item.second);
    }
    int piIndex = m_pollingIntervalCombo->findData(m_mailBasicalConfig.pollingInterval);
    if (piIndex != -1) {
        m_pollingIntervalCombo->setCurrentIndex(piIndex);
    }
    pollingIntervalLyt->addWidget(m_lblPollingIntervalTest);
    pollingIntervalLyt->addWidget(m_pollingIntervalCombo);
    contentLayout->addLayout(pollingIntervalLyt);
    // 开机读取邮件数
    QHBoxLayout *initialNumberLyt = new QHBoxLayout();
    m_lblInitialNumberTest = new QLabel("开机读取邮件数", this);
    m_initialNumberCombo = new QComboBox(this);
    for (const auto &item : ZTL::Setting::mailpage::initNumbers){
        m_initialNumberCombo->addItem(item.first, item.second);
    }
    int inIndex = m_initialNumberCombo->findData(m_mailBasicalConfig.initialNumber);
    if (inIndex != -1) {
        m_initialNumberCombo->setCurrentIndex(inIndex);
    }
    initialNumberLyt->addWidget(m_lblInitialNumberTest);
    initialNumberLyt->addWidget(m_initialNumberCombo);
    contentLayout->addLayout(initialNumberLyt);
    // 每次轮询邮件数
    QHBoxLayout *earlierNumberLyt = new QHBoxLayout();
    m_lblEarlierNumberTest = new QLabel("每次更新邮件数", this);
    m_earlierNumberCombo = new QComboBox(this);
    for (const auto &item : ZTL::Setting::mailpage::pollNumbers){
        m_earlierNumberCombo->addItem(item.first, item.second);
    }
    int enIndex = m_earlierNumberCombo->findData(m_mailBasicalConfig.earlierNumber);
    if (enIndex != -1) {
        m_earlierNumberCombo->setCurrentIndex(enIndex);
    }
    earlierNumberLyt->addWidget(m_lblEarlierNumberTest);
    earlierNumberLyt->addWidget(m_earlierNumberCombo);
    contentLayout->addLayout(earlierNumberLyt);
    // 本地邮件储存上限
    QHBoxLayout *storageLimitLyt = new QHBoxLayout();
    m_lblStorageLimitTest = new QLabel("本地邮件储存上限", this);
    QVBoxLayout *storageInfoLyt = new QVBoxLayout();
    m_storageLimitDisplay = new QLabel(QString::number(m_mailBasicalConfig.storageLimit), this);
    QHBoxLayout *storageModifyLyt = new QHBoxLayout();
    m_storageLimitInput = new QLineEdit(this);
    QRegularExpression re("^[1-9]\\d*$");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(re, this);
    m_storageLimitInput->setValidator(validator);
    m_storageSubmitBtn = new QPushButton("提交", this);
    storageModifyLyt->addWidget(m_storageLimitInput);
    storageModifyLyt->addWidget(m_storageSubmitBtn);
    storageInfoLyt->addWidget(m_storageLimitDisplay);
    storageInfoLyt->addLayout(storageModifyLyt);
    storageLimitLyt->addWidget(m_lblStorageLimitTest);
    storageLimitLyt->addLayout(storageInfoLyt);
    contentLayout->addLayout(storageLimitLyt);
    // 邮箱账号展示部分
    m_lblAccountsTitle = new QLabel("-邮箱-", this);
    contentLayout->addWidget(m_lblAccountsTitle);
    // 现有邮箱
    m_lblSavedAccountsTest = new QLabel("现有邮箱", this);
    m_accountsWidget = new QWidget(this);
    m_accountWidgetLyt = new QVBoxLayout(m_accountsWidget);
    contentLayout->addWidget(m_lblSavedAccountsTest);
    contentLayout->addWidget(m_accountsWidget);
    // 添加邮箱
    m_lblMakeAccountsTest = new QLabel("处理邮箱", this);
    m_makeAccountStack = new QStackedWidget(this);
    m_makeAccountWaiting = new QPushButton("点击添加新邮箱", this);
    m_makeAccountDo = new SettingMailAccountsWidget(this);
    m_makeAccountStack->addWidget(m_makeAccountWaiting);
    m_makeAccountStack->addWidget(m_makeAccountDo);
    m_makeAccountStack->setCurrentIndex(0);
    contentLayout->addWidget(m_lblMakeAccountsTest);
    contentLayout->addWidget(m_makeAccountStack);





    // 收尾
    m_mainScroll->setWidget(scrollContent);
    m_mainScroll->setWidgetResizable(true);

    resetUI();


    // 连接
    connect(m_pollingIntervalCombo, &QComboBox::currentIndexChanged, this, [=](int index){
        if (index < 0) return;
        int interval = m_pollingIntervalCombo->itemData(index).toInt();
        qDebug() << "[SettingMailPage] 轮询间隔改为：" << interval;
        m_mailBasicalConfig.pollingInterval = interval;
        ConfigManager::getInstance()->setMailBasical(m_mailBasicalConfig);
    });
    connect(m_initialNumberCombo, &QComboBox::currentIndexChanged, this, [=](int index){
        if (index < 0) return;
        int data = m_initialNumberCombo->itemData(index).toInt();
        qDebug() << "[SettingMailPage] 启动邮件数改为：" << data;
        m_mailBasicalConfig.initialNumber = data;
        ConfigManager::getInstance()->setMailBasical(m_mailBasicalConfig);
    });
    connect(m_earlierNumberCombo, &QComboBox::currentIndexChanged, this, [=](int index){
        if (index < 0) return;
        int data = m_earlierNumberCombo->itemData(index).toInt();
        qDebug() << "[SettingMailPage] 刷新邮件数改为：" << data;
        m_mailBasicalConfig.earlierNumber = data;
        ConfigManager::getInstance()->setMailBasical(m_mailBasicalConfig);
    });
    connect(m_storageSubmitBtn, &QPushButton::clicked, this, [=](){
        if (m_storageLimitInput->text().isEmpty()) return;
        int data = m_storageLimitInput->text().toInt();
        qDebug() << "[SettingMailPage] 储存上限改为：" << data;
        m_mailBasicalConfig.storageLimit = data;
        m_storageLimitDisplay->setText(QString::number(m_mailBasicalConfig.storageLimit));
        ConfigManager::getInstance()->setMailBasical(m_mailBasicalConfig);
        m_storageLimitInput->clear();
    });
    connect(m_makeAccountWaiting, &QPushButton::clicked, this, [=](){
        m_makeAccountStack->setCurrentIndex(1);
    });
    connect(m_makeAccountDo, &SettingMailAccountsWidget::cancel, this, [=](){
        m_makeAccountStack->setCurrentIndex(0);
    });
    connect(MailAccountManager::getInstance(),
            &MailAccountManager::mailAccountChanged, this, [=](){
                resetUI();
            });

}

void SettingMailPage::resetUI(){
    qInfo() << "[SettingMailPage] 刷新setting页的mail页的UI。";
    if (m_accountWidgetLyt) {
        while (m_accountWidgetLyt->count() > 0) {
            QLayoutItem *item = m_accountWidgetLyt->takeAt(0);
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
    }
    m_mailAccounts = MailAccountManager::getInstance()->getMailAccounts();

    if(m_mailAccounts.isEmpty()){
        QLabel *holder = new QLabel("尚未添加邮箱");
        m_accountWidgetLyt->addWidget(holder);
    }
    else{
        for (MailAccount acc:m_mailAccounts){
            QWidget *widget = new QWidget();
            QHBoxLayout *layout = new QHBoxLayout(widget);
            QLabel *label = new QLabel(QString("<b>%1</b><br><small style='color:gray;'>%2</small>").arg(acc.name, acc.address));
            layout->addWidget(label);
            QPushButton *delBtn = new QPushButton("删除");
            QPushButton *editBtn = new QPushButton("修改");
            layout->addWidget(editBtn);
            layout->addWidget(delBtn);
            m_accountWidgetLyt->addWidget(widget);
            connect(editBtn, &QPushButton::clicked, this, [=](){
                m_makeAccountDo->setAccount(acc);
                m_makeAccountStack->setCurrentIndex(1);
            });
            connect(delBtn, &QPushButton::clicked, this, [=](){
                MailAccountManager::getInstance()->delMailAccount(acc.address);
            });
        }
    }






}






























