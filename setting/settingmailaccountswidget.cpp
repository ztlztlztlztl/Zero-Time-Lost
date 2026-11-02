#include "settingmailaccountswidget.h"
#include "processworker.h"
#include "settingcontrols.h"

SettingMailAccountsWidget::SettingMailAccountsWidget(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *mainLyt = new QVBoxLayout(this);
    QHBoxLayout *typeLyt = new QHBoxLayout();
    m_typeText = new QLabel("邮箱类型：");
    m_typeText->setFixedWidth(100);
    m_typeCombo = new NoWheelComboBox();
    m_typeCombo->addItems(ZTL::Setting::mailpage::mailTypes);
    m_typeCombo->setPlaceholderText("请选择你的邮箱");
    m_typeCombo->setCurrentIndex(-1);
    typeLyt->addWidget(m_typeText);
    typeLyt->addWidget(m_typeCombo);
    mainLyt->addLayout(typeLyt);
    QHBoxLayout *nameLyt = new QHBoxLayout();
    m_nameText = new QLabel("账号名：");
    m_nameText->setFixedWidth(100);
    m_nameInput = new QLineEdit();
    m_nameInput->setObjectName("accountName");
    m_nameInput->setPlaceholderText("例：工作邮箱");
    nameLyt->addWidget(m_nameText);
    nameLyt->addWidget(m_nameInput);
    mainLyt->addLayout(nameLyt);
    QHBoxLayout *addressLyt = new QHBoxLayout();
    m_addressText = new QLabel("邮箱地址：");
    m_addressText->setFixedWidth(100);
    m_addressInput = new QLineEdit();
    m_addressInput->setObjectName("accountAddress");
    m_addressInput->setPlaceholderText("xxx@xx.com");
    addressLyt->addWidget(m_addressText);
    addressLyt->addWidget(m_addressInput);
    mainLyt->addLayout(addressLyt);
    QHBoxLayout *codeLyt = new QHBoxLayout();
    m_codeText = new QLabel("授权码：");
    m_codeText->setFixedWidth(100);
    m_codeInput = new QLineEdit();
    m_codeInput->setObjectName("accountCode");
    m_codeInput->setEchoMode(QLineEdit::Password);
    m_codeInput->setPlaceholderText("请输入IMAP授权码");
    codeLyt->addWidget(m_codeText);
    codeLyt->addWidget(m_codeInput);
    mainLyt->addLayout(codeLyt);
    QHBoxLayout *serverLyt = new QHBoxLayout();
    m_serverText = new QLabel("服务器：");
    m_serverText->setFixedWidth(100);
    m_serverInput = new QLineEdit();
    m_serverInput->setObjectName("accountServer");
    m_serverInput->setPlaceholderText("请输入服务器地址");
    serverLyt->addWidget(m_serverText);
    serverLyt->addWidget(m_serverInput);
    mainLyt->addLayout(serverLyt);
    QHBoxLayout *portLyt = new QHBoxLayout();
    m_portText = new QLabel("端口号：");
    m_portText->setFixedWidth(100);
    m_portInput = new QLineEdit();
    m_portInput->setObjectName("accountPort");
    m_portInput->setText("993");
    QRegularExpression portRegx("^\\d{1,5}$");
    QRegularExpressionValidator *portValidation = new QRegularExpressionValidator(portRegx, this);
    m_portInput->setValidator(portValidation);
    portLyt->addWidget(m_portText);
    portLyt->addWidget(m_portInput);
    mainLyt->addLayout(portLyt);
    m_feedback=new QLabel;m_feedback->setObjectName("accountFeedback");m_feedback->setProperty("role","muted");
    m_feedback->setWordWrap(true);m_feedback->hide();mainLyt->addWidget(m_feedback);
    QHBoxLayout *btnsLyt = new QHBoxLayout();
    m_saveBtn = new SettingButton("保存(未验证)");m_saveBtn->setObjectName("accountSave");
    m_saveBtn->setDisabled(true);
    m_linkBtn = new SettingButton("测试连接");m_linkBtn->setObjectName("accountTest");
    m_cancelBtn = new SettingButton("取消");
    btnsLyt->addWidget(SettingControls::buttonSlot(m_saveBtn));
    btnsLyt->addWidget(SettingControls::buttonSlot(m_linkBtn));
    btnsLyt->addWidget(SettingControls::buttonSlot(m_cancelBtn));
    mainLyt->addLayout(btnsLyt);


    connect(m_saveBtn, &QPushButton::clicked, this, [=](){
        if(!validateFields())return;
        QString name = m_nameInput->text();
        QString address = m_addressInput->text();
        QString code = m_codeInput->text();
        QString server = m_serverInput->text();
        QString port = m_portInput->text();
        MailAccount newAcc;
        newAcc.name = name;
        newAcc.address = address;
        newAcc.code = code;
        newAcc.server = server;
        newAcc.port = port;
        MailAccountManager::getInstance()->setMailAccount(newAcc);
        m_cancelBtn->click();
    });
    connect(m_cancelBtn, &QPushButton::clicked, this, [=](){
        m_typeCombo->setCurrentIndex(-1);
        m_nameInput->clear();
        m_addressInput->clear();
        m_addressInput->setReadOnly(false);
        m_addressInput->setToolTip({});
        m_codeInput->clear();
        m_serverInput->clear();
        m_portInput->setText("993");
        emit cancel();
    });
    connect(m_linkBtn, &QPushButton::clicked, this, [=](){
        ckeckLinkable();
    });
    connect(m_addressInput, &QLineEdit::textChanged, this, [=](){
        m_saveBtn->setText("保存(未验证)");
        m_saveBtn->setDisabled(true);
    });
    connect(m_codeInput, &QLineEdit::textChanged, this, [=](){
        m_saveBtn->setText("保存(未验证)");
        m_saveBtn->setDisabled(true);
    });
    connect(m_serverInput, &QLineEdit::textChanged, this, [=](){
        m_saveBtn->setText("保存(未验证)");
        m_saveBtn->setDisabled(true);
    });
    connect(m_portInput, &QLineEdit::textChanged, this, [=](){
        m_saveBtn->setText("保存(未验证)");
        m_saveBtn->setDisabled(true);
    });
    connect(m_typeCombo, &QComboBox::currentIndexChanged, this, [=](int index){
        m_serverInput->setText((index == -1)?"":ZTL::Setting::mailpage::mailServers[index]);
    });

}

