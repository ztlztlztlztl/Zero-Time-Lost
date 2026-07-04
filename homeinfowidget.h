#ifndef HOMEINFOWIDGET_H
#define HOMEINFOWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QDateTime>
#include <QLocale>

class HomeInfoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HomeInfoWidget(QWidget *parent = nullptr);
    ~HomeInfoWidget();
    // 天气信息更新函数
    void updateWeather(const QString &temp, const QString &status);
    // 每日一言更新函数
    void updateQuote(const QString &quote);
protected:
    // 捕获窗口大小改变事件
    void resizeEvent(QResizeEvent *event) override;
private slots:
    // 刷新数字时间
    void onClockTick();
private:
    QLabel *m_lblTemp; // 左侧：温度
    QLabel *m_lblWeather; // 左侧：天气状态
    QLabel *m_lblClock; // 中间：数字时钟
    QLabel *m_lblDate; // 右侧：日期与节日
    QLabel *m_lblQuote; // 右侧：每日一言
    QTimer *m_clockTimer; // 计时器
    void refreshDateTime(); // 把当前时间、日期、节日格式化成字符串填入
};

#endif // HOMEINFOWIDGET_H
