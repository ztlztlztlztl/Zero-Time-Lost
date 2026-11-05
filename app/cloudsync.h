#pragma once
#include <QObject>
#include <QJsonObject>
#include <QTimer>
class TaskRepository;
class DDLDepartment;
class NoteDepartment;
class CloudSync : public QObject {
    Q_OBJECT
public:
    CloudSync(TaskRepository *tasks,DDLDepartment *ddl,NoteDepartment *notes,QObject *parent=nullptr);
    void sync();
    void capture();
    void showConflicts();
    QJsonObject snapshot() const;
signals:
    void status(const QString &message);
    void synced();
private:
    friend class CloudTests;
    bool save();
    bool replay();
    bool apply(const QJsonObject &record);
    bool resolveConflict(const QString &key,bool keepLocal);
    void next();
    void pull();
    void finish(const QString &error={});
    bool resolving=false;
    TaskRepository *tasks;
    DDLDepartment *ddl;
    NoteDepartment *notes;
    QJsonObject state;
    QTimer debounce,periodic;
    QString path;
    bool busy=false,applying=false,healthy=true;
    mutable bool readable=true;
};
