#include "EvaluationQueryHandler.h"
#include "Histogram.h"
#include "QueryHelpers.h"
#include "CIS3DCellTypes.h"
#include "CIS3DNeurons.h"
#include "CIS3DRegions.h"
#include "CIS3DConstantsHelpers.h"
#include "CIS3DSparseVectorSet.h"
#include "InnervationStatistic.h"
#include "Util.h"
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <stdexcept>


EvaluationQueryHandler::EvaluationQueryHandler(QObject* parent)
    : QObject(parent)
{
}


void EvaluationQueryHandler::process(const QString& evaluationQueryId,
                                     const QJsonObject& config)
{
    mConfig = config;
    mQueryId = evaluationQueryId;

    const QString baseUrl = mConfig["METEOR_URL_CIS3D"].toString();
    const QString queryEndPoint = mConfig["METEOR_EVALUATIONQUERY_ENDPOINT"].toString();
    const QString loginEndPoint = mConfig["METEOR_LOGIN_ENDPOINT"].toString();
    const QString logoutEndPoint = mConfig["METEOR_LOGOUT_ENDPOINT"].toString();

    if (baseUrl.isEmpty()) {
        throw std::runtime_error("EvaluationQueryHandler: Cannot find METEOR_URL_CIS3D");
    }
    if (queryEndPoint.isEmpty()) {
        throw std::runtime_error("EvaluationQueryHandler: Cannot find METEOR_EVALUATIONQUERY_ENDPOINT");
    }
    if (loginEndPoint.isEmpty()) {
        throw std::runtime_error("EvaluationQueryHandler: Cannot find METEOR_LOGIN_ENDPOINT");
    }
    if (logoutEndPoint.isEmpty()) {
        throw std::runtime_error("EvaluationQueryHandler: Cannot find METEOR_LOGOUT_ENDPOINT");
    }

    mQueryUrl = baseUrl + queryEndPoint + mQueryId;
    mLoginUrl = baseUrl + loginEndPoint;
    mLogoutUrl = baseUrl + logoutEndPoint;

    mDataRoot = mConfig["WORKER_PRIMARY_DATA_DIR"].toString();
    if (mDataRoot.isEmpty()) {
        throw std::runtime_error("EvaluationQueryHandler: No WORKER_PRIMARY_DATA_DIR provided");
    }

    mAuthInfo = QueryHelpers::login(mLoginUrl,
                                    mConfig["WORKER_USERNAME"].toString(),
                                    mConfig["WORKER_PASSWORD"].toString(),
                                    mNetworkManager);

    QNetworkRequest request;
    request.setUrl(mQueryUrl);
    request.setRawHeader(QByteArray("X-User-Id"), mAuthInfo.userId.toLocal8Bit());
    request.setRawHeader(QByteArray("X-Auth-Token"), mAuthInfo.authToken.toLocal8Bit());
    request.setAttribute(QNetworkRequest::User, QVariant("getQueryData"));

    connect(&mNetworkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyGetQueryFinished(QNetworkReply*)));
    mNetworkManager.get(request);
}


void EvaluationQueryHandler::reportUpdate(NetworkStatistic* stat){

    long long numConnections = stat->getNumConnections();
    long long connectionsDone = stat->getConnectionsDone();

    QJsonObject result = stat->createJson("", 0);
    QJsonObject progress;
    progress.insert("completed", connectionsDone);
    progress.insert("total", numConnections);
    const double percent = double(connectionsDone)*100.0/double(numConnections);
    progress.insert("percent", percent);
    QJsonObject jobStatus;
    jobStatus.insert("status", "running");
    jobStatus.insert("progress", progress);

    QJsonObject payload;
    payload.insert("result", result);
    payload.insert("jobStatus", jobStatus);

    QNetworkRequest putRequest;
    putRequest.setUrl(mQueryUrl);
    putRequest.setRawHeader(QByteArray("X-User-Id"), mAuthInfo.userId.toLocal8Bit());
    putRequest.setRawHeader(QByteArray("X-Auth-Token"), mAuthInfo.authToken.toLocal8Bit());
    putRequest.setAttribute(QNetworkRequest::User, QVariant("putIntermediateEvaluationResult"));
    putRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonDocument putDoc(payload);
    QString putData(putDoc.toJson());

    qDebug() << "Posting intermediate result:" << percent;
    QEventLoop loop;
    QNetworkReply* reply = mNetworkManager.put(putRequest, putData.toLocal8Bit());
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
}

