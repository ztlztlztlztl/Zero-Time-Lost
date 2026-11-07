#pragma once
#include "noterepository.h"
#include "vaultrepository.h"
#include <QHash>
#include <QJsonArray>
#include <QObject>
class NoteDepartment : public QObject {
    Q_OBJECT
  public:
    QJsonArray cloudEntries() const;
    quint64 cloudRevision() const { return publicRepo.revision(); }
    QStringList cloudChangesSince(quint64 since) const { return publicRepo.changesSince(since); }
    QJsonObject cloudEntry(const QString &id) const {
        const auto e = publicRepo.find(id);
        return e["kind"] == 2 || (e["kind"] == 1 && e["targetType"] == "website") ? e : QJsonObject();
    }
    bool cloudReady() const { return publicRepo.ready(); }
    bool applyCloudEntry(const QString &id, const QJsonObject &entry, bool deleted);
    bool applyCloudOrder(int kind, const QStringList &ids);
    explicit NoteDepartment(VaultRepository *repository, QObject *parent = nullptr);
    ~NoteDepartment() override;
    bool exists() const { return repo->exists(); }
    bool unlocked() const { return !key.isEmpty(); }
    bool unlock(const QString &password);
    bool create(const QString &password);
    void lock(bool clearClipboard = true);
    QJsonArray entries() const { return unlocked() ? items : QJsonArray(); }
    QJsonArray catalog() const;
    bool creationReady() const;
    QJsonObject entry(const QString &id) const;
    bool needsMigration() const { return exists() && !creationReady() && !publicRepo.indexed(); }
    bool save(QJsonObject entry);
    bool remove(const QString &id);
    QStringList order(int kind) const { return publicRepo.order(kind); }
    bool reorder(int kind, const QStringList &ids);
    bool preserveDraft(const QJsonObject &entry);
    QJsonObject recoveryDraft(const QString &id);
    QString migrationNotice() const { return migrationMessage; }
    bool backup(const QString &path);
    bool restore(const QString &path, const QString &password);
    bool changePassword(const QString &password);
    bool flush() { return flushDrafts(); }
    QString error() const { return message; }
    void copySecret(const QString &text);
  signals:
    void aboutToLock();
    void changed();
    void locked();

  private:
    bool migrateLegacy(const QJsonObject &envelope, const QString &password);
    bool flushDrafts();
    VaultRepository *repo;
    NoteRepository publicRepo;
    QByteArray key, privateKey, clipboardFingerprint;
    QJsonArray items;
    mutable QJsonArray cachedCatalog;
    mutable QHash<QString, QJsonObject> catalogIndex;
    mutable quint64 catalogVault = ~quint64(0), catalogPublic = ~quint64(0);
    QHash<QString, QJsonObject> unlockedIndex;
    QJsonObject sessionVault;
    QHash<QString, QJsonObject> pendingDrafts;
    bool locking = false, cryptoBusy = false;
    quint64 cryptoGeneration = 0;
    QString message, migrationMessage;
};
