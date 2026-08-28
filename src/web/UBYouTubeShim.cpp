/*
 * WistOpenboard fork. See UBYouTubeShim.h.
 */

#include "UBYouTubeShim.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

UBYouTubeShim::UBYouTubeShim()
    : QObject(nullptr)
    , mServer(new QTcpServer(this))
{
    if (mServer->listen(QHostAddress::LocalHost, 0))
        connect(mServer, &QTcpServer::newConnection, this, &UBYouTubeShim::handleConnection);
}

UBYouTubeShim* UBYouTubeShim::instance()
{
    static UBYouTubeShim* shim = new UBYouTubeShim();
    return shim;
}

QString UBYouTubeShim::playerUrl(const QString& videoId)
{
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9_-]{11}$"));

    if (!valid.match(videoId).hasMatch())
        return QString();

    UBYouTubeShim* shim = instance();

    if (!shim->mServer->isListening())
        return QString();

    return QStringLiteral("http://127.0.0.1:%1/ytplayer?v=%2")
            .arg(shim->mServer->serverPort())
            .arg(videoId);
}

void UBYouTubeShim::handleConnection()
{
    while (QTcpSocket* socket = mServer->nextPendingConnection())
    {
        connect(socket, &QTcpSocket::readyRead, this, [socket]() {
            const QByteArray request = socket->readAll();
            const int lineEnd = request.indexOf("\r\n");
            const QByteArray requestLine = lineEnd > 0 ? request.left(lineEnd) : request;

            QString videoId;
            const QList<QByteArray> parts = requestLine.split(' ');

            if (parts.size() >= 2 && parts[0] == "GET")
            {
                const QUrl url = QUrl(QString::fromLatin1(parts[1]));

                if (url.path() == QLatin1String("/ytplayer"))
                    videoId = QUrlQuery(url).queryItemValue(QStringLiteral("v"));
            }

            static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9_-]{11}$"));

            QByteArray body;
            QByteArray status;

            if (valid.match(videoId).hasMatch())
            {
                status = "200 OK";
                body = QByteArray(
                    "<!doctype html><html><head><meta charset=\"utf-8\">"
                    "<style>html,body{margin:0;height:100%;background:#000;overflow:hidden}"
                    "iframe{border:0;width:100%;height:100%;display:block}</style></head><body>"
                    "<iframe src=\"https://www.youtube.com/embed/") + videoId.toLatin1() +
                    QByteArray("?rel=0&autoplay=1&mute=1&playsinline=1\" "
                    "allow=\"autoplay; fullscreen; encrypted-media; picture-in-picture\" "
                    "allowfullscreen referrerpolicy=\"origin\"></iframe></body></html>");
            }
            else
            {
                status = "404 Not Found";
                body = "not found";
            }

            socket->write("HTTP/1.1 " + status + "\r\n"
                          "Content-Type: text/html; charset=utf-8\r\n"
                          "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                          "Connection: close\r\n\r\n" + body);
            socket->disconnectFromHost();
        });

        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}
