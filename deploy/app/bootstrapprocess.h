#pragma once
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QTimer>

class BootstrapProcess : public QObject {
    Q_OBJECT
  public:
    explicit BootstrapProcess(QObject *parent = nullptr);
    bool busy() const { return process.state() != QProcess::NotRunning; }
    void start(QJsonObject request);
  signals:
    void progress(const QString &message);
    void completed(const QJsonObject &result, const QString &error);

  private:
    void consume();
    QProcess process;
    QTimer timeout;
    QByteArray buffer;
    QJsonObject result;
    QString failure;
    bool terminal = false;
};
