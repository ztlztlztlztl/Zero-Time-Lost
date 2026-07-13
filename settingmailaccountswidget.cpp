#include "settingmailaccountswidget.h"

SettingMailAccountsWidget::SettingMailAccountsWidget(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *mainLyt = new QVBoxLayout(this);
    QHBoxLayout *typeLyt = new QHBoxLayout();
    m_typeText = new QLabel("邮箱类型：");
    m_typeCombo = new QComboBox();
    m_typeCombo->addItems(ZTL::Setting::mailpage::mailTypes);
    m_typeCombo->setPlaceholderText("请选择你的邮箱");
    m_typeCombo->setCurrentIndex(-1);
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
    m_portText = new QLabel("端口号：");
    m_portInput = new QLineEdit();
    m_portInput->setText("993");
    QRegularExpression portRegx("^\\d{1,5}$");
    QRegularExpressionValidator *portValidation = new QRegularExpressionValidator(portRegx, this);
    m_portInput->setValidator(portValidation);
    portLyt->addWidget(m_portText);
    portLyt->addWidget(m_portInput);
    mainLyt->addLayout(portLyt);
    QHBoxLayout *btnsLyt = new QHBoxLayout();
    m_saveBtn = new QPushButton("保存(未验证)");
    m_saveBtn->setDisabled(true);
    m_linkBtn = new QPushButton("测试连接");
    m_cancelBtn = new QPushButton("取消");
    btnsLyt->addWidget(m_saveBtn);
    btnsLyt->addWidget(m_linkBtn);
    btnsLyt->addWidget(m_cancelBtn);
    mainLyt->addLayout(btnsLyt);


    connect(m_saveBtn, &QPushButton::clicked, this, [=](){
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
    });
    connect(m_cancelBtn, &QPushButton::clicked, this, [=](){
        m_typeCombo->setCurrentIndex(-1);
        m_nameInput->clear();
        m_addressInput->clear();
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
        m_serverInput->setText(ZTL::Setting::mailpage::mailServers[index]);
    });

}

void SettingMailAccountsWidget::ckeckLinkable(){
    QString name = m_nameInput->text();
    if (name.isEmpty()) return;
    QString address = m_addressInput->text();
    QRegularExpression addressRegx(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    if (!addressRegx.match(address).hasMatch()) return;
    QString code = m_codeInput->text();
    if (code.isEmpty()) return;
    QString server = m_serverInput->text();
    QRegularExpression serverRegx(R"(^(?=.{1,255}$)[0-9a-zA-Z](?:(?:[0-9a-zA-Z]|-){0,61}[0-9a-zA-Z])?(?:\.[0-9a-zA-Z](?:(?:[0-9a-zA-Z]|-){0,61}[0-9a-zA-Z])?)*$)");
    if (!serverRegx.match(server).hasMatch()) return;
    QString port = m_portInput->text();
    if (port.isEmpty()) return;
    qDebug() << "[SettingMailPage] 检查邮箱填写信息。";
    // 准备访问条件
    QDir scriptdir("./scripts");
    if (!scriptdir.exists()) {
        qWarning() << "[SettingMailPage] script文件夹不存在。";
        return;
    }
    QFile scriptfile("./scripts/checkMail.py");
    if (!scriptfile.exists()) {
        qWarning() << "[SettingMailPage] checkMail.py不存在。";
        return;
    }
    qDebug() << "[SettingMailPage] checkMail.py存在。";
    // 启动进程
    QProcess *process = new QProcess(this);
    QString program = "python";
    QStringList arguments;
    arguments << "-u";
    arguments << "./scripts/checkMail.py";
    arguments << "--address" << address;
    arguments << "--code" << code;
    arguments << "--server" << server;
    arguments << "--port" << port;
    m_linkBtn->setText("测试中...");
    m_saveBtn->setDisabled(true);
    // 链接信号
    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray output = process->readAllStandardOutput();
        QString outputStr = QString::fromLocal8Bit(output).trimmed();
        if (outputStr.isEmpty()){
            qDebug() << "[SettingMailPage] 脚本返回为空。";
            return;
        }
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(outputStr.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError) {
            qWarning() << "[SettingMailPage] JSON 解析失败，错误原因：" << error.errorString();
            return;
        }
        QJsonObject jsonObj = doc.object();
        QString status = jsonObj.value("status").toString();
        if (status == "FAILED") {
            qWarning() << "[SettingMailPage] 错误:" << jsonObj["message"].toString();
            return;
        }
        qDebug() << "[SettingMailPage] 验证通过";
        m_saveBtn->setText("保存");
        m_saveBtn->setDisabled(false);
    });
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        qDebug() << "[SettingMailPage] 脚本错误：" << QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
    });
    connect(process, &QProcess::finished, process, [=](){
        m_linkBtn->setText("测试连接");
        m_linkBtn->setDisabled(false);
        process->deleteLater();
        qDebug() << "[SettingMailPage] 邮箱测试链接脚本调用完毕。";
    });
    process->start(program, arguments);
}










