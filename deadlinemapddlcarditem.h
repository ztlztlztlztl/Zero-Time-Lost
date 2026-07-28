#ifndef DEADLINEMAPDDLCARDITEM_H
#define DEADLINEMAPDDLCARDITEM_H

#include "datatype.h"

#include <QGraphicsItem>
#include <QWidget>
#include <QPainter>
#include <QStyleOptionGraphicsItem>










class ToolTipItem : public QGraphicsItem {
public:
    ToolTipItem(QGraphicsItem *parent = nullptr) : QGraphicsItem(parent) {
        // 防遮挡鼠标
        setAcceptedMouseButtons(Qt::NoButton);
        setAcceptHoverEvents(false);
        hide(); // 默认隐藏
    }

    void setText(const QString &text) {
        m_text = text;
        update();
    }

    QRectF boundingRect() const override {
        return QRectF(-156, -20, 156, 40);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        Q_UNUSED(option);
        Q_UNUSED(widget);

        painter->setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        QRectF rect(-156, -20, 150, 40);
        path.addRoundedRect(rect, 5, 5);
        // 右侧小三角箭头
        path.moveTo(-6, -6);
        path.lineTo(0, 0);
        path.lineTo(-6, 6);
        // 边框
        painter->setPen(QPen(QColor("#BBBBBB"), 1));
        painter->drawPath(path);
        // 文字
        painter->setPen(Qt::white);
        QFont font("Microsoft YaHei", 9);
        painter->setFont(font);
        QRectF textRect = rect.adjusted(0, 0, -6, 0);
        painter->drawText(textRect, Qt::AlignVCenter|Qt::AlignRight, m_text);
    }

private:
    QString m_text;
};














class DeadlineMapDDLCardItem : public QGraphicsObject
{
    Q_OBJECT
public:
    DeadlineMapDDLCardItem(const DeadlineEntity &ddl, QGraphicsItem *parent = nullptr);
    // 返回元件的范围
    QRectF boundingRect() const override;
    // paint函数
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    // 鼠标检测区域
    QPainterPath shape() const override;
    // 控制样式
    void setStyle(bool flag){
        m_isFullShow = flag;
    }


    // DDL信息
    int m_id;
    QString m_title;
    qint64 m_dueTimestamp;
    QList<QString> m_tags;
    bool m_isDone;
    bool m_isIgnore;




protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:

    // 指针悬浮
    bool m_isHovered = false;
    ToolTipItem *m_tooltip;
    // 样式控制
    bool m_isFullShow = true;


};

#endif // DEADLINEMAPDDLCARDITEM_H
