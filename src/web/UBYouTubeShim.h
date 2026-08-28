/*
 * WistOpenboard fork.
 *
 * YouTube's embed player refuses playback ("error 153") when the embedding
 * page sends no Referer header. Board widgets load from file://, which never
 * produces a referrer, and Chromium overrides any Referer injected through
 * the URL request interceptor for iframe navigations. The reliable fix is to
 * give the embed a real HTTP embedding page: a tiny localhost server inside
 * OpenBoard serves a one-iframe wrapper, so Chromium itself sends
 * "Referer: http://127.0.0.1:<port>/" to YouTube and playback is allowed.
 *
 * The server binds to 127.0.0.1 only (never reachable from the network),
 * serves exactly one page shape, and validates the video id strictly.
 */

#ifndef UBYOUTUBESHIM_H_
#define UBYOUTUBESHIM_H_

#include <QObject>
#include <QString>

class QTcpServer;

class UBYouTubeShim : public QObject
{
    Q_OBJECT

public:
    // "http://127.0.0.1:<port>/ytplayer?v=<id>" for a valid 11-char video id,
    // empty string for an invalid id or if the server failed to start.
    static QString playerUrl(const QString& videoId);

private:
    UBYouTubeShim();
    static UBYouTubeShim* instance();

    void handleConnection();

    QTcpServer* mServer;
};

#endif /* UBYOUTUBESHIM_H_ */
