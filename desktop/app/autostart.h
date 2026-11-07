#pragma once
#include <QString>
namespace AutoStart {
bool enabled();
bool setEnabled(bool enabled,QString *error);
QString command();
}
