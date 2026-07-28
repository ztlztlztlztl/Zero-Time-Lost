#ifndef DEADLINEMAPTIMELINEITEM_H
#define DEADLINEMAPTIMELINEITEM_H

#include <QGraphicsItem>
#include <QWidget>
#include <QDateTime>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <cmath>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QTimer>
#include <QGraphicsSceneMouseEvent>

class DeadlineMapTimelineItem : public QGraphicsObject
{
    Q_OBJECT
public:
    DeadlineMapTimelineItem(QDateTime baseTime, QRectF baseRect, double pixelsPerMin, QGraphicsItem *parent = nullptr);
    // 返回元件的范围
    QRectF boundingRect() const override;
    // paint函数
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;


    // 调整缩放因子
    void setZoomFactor(double factor);

    // 蓝色气泡
    void updateHoverPosition(double mouseItemX);
    void clearHover();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

signals:
    void addRequest(int Minutes);
    void cancelRequest();


private:
    QRectF m_baseRect;
    double m_basePixelsPerMinute;
    QDateTime m_baseTime;

    double m_zoomFactor;
    // 蓝色整 5 分气泡
    bool m_showHover = false; // 是否显示气泡
    double m_hoverSnappedX = 0.0; // 气泡的 X 坐标
    int m_hoverSnappedMinutes = 0; // 气泡的整 5 分钟数




};

#endif // DEADLINEMAPTIMELINEITEM_H
