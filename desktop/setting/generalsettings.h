#pragma once
#include <QWidget>
#include <QTimer>
class QSettings;
class GeneralSettings:public QWidget {
    Q_OBJECT
public:
    enum Section {Home,Mail,DDL,Note,App};
    explicit GeneralSettings(Section section=App,QWidget *parent=nullptr);
signals:
    void applied();
    void noteAction(const QString &action);
    void saveStatus(const QString &message);
private:
    void syncSettings(QSettings &settings);
    QTimer retry;
};
