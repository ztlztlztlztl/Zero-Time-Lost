#include "deadlinetimemapview.h"

DeadlineTimeMapView::DeadlineTimeMapView(QWidget *parent):
    QGraphicsView(parent), m_baseRect(QRectF(-8000, -500, 640000, 1000)),
    m_basePixelsPerMinute(0.06), m_baseTime(QDateTime(QDate::currentDate(), QTime(0, 0, 0))),
    m_zoomFactor(1)
{


    m_scene = new QGraphicsScene(this);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setScene(m_scene);
    m_scene->setSceneRect(m_baseRect);

    // 禁用滚动条
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 开始鼠标拖动
    setDragMode(QGraphicsView::ScrollHandDrag);
    // 开启鼠标追踪
    setMouseTracking(true);
    // 以鼠标当前位置为缩放中心
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);

    // 优化性能
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);


    // 初始化时间轴
    // DeadlineMapTimelineItem(QDateTime baseTime, QRect baseRect, double pixelsPerMin, QGraphicsItem *parent = nullptr);
    m_timeLine = new DeadlineMapTimelineItem(m_baseTime, m_baseRect, m_basePixelsPerMinute);
    m_scene->addItem(m_timeLine);


    // 将一个坐标初始居中
    centerOn(0, 0);

    QTimer *OrangeLineTimer = new QTimer(this);
    connect(OrangeLineTimer, &QTimer::timeout, this, [this]() {
        if (m_timeLine) {
            m_timeLine->update();
        }
    });
    OrangeLineTimer->start(15000);

}


void DeadlineTimeMapView::wheelEvent(QWheelEvent *event) {
    int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }

    double scaleFactor = (delta > 0) ? 1.15 : (1.0 / 1.15);
    double newZoom = m_zoomFactor * scaleFactor;
    if (newZoom < 0.01 || newZoom > 200.0) {
        event->accept();
        return;
    }
    // 不能放到填不满当前窗口
    double newPPM = m_basePixelsPerMinute * newZoom;
    double minPPM = viewport()->width() / 1051200.0;
    double maxPPM = 30;
    newPPM = qBound(minPPM, newPPM, maxPPM);

    if (qFuzzyCompare(newPPM, m_zoomFactor * m_basePixelsPerMinute)) {
        event->accept();
        return;
    }

    double actualZoom = newPPM / m_basePixelsPerMinute;
    double actualScaleFactor = actualZoom / m_zoomFactor;

    m_zoomFactor = actualZoom;

    QPointF mouseScenePos = mapToScene(event->position().toPoint());
    double mouseX = mouseScenePos.x();

    if (m_timeLine) {
        m_timeLine->setZoomFactor(m_zoomFactor);
    }

   m_scene->setSceneRect(m_baseRect.x() * m_zoomFactor,
                          m_baseRect.y(),
                          m_baseRect.width() * m_zoomFactor,
                          m_baseRect.height());

    double newMouseX = mouseX * actualScaleFactor;
    double deltaX = newMouseX - mouseX;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + deltaX);

    event->accept();
}

void DeadlineTimeMapView::mouseMoveEvent(QMouseEvent *event) {
    QGraphicsView::mouseMoveEvent(event); // 保留拖拽等默认逻辑

    if (m_timeLine) {
        // 将鼠标在 View 中的物理像素点，映射为场景/Item 局部坐标点
        QPointF scenePos = mapToScene(event->pos());
        QPointF itemPos = m_timeLine->mapFromScene(scenePos);

        // 传递给 Item 刷新悬停气泡
        m_timeLine->updateHoverPosition(itemPos.x());
    }
}

// 2. 鼠标离开 View 视口时，隐藏气泡
void DeadlineTimeMapView::leaveEvent(QEvent *event) {
    QGraphicsView::leaveEvent(event);

    if (m_timeLine) {
        m_timeLine->clearHover();
    }
}

















