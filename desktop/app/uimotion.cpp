#include "uimotion.h"
#include <QApplication>
#include <QDynamicPropertyChangeEvent>
#include <QGraphicsOpacityEffect>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

UiMotion::UiMotion(QObject *parent) : QObject(parent) { qApp->installEventFilter(this); }
UiMotion *UiMotion::install() {
    static QPointer<UiMotion> instance;
    if (!instance) instance = new UiMotion(qApp);
    return instance;
}
bool UiMotion::enabled() {
    if (!qApp || (qApp->property("ztlAnimation").isValid() && !qApp->property("ztlAnimation").toBool()))
        return false;
#ifdef Q_OS_WIN
    BOOL animate = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0) && !animate) return false;
#endif
    return true;
}
void UiMotion::settle() {
    if (fade) { fade->stop(); fade->deleteLater(); fade = nullptr; }
    if (fadingPage) { fadingPage->setGraphicsEffect(nullptr); fadingPage = nullptr; }
}
void UiMotion::reveal(QWidget *page) {
    settle();
    // OpenGL Home never participates in a graphics-effect render pass.
    if (!enabled() || !page || page->objectName() == "homePage" || !page->isVisible()) return;
    auto effect = new QGraphicsOpacityEffect(page);
    effect->setOpacity(.86);
    page->setGraphicsEffect(effect);
    fadingPage = page;
    auto animation = new QPropertyAnimation(effect, "opacity", this);
    fade = animation;
    animation->setStartValue(.86); animation->setEndValue(1.0);
    animation->setDuration(160); animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QPropertyAnimation::finished, this, [this] { settle(); });
    animation->start();
}
bool UiMotion::eventFilter(QObject *object, QEvent *event) {
    if (object == qApp && event->type() == QEvent::DynamicPropertyChange &&
        static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == "ztlAnimation" && !enabled())
        settle();
    if (object == fadingPage && event->type() == QEvent::Hide) settle();
    return QObject::eventFilter(object, event);
}
