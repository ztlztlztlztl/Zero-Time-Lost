#pragma once
#include "businessclock.h"
#include "taskrepository.h"
#include "vaultrepository.h"
#include "ddldepartment.h"
#include "notedepartment.h"
#include "mailgetservice.h"
#include "homedepartment.h"
#include "cloudsync.h"
class AppContext:public QObject {
    Q_OBJECT
public:
    explicit AppContext(QObject *parent=nullptr);
    ~AppContext() override;
    void reloadSettings();
    void shutdown();
    BusinessClock clock;
    TaskRepository tasks;
    VaultRepository vault;
    DDLDepartment ddl;
    NoteDepartment notes;
    MailGetService mail;
    HomeDepartment home;
    CloudSync *cloud=nullptr;
private:
    bool stopped=false;
};
