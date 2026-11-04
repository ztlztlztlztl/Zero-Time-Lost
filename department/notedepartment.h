#pragma once
#include "vaultrepository.h"
#include "noterepository.h"
#include <QObject>
#include <QJsonArray>
#include <QHash>
class NoteDepartment : public QObject {
    Q_OBJECT
public:
    explicit NoteDepartment(VaultRepository *repository,QObject *parent=nullptr);
    ~NoteDepartment() override;
    bool exists() const { return repo->exists(); }
    bool unlocked() const { return !key.isEmpty(); }
    bool unlock(const QString &password);
    bool create(const QString &password);
    void lock(bool clearClipboard=true);
    QJsonArray entries() const { return unlocked()?items:QJsonArray(); }
    QJsonArray catalog() const;
    bool creationReady() const;
    QJsonObject entry(const QString &id) const;
    bool needsMigration() const {return exists()&&!creationReady()&&!publicRepo.indexed();}
    bool save(QJsonObject entry);
    bool remove(const QString &id);
    QStringList order(int kind) const { return publicRepo.order(kind); }
    bool reorder(int kind,const QStringList &ids);
    bool preserveDraft(const QJsonObject &entry);
    QJsonObject recoveryDraft(const QString &id);
    QString migrationNotice() const {return migrationMessage;}
    bool backup(const QString &path);
    bool restore(const QString &path,const QString &password);
    bool changePassword(const QString &password);
    QString error() const { return message; }
    void copySecret(const QString &text);
signals:
    void aboutToLock();
    void changed();
    void locked();
private:
    bool migrateLegacy(const QJsonObject &envelope,const QString &password);
    bool flushDrafts();
    VaultRepository *repo;
    NoteRepository publicRepo;
    QByteArray key,privateKey,clipboardFingerprint;
    QJsonArray items;
    QJsonObject sessionVault;
    QHash<QString,QJsonObject> pendingDrafts;
    bool locking=false;
    QString message,migrationMessage;
};