void SettingMailAccountsWidget::setAccount(const MailAccount& iacc){
    m_nameInput->setText(iacc.name);
    m_addressInput->setText(iacc.address);
    m_addressInput->setReadOnly(true);
    m_addressInput->setToolTip("邮箱地址用于定位缓存邮件；更换地址请新增账号，原邮件缓存保留。" );
    m_codeInput->setText(iacc.code);
    m_serverInput->setText(iacc.server);
    m_portInput->setText(iacc.port);
    m_saveBtn->setDisabled(false);
    m_saveBtn->setText("保存");
}


bool SettingMailAccountsWidget::validateFields(){
    if(m_nameInput->text().trimmed().isEmpty()||m_addressInput->text().trimmed().isEmpty()||
       m_serverInput->text().trimmed().isEmpty()||m_codeInput->text().isEmpty()){
        m_feedback->setText("信息不完整：请填写名称、地址、服务器和授权码");m_feedback->show();return false;
    }
    bool validPort=false;const int port=m_portInput->text().toInt(&validPort);
    if(!validPort||port<1||port>65535){m_feedback->setText("请填写 1–65535 之间的端口号");m_feedback->show();return false;}
    return true;
}
void SettingMailAccountsWidget::ckeckLinkable(){
    if(!validateFields())return;
    m_feedback->setText("正在测试连接…");m_feedback->show();
    m_linkBtn->setDisabled(true);m_saveBtn->setDisabled(true);m_linkBtn->setText("测试中…");
    const QString address=m_addressInput->text(),code=m_codeInput->text(),server=m_serverInput->text(),port=m_portInput->text();
    auto worker=new ProcessWorker(this);
    worker->run({{"command","check"},{"account",QJsonObject{{"address",address},{"code",code},{"server",server},{"port",port}}}},
        [this,worker,address,code,server,port](const QJsonObject &,const QString &error){
            m_linkBtn->setEnabled(true);m_linkBtn->setText("测试连接");
            const bool unchanged=address==m_addressInput->text()&&code==m_codeInput->text()&&server==m_serverInput->text()&&port==m_portInput->text();
            if(error.isEmpty()&&unchanged){m_saveBtn->setEnabled(true);m_saveBtn->setText("保存");m_feedback->setText("连接成功，可以保存");}
            else if(!error.isEmpty())m_feedback->setText("连接失败："+error);
            else m_feedback->setText("配置已更改，请重新测试连接");
            m_feedback->show();
            worker->deleteLater();
        });
}










