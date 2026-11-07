#pragma once
#include <QWidget>
#include <QStringList>
class QListWidget;class QComboBox;
class SettingDeadlinePage:public QWidget {
    Q_OBJECT
public:
    explicit SettingDeadlinePage(QWidget *parent=nullptr,bool noteScope=false);
    void resetUI();
private:
    void editTag(bool creating);
    QListWidget *tags;
    QComboBox *advance;
    bool noteScope=false;
    QStringList values() const;
    void applyValues(const QStringList &values);
};
