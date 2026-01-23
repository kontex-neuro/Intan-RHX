#include "httpclientthread.h"

#include <fmt/core.h>
#include <fmt/format.h>

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QUrl>

HttpClientThread::HttpClientThread(const QString &ip, int port, QObject *parent)
    : QThread(parent), _ip(ip), _port(port)
{
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
}

HttpClientThread::~HttpClientThread()
{
    requestInterruption();
    wait();
}

void HttpClientThread::run()
{
    while (!isInterruptionRequested()) {
        pingHost();
        QThread::msleep(1000);
    }
}

void HttpClientThread::pingHost()
{
    QNetworkAccessManager manager;

    const QUrl url(QStringLiteral("http://%1:%2/ping").arg(_ip).arg(_port));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");

    QByteArray body("XDAQ-RHX");
    const auto &reply = manager.sendCustomRequest(request, "PUT", body);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(1000);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    loop.exec();

    reply->deleteLater();
}

void HttpClientThread::startRecording()
{
    const auto &endpoint = QStringLiteral("http://%1:%2/start").arg(_ip).arg(_port);
    sendPutRequest(endpoint);
}

void HttpClientThread::stopRecording()
{
    const auto &endpoint = QStringLiteral("http://%1:%2/stop").arg(_ip).arg(_port);
    sendPutRequest(endpoint);
}

void HttpClientThread::sendPutRequest(const QString &endpoint, const QString &body)
{
    QNetworkAccessManager manager;

    QNetworkRequest request{QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");

    const auto &reply = manager.sendCustomRequest(request, "PUT", body.toUtf8());

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(1000);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << reply->errorString();
    } else {
        qDebug() << reply->readAll();
    }

    reply->deleteLater();
}
