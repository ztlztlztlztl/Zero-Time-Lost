#pragma once
#include "bootstrapprocess.h"
#include <QJsonObject>
#include <QWidget>
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class ManagerWindow : public QWidget {
    Q_OBJECT
  public:
    explicit ManagerWindow(QWidget *parent = nullptr);
    bool exportConnectionFile(const QString &path);

  protected:
    void closeEvent(QCloseEvent *) override;

  private:
    void start(const QString &action);
    void append(const QString &);
    void completed(const QJsonObject &, const QString &);
    void saveDraft();
    void chooseExportFile();
    QJsonObject connection() const;
    BootstrapProcess worker;
    BootstrapProcess observer;
    QJsonObject plan, profile;
    QString action;
    bool working = false, releaseReady = false;
    QLineEdit *host, *password, *keyFile, *passphrase, *imagePrefix, *release, *url, *sha;
    QSpinBox *port;
    QComboBox *mode, *backups;
    QCheckBox *trust;
    QLabel *summary, *releaseInfo;
    QPlainTextEdit *log;
    QWidget *controls;
};
