#include "mailgetservice.h"

MailGetService::MailGetService(QObject *parent)
    : QObject{parent}
{
    qInfo() << "[MailGetService] 启动。";

    m_watcher = new QFutureWatcher<bool>(this);
    connect(m_watcher, &QFutureWatcher<QString>::finished, this, [=]() {
        emit pollingOver();
    });
}




void MailGetService::mailPolling(){
    const QList<MailAccount> mailAccounts = MailAccountManager::getInstance()->getMailAccounts();
    QFuture<bool> future = QtConcurrent::run([=]() {
        for (const MailAccount &acc: mailAccounts){
            if (useGetMail(acc)) return true;
        }
        return false;
    });
    m_watcher->setFuture(future);
}



bool MailGetService::useGetMail(const MailAccount& acc){
    // 准备访问条件
    QDir scriptdir("./scripts");
    if (!scriptdir.exists()) {
        qWarning() << "[MailGetService] script文件夹不存在。";
        return false;
    }
    QFile scriptfile("./scripts/getSimpleMail.py");
    if (!scriptfile.exists()) {
        qWarning() << "[MailGetService] getSimpleMail.py不存在。";
        return false;
    }
    qDebug() << "[MailGetService] getSimpleMail.py存在。";
    // 启动进程
    QProcess process = QProcess();
    QString program = "python";
    QStringList arguments;
    arguments << "-u";
    arguments << "./scripts/getSimpleMail.py";
    arguments << "--address" << acc.address;
    arguments << "--code" << acc.code;
    arguments << "--server" << acc.server;
    arguments << "--port" << acc.port;
    process.start(program, arguments);
    process.waitForFinished(-1);
    QByteArray output = process.readAllStandardOutput();
    QString outputStr = QString::fromLocal8Bit(output).trimmed();
    if (outputStr.isEmpty()){
        qDebug() << "[MailGetService] 脚本返回为空。";
        return false;
    }
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(outputStr.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[MailGetService] JSON 解析失败，错误原因：" << error.errorString();
        return false;
    }
    QJsonObject jsonObj = doc.object();
    QString status = jsonObj.value("status").toString();
    if (status == "FAILED") {
        qWarning() << "[MailGetService] 错误:" << jsonObj["message"].toString();
        return false;
    }
    else if(status == "SUCCESS"){
        qDebug() << "[MailGetService] 找到新邮件。";
        QJsonObject data = jsonObj.value("data").toObject();
        emit newMailBrief(MailBrief(data, acc.address));
        return true;
    }
    else{
        qDebug() << "[MailGetService] 没有新邮件。";
        return false;
    }

}









