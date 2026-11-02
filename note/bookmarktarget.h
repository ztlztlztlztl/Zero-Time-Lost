#pragma once
#include <QJsonObject>
namespace BookmarkTarget {
QString validate(const QJsonObject &entry);
bool open(const QJsonObject &entry,QString *error,bool locate=false);
}
