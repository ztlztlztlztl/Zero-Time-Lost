#pragma once
#include <QWidget>
#include "configmanager.h"
class QLabel;class QListWidget;
class SettingHomePage:public QWidget {
    Q_OBJECT
public:
    explicit SettingHomePage(QWidget *parent=nullptr);
    void resetUI();
private:
    void editCity(bool creating);
    QLabel *current;
    QListWidget *cities;
};
