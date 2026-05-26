#ifndef HTTPCLIENTTHREAD_H
#define HTTPCLIENTTHREAD_H

#include <QObject>
#include <QThread>

class HttpClientThread final : public QThread
{
    Q_OBJECT
public:
    explicit HttpClientThread(
        const QString &ip = "127.0.0.1", int port = 8001, QObject *parent = nullptr
    );
    ~HttpClientThread();

    void run() override;

    void startRecording();
    void stopRecording();

private:
    QString _ip;
    int _port;

    void pingHost();
    
    void sendPutRequest(const QString &endpoint, const QString &body = "XDAQ-RHX");
};

#endif  // HTTPCLIENTTHREAD_H
