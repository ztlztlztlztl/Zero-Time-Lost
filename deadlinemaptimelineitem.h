#ifndef DEADLINEMAPTIMELINEITEM_H
#define DEADLINEMAPTIMELINEITEM_H

#include <QGraphicsItem>
#include <QWidget>
#include <QDateTime>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <cmath>

class DeadlineMapTimelineItem : public QGraphicsItem
{
public:
    DeadlineMapTimelineItem(QDateTime baseTime, double pixelsPerMin, QGraphicsItem *parent = nullptr);

    void setPixelsPerMinute(double pxPerMin);

    QRectF boundingRect() const override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;


    // 🚀 更新鼠标 X 坐标，并开启显示
    void updateHoverPosition(double mouseX) {
        m_hoverX = mouseX;
        m_showIndicator = true;
        update();
    }

    // 🚀 隐藏指示器
    void setIndicatorVisible(bool visible) {
        m_showIndicator = visible;
        update();
    }


private:
    QDateTime m_baseTime;       // 零点基准时间
    double m_pixelsPerMinute;   // 比例尺：每分对应的像素数

    bool m_showIndicator = false; // 默认为 false（初始不显示小球）
    double m_hoverX = 0.0;        // 记录鼠标的 X 坐标
};

#endif // DEADLINEMAPTIMELINEITEM_H
