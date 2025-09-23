#ifndef SERVERWIDGET_H
#define SERVERWIDGET_H

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>

#define PORT 5000
#define BLOCK_SIZE 1024

struct ClientState;

class ServerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ServerWidget(QWidget *parent = nullptr);
    ~ServerWidget();

public slots:
    bool startServer();
    void stopServer();

signals:
    void socketRecvDataSig(const QString &data);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    struct Client {
        QString id;
        QString ip;
        bool authed = false;
        QByteArray rxBuf;
        QString pendingTag;
    };

    bool loadIdPassFile();
    bool checkAuth(const QString &id, const QString &pw) const;
    void handleLine(Client &c, const QByteArray &line);
    void handleGuita(const QString &payload);
    void handleDrum(const QString &payload);
    void handlePiano(const QString &payload);
    void playWavAsync(const QString &path);

    static inline QStringList splitOnce(const QString &s, QChar delim) {
        int p = s.indexOf(delim);
        if (p < 0) return {s};
        return {s.left(p), s.mid(p+1)};
    }

private:
    QTcpServer *server = nullptr;
    QHash<QTcpSocket*, Client> clients;
    QHash<QString, QString> idpw;

    const QString WAV_GUITA_G = QStringLiteral(":/G.wav");
    const QString WAV_GUITA_D = QStringLiteral(":/D.wav");
    const QString WAV_GUITA_C = QStringLiteral(":/C.wav");

    const QString WAV_PIANO_C = QStringLiteral(":/PIANO_C.wav");
    const QString WAV_PIANO_D = QStringLiteral(":/PIANO_D.wav");
    const QString WAV_PIANO_E = QStringLiteral(":/PIANO_E.wav");
    const QString WAV_PIANO_F = QStringLiteral(":/PIANO_F.wav");
    const QString WAV_PIANO_G = QStringLiteral(":/PIANO_G.wav");
    const QString WAV_PIANO_A = QStringLiteral(":/PIANO_A.wav");
    const QString WAV_PIANO_B = QStringLiteral(":/PIANO_B.wav");

    const QStringList WAV_DRUM = {
        QStringLiteral(":/tom_hi.wav"),
        QStringLiteral(":/tom_mid.wav"),
        QStringLiteral(":/cymbal_left.wav"),
        QStringLiteral(":/kick.wav"),
        QStringLiteral(":/cymbal_right.wav")
    };

    bool running = false;
};

#endif // SERVERWIDGET_H
