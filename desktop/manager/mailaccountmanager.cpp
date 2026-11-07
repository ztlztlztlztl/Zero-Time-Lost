#include <QSet>
#include "mailaccountmanager.h"

MailAccountManager* MailAccountManager::getInstance()
{
    // 单例的标准写法
    static MailAccountManager instance;
    return &instance; // 返回它的指针
}

MailAccountManager::MailAccountManager(QObject *parent)
    : QObject(nullptr) // 单例的父指针为 nullptr
{
    qDebug() << "[MailAccountManager] 构造函数被调用。";
}





MailAccountManager::~MailAccountManager() = default;

void MailAccountManager::initialize()
{
    qInfo() << "[MailAccountManager] 开始自检与读取。";
    delete m_mailEngine;
    m_mailEngine = new HelpJsonIOEngine("./config", "mails.json", 1000, this);
    loadFromFile();
}

void MailAccountManager::loadFromFile(){
    qDebug() << "[MailAccountManager] 加载账号：./config";
    QJsonObject mailObj = m_mailEngine->read();
    m_accountsIndex.clear();
    unreadableAccounts={};
    if (mailObj.isEmpty()) {
        qWarning() << "[MailAccountManager] ./settings.json JSON 格式损坏或空。";
        m_mailAccounts = {};
        return;
    }
    QJsonArray accounts = mailObj["accounts"].toArray();
    bool legacySecret=false;
    m_mailAccounts = {};
    for (int i = 0;i < accounts.size(); i++){
        QJsonObject account = accounts[i].toObject();
        if(!accounts[i].isObject()||!account["address"].isString()||account["address"].toString().trimmed().isEmpty()||!account["server"].isString()||!account["port"].isString()||m_accountsIndex.contains(account["address"].toString())){unreadableAccounts.append(accounts[i]);continue;}
        try {
            MailAccount item(account);
            if(item.address.isEmpty())continue;
            legacySecret|=account.contains("code");
            m_accountsIndex[item.address]=m_mailAccounts.size();
            m_mailAccounts.append(item);
        } catch(...) {
            unreadableAccounts.append(account);
            qWarning() << "无法解密一个邮件账号，请在原 Windows 用户下导出或重新添加账号。";
        }
    }
    if(legacySecret){
        try {saveMailAccounts();} catch(...) {qCritical() << "旧账号凭据保护失败，请重新保存账号。";}
    }
}

void MailAccountManager::saveMailAccounts(){
    qDebug() << "[MailAccountManager] 保存 Mail 账号。";
    QJsonObject mailObj;
    QJsonArray accountsArray=unreadableAccounts;
    for (const MailAccount& account: m_mailAccounts){
        accountsArray.append(account.toJson());
    }
    mailObj["accounts"] = accountsArray;
    // 打包保存
    m_mailEngine->endWrite(mailObj);
}
bool MailAccountManager::replaceAll(const QList<MailAccount> &accounts){
    QJsonArray values;QSet<QString> ids;
    try{for(const auto &a:accounts){if(a.address.isEmpty()||ids.contains(a.address))return false;ids.insert(a.address);values.append(a.toJson());}}catch(...){return false;}
    m_mailEngine->endWrite({{"accounts",values}});if(!m_mailEngine->flush())return false;
    m_mailAccounts=accounts;unreadableAccounts={};m_accountsIndex.clear();for(int i=0;i<accounts.size();++i)m_accountsIndex[accounts[i].address]=i;emit mailAccountChanged();return true;
}

const QList<MailAccount>& MailAccountManager::getMailAccounts() const {
    return m_mailAccounts;
}

QString MailAccountManager::getAddressName(QString address) const{
    if(!m_accountsIndex.contains(address))return address + "（离线账号）";
    return m_mailAccounts[m_accountsIndex[address]].name;
}

void MailAccountManager::setMailAccount(const MailAccount& newAccount){
    QJsonArray keep;for(auto value:unreadableAccounts)if(value.toObject()["address"].toString()!=newAccount.address)keep.append(value);unreadableAccounts=keep;
    if (m_accountsIndex.contains(newAccount.address)){
        m_mailAccounts[m_accountsIndex[newAccount.address]] = newAccount;
    }
    else{
        m_accountsIndex[newAccount.address] = m_mailAccounts.size();
        m_mailAccounts.append(newAccount);
    }
    saveMailAccounts();
    emit mailAccountChanged();
}

void MailAccountManager::delMailAccount(const QString& address){
    QJsonArray keep;for(auto value:unreadableAccounts)if(value.toObject()["address"].toString()!=address)keep.append(value);unreadableAccounts=keep;
    if (!m_accountsIndex.contains(address)) {saveMailAccounts();emit mailAccountChanged();return;}
    int tarIndex = m_accountsIndex[address];
    m_mailAccounts.remove(tarIndex);
    m_accountsIndex.clear();
    for (int i = 0;i < m_mailAccounts.size();i++){
        m_accountsIndex[m_mailAccounts[i].address] = i;
    }
    saveMailAccounts();
    emit mailAccountChanged();
}

MailAccount MailAccountManager::getMailAcc(QString address) const{
    if(!m_accountsIndex.contains(address))return {};
    return m_mailAccounts[m_accountsIndex[address]];
}




































