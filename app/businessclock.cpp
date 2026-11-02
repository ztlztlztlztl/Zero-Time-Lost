#include "businessclock.h"
BusinessClock::BusinessClock(QObject *parent) : QObject(parent) {
    connect(&timer, &QTimer::timeout, this, &BusinessClock::poll);
    timer.start(1000);
}
void BusinessClock::every(const QString &id, int seconds, std::function<void()> action, bool immediate) {
    jobs.insert(id, {qMax(1, seconds), now() + (immediate ? 0 : seconds), std::move(action)});
}
void BusinessClock::poll() {
    const auto current = now();
    const bool backwards = last && current < last;
    last = current;
    for (const auto &id : jobs.keys()) {
        if (!jobs.contains(id)) continue;
        auto &job = jobs[id];
        if (backwards) job.next = current;
        if (job.next > current) continue;
        auto action = job.action;
        job.next = current + job.seconds; // Coalesce missed ticks after sleep.
        action();
    }
}
