#pragma once
#include <QJsonObject>
#include <QString>
class VaultRepository {
public:
    explicit VaultRepository(QString path):file(std::move(path)) {}
    bool exists() const;
    QJsonObject read() const;
    bool write(const QJsonObject &envelope);
    bool backup(const QString &destination) const;
    QString path() const { return file; }
    QString error() const { return message; }
private:
    QString file;
    mutable QString message;
};
