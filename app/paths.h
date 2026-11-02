#pragma once
#include <QString>
namespace Paths {
QString root();
QString data(const QString &relative);
QString scripts();
QString python();
}
