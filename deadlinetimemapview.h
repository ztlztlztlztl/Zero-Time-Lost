#ifndef DEADLINETIMEMAPVIEW_H
#define DEADLINETIMEMAPVIEW_H


#include "deadlinemaptimelineitem.h"
#include "deadlinemapddlcarditem.h"
#include "datatype.h"
#include "deadlinemanager.h"



#include <QObject>
#include <QTimer>
#include <QGraphicsView>
#include <QWheelEvent>
#include <QScrollBar>
#include <QOpenGLWidget>
#include <QPointer>

class DeadlineTimeMapView : public QGraphicsView {
    Q_OBJECT
public:
    DeadlineTimeMapView(QWidget *parent = nullptr);

    void addNewDDL(const DeadlineEntity& newDDL);

protected:
    void wheelEvent(QWheelEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

signals:
    void addRequest(const QDateTime& initTime);


private:
    QGraphicsScene *m_scene;

    // 元件
    DeadlineMapTimelineItem *m_timeLine;
    QList<QPointer<DeadlineMapDDLCardItem>> m_ddlCards;

    // 基本属性
    QRectF m_baseRect;
    double m_basePixelsPerMinute;
    QDateTime m_baseTime;

    double m_zoomFactor;

    // 辅助函数
    void updateCardsX();
    void updateCardsY();
    QTimer *m_layoutTimer;

};

#endif // DEADLINETIMEMAPVIEW_H
