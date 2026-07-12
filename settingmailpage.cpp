#include "settingmailpage.h"

SettingMailPage::SettingMailPage(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[SettingMailPage] 启动。";
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
    m_lblPollingIntervalTest = new QLabel("轮询间隔(分钟)", this);
    m_pollingIntervalCombo = new QComboBox(this);
    for (const auto &item : ZTL::Setting::mailpage::intervals){
        m_pollingIntervalCombo->addItem(item.first, item.second);
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
    initialNumberLyt->addWidget(m_lblInitialNumberTest);
    initialNumberLyt->addWidget(m_initialNumberCombo);
    contentLayout->addLayout(initialNumberLyt);
    // 每次轮询邮件数
    QHBoxLayout *earlierNumberLyt = new QHBoxLayout();
    m_lblEarlierNumberTest = new QLabel("每次轮询邮件数", this);
    m_earlierNumberCombo = new QComboBox(this);
    for (const auto &item : ZTL::Setting::mailpage::pollNumbers){
        m_earlierNumberCombo->addItem(item.first, item.second);
    }
    earlierNumberLyt->addWidget(m_lblEarlierNumberTest);
    earlierNumberLyt->addWidget(m_earlierNumberCombo);
    contentLayout->addLayout(earlierNumberLyt);
    // 本地邮件储存上限
    QHBoxLayout *storageLimitLyt = new QHBoxLayout();
    m_lblStorageLimitTest = new QLabel("本地邮件储存上限", this);
    QVBoxLayout *storageInfoLyt = new QVBoxLayout();
    m_storageLimitDisplay = new QLabel("--", this);
    QHBoxLayout *storageModifyLyt = new QHBoxLayout();
    m_storageLimitInput = new QLineEdit(this);
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
    m_accountsListWidget = new QListWidget(this);
    m_accountsListWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    contentLayout->addWidget(m_lblSavedAccountsTest);
    contentLayout->addWidget(m_accountsListWidget);
    // 添加邮箱
    m_lblAddAccountsTest = new QLabel("添加邮箱", this);
    m_addAccountStack = new QStackedWidget(this);
    QLabel *test = new QLabel("addAccountStack 在这里", this);
    m_addAccountStack->addWidget(test);
    contentLayout->addWidget(m_lblAddAccountsTest);
    contentLayout->addWidget(m_addAccountStack);





    // 收尾
    m_mainScroll->setWidget(scrollContent);
    m_mainScroll->setWidgetResizable(true);
}
