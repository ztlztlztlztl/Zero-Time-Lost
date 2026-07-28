#include "deadlinemaptimelineitem.h"







DeadlineMapTimelineItem::DeadlineMapTimelineItem(
    QDateTime baseTime, QRectF baseRect, double pixelsPerMin,QGraphicsItem *parent):
    QGraphicsObject(parent), m_baseRect(baseRect),
    m_basePixelsPerMinute(pixelsPerMin), m_baseTime(baseTime),
    m_zoomFactor(1)
{

}



QRectF DeadlineMapTimelineItem::boundingRect() const {
    return QRectF(m_baseRect.x() * m_zoomFactor, m_baseRect.y(),
                  m_baseRect.width() * m_zoomFactor, m_baseRect.height());
}



void DeadlineMapTimelineItem::paint
    (QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // 开启抗锯齿
    painter->setRenderHint(QPainter::Antialiasing);
    // 局部绘制
    // 获取可视矩形
    QRectF visibleRect;
    if (widget) {
        visibleRect = painter->transform().inverted().mapRect(QRectF(widget->rect()));
    } else {
        visibleRect = option->exposedRect;
    }

    double leftX = visibleRect.left();
    double rightX = visibleRect.right();

    // 画轴
    QPen pen(QColor("#DDDDDD"), 1.5);
    painter->setPen(pen);

    painter->drawLine(QPointF(leftX, 0.0), QPointF(rightX, 0.0));
    // 画刻度
    double currentPPM = m_basePixelsPerMinute * m_zoomFactor;
    QList<int> minutesSteps =
        {15, 60, 180, 360, 1440, 4320, 10080, 43200, 129600, 525600};
    //   15m 1h  3h   6h   1d    3d    7d     1m     3m      1y
    int index = 0;
    for(;index < 9;index++){
        if (minutesSteps[index] * currentPPM >= 60) break;
    }
    double leftMinute  = leftX / currentPPM;
    double rightMinute = rightX / currentPPM;
    int stepMinutes = minutesSteps[index];
    // 左边向下取整，右边向上取整
    int startMinute = std::floor(leftMinute / stepMinutes) * stepMinutes;
    int endMinute   = std::ceil(rightMinute / stepMinutes) * stepMinutes;
    painter->setPen(QPen(QColor("#DDDDDD"), 1)); // 灰色刻度线
    QString strFormat;
    if (index == 0) {            // 15分钟刻度 -> 显示 14:15
        strFormat = "HH:mm";
    } else if (index < 4) {      // 1h / 3h / 6h 刻度 -> 显示 14:00
        strFormat = "HH:mm";
    } else if (index < 7) {      // 1d / 3d / 1w 刻度 -> 显示 05-20 (月-日)
        strFormat = "MM-dd";
    } else {                     // 1m / 3m / 1y 刻度 -> 显示 2026-05
        strFormat = "yyyy-MM";
    }
    QRectF textRect(0, 6, 80, 20);
    for (int min = startMinute; min <= endMinute; min += stepMinutes) {
        double x = min * currentPPM;
        painter->drawLine(QPointF(x, 0), QPointF(x, -10));
        QDateTime tickTime = m_baseTime.addSecs(min * 60);
        QString timeStr = tickTime.toString(strFormat);
        textRect.moveCenter(QPointF(x, 13));
        painter->drawText(textRect, Qt::AlignCenter, timeStr);
    }
    // 画当前时间
    QDateTime now = QDateTime::currentDateTime();
    double nowMinutes = m_baseTime.secsTo(now) / 60.0;
    double nowX = nowMinutes * currentPPM;
    QPen nowPen(QColor("#EEA560"), 2, Qt::SolidLine);
    painter->setPen(nowPen);
    painter->drawLine(QPointF(nowX, -1000), QPointF(nowX, 1000));
    // 画气泡
    if (m_showHover) {
        double x = m_hoverSnappedX;

        QPen linePen(QColor("#007ACC"), 1, Qt::DashLine);
        painter->setPen(linePen);
        painter->drawLine(QPointF(x, 0.0), QPointF(x, -10.0));

        QDateTime hoverTime = m_baseTime.addSecs(m_hoverSnappedMinutes * 60);
        QString timeStr = hoverTime.toString("yyyy-MM-dd HH:mm");

        QFont font = painter->font();
        font.setPixelSize(11);
        painter->setFont(font);

        QFontMetrics fm(font);
        int textWidth = fm.horizontalAdvance(timeStr);
        int paddingH = 8; // 水平内边距
        int paddingV = 4; // 垂直内边距

        double bubbleWidth = textWidth + paddingH * 2;
        double bubbleHeight = fm.height() + paddingV * 2;

        // 气泡矩形，中心水平对齐吸附 X 点
        QRectF bubbleRect(x - 50, -36, 100, 20);

        // 4. 画蓝色圆角矩形背景
        painter->setRenderHint(QPainter::Antialiasing, true); // 气泡开启抗锯齿
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#007ACC")); // 经典蓝色背景
        painter->drawRoundedRect(bubbleRect, 4, 4);

        // 5. 画文字 (白色)
        painter->setPen(QColor("#FFFFFF"));
        painter->drawText(bubbleRect, Qt::AlignCenter, timeStr);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#007ACC"));
        painter->drawEllipse(QPoint(x, 0.0), 3, 3); // 3px 半径小圆点
    }






}

void DeadlineMapTimelineItem::setZoomFactor(double factor){
    prepareGeometryChange();
    m_zoomFactor = factor;
    update();
}

void DeadlineMapTimelineItem::updateHoverPosition(double mouseItemX) {
    double currentPPM = m_basePixelsPerMinute * m_zoomFactor;

    double minutes = mouseItemX / currentPPM;
    int snappedMin = static_cast<int>(std::round(minutes / 5.0) * 5.0);

    if (!m_showHover || m_hoverSnappedMinutes != snappedMin) {
        m_showHover = true;
        m_hoverSnappedMinutes = snappedMin;
        m_hoverSnappedX = snappedMin * currentPPM;

        update();
    }
}

void DeadlineMapTimelineItem::clearHover() {
    if (m_showHover) {
        m_showHover = false;
        update();
    }
}

void DeadlineMapTimelineItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPointF clickPos = event->pos();
        QRectF addBtnRect(m_hoverSnappedX - 50, -36, 100, 20);

        if (addBtnRect.contains(clickPos)) {
            event->accept();
            return;
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

// 2. 鼠标释放事件（真正的“点击完成”时刻）
void DeadlineMapTimelineItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {

        QPointF clickPos = event->pos();
        QRectF addBtnRect(m_hoverSnappedX - 50, -36, 100, 20);

        if (addBtnRect.contains(clickPos)) {
            qDebug() << "[DeadlineMapTimelineItem] 点击了 [add]";
            emit addRequest(m_hoverSnappedMinutes);
        }

        QGraphicsItem::mouseReleaseEvent(event);
    }
}







