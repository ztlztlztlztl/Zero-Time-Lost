#pragma once
#include <QPushButton>
#include <QPainter>

// QSS owns geometry; short colored strokes echo the page's title and navigation.
class AccentButton : public QPushButton {
public:
    AccentButton(const QString &text,const QColor &accent,const QColor &disabled,
                 bool plainClose,QWidget *parent=nullptr)
        :QPushButton(text,parent),m_accent(accent),m_disabled(disabled),m_plainClose(plainClose){
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    }
protected:
    void paintEvent(QPaintEvent *event) override {
        QPushButton::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if(!(m_plainClose&&objectName()=="popupClose")){
            auto stroke=isEnabled()?m_accent:m_disabled;
            if(isEnabled())stroke.setAlpha(underMouse()||hasFocus()||isDown()?240:155);
            painter.setPen(QPen(stroke,1.5,Qt::SolidLine,Qt::RoundCap));
            const qreal y=height()/2.;
            painter.drawLine(QPointF(5,y-5),QPointF(5,y+5));
            painter.drawLine(QPointF(width()-6,y-5),QPointF(width()-6,y+5));
        }
        if(!hasFocus()||!isEnabled())return;
        painter.setPen(QPen(m_accent,1));painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect().adjusted(3,3,-4,-4),3,3);
    }
private:
    QColor m_accent,m_disabled;
    bool m_plainClose;
};
