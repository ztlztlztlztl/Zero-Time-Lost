#include "deadlinemaptimelineitem.h"







DeadlineMapTimelineItem::DeadlineMapTimelineItem(QDateTime baseTime, double pixelsPerMin, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_baseTime(baseTime), m_pixelsPerMinute(pixelsPerMin) {}


void DeadlineMapTimelineItem::setPixelsPerMinute(double pxPerMin) {
    m_pixelsPerMinute = pxPerMin;
    update(); // 重绘
}

QRectF DeadlineMapTimelineItem::boundingRect() const {
    // 假设时间轴高度为 50px，宽度设置足够大（如 1,000,000px）
    return QRectF(-500, -250, 3000, 500);
}

void DeadlineMapTimelineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget){
    Q_UNUSED(widget);

    QRectF exposed = option->exposedRect;

    painter->fillRect(exposed, QColor("transparent"));
    painter->setPen(QPen(QColor("#cccccc"), 1));
    painter->drawLine(exposed.left(), 0, exposed.right(), 0);

    // 3. 动态确定刻度间隔（示例：若1小时占用的像素比 40px 大，就按“小时”画，否则按“天”画）
    QList<int> stepMinutes = {15, 30, 60, 120, 360, 1440, 10080, 43200, 129600, 525600};
    int index = 0; // 默认 5 分钟为最低刻度
    for (;index < 10;index++){
        if (stepMinutes[index] * m_pixelsPerMinute >= 60) break;
    }

    // 4. 计算当前可视区域起始和结束的时间点
    int startMin = std::floor(exposed.left() / (stepMinutes[index] * m_pixelsPerMinute)) * stepMinutes[index];
    int endMin = std::ceil(exposed.right() / (stepMinutes[index] * m_pixelsPerMinute)) * stepMinutes[index];

    // 5. 循环绘制可见区域内的刻度线和文字
    painter->setPen(QColor("#cccccc"));
    for (int min = startMin; min <= endMin; min += stepMinutes[index]) {
        double x = min * m_pixelsPerMinute;
        QDateTime tickTime = m_baseTime.addSecs(min * 60);

        // 画主刻度线（长线）
        painter->drawLine(QPointF(x, 0), QPointF(x, -20));

        // 绘制时间字符串
        QString textFormat;
        if (index < 5) textFormat = "HH:mm";
        else if (index == 9) textFormat = "yyyy-MM";
        else textFormat = "MM-dd";
        painter->drawText(QRectF(x - 40, -45, 80, 20), Qt::AlignCenter, tickTime.toString(textFormat));
    }
    // 蓝色小球
    if (m_showIndicator) { // 只有鼠标在视口内时才绘制
        double currentMinutes = m_hoverX / m_pixelsPerMinute;

        int roundedMinutes;
        if (index < 5) roundedMinutes = std::round(currentMinutes / 5.0) * 5;
        else if (index > 6) roundedMinutes = std::round(currentMinutes / 1440.0) * 1440.0;
        else roundedMinutes = std::round(currentMinutes / 60.0) * 60.0;

        double ballX = roundedMinutes * m_pixelsPerMinute;

        QDateTime targetTime = m_baseTime.addSecs(roundedMinutes * 60);
        QString timeStr;
        if (index < 5) timeStr = targetTime.toString("HH:mm"); // 格式如 "14:25"
        else if (index > 6) timeStr = targetTime.toString("MM-dd");
        else timeStr = targetTime.toString("dd HH");


        painter->setRenderHint(QPainter::Antialiasing); // 开启抗锯齿，让圆球和圆角矩形更平滑


        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#1890ff")); // 蓝色小球
        painter->drawEllipse(QPointF(ballX, 0.0), 5.0, 5.0); // 半径为 5 的小圆

        // 3. 绘制气泡背景（带圆角的矩形）
        QRectF bubbleRect(ballX - 25, - 30, 50, 22); // 位于小球上方
        painter->setBrush(QColor(24, 144, 255, 220));      // 半透明蓝色背景
        painter->drawRoundedRect(bubbleRect, 4, 4);         // 4px 圆角

        // 4. 绘制气泡里的时间文字
        painter->setPen(Qt::white);
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(bubbleRect, Qt::AlignCenter, timeStr);
    }

    painter->setRenderHint(QPainter::Antialiasing); // 开启抗锯齿

    // 1. 计算当前时间的 X 坐标
    QDateTime now = QDateTime::currentDateTime();
    qint64 secondsFromBase = m_baseTime.secsTo(now);
    double nowX = (secondsFromBase / 60.0) * m_pixelsPerMinute;

    // 2. 绘制贯穿时间轴的当前时间红色线
    QPen nowPen(QColor("#ff4d4f"), 2, Qt::SolidLine); // 醒目的红色
    painter->setPen(nowPen);
    painter->drawLine(QPointF(nowX, 0), QPointF(nowX, boundingRect().bottom()));

    // 3. 绘制顶部当前时间小方块/气泡
    double blockWidth = 50.0;  // 方块宽度
    double blockHeight = 20.0; // 方块高度

    // 居中在 nowX 上
    QRectF nowRect(nowX - blockWidth / 2.0, 0.0, blockWidth, blockHeight);

    // 画小方块背景（带圆角矩形）
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor("#ff4d4f")); // 红色背景
    painter->drawRoundedRect(nowRect, 3, 3);

    // 4. 在小方块里写上当前时间 (如 "21:28" 或 "NOW")
    painter->setPen(Qt::white);
    QFont font = painter->font();
    font.setPointSize(8);
    font.setBold(true);
    painter->setFont(font);

    // 显示格式，例如：HH:mm
    painter->drawText(nowRect, Qt::AlignCenter, now.toString("HH:mm"));





}
