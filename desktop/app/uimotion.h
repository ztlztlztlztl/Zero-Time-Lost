#pragma once
#include <QObject>
#include <QPointer>
#include <QPropertyAnimation>
#include <QWidget>

class UiMotion : public QObject {
  public:
    static UiMotion *install();
    static bool enabled();
    void reveal(QWidget *page);
  protected:
    bool eventFilter(QObject *object, QEvent *event) override;
  private:
    explicit UiMotion(QObject *parent);
    void settle();
    QPointer<QPropertyAnimation> fade;
    QPointer<QWidget> fadingPage;
};
