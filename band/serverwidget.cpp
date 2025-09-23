#include "serverwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QShowEvent>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QProcess>
#include <QtConcurrent>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QDebug>                 // 없으면 추가

// ★ 먼저 사용하므로 프로토타입 필요
static QString extractIfResource(const QString &path);

static inline qreal volPercentToGain(int volPercent) {
    // QSoundEffect::setVolume은 0.0~1.0
    // 사람 귀는 로그 스케일이라 약간의 감쇠를 줘도 됨. 일단 선형 매핑.
    return qBound(0, volPercent, 100) / 100.0;
}

struct ClientState {
    bool authed = false;
    QByteArray buf;
    QString id;
    QString ip;
};

ServerWidget::ServerWidget(QWidget *parent)
: QWidget{parent}
{
    m_volumes["PIANO"] = 100;
    m_volumes["GUITA"] = 100;
    m_volumes["DRUM"]  = 100;

    m_mutes["PIANO"] = false;
    m_mutes["GUITA"] = false;
    m_mutes["DRUM"]  = false;

    auto initFx = [](QSoundEffect& fx, const QUrl& url){
        fx.setSource(url);        // qrc 경로
        fx.setLoopCount(1);
        fx.setVolume(1.0);        // 세션 볼륨은 재생 직전에 반영
        fx.setMuted(false);
    };

    // Piano
    initFx(m_pianoC, QUrl("qrc:/PIANO_C.wav"));
    initFx(m_pianoD, QUrl("qrc:/PIANO_D.wav"));
    initFx(m_pianoE, QUrl("qrc:/PIANO_E.wav"));
    initFx(m_pianoF, QUrl("qrc:/PIANO_F.wav"));
    initFx(m_pianoG, QUrl("qrc:/PIANO_G.wav"));
    initFx(m_pianoA, QUrl("qrc:/PIANO_A.wav"));
    initFx(m_pianoB, QUrl("qrc:/PIANO_B.wav"));

    // Guita (리소스 키 이름은 네 프로젝트에 맞춰 그대로)
    initFx(m_guitaG, QUrl("qrc:/G.wav"));
    initFx(m_guitaD, QUrl("qrc:/D.wav"));
    initFx(m_guitaC, QUrl("qrc:/C.wav"));

    // Drum: tom_hi, tom_mid, cymbal_left, kick, cymbal_right
    m_drum.resize(5);
    m_drum[0] = new QSoundEffect(this); initFx(*m_drum[0], QUrl("qrc:/tom_hi.wav"));
    m_drum[1] = new QSoundEffect(this); initFx(*m_drum[1], QUrl("qrc:/tom_mid.wav"));
    m_drum[2] = new QSoundEffect(this); initFx(*m_drum[2], QUrl("qrc:/cymbal_left.wav"));
    m_drum[3] = new QSoundEffect(this); initFx(*m_drum[3], QUrl("qrc:/kick.wav"));
    m_drum[4] = new QSoundEffect(this); initFx(*m_drum[4], QUrl("qrc:/cymbal_right.wav"));

}


ServerWidget::~ServerWidget() {
    stopServer();
}

bool ServerWidget::loadIdPassFile()
{
    idpw.clear();

    QFile f(":/idpasswd.txt");    // ✅ 리소스 경로
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "idpasswd.txt open fail (resource)";
        return false;
    }

    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const auto parts = line.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) idpw.insert(parts[0], parts[1]);
    }

    qInfo() << "Loaded" << idpw.size() << "id/pass entries (from resource)";
    return true;
}

bool ServerWidget::checkAuth(const QString &id, const QString &pw) const
{
    auto it = idpw.constFind(id);
    return (it != idpw.constEnd() && it.value() == pw);
}

bool ServerWidget::startServer()
{
    if (running) return false;
    loadIdPassFile();

    server = new QTcpServer(this);
    if (!server->listen(QHostAddress::Any, PORT)) {
        qWarning() << "Server listen failed:" << server->errorString();
        server->deleteLater();
        server = nullptr;
        return false;
    }

    connect(server, &QTcpServer::newConnection, this, &ServerWidget::onNewConnection);
    running = true;
    qInfo() << "ServerWidget started on port" << PORT;
    return true;
}

void ServerWidget::stopServer()
{
    if (!running) return;
    running = false;

    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it.key()) {
            it.key()->disconnect(this);
            it.key()->close();
            it.key()->deleteLater();
        }
    }
    clients.clear();

    if (server) {
        server->close();
        server->deleteLater();
        server = nullptr;
    }
    qInfo() << "ServerWidget stopped";
}

void ServerWidget::onNewConnection() {
    while (server->hasPendingConnections()) {
        QTcpSocket *sock = server->nextPendingConnection();
        sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        sock->setParent(this);
        clients.insert(sock, Client{});                   // sock을 키로 상태 저장
        clients[sock].ip = sock->peerAddress().toString();

        connect(sock, &QTcpSocket::readyRead,    this, &ServerWidget::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &ServerWidget::onDisconnected);

        sock->write("Welcome. Please login with 'id:pw'\n");
    }
}

