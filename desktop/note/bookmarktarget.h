#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QUrl>
namespace BookmarkTarget {
QStringList browserCandidates(const QString &browser);
QString firstExecutable(const QStringList &candidates);
// Use the default browser when none of the supplied executable paths exists.
bool openWebsite(const QUrl &url,const QStringList &candidates,QString *error);
QString validate(const QJsonObject &entry);
bool open(const QJsonObject &entry,QString *error,bool locate=false);
}
