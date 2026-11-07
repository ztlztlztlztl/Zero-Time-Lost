#ifndef MAILACCOUNTMANAGER_H
#define MAILACCOUNTMANAGER_H


#include "helpjsonioengine.h"
#include "cryptotool.h"


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
    QString originalProtectedCode;

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
        code = mailAccountObj.contains("protectedCode") ? CryptoTool::unprotect(mailAccountObj["protectedCode"].toString()) : mailAccountObj["code"].toString();
        originalProtectedCode=mailAccountObj["protectedCode"].toString();
        server = mailAccountObj["server"].toString();
        port = mailAccountObj["port"].toString();
    }

    QJsonObject toJson() const{
        QJsonObject mailAccountObj;
        mailAccountObj["name"] = name;
        mailAccountObj["address"] = address;
        mailAccountObj["protectedCode"] = code.isEmpty()&&!originalProtectedCode.isEmpty()?originalProtectedCode:CryptoTool::protect(code);
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
    QString getAddressName(QString address) const;
    MailAccount getMailAcc(QString address) const;
    void setMailAccount(const MailAccount& newAccount);
    void delMailAccount(const QString& address);
    bool replaceAll(const QList<MailAccount> &accounts);
    void reload(){loadFromFile();}
    bool cloudReady() const {if(!unreadableAccounts.isEmpty())return false;for(const auto &a:m_mailAccounts)if(a.code.isEmpty())return false;return true;}

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
    QJsonArray unreadableAccounts;




    HelpJsonIOEngine *m_mailEngine = nullptr;







};

#endif // MAILACCOUNTMANAGER_H
