#pragma once
#include "settingcontrols.h"
#include "mailaccountmanager.h"
class SettingMailPage:public QWidget {
    Q_OBJECT
public:
    explicit SettingMailPage(QWidget *parent=nullptr);
    void resetUI();
private:
    void editAccount(bool creating);
    NoWheelComboBox *polling,*initial,*earlier;
    QLineEdit *storage;
    QListWidget *accounts;
};
