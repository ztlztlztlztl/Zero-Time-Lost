#pragma once
#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSet>
#include <functional>

class CloudConnection : public QObject {
    Q_OBJECT
public:
    using Callback=std::function<void(const QJsonObject &,const QString &)>;
    static CloudConnection *instance();
    static bool enabled();
    static QString identity();
    void request(const QString &path,const QJsonObject &body,Callback callback,bool post=false);
    void notifySettingsChanged();
signals:
    void settingsChanged();
    void syncRequested();
    void conflictsRequested();
    void status(const QString &message);
private:
    explicit CloudConnection(QObject *parent=nullptr):QObject(parent),network(new QNetworkAccessManager(this)){}
    QNetworkAccessManager *network;
    QSet<QNetworkReply*> activeReplies;
    quint64 generation=0;
};
