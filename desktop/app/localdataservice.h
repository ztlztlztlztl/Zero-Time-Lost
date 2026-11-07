#pragma once
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <atomic>
#include <memory>
class QThread;
class QProgressDialog;
class QWidget;

struct StorageReport {
    QMap<QString, qint64> directories;
    qint64 rootBytes = 0, total = 0, files = 0, skippedLinks = 0, unreadable = 0;
    bool cancelled = false;
    QStringList errors;
    QString text() const;
};

// Local file operations stay independent of the main window's navigation and save gates.
class LocalDataService : public QObject {
    Q_OBJECT
  public:
    explicit LocalDataService(QWidget *parent);
    ~LocalDataService() override;
    static StorageReport scan(const QString &root, const std::atomic_bool &cancel);
    bool startStatistics(const QString &root);
    void showStatistics();
    void cancelStatistics();
    static bool launchBackup(bool restore, const QString &archive);
  signals:
    void statisticsReady(const QString &text);

  private:
    QWidget *window;
    QThread *worker = nullptr;
    std::shared_ptr<std::atomic_bool> cancelled;
    QPointer<QProgressDialog> progress;
};
