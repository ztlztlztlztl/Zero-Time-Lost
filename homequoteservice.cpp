#include "homequoteservice.h"

HomeQuoteService::HomeQuoteService(QObject *parent)
    : QObject{parent}
{
    qInfo() << "[HomeQuoteService] 启动。";
    m_lastRecordDate = QDate::currentDate(); // 现在日期
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [=](){
        QDate currentDate = QDate::currentDate();
        if (currentDate != m_lastRecordDate) {
            qDebug() << "检测到新的一天！旧日期：" << m_lastRecordDate.toString("yyyy-MM-dd")
                << " -> 新日期：" << currentDate.toString("yyyy-MM-dd");
            m_lastRecordDate = currentDate;
            updateQuote();
        }

    });
    m_timer->start(300000);
}

void HomeQuoteService::updateQuote(){
    qInfo() << "[HomeQuoteService] 请求更新每日一言信息。";
    QuoteInfo info = CacheManager::getInstance()->getQuoteInfo();
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(info.timestamp);
    QDate tsDate = dateTime.date();
    QDate today = QDate::currentDate();
    if (tsDate == today){
        qDebug() << "[HomeQuoteService] 每日一言信息不需更新";
        emit updateQuoteRequest(info.content);
        return;
    }
    qDebug() << "[HomeQuoteService] 每日一言信息需更新";
    if (!useQuoteAPI()){
        emit updateQuoteRequest("愿你千帆阅尽，归来仍是少年");
    }
    return;

}





bool HomeQuoteService::useQuoteAPI(){
    qInfo() << "[HomeQuoteService] 请求调用每日一言脚本。";
    // 准备访问条件
    QDir scriptdir("./scripts");
    if (!scriptdir.exists()) {
        qWarning() << "[HomeQuoteService] script文件夹不存在。";
        return false;
    }
    QFile scriptfile("./scripts/getQuote.py");
    if (!scriptfile.exists()) {
        qWarning() << "[HomeQuoteService] getQuote.py不存在。";
        return false;
    }
    qDebug() << "[HomeQuoteService] getQuote.py存在。";
    // 启动进程
    if (ConfigManager::getInstance()->getHomeQuote().savedThemes.size() == 0) return false;
    int index = ConfigManager::getInstance()->getHomeQuote().currentThemeIndex;
    QString theme =ConfigManager::getInstance()->getHomeQuote().savedThemes[index];
    QProcess *process = new QProcess(this);
    QString program = "python";
    QStringList arguments;
    arguments << "-u";
    arguments << "./scripts/getQuote.py";
    arguments << "--tag" << theme;
    // 链接信号
    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray output = process->readAllStandardOutput();
        QString outputStr = QString::fromLocal8Bit(output).trimmed();
        QuoteInfo info;
        info.content = outputStr;
        info.timestamp = QDateTime::currentSecsSinceEpoch();
        CacheManager::getInstance()->updateQuoteCache(info);
    });
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        qDebug() << "[HomeQuoteService] 脚本错误：" << QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
    });
    connect(process, &QProcess::finished, process, [=](){
        process->deleteLater();
        qInfo() << "[HomeeQuoteService] 每日一言脚本调用完毕。";
    });
    process->start(program, arguments);
    return true;

}




