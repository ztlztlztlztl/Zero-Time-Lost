#include "homeinfowidget.h"

HomeInfoWidget::HomeInfoWidget(QWidget *parent) : QWidget(parent)
{
    // 总水平布局
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(40, 20, 40, 20); // 四周边距
    mainLayout->setSpacing(30); // 组件间距
    mainLayout->setAlignment(Qt::AlignVCenter); // 垂直方向上居中对齐
    // 左侧：天气区块
    QWidget *weatherWidget = new QWidget(this);
    QVBoxLayout *weatherLayout = new QVBoxLayout(weatherWidget);
    weatherLayout->setContentsMargins(0, 0, 0, 0);
    weatherLayout->setSpacing(4);
    m_lblTemp = new QLabel("--°C", this); // 初始给个占位符
    m_lblWeather = new QLabel("正在加载...", this); // 状况待定
    m_lblTemp->setStyleSheet("color: #FFFFFF; font-size: 26px; font-family: 'JetBrains Mono', 'Segoe UI'; font-weight: bold;");
    m_lblWeather->setStyleSheet("color: rgba(255, 255, 255, 0.7); font-size: 14px; font-family: 'Microsoft YaHei';");
    m_lblTemp->setAlignment(Qt::AlignRight | Qt::AlignVCenter); // 居中右对齐
    m_lblWeather->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    weatherLayout->addWidget(m_lblTemp);
    weatherLayout->addWidget(m_lblWeather);
    // 竖线一
    QFrame *line1 = new QFrame(this);
    line1->setFrameShape(QFrame::VLine); // 设为竖线
    line1->setFrameShadow(QFrame::Plain);
    line1->setStyleSheet("color: rgba(255, 255, 255, 0.3); qproperty-lineWidth: 2;"); // 30% 透明度的精致白线
    line1->setFixedHeight(75); // 固定高度，略矮于时间
    // 数字时钟
    m_lblClock = new QLabel("00:00", this);
    m_lblClock->setStyleSheet("color: #FFFFFF; font-size: 84px; font-family: 'JetBrains Mono', 'Consolas'; font-weight: 300;");
    m_lblClock->setAlignment(Qt::AlignCenter);
    // 竖线二
    QFrame *line2 = new QFrame(this);
    line2->setFrameShape(QFrame::VLine);
    line2->setFrameShadow(QFrame::Plain);
    line2->setStyleSheet("color: rgba(255, 255, 255, 0.3); qproperty-lineWidth: 2;");
    line2->setFixedHeight(75);
    // 期与金句区块
    QWidget *contentWidget = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);
    m_lblDate = new QLabel("0000.0.0 星期几", this);
    m_lblQuote = new QLabel("正在加载每日一言...", this);
    m_lblDate->setStyleSheet("color: rgba(255, 255, 255, 0.7); font-size: 15px; font-family: 'JetBrains Mono';");
    m_lblQuote->setStyleSheet("color: #FFFFFF; font-size: 16px; font-family: 'Microsoft YaHei'; font-weight: bold;");
    m_lblDate->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 居中左对齐
    m_lblQuote->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    contentLayout->addWidget(m_lblDate);
    contentLayout->addWidget(m_lblQuote);
    // 组装
    mainLayout->addWidget(weatherWidget);
    mainLayout->addWidget(line1);
    mainLayout->addWidget(m_lblClock);
    mainLayout->addWidget(line2);
    mainLayout->addWidget(contentWidget);
    mainLayout->setStretchFactor(weatherWidget, 1);
    mainLayout->setStretchFactor(m_lblClock, 1);
    mainLayout->setStretchFactor(contentWidget, 2);
    // 时间刷新机制
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &HomeInfoWidget::onClockTick);
    m_clockTimer->start(1000); // 1000毫秒
    // 初始化
    refreshDateTime();
}

HomeInfoWidget::~HomeInfoWidget()
{}

void HomeInfoWidget::onClockTick()
{
    refreshDateTime();
}

void HomeInfoWidget::refreshDateTime()
{
    QDateTime current = QDateTime::currentDateTime();
    QString timeStr = current.toString("hh:mm"); // 刷新时钟
    m_lblClock->setText(timeStr);
    QString dateStr = current.toString("yyyy.M.d ddd"); // 刷新日期
    m_lblDate->setText(dateStr);
}

void HomeInfoWidget::updateWeather(const QString &temp, const QString &status)
{
    m_lblTemp->setText(temp);
    m_lblWeather->setText(status);
}

void HomeInfoWidget::updateQuote(const QString &quote)
{
    m_lblQuote->setText(quote);
}

void HomeInfoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    double baseWidth = 720.0; // 基准像素
    double scale = this->width() / baseWidth; // 缩放比例
    // 设界 0.2 ~ 2.0
    if (scale < 0.6) scale = 0.6;
    if (scale > 2.0) scale = 2.0;
    // 计算新字号
    int clockSize = static_cast<int>(84 * scale);
    int tempSize = static_cast<int>(26 * scale);
    int weatherSize = static_cast<int>(14 * scale);
    int dateSize = static_cast<int>(15 * scale);
    int quoteSize = static_cast<int>(16 * scale);
    // 中间：时间
    m_lblClock->setStyleSheet(QString("color: #FFFFFF; font-size: %1px; font-family: 'JetBrains Mono', 'Consolas'; font-weight: 300;").arg(clockSize));

    // 左侧：天气
    m_lblTemp->setStyleSheet(QString("color: #FFFFFF; font-size: %1px; font-family: 'JetBrains Mono', 'Segoe UI'; font-weight: bold;").arg(tempSize));
    m_lblWeather->setStyleSheet(QString("color: rgba(255, 255, 255, 0.7); font-size: %1px; font-family: 'Microsoft YaHei';").arg(weatherSize));

    // 右侧：日期金句
    m_lblDate->setStyleSheet(QString("color: rgba(255, 255, 255, 0.7); font-size: %1px; font-family: 'JetBrains Mono';").arg(dateSize));
    m_lblQuote->setStyleSheet(QString("color: #FFFFFF; font-size: %1px; font-family: 'Microsoft YaHei'; font-weight: bold;").arg(quoteSize));

    // 动态调整主布局的边距
    if (this->layout()) {
        int horizontalMargin = static_cast<int>(40 * scale);
        int verticalMargin   = static_cast<int>(20 * scale);
        this->layout()->setContentsMargins(horizontalMargin, verticalMargin, horizontalMargin, verticalMargin);
    }
}










