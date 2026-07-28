#include "deadlinemapddlcarditem.h"

DeadlineMapDDLCardItem::DeadlineMapDDLCardItem(const DeadlineEntity &ddl, QGraphicsItem *parent):
    QGraphicsObject(parent), m_id(ddl.id), m_title(ddl.title),
    m_dueTimestamp(ddl.dueTimestamp), m_isDone(ddl.isDone),
    m_isIgnore(ddl.isIgnore), m_tags(ddl.tags)
{
    setAcceptHoverEvents(true);

    m_tooltip = new ToolTipItem(this);
    m_tooltip->setText(QString("%1\n%2").
        arg(QDateTime::fromSecsSinceEpoch(m_dueTimestamp).toString("yyyy-MM-dd HH:mm:ss")).
        arg(m_tags.join("|")));

    m_tooltip->setPos(0, 0);
    m_tooltip->setZValue(999);
}
// 返回元件的范围
QRectF DeadlineMapDDLCardItem::boundingRect() const {
    if (m_isFullShow) return QRectF(0, (scenePos().y() > 0) ? (- scenePos().y()) : (- 12.0), 150, 12 + abs(scenePos().y()));
    else return QRectF(0, -2, 30, 4);
}
// paint函数
void DeadlineMapDDLCardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget){
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // 抗锯齿
    painter->setRenderHint(QPainter::Antialiasing);

    if (!m_isFullShow){
        painter->setPen(QPen(QColor("#AAAAAA"), 1));
        painter->setBrush(QBrush(QColor("#AAAAAA")));
        painter->drawRoundedRect(QRectF(0, -2, 30, 4), 2, 2);
        return;
    }

    // 背景圆角矩形
    painter->setPen(QPen(QColor("#AAAAAA"), 1));
    painter->drawRoundedRect(QRectF(0, -12, 150, 24), 4, 4);

    // 画字
    painter->setPen(QColor("#cccccc"));
    QFont font("Microsoft YaHei", 13, QFont::Bold);
    painter->setFont(font);
    QRectF rect(10, -11, 200, 24);
    painter->drawText(rect, Qt::AlignLeft, m_title);

    // 画线
    if (m_isHovered) {
        QPen linePen(QColor("#00CC7A"), 1.5);
        painter->setPen(linePen);
        painter->drawLine(QPoint(0, 0), QPoint(0, (- scenePos().y())));

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#00CC7A"));
        painter->drawEllipse(QPoint(0, (- scenePos().y())), 3, 3); // 3px 半径小圆点
    }





}


QPainterPath DeadlineMapDDLCardItem::shape() const {


    QPainterPath path;
    path.addRoundedRect(QRectF(0, -12, 150, 24), 4, 4);

    return path;
}



void DeadlineMapDDLCardItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    if (m_isFullShow){
        m_isHovered = true;
        m_tooltip->show();
        update();
    }
    QGraphicsItem::hoverEnterEvent(event);
}

void DeadlineMapDDLCardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    if (m_isFullShow){
        m_isHovered = false;
        m_tooltip->hide();
        update();
    }

    QGraphicsItem::hoverLeaveEvent(event);
}









