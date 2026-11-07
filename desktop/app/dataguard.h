#pragma once
#include <QtCore>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
// Held by the client and the backup helper across directory switches.
class DataGuard {
#ifdef Q_OS_WIN
    HANDLE handle = nullptr;
#endif
    bool owned = false;

  public:
    explicit DataGuard(const QString &path) {
#ifdef Q_OS_WIN
        auto key = QDir::cleanPath(QFileInfo(path).absoluteFilePath()).replace('\\', '/').toLower();
        const auto name =
            "Local\\ZTLData-" +
            QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex());
        handle = CreateMutexW(nullptr, FALSE, reinterpret_cast<LPCWSTR>(name.utf16()));
        if (handle) {
            auto result = WaitForSingleObject(handle, 0);
            owned = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
        }
#else
        owned = true;
#endif
    }
    bool locked() const { return owned; }
    ~DataGuard() {
#ifdef Q_OS_WIN
        if (handle) {
            if (owned)
                ReleaseMutex(handle);
            CloseHandle(handle);
        }
#endif
    }
};
