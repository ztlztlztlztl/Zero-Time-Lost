#pragma once
#include <QString>
namespace Paths {
QString root();
void initialize();
QString data(const QString &relative);
QString scripts();
QString python();
}