void ServerWidget::onDisconnected()
{
    auto *sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    auto it = clients.find(sock);
    if (it != clients.end()) {
        qInfo() << "[DISCONNECT]" << it->ip << (it->authed ? it->id : "(unauth)");
        clients.erase(it);
    }
    sock->deleteLater();
}

void ServerWidget::onReadyRead()
{
    auto *sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    auto it = clients.find(sock);
    if (it == clients.end()) return;
    Client &c = it.value();

    c.rxBuf += sock->readAll();

    while (true) {
        int pos = c.rxBuf.indexOf('\n');
        if (pos < 0) break;

        // 1) 원본 라인(개행 포함) 먼저 확보
        const int len = pos + 1;                 // '\n' 포함 길이
        QByteArray rawLine = c.rxBuf.left(len);  // 원본 그대로 (CR/LF 포함 가능)

        // 2) 내부 파싱용 라인 만들기 (개행/CR 제거)
        QByteArray line = rawLine;               // 복사본
        if (!line.isEmpty() && line.endsWith('\n')) line.chop(1);
        if (!line.isEmpty() && line.endsWith('\r')) line.chop(1);

        // 3) 외부로 “원본 그대로” 내보내기
        //   - 시그널이 QByteArray면 그대로
        //   - 시그널이 QString이면 변환만 하고 trim/chop 금지
        //emit socketRecvDataSig(rawLine); // 시그널이 QByteArray인 경우
        emit socketRecvDataSig(QString::fromUtf8(rawLine)); // QString 시그널인 경우

        // 4) 내부 처리
        handleLine(c, line);  // 기존 파서 유지 (QByteArray)

        // 5) 마지막에 버퍼에서 제거 (한 번만!)
        c.rxBuf.remove(0, len);
    }
}


void ServerWidget::handleLine(Client &c, const QByteArray &lineBA)
{
    const QString s = QString::fromUtf8(lineBA).trimmed();
    if (s.isEmpty()) return;

    // 0) 로그인 먼저 처리하고 종료
    if (!c.authed) {
        const int p = s.indexOf(':');
        if (p > 0) {
            const QString id = s.left(p).trimmed();
            const QString pw = s.mid(p+1).trimmed();
            qDebug() << "[LOGIN try]" << id << pw;
            if (checkAuth(id, pw)) {
                c.authed = true;
                c.id = id;
                qInfo() << "[LOGIN OK]" << c.ip << c.id;
                // (옵션) 소켓에 OK 내려주면 클라가 상태 확인하기 편함
                // sock->write("OK\n");
            } else {
                qWarning() << "[LOGIN FAIL]" << c.ip << s;
                // sock->write("Auth Error\n"); sock->disconnectFromHost();
            }
        } else {
            qWarning() << "[LOGIN WAIT] expecting id:pw, got:" << s;
        }
        return;  // 🔴 로그인 라인은 여기서 끝!
    }

    // 1) 여기서부터 명령 파싱 ([TAG]payload 또는 TAG:payload)
    QString tag, payload;
    static const QRegularExpression reBracket(
        R"(^\s*\[\s*([A-Za-z0-9_]+)\s*\]\s*(.*?)\s*$)"
    );
    auto m = reBracket.match(s);
    if (m.hasMatch()) {
        tag = m.captured(1).toUpper();
        payload = m.captured(2);
        qDebug() << "[handleLine] bracket" << tag << payload;
    } else {
        const int p2 = s.indexOf(':');
        if (p2 >= 0) {
            tag = s.left(p2).trimmed().toUpper();
            payload = s.mid(p2+1);
            qDebug() << "[handleLine] colon" << tag << payload;
        } else {
            qWarning() << "[handleLine] unrecognized:" << s;
            return;
        }
    }

    qInfo() << "[PARSE]" << "tag=" << tag << ", payload=[" << payload << "], len=" << payload.size();

    if (tag == "PIANO") { handlePiano(payload); return; }
    if (tag == "DRUM")  { handleDrum(payload);  return; }
    if (tag == "GUITA") { handleGuita(payload); return; }

    qInfo() << "[RX]" << tag << payload;
}

