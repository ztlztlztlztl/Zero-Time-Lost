#pragma once
#include "notedepartment.h"
#include "vaultrepository.h"
#include <QDialog>
#include <QJsonObject>
class QLineEdit;
class QLabel;
class QStackedWidget;
class QPushButton;
class QSpinBox;
class SetupWizard : public QDialog {
    Q_OBJECT
  public:
    explicit SetupWizard(bool firstRun, QWidget *parent = nullptr);
    static bool completed();
    static bool required();
    bool importProfileFile(const QString &path);

  protected:
    void reject() override;
    void closeEvent(QCloseEvent *) override;

  private:
    void page(int);
    void setBusy(bool);
    bool saveBasics();
    bool saveCloud(const QJsonObject &);
    void requestExisting();
    void next();
    void finishSetup();
    bool initial, working = false;
    int step = 0;
    VaultRepository vault;
    NoteDepartment notes;
    QByteArray importedCa;
    QStackedWidget *pages;
    QLabel *heading, *message;
    QList<QLabel *> steps;
    QPushButton *cancel, *back, *forward;
    QLineEdit *password, *confirmation, *weatherKey, *cityName, *cityCode, *mailName, *mailAddress, *mailCode,
        *mailServer, *host, *token, *caPath;
    QSpinBox *mailPort;
};
