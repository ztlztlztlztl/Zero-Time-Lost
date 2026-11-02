#pragma once
#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <functional>
class BusinessClock : public QObject {
    Q_OBJECT
public:
    explicit BusinessClock(QObject *parent = nullptr);
    static qint64 now() { return QDateTime::currentSecsSinceEpoch(); }
    void every(const QString &id, int seconds, std::function<void()> action, bool immediate = false);
    void remove(const QString &id) { jobs.remove(id); }
    void stop() { timer.stop(); jobs.clear(); }
    void poll();
private:
    struct Job { int seconds; qint64 next; std::function<void()> action; };
    QHash<QString, Job> jobs;
    QTimer timer;
    qint64 last = 0;
};
