#pragma once
#include <QPushButton>
#include <QPainter>

// Shared edge treatment; each module keeps its own colours and QSS geometry.
class AccentButton : public QPushButton {
public:
    AccentButton(const QString &text,const QColor &accent,const QColor &disabled,
                 bool plainClose,QWidget *parent=nullptr)
        :QPushButton(text,parent),m_accent(accent),m_disabled(disabled),plainClose(plainClose){
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    }
protected:
    void paintEvent(QPaintEvent *event) override {
        QPushButton::paintEvent(event);
        if(plainClose&&objectName()=="popupClose")return;
        const int length=qMin(height()-4,qRound(QFontMetrics(font()).tightBoundingRect(QStringLiteral("字")).height()*1.2));
        const int stroke=isEnabled()&&(underMouse()||isDown()||hasFocus())?2:1;
        QPainter painter(this);
        const auto color=isEnabled()?m_accent:m_disabled;
        painter.fillRect(0,(height()-length)/2,stroke,length,color);
        painter.fillRect(width()-stroke,(height()-length)/2,stroke,length,color);
    }
private:
    QColor m_accent,m_disabled;
    bool plainClose;
};