void ServerWidget::handlePiano(const QString &payload)
{
    if (payload.isEmpty()) return;
    QChar note = payload.at(0).toUpper();

    QSoundEffect* fx = nullptr;
    if (note == QLatin1Char('C')) fx = &m_pianoC;
    else if (note == QLatin1Char('D')) fx = &m_pianoD;
    else if (note == QLatin1Char('E')) fx = &m_pianoE;
    else if (note == QLatin1Char('F')) fx = &m_pianoF;
    else if (note == QLatin1Char('G')) fx = &m_pianoG;
    else if (note == QLatin1Char('A')) fx = &m_pianoA;
    else if (note == QLatin1Char('B')) fx = &m_pianoB;

    if (!fx) { qWarning() << "[PIANO] invalid payload:" << payload; return; }

    if (isMuted("PIANO")) { qInfo() << "[AUDIO] PIANO muted -> skip"; return; }
    fx->setVolume(volPercentToGain(volumeOf("PIANO")));
    fx->setMuted(false);
    fx->play();
}

void ServerWidget::handleGuita(const QString &payload)
{
    if (payload.isEmpty()) return;
    QChar note = payload.at(0).toUpper();

    QSoundEffect* fx = nullptr;
    if (note == QLatin1Char('G')) fx = &m_guitaG;
    else if (note == QLatin1Char('D')) fx = &m_guitaD;
    else if (note == QLatin1Char('C')) fx = &m_guitaC;

    if (!fx) { qWarning() << "[GUITA] invalid payload:" << payload; return; }

    if (isMuted("GUITA")) { qInfo() << "[AUDIO] GUITA muted -> skip"; return; }
    fx->setVolume(volPercentToGain(volumeOf("GUITA")));
    fx->setMuted(false);
    fx->play();
}

void ServerWidget::handleDrum(const QString &payload)
{
    const QString p = payload.trimmed();
    bool ok = false;
    int v = p.toInt(&ok, 10);
    if (!ok || v < 0 || v >= m_drum.size()) {
        qWarning() << "[DRUM] invalid payload:" << payload;
        return;
    }
    auto *fx = m_drum[v];
    if (!fx) return;

    if (isMuted("DRUM")) { qInfo() << "[AUDIO] DRUM muted -> skip"; return; }
    fx->setVolume(volPercentToGain(volumeOf("DRUM")));
    fx->setMuted(false);
    fx->play();
}

static QString extractIfResource(const QString &path)
{
    if (!path.startsWith(":/"))
        return path; // 파일시스템 경로면 그대로

    QFile rf(path);
    if (!rf.open(QIODevice::ReadOnly)) {
        qWarning() << "[AUDIO] resource open fail:" << path;
        return QString();
    }

    // 확장자 유지 (ffplay/aplay가 포맷 추정)
    QString suffix = QFileInfo(path).suffix();
    if (suffix.isEmpty()) suffix = "wav";

    // /tmp로 복사 (Detached 실행이므로 파일 생명주기 보장 위해 autoRemove=false)
    QTemporaryFile tf(QDir::tempPath() + "/wav_XXXXXX." + suffix);
    tf.setAutoRemove(false);
    if (!tf.open()) {
        qWarning() << "[AUDIO] temp file create fail";
        return QString();
    }
    tf.write(rf.readAll());
    tf.flush();
    QString outPath = tf.fileName();
    tf.close();

    // 일정 시간 뒤 청소 (대충 10초 뒤)
    QTimer::singleShot(10000, [outPath](){
        QFile::remove(outPath);
    });

    return outPath;
}

void ServerWidget::playWavAsync(const QString &path)
{
    QString fsPath = extractIfResource(path);
    if (fsPath.isEmpty()) return;

    // 한 번은 어떤 플레이어가 되는지 로그를 보고 싶다면 startDetached 대신 간단 로그:
    auto tryRun = [&](const QString &cmd)->bool {
        // bash -lc 로 which + 실행
        QString sh = QString("if command -v %1 >/dev/null 2>&1; then %1 %2; else exit 127; fi")
                     .arg(cmd, cmd=="ffplay" ? "-nodisp -autoexit -loglevel error \""+fsPath+"\"" :
                          cmd=="paplay" ? "\""+fsPath+"\"" :
                          /* aplay */   "-q \""+fsPath+"\"");
        bool ok = QProcess::startDetached("bash", {"-lc", sh});
        if (ok) qInfo() << "[AUDIO]" << cmd << "->" << fsPath;
        return ok;
    };

    if (tryRun("paplay")) return;
    if (tryRun("ffplay")) return;
    if (tryRun("aplay"))  return;

    qWarning() << "[AUDIO] no player worked for" << fsPath;
}

void ServerWidget::onMixerVolume(const QString &session, int volume)
{
    QString key = session.trimmed().toUpper();
    if (key == "GUITAR") key = "GUITA";        // 오타 대비 매핑
    m_volumes[key] = qBound(0, volume, 100);
    qInfo() << "[MIXER] volume" << key << "->" << m_volumes[key] << "%";
}

void ServerWidget::onMixerMute(const QString &session, bool mute)
{
    QString key = session.trimmed().toUpper();
    if (key == "GUITAR") key = "GUITA";
    m_mutes[key] = mute;
    qInfo() << "[MIXER] mute" << key << "->" << (mute ? "on" : "off");
}
