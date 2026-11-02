#pragma once
#include <QWidget>
class QListWidget;class QComboBox;
class SettingDeadlinePage:public QWidget {
    Q_OBJECT
public:
    explicit SettingDeadlinePage(QWidget *parent=nullptr);
    void resetUI();
private:
    void editTag(bool creating);
    QListWidget *tags;
    QComboBox *advance;
};
