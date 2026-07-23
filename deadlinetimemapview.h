#ifndef DEADLINETIMEMAPVIEW_H
#define DEADLINETIMEMAPVIEW_H


#include "deadlinemaptimelineitem.h"



#include <QObject>
#include <QTimer>
#include <QGraphicsView>
#include <QWheelEvent>
#include <QScrollBar>

class DeadlineTimeMapTimelineView : public QGraphicsView {
    Q_OBJECT
public:
    DeadlineTimeMapTimelineView(QWidget *parent = nullptr);

protected:
    // 🚀 重写滚轮事件：实现以鼠标指针为中心的时间轴缩放
    void wheelEvent(QWheelEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QGraphicsScene *m_scene = nullptr;
    DeadlineMapTimelineItem *m_timelineItem = nullptr;
    QDateTime m_baseTime;
    double m_pixelsPerMinute;
};

#endif // DEADLINETIMEMAPVIEW_H
