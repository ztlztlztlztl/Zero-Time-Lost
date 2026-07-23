#ifndef DEADLINEADDWIDGET_H
#define DEADLINEADDWIDGET_H


#include "deadlineaddtagselector.h"
#include "configmanager.h"
#include "datatype.h"


#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTimeEdit>
#include <QDateTime>
#include <QTextEdit>
#include <QRegularExpressionValidator>




class DeadlineAddWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeadlineAddWidget(QWidget *parent = nullptr);

    void awake(bool flag, const DeadlineEntity& deadline);
    // false 是 add true 是 modify



signals:
    void addNewDeadlineRequest(const DeadlineEntity& newDeadline);

private:
    QLabel *m_dueTimeText;
    QDateTimeEdit *m_dueTimeEdit;

    QLabel *m_titleText;
    QLineEdit *m_titleInput;

    QLabel *m_describeText;
    QTextEdit *m_describeInput;

    QLabel *m_tagsText;
    DeadlineAddTagSelector *m_tagsSelect;

    QPushButton *m_saveBtn;











};

#endif // DEADLINEADDWIDGET_H
