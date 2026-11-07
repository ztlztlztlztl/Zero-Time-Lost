#pragma once
#include <QWidget>
#include <QVariantAnimation>
class QAbstractButton;
class QLabel;

class NavigationRail : public QWidget {
    Q_OBJECT
  public:
    explicit NavigationRail(QWidget *parent = nullptr);
    void setCurrent(int page);
    void setCompact(bool compact);
    void setCloudStatus(const QString &message = {});
    int currentPage() const { return current; }
  signals:
    void pageRequested(int page);
    void lockRequested();
  protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void hideEvent(QHideEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;
  private:
    void arrange();
    void settle();
    QList<QAbstractButton *> buttons;
    QAbstractButton *lockButton;
    QLabel *brand, *cloud;
    QVariantAnimation motion;
    qreal selectionY = 108;
    int current = 0;
    bool compact = false;
    QString cloudMessage;
};
