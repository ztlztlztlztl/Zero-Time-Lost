#pragma once
#include <QtWidgets>
#include <chrono>
#include <future>
// Only pure calculations run here; repositories and QObject signals remain on their owner thread.
namespace CryptoJob {
class Dialog : public QDialog {
  public:
    bool cancelled = false;
    QLabel *message;
    Dialog() {
        setWindowTitle("Z.T.L · 秘密");
        setWindowModality(Qt::ApplicationModal);
        auto layout = new QVBoxLayout(this);
        message = new QLabel("正在处理加密，请稍候…");
        layout->addWidget(message);
        auto progress = new QProgressBar;
        progress->setRange(0, 0);
        layout->addWidget(progress);
        auto cancel = new QPushButton("取消");
        layout->addWidget(cancel);
        QObject::connect(cancel, &QPushButton::clicked, this, [this, cancel] {
            cancelled = true;
            cancel->setEnabled(false);
            message->setText("正在结束计算，完成后丢弃结果…");
        });
    }
    void reject() override {
        cancelled = true;
        message->setText("正在结束计算，完成后丢弃结果…");
    }
};
template <class Work> auto run(Work work, bool *cancelled) {
    auto future = std::async(std::launch::async, std::move(work));
    Dialog dialog;
    QTimer timer;
    timer.setInterval(15);
    QObject::connect(&timer, &QTimer::timeout, &dialog, [&] {
        if (future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            dialog.accept();
    });
    timer.start();
    dialog.exec();
    *cancelled = dialog.cancelled;
    return future.get();
}
} // namespace CryptoJob
