#pragma once
#include <QWidget>
class SettingHomePage;class SettingMailPage;class SettingDeadlinePage;
class SettingWidget:public QWidget {
    Q_OBJECT
public:
    explicit SettingWidget(QWidget *parent=nullptr);
    void resetUI();
    void setSecretInitialized(bool initialized);
signals:
    void generalSettingsChanged();
    void noteAction(const QString &action);
    void saveStatus(const QString &message);
protected:
    void showEvent(QShowEvent *event) override;
private:
    SettingHomePage *m_homePage;
    SettingMailPage *m_mailPage;
    SettingDeadlinePage *m_deadlinePage;
};
