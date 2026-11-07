#pragma once
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
class NoteRepository {
  public:
    explicit NoteRepository(QString path);
    QJsonArray entries() const { return items; }
    QJsonObject find(const QString &id) const { return byId.value(id); }
    quint64 revision() const { return generation; }
    QStringList changesSince(quint64 since) const {
        QStringList ids;
        for (auto it = changedIds.begin(); it != changedIds.end(); ++it)
            if (it.value() > since)
                ids.append(it.key());
        return ids;
    }
    bool indexed() const { return vaultIndexed; }
    QStringList order(int kind) const;
    bool saveOrder(int kind, const QStringList &ids);
    bool save(const QJsonArray &entries, bool indexed);
    bool ready() const { return valid; }
    QString error() const { return message; }

  private:
    QString file, message;
    QJsonArray items;
    QHash<QString, QJsonObject> byId;
    quint64 generation = 0;
    QHash<QString, quint64> changedIds;
    void reindex() {
        QHash<QString, QJsonObject> next;
        for (auto v : items)
            next[v.toObject()["id"].toString()] = v.toObject();
        ++generation;
        for (auto it = byId.begin(); it != byId.end(); ++it)
            if (next.value(it.key()) != it.value())
                changedIds[it.key()] = generation;
        for (auto it = next.begin(); it != next.end(); ++it)
            if (byId.value(it.key()) != it.value())
                changedIds[it.key()] = generation;
        byId = next;
    }
    QJsonObject orders;
    bool valid = true, vaultIndexed = false;
};
