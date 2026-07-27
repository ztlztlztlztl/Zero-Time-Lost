#ifndef DEADLINETIMEMAPVIEW_H
#define DEADLINETIMEMAPVIEW_H


#include "deadlinemaptimelineitem.h"



#include <QObject>
#include <QTimer>
#include <QGraphicsView>
#include <QWheelEvent>
#include <QScrollBar>
#include <QOpenGLWidget>

class DeadlineTimeMapView : public QGraphicsView {
    Q_OBJECT
public:
    DeadlineTimeMapView(QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;


private:
    QGraphicsScene *m_scene;

    // 元件
    DeadlineMapTimelineItem *m_timeLine;

    // 基本属性
    QRectF m_baseRect;
    double m_basePixelsPerMinute;
    QDateTime m_baseTime;

    double m_zoomFactor;

};

#endif // DEADLINETIMEMAPVIEW_H
