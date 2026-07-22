#ifndef DEADLINEADDWIDGET_H
#define DEADLINEADDWIDGET_H

#include <QWidget>
#include <QLabel>

class DeadlineAddWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeadlineAddWidget(QWidget *parent = nullptr);

signals:

private:
    // int id; // ID
    // QString title; // 标题
    // QString describe; // 详细备注
    // qint64 dueTimestamp; // 截止时间戳 (秒级),最小间隔为分
    // QList<QString> tags; // DDL的tag
    // bool isDone; // 是否已完成
    // bool isIgnore; // 是否置灰
    QLabel *m_titleText;
    QLabel *m_titleInput;

    QLabel *m_describeText;
    QLabel *m_describeInput;

    QLabel *m_tagsText;
    QLabel *m_titleInput;












};

#endif // DEADLINEADDWIDGET_H
