#ifndef SERVERWIDGET_H
#define SERVERWIDGET_H

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QSoundEffect>
#include <QVector>

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

    // ★ Mixer 연동
    void onMixerVolume(const QString &session, int volume); // 0~100
    void onMixerMute(const QString &session, bool mute);

signals:
    void socketRecvDataSig(const QString &data);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QSoundEffect m_pianoC, m_pianoD, m_pianoE, m_pianoF, m_pianoG, m_pianoA, m_pianoB;
    QSoundEffect m_guitaG, m_guitaD, m_guitaC;
    QVector<QSoundEffect*> m_drum;
    QHash<QString,int>  m_volumes; // 0~100
    QHash<QString,bool> m_mutes;   // true=mute

    inline bool isMuted(const QString &session) const {
        auto it = m_mutes.constFind(session);
        return (it != m_mutes.cend()) ? it.value() : false;
    }
    inline int volumeOf(const QString &session) const {
        auto it = m_volumes.constFind(session);
        return (it != m_volumes.cend()) ? it.value() : 100;
    }

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