void EvaluationQueryHandler::reportComplete(NetworkStatistic* stat){

    long long numConnections = stat->getNumConnections();

    const QString preSelId = mCurrentJsonData["presynapticSelectionId"].toString();
    const QString postSelId = mCurrentJsonData["postsynapticSelectionId"].toString();
    const QString key = QString("innervation_%1_%2_%3.csv").arg(mQueryId).arg(preSelId).arg(postSelId);
    const QString presynSelectionText = mCurrentJsonData["presynapticSelectionFilterAsText"].toString();
    const QString postsynSelectionText = mCurrentJsonData["postsynapticSelectionFilterAsText"].toString();
    const QString csvfile = stat->createCSVFile(key, presynSelectionText, postsynSelectionText,
                                          mConfig["WORKER_TMP_DIR"].toString());
    const qint64 fileSizeBytes = QFileInfo(csvfile).size();
    if (QueryHelpers::uploadToS3(key, csvfile, mConfig) != 0) {
        qDebug() << "Error uploading csv file to S3:" << csvfile;
        logoutAndExit(1);
    }

    QJsonObject result = stat->createJson(key, fileSizeBytes);
    QJsonObject progress;
    progress.insert("completed", numConnections);
    progress.insert("total", numConnections);
    progress.insert("percent", 100);
    QJsonObject jobStatus;
    jobStatus.insert("status", "completed");
    jobStatus.insert("progress", progress);

    QJsonObject payload;
    payload.insert("result", result);
    payload.insert("jobStatus", jobStatus);

    QNetworkRequest putRequest;
    putRequest.setUrl(mQueryUrl);
    putRequest.setRawHeader(QByteArray("X-User-Id"), mAuthInfo.userId.toLocal8Bit());
    putRequest.setRawHeader(QByteArray("X-Auth-Token"), mAuthInfo.authToken.toLocal8Bit());
    putRequest.setAttribute(QNetworkRequest::User, QVariant("putEvaluationResult"));
    putRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonDocument putDoc(payload);
    QString putData(putDoc.toJson());

    connect(&mNetworkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyPutResultFinished(QNetworkReply*)));
    mNetworkManager.put(putRequest, putData.toLocal8Bit());
};

void EvaluationQueryHandler::replyGetQueryFinished(QNetworkReply* reply) {
    QNetworkReply::NetworkError error = reply->error();
    if (error == QNetworkReply::NoError && !reply->request().attribute(QNetworkRequest::User).toString().contains("getQueryData")) {
        return;
    }
    else if (error == QNetworkReply::NoError) {
        qDebug() << "[*] Starting computation of innervation query";

        mNetwork.setDataRoot(mDataRoot);
        mNetwork.loadFilesForQuery();

        const QByteArray content = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(content);
        QJsonObject jsonData = jsonResponse.object().value("data").toObject();
        mCurrentJsonData = jsonData;
        reply->deleteLater();

        QString preSelString = jsonData["presynapticSelectionFilter"].toString();
        QJsonDocument preDoc = QJsonDocument::fromJson(preSelString.toLocal8Bit());
        QJsonArray preArr = preDoc.array();

        qDebug() << "filter pre" << preArr;

        SelectionFilter preFilter = Util::getSelectionFilterFromJson(preArr, mNetwork);
        preFilter.synapticSide = CIS3D::PRESYNAPTIC;
        const IdList preNeurons = mNetwork.neurons.getFilteredNeuronIds(preFilter);

        QString postSelString = jsonData["postsynapticSelectionFilter"].toString();
        QJsonDocument postDoc = QJsonDocument::fromJson(postSelString.toLocal8Bit());
        QJsonArray postArr = postDoc.array();
        SelectionFilter postFilter = Util::getSelectionFilterFromJson(postArr, mNetwork);
        postFilter.synapticSide = CIS3D::POSTSYNAPTIC;
        const IdList postNeurons = mNetwork.neurons.getFilteredNeuronIds(postFilter);

        qDebug() << "[*] Start processing " << preNeurons.size() << " presynaptic and " << postNeurons.size() << " postsynaptic neurons.";
        InnervationStatistic innervation(mNetwork);
        connect(&innervation, SIGNAL(update(NetworkStatistic*)), this, SLOT(reportUpdate(NetworkStatistic*)));
        connect(&innervation, SIGNAL(complete(NetworkStatistic*)), this, SLOT(reportComplete(NetworkStatistic*)));
        innervation.calculate(preNeurons, postNeurons);
    }
    else {
        qDebug() << "[-] Error obtaining EvaluationQuery data:";
        qDebug() << reply->errorString();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) {
            qDebug() << QString(reply->readAll().replace("\"", ""));
        }
        reply->deleteLater();
        logoutAndExit(1);
    }
}


void EvaluationQueryHandler::replyPutResultFinished(QNetworkReply *reply) {
    QNetworkReply::NetworkError error = reply->error();
    const QString requestId = reply->request().attribute(QNetworkRequest::User).toString();
    if (error == QNetworkReply::NoError && !(requestId == "putEvaluationResult")) {
        return;
    }
    else if (error == QNetworkReply::NoError) {
        qDebug() << "    Completed processing evaluation query" << mQueryId;
        reply->deleteLater();
        logoutAndExit(0);
    }
    else {
        qDebug() << "[-] Error putting Evaluation result (queryId" << mQueryId << "):";
        qDebug() << reply->errorString();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) {
            qDebug() << QString(reply->readAll().replace("\"", ""));
        }
        reply->deleteLater();
        logoutAndExit(1);
    }
}


void EvaluationQueryHandler::logoutAndExit(const int exitCode)
{

    QueryHelpers::logout(mLogoutUrl,
                         mAuthInfo,
                         mNetworkManager);

    if (exitCode == 0) {
        emit completedProcessing();
    }
    else {
        QCoreApplication::exit(exitCode);
    }
}