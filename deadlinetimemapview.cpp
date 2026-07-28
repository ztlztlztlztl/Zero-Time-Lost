#include "deadlinetimemapview.h"

DeadlineTimeMapView::DeadlineTimeMapView(QWidget *parent):
    QGraphicsView(parent), m_baseRect(QRectF(-8000, -1000, 640000, 2000)),
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
    m_timeLine = new DeadlineMapTimelineItem(m_baseTime, m_baseRect, m_basePixelsPerMinute);
    m_scene->addItem(m_timeLine);
    QList<DeadlineEntity> ddls = DeadlineManager::getInstance()->initFutureList(QDateTime::currentSecsSinceEpoch());
    for (const DeadlineEntity& ddl:ddls){
        QPointer<DeadlineMapDDLCardItem> ddlCard = new DeadlineMapDDLCardItem(ddl);
        m_scene->addItem(ddlCard);
        ddlCard->setPos(0, -100);
        m_ddlCards.append(ddlCard);
    }
    updateCardsX();
    updateCardsY();



    // 将一个坐标初始居中
    centerOn(0, 0);

    QTimer *OrangeLineTimer = new QTimer(this);
    connect(OrangeLineTimer, &QTimer::timeout, this, [this]() {
        if (m_timeLine) {
            m_timeLine->update();
        }
    });
    OrangeLineTimer->start(15000);

    m_layoutTimer = new QTimer(this);
    m_layoutTimer->setSingleShot(true); // 极其关键！只触发一次
    connect(m_layoutTimer, &QTimer::timeout, this, &DeadlineTimeMapView::updateCardsY);

    connect(m_timeLine, &DeadlineMapTimelineItem::addRequest, this, [=](int Minutes){
        QDateTime initTime = m_baseTime.addSecs(Minutes * 60);
        emit addRequest(initTime);
    });




}


void DeadlineTimeMapView::addNewDDL(const DeadlineEntity& newDDL){
    QPointer<DeadlineMapDDLCardItem> ddlCard = new DeadlineMapDDLCardItem(newDDL);
    m_scene->addItem(ddlCard);
    ddlCard->setPos(0, -100);
    auto it = std::lower_bound(m_ddlCards.begin(), m_ddlCards.end(), ddlCard,
                               [](const QPointer<DeadlineMapDDLCardItem>& a, const DeadlineMapDDLCardItem* b) {
                                   return a->m_dueTimestamp < b->m_dueTimestamp;
                               });
    m_ddlCards.insert(it, ddlCard);
    updateCardsX();
    updateCardsY();
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

    updateCardsX();
    m_layoutTimer->start(150);
    bool flag = m_basePixelsPerMinute * m_zoomFactor < 0.005953;
    for (auto ddl:m_ddlCards){
        ddl->setStyle(!flag);
    }


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

void DeadlineTimeMapView::updateCardsX(){
    m_ddlCards.erase(
        std::remove_if(m_ddlCards.begin(), m_ddlCards.end(),
                       [](const QPointer<DeadlineMapDDLCardItem> &ptr) { return ptr.isNull(); }),
        m_ddlCards.end()
        );
    for (auto ddl:m_ddlCards){
        double deltaMins = (ddl->m_dueTimestamp - m_baseTime.toSecsSinceEpoch()) / 60.0;
        double deltaPx = deltaMins * m_basePixelsPerMinute * m_zoomFactor;
        ddl->setPos(deltaPx, ddl->pos().y());
    }
}

void DeadlineTimeMapView::updateCardsY(){
    m_ddlCards.erase(
        std::remove_if(m_ddlCards.begin(), m_ddlCards.end(),
                       [](const QPointer<DeadlineMapDDLCardItem> &ptr) { return ptr.isNull(); }),
        m_ddlCards.end()
        );

    double startPx = 60.0;
    QList<double> upMaxRightPx;
    QList<double> downMaxRightPx;
    double hSpacing; // 卡片起点横向最小间隔
    double vSpacing; // 竖向轨道间间隔
    double hWidth;
    if(m_basePixelsPerMinute * m_zoomFactor < 0.005953){
        // 色条
        hSpacing = 2.0;
        vSpacing = 6.0;
        hWidth = 30.0;
    }
    else{
        // 标题条
        hSpacing = 10.0;
        vSpacing = 30.0;
        hWidth = 150.0;
    }
    int usedTrackNumber = 0;
    int count = 0;
    for (auto ddl:m_ddlCards) {
        count++;
        double cardLeftX = ddl->pos().x();

        int assignedTrack = 0;
        bool trackFound = false;

        for (int i = 0; i < usedTrackNumber; ++i) {
            double& rightEdge = (i % 2 == 0) ? upMaxRightPx[i / 2] : downMaxRightPx[i / 2];
            if (rightEdge + hSpacing < cardLeftX) {
                assignedTrack = i;
                rightEdge = cardLeftX + hWidth;
                trackFound = true;
                break;
            }
        }

        if (!trackFound) {
            assignedTrack = usedTrackNumber;
            if (usedTrackNumber % 2 == 0) upMaxRightPx.append(cardLeftX + hWidth);
            else downMaxRightPx.append(cardLeftX + hWidth);
            usedTrackNumber++;
        }

        double y = ((assignedTrack % 2 == 0) ? -1 : 1) * (startPx + vSpacing * int(assignedTrack / 2));
        ddl->setPos(cardLeftX, y);
    }
}













