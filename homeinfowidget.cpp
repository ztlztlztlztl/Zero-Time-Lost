#include "homeinfowidget.h"

HomeInfoWidget::HomeInfoWidget(QWidget *parent) : QWidget(parent)
{
    qInfo() << "[HomeInfoWidget] 启动。";
    // 总体垂直布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 20, 40, 20);
    mainLayout->setSpacing(15); // 上下两行的间距
    mainLayout->setAlignment(Qt::AlignVCenter);
    // 第一层，水平布局，天气 - 时间 - 日期
    QWidget *topWidget = new QWidget(this);
    QHBoxLayout *topLayout = new QHBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(30);

    // 天气
    QWidget *weatherWidget = new QWidget(this);
    QHBoxLayout *weatherLayout = new QHBoxLayout(weatherWidget);
    weatherLayout->setContentsMargins(0, 0, 0, 0);
    weatherLayout->setSpacing(8);
    m_lblLocation = new QLabel(" 无 ", this);
    m_lblTemp = new QLabel(" -- ", this);
    m_lblWeather = new QLabel(" 正在加载... ", this);
    m_lblLocation->setStyleSheet(ZTL::Home::Info::locationStyle);
    m_lblTemp->setStyleSheet(ZTL::Home::Info::tempStyle);
    m_lblWeather->setStyleSheet(ZTL::Home::Info::weatherStyle);
    // 居中右对齐
    weatherLayout->addWidget(m_lblLocation, 0, Qt::AlignRight | Qt::AlignVCenter);
    weatherLayout->addWidget(m_lblTemp, 0, Qt::AlignRight | Qt::AlignVCenter);
    weatherLayout->addWidget(m_lblWeather, 0, Qt::AlignRight | Qt::AlignVCenter);
    // 时间
    m_lblClock = new QLabel("00 : 00", this);
    m_lblClock->setStyleSheet(ZTL::Home::Info::timeStyle);
    m_lblClock->setAlignment(Qt::AlignCenter);
    // 日期
    m_lblDate = new QLabel("0000.0.0 星期几", this);
    m_lblDate->setStyleSheet(ZTL::Home::Info::dateStyle);
    m_lblDate->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // 组装
    topLayout->addWidget(weatherWidget, 1, Qt::AlignRight);
    topLayout->addWidget(m_lblClock, 0, Qt::AlignCenter);
    topLayout->addWidget(m_lblDate, 1, Qt::AlignLeft);
    // 第二层
    m_lblQuote = new QLabel("愿你千帆阅尽，归来仍是少年", this);
    m_lblQuote->setStyleSheet(ZTL::Home::Info::quoteStyle);
    m_lblQuote->setAlignment(Qt::AlignCenter);
    m_lblQuote->setWordWrap(true);
    // 总组装
    mainLayout->addWidget(topWidget);
    mainLayout->addWidget(m_lblQuote);
    // 启动计时器
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &HomeInfoWidget::onClockTick);
    m_clockTimer->start(1000);
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

void HomeInfoWidget::updateWeather(QString name, QString temp, QString account)
{
    if (!name.isEmpty()) m_lblLocation->setText(name);
    m_lblTemp->setText(temp + "°C");
    m_lblWeather->setText(account);
}

void HomeInfoWidget::updateQuote(const QString &quote)
{
    m_lblQuote->setText(quote);
}

void HomeInfoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    double baseWidth = 1280.0;
    double scale = this->width() / baseWidth;

    if (scale < 0.6) scale = 0.6;
    if (scale > 2.0) scale = 2.0;
    int clockSize = static_cast<int>(110 * scale);
    int locationSize = static_cast<int>(32 * scale);
    int tempSize = static_cast<int>(32 * scale);
    int weatherSize = static_cast<int>(28 * scale);
    int dateSize = static_cast<int>(28 * scale);
    int quoteSize = static_cast<int>(24 * scale);

    int letterSpacing = static_cast<int>(12 * scale);

    m_lblClock->setStyleSheet(QString(ZTL::Home::Info::timeStyleResize).arg(clockSize));

    m_lblLocation->setStyleSheet(QString(ZTL::Home::Info::locationStyleResize).arg(locationSize));

    m_lblTemp->setStyleSheet(QString(ZTL::Home::Info::tempStyleResize).arg(tempSize));

    m_lblWeather->setStyleSheet(QString(ZTL::Home::Info::weatherStyleResize).arg(weatherSize));

    m_lblDate->setStyleSheet(QString(ZTL::Home::Info::dateStyleResize).arg(dateSize));

    m_lblQuote->setStyleSheet(QString(ZTL::Home::Info::quoteStyleResize).arg(quoteSize).arg(letterSpacing));

    if (this->layout()) {
        int horizontalMargin = static_cast<int>(40 * scale);
        int verticalMargin   = static_cast<int>(20 * scale);
        this->layout()->setContentsMargins(horizontalMargin, verticalMargin, horizontalMargin, verticalMargin);
    }
}










