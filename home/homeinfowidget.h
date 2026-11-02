#pragma once
#include <QWidget>
#include <QDateTime>
class QLabel;class QTimer;
class HomeInfoWidget:public QWidget {
    Q_OBJECT
public:
    explicit HomeInfoWidget(QWidget *parent=nullptr);
    void refreshDateTime(const QDateTime &now=QDateTime::currentDateTime());
    void setFlowTime(qreal seconds);
signals:
    void dayChanged();
protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
private:
    QLabel *date,*clock;
    QTimer *timer;
    QDate displayedDay;
};
