#pragma once
#include <QJsonObject>
#include <QSet>
#include <QUuid>

// Pure queue planning: network retries keep their original operation and payload.
namespace CloudOutbox {
inline bool same(const QJsonObject &a, const QJsonObject &b) {
    const bool goneA = a.isEmpty() || a["deleted"].toBool();
    const bool goneB = b.isEmpty() || b["deleted"].toBool();
    return goneA == goneB && (goneA || a["payload"] == b["payload"]);
}
inline QJsonObject collect(const QJsonObject &local, const QJsonObject &shadow, QJsonObject pending,
                           const QJsonObject &conflicts, const QSet<QString> &keys) {
    for (const auto &key : keys) {
        if (pending.contains(key) || conflicts.contains(key))
            continue;
        const auto base = shadow[key].toObject();
        auto current = local[key].toObject();
        if (same(current, base))
            continue;
        if (current.isEmpty()) {
            current = base;
            current["deleted"] = true;
            current["payload"] = QJsonObject();
        }
        current.remove("rev");
        current["base"] = base["rev"].toInteger();
        current["op"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        pending[key] = current;
    }
    return pending;
}
} // namespace CloudOutbox
