#ifndef DEADLINEADDTAGSELECTOR_H
#define DEADLINEADDTAGSELECTOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QRegularExpressionValidator>

class DeadlineAddTagSelector : public QWidget
{
    Q_OBJECT
public:
    explicit DeadlineAddTagSelector(QWidget *parent = nullptr);

    void loadData(const QList<QString>& commonTags,
                  const QList<QString>& selectedTags);

    const QList<QString>& text();


signals:











private:

    QList<QPushButton*> m_tagBtns;

    QLineEdit *m_customTagInput;
    QPushButton *m_customTagBtn;
    QListWidget *m_commonTagList;

    QList<QString> m_selectTags;

    void refreshBtns();




};

#endif // DEADLINEADDTAGSELECTOR_H
