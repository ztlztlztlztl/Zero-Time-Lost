#ifndef MAILACCOUNTMANAGER_H
#define MAILACCOUNTMANAGER_H


#include "helpjsonioengine.h"


#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>


struct MailAccount{
    QString name;
    QString address;
    QString code;
    QString server;
    QString port;

    MailAccount(){
        name = "";
        address = "";
        code = "";
        server = "";
        port = "";
    }

    MailAccount(const QJsonObject& mailAccountObj){
        name = mailAccountObj["name"].toString();
        address = mailAccountObj["address"].toString();
        code = mailAccountObj["code"].toString();
        server = mailAccountObj["server"].toString();
        port = mailAccountObj["port"].toString();
    }

    QJsonObject toJson() const{
        QJsonObject mailAccountObj;
        mailAccountObj["name"] = name;
        mailAccountObj["address"] = address;
        mailAccountObj["code"] = code;
        mailAccountObj["server"] = server;
        mailAccountObj["port"] = port;
        return mailAccountObj;
    }
};










class MailAccountManager : public QObject
{
    Q_OBJECT
public:
    // 单例模式
    static MailAccountManager* getInstance();
    void initialize();

    // 获取信息
    const QList<MailAccount>& getMailAccounts() const;
    void setMailAccount(const MailAccount& newAccount);

signals:
    void mailAccountChanged();






private:
    explicit MailAccountManager(QObject *parent = nullptr);
    ~MailAccountManager();
    MailAccountManager(const MailAccountManager&) = delete;
    MailAccountManager& operator=(const MailAccountManager&) = delete;

    // 文件 I/O 与自愈层
    void loadFromFile();
    void saveMailAccounts();


    QList<MailAccount> m_mailAccounts;
    QHash<QString, int> m_accountsIndex;




    HelpJsonIOEngine *m_mailEngine;







};

#endif // MAILACCOUNTMANAGER_H
