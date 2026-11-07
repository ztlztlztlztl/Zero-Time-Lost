#pragma once
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTimer>
class TaskRepository;
class DDLDepartment;
class NoteDepartment;
class CloudSync : public QObject {
    Q_OBJECT
  public:
    CloudSync(TaskRepository *tasks, DDLDepartment *ddl, NoteDepartment *notes, QObject *parent = nullptr);
    void sync();
    void capture();
    void showConflicts();
    bool prepareReconnect();
    bool flush();
    bool recover();
    QJsonObject snapshot() const;
  signals:
    void status(const QString &message);
    void synced();

  private:
    friend class CloudTests;
    friend class OptimizeTests;
    bool save();
    bool replay();
    bool apply(const QJsonObject &record);
    bool resolveConflict(const QString &key, bool keepLocal);
    void next();
    void pull();
    void finish(const QString &error = {});
    bool resolving = false;
    TaskRepository *tasks;
    DDLDepartment *ddl;
    NoteDepartment *notes;
    QJsonObject state;
    QTimer debounce, periodic;
    QString path;
    bool busy = false, applying = false, healthy = true;
    bool stateCorrupt = false;
    bool stateReadFailed = false;
    mutable bool readable = true;
    mutable QJsonObject snapshotCache;
    mutable QMap<QString, QSet<QString>> snapshotKeys;
    mutable QSet<QString> publicNoteIds, captureKeys;
    bool fullCapture = true;
    qsizetype lastCaptureCount = 0;
    mutable quint64 taskRevision = 0, noteRevision = 0;
    mutable QSet<QString> invalidKinds{"calendar", "note", "order", "mailflag", "settings"};
    QJsonObject dirtyObjects;
    mutable QMap<QString, QPair<QString, QString>> dirtyMail;
    void markDirty(const QString &kind, const QString &id = "*");
};
