#ifndef MOTIFQUERYHANDLER_H
#define MOTIFQUERYHANDLER_H

#include "CIS3DNetworkProps.h"
#include "QueryHelpers.h"
#include "NetworkStatistic.h"
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QtNetwork/QNetworkAccessManager>

class MotifQueryHandler : public QObject {

    Q_OBJECT

public:
    MotifQueryHandler(QObject *parent = 0);

    void process(const QString &motifQueryId, const QJsonObject &config);

signals:
    void completedProcessing();

private slots:
    void replyGetQueryFinished(QNetworkReply *reply);
    void replyPutResultFinished(QNetworkReply *reply);
    void reportUpdate(NetworkStatistic* stat);
    void reportComplete(NetworkStatistic* stat);

private:
    QString mQueryId;
    QJsonObject mConfig;
    QString mDataRoot;
    QNetworkAccessManager mNetworkManager;
    NetworkProps mNetwork;
    AuthInfo mAuthInfo;
    QString mLoginUrl;
    QString mLogoutUrl;
    QString mQueryUrl;
    QJsonObject mCurrentJsonData;


    void logoutAndExit(const int exitCode);
};

#endif // MOTIFQUERYHANDLER_H
