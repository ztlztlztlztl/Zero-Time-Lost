#include "homequoteservice.h"

HomeQuoteService::HomeQuoteService(QObject *parent)
    : QObject{parent}
{
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
    qDebug() << "跨天监控启动，当前初始日期：" << m_lastRecordDate.toString("yyyy-MM-dd");
}

void HomeQuoteService::updateQuote(){
    QuoteInfo info = CacheManager::getInstance()->getQuoteInfo();
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(info.timestamp);
    QDate tsDate = dateTime.date();
    QDate today = QDate::currentDate();
    if (tsDate == today){
        qDebug() << "不需更新";
        emit updateQuoteRequest(info.content);
        return;
    }
    qDebug() << "需更新";
    if (!useQuoteAPI()){
        emit updateQuoteRequest("愿你千帆阅尽，归来仍是少年");
    }
    return;

}





bool HomeQuoteService::useQuoteAPI(){
    // 准备访问条件
    QDir scriptdir("./scripts");
    if (!scriptdir.exists()) {
        qDebug() << "script文件夹不存在";
        return false;
    }
    QFile scriptfile("./scripts/getQuote.py");
    if (!scriptfile.exists()) {
        qDebug() << "script文件不存在";
        return false;
    }
    qDebug() << "都存在";
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
        qDebug() << outputStr;
        QuoteInfo info;
        info.content = outputStr;
        info.timestamp = QDateTime::currentSecsSinceEpoch();
        CacheManager::getInstance()->updateQuoteCache(info);
    });
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        qDebug() << "脚本错误：" << QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
    });
    connect(process, &QProcess::finished, process, &QProcess::deleteLater);
    process->start(program, arguments);
    return true;

}




