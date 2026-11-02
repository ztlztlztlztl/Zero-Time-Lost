#pragma once
#include <QObject>
#include <QJsonObject>
#include <functional>
#include <QSet>
class QProcess;
class ProcessWorker : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(const QJsonObject &, const QString &)>;
    explicit ProcessWorker(QObject *parent = nullptr) : QObject(parent) {}
    ~ProcessWorker() override;
    void run(const QJsonObject &request, Callback callback);
    void stop();
private:
    QSet<QProcess*> running;
};
