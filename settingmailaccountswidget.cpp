#include "settingmailaccountswidget.h"

SettingMailAccountsWidget::SettingMailAccountsWidget(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *mainLyt = new QVBoxLayout(this);
    QHBoxLayout *typeLyt = new QHBoxLayout();
    m_typeText = new QLabel("邮箱类型：");
    m_typeCombo = new QComboBox();
    m_typeCombo->setPlaceholderText("请选择你的邮箱");
    typeLyt->addWidget(m_typeText);
    typeLyt->addWidget(m_typeCombo);
    mainLyt->addLayout(typeLyt);
    QHBoxLayout *nameLyt = new QHBoxLayout();
    m_nameText = new QLabel("账号名：");
    m_nameInput = new QLineEdit();
    m_nameInput->setPlaceholderText("例：工作邮箱");
    nameLyt->addWidget(m_nameText);
    nameLyt->addWidget(m_nameInput);
    mainLyt->addLayout(nameLyt);
    QHBoxLayout *addressLyt = new QHBoxLayout();
    m_addressText = new QLabel("邮箱地址：");
    m_addressInput = new QLineEdit();
    m_addressInput->setPlaceholderText("xxx@xx.com");
    addressLyt->addWidget(m_addressText);
    addressLyt->addWidget(m_addressInput);
    mainLyt->addLayout(addressLyt);
    QHBoxLayout *codeLyt = new QHBoxLayout();
    m_codeText = new QLabel("授权码：");
    m_codeInput = new QLineEdit();
    m_codeInput->setPlaceholderText("请输入IMAP授权码");
    codeLyt->addWidget(m_codeText);
    codeLyt->addWidget(m_codeInput);
    mainLyt->addLayout(codeLyt);
    QHBoxLayout *serverLyt = new QHBoxLayout();
    m_serverText = new QLabel("服务器：");
    m_serverInput = new QLineEdit();
    m_serverInput->setPlaceholderText("请输入服务器地址");
    serverLyt->addWidget(m_serverText);
    serverLyt->addWidget(m_serverInput);
    mainLyt->addLayout(serverLyt);
    QHBoxLayout *portLyt = new QHBoxLayout();
    m_portText = new QLabel("服务器：");
    m_portInput = new QLineEdit();
    m_portInput->setText("993");
    portLyt->addWidget(m_portText);
    portLyt->addWidget(m_portInput);
    mainLyt->addLayout(portLyt);
    QHBoxLayout *btnsLyt = new QHBoxLayout();
    m_saveBtn = new QPushButton("保存(未验证)");
    m_linkBtn = new QPushButton("测试连接");
    m_cancelBtn = new QPushButton("取消");
    btnsLyt->addWidget(m_saveBtn);
    btnsLyt->addWidget(m_linkBtn);
    btnsLyt->addWidget(m_cancelBtn);
    mainLyt->addLayout(btnsLyt);

    connect(m_cancelBtn, &QPushButton::clicked, this, [=](){
        m_typeCombo->setCurrentIndex(-1);
        m_nameInput->clear();
        m_addressInput->clear();
        m_codeInput->clear();
        m_serverInput->clear();
        m_portInput->setText("993");
        emit cancel();
    });

}
