#include "deadlinetimemapview.h"

DeadlineTimeMapTimelineView::DeadlineTimeMapTimelineView(QWidget *parent) : QGraphicsView(parent) {
    // 1. 创建场景并设置统一的场景大小
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    m_scene->setSceneRect(-500, -250, 3000, 500);

    // 2. 基础配置：基准时间设为 2026-01-01 00:00:00
    m_baseTime = QDateTime(QDate(2026, 1, 1), QTime(0, 0, 0));
    m_pixelsPerMinute = 0.6;

    // 3. 添加时间轴图元
    m_timelineItem = new DeadlineMapTimelineItem(m_baseTime, m_pixelsPerMinute);
    m_scene->addItem(m_timelineItem);

    // 4. 交互与滚动条配置
    setDragMode(QGraphicsView::ScrollHandDrag);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    setMouseTracking(true);

    centerOn(0, 0);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        if (m_timelineItem) {
            m_timelineItem->update(); // 触发重绘，当前时间小方块会自动向前移动！
        }
    });
    timer->start(60000); // 1000毫秒 = 1秒
}

// 🚀 重写滚轮事件：实现以鼠标指针为中心的时间轴缩放
void DeadlineTimeMapTimelineView::wheelEvent(QWheelEvent *event){
    // 1. 获取鼠标在场景（Scene）中的绝对 X 坐标
    QPointF mouseScenePos = mapToScene(event->position().toPoint());
    double mouseX = mouseScenePos.x();

    // 2. 根据滚轮方向决定放大还是缩小
    double zoomFactor = (event->angleDelta().y() > 0) ? 1.25 : 0.8;

    // 限制缩放范围（防止无限缩小导致像素溢出，或无限放大导致精度丢失）
    double newPixelsPerMin = m_pixelsPerMinute * zoomFactor;
    if (newPixelsPerMin < 0.0002 || newPixelsPerMin > 50.0) {
        return;
    }

    // 3. 更新比例尺
    double scaleRatio = newPixelsPerMin / m_pixelsPerMinute;
    m_pixelsPerMinute = newPixelsPerMin;
    m_timelineItem->setPixelsPerMinute(m_pixelsPerMinute);

    // 4. 关键几何计算：调整水平滚动条，保持鼠标下方指向的时间点在屏幕上的位置不移动
    double newMouseX = mouseX * scaleRatio;
    double deltaX = newMouseX - mouseX;

    // 滚动水平条以平移视图
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + deltaX);












    event->accept();
}


void DeadlineTimeMapTimelineView::mouseMoveEvent(QMouseEvent *event) {
    QGraphicsView::mouseMoveEvent(event); // 保持原有的拖拽响应

    // 1. 将鼠标的屏幕坐标转为 Scene 逻辑坐标
    QPointF scenePos = mapToScene(event->pos());

    // 2. 将坐标通知 TimelineItem 重新计算并触发 update()
    if (m_timelineItem) {
        m_timelineItem->updateHoverPosition(scenePos.x());
    }
}

// 鼠标离开视口时隐藏小球
void DeadlineTimeMapTimelineView::leaveEvent(QEvent *event) {
    QGraphicsView::leaveEvent(event);
    if (m_timelineItem) {
        m_timelineItem->setIndicatorVisible(false);
    }
}




















