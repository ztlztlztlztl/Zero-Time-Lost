#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QString>
class VaultRepository {
  public:
    explicit VaultRepository(QString path) : file(std::move(path)) {}
    bool exists() const;
    QJsonObject read() const;
    bool write(const QJsonObject &envelope);
    bool backup(const QString &destination) const;
    QString path() const { return file; }
    quint64 revision() const { return generation; }
    QString error() const { return message; }

  private:
    QString file;
    mutable QString message;
    mutable QJsonObject cached;
    mutable QDateTime stamp, metadataStamp;
    mutable quint64 generation = 0;
    mutable qint64 size = -1;
};
