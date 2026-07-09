#ifndef HOMEWEATHERSERVICE_H
#define HOMEWEATHERSERVICE_H

#include "constants.h"
#include "configmanager.h"
#include "cachemanager.h"

#include <QObject>
#include <QTime>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTimer>

class homeWeatherService : public QObject
{
    Q_OBJECT
public:
    explicit homeWeatherService(QObject *parent = nullptr);
    void updateWeather();

signals:
    void updateWeatherRequest(QString name, QString temp, QString account);

private slots:

private:
    bool useWeatherAPI(QString code);

    QTimer *m_timer;  // 🚀 计时器指针
    int m_count = 0;







};

#endif // HOMEWEATHERSERVICE_H
