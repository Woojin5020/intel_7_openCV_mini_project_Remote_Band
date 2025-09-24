#include <opencv2/opencv.hpp>
#include <iostream>
#include <memory>
#include <set>
#include <map>
#include <string>
#include <chrono>
#include <thread>

// ===== 소켓용 헤더 =====
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

// 기존 프로젝트 헤더 (OpenCV 피아노 UI, 손 검출)
#include "OpenCVPiano.h"
#include "HandDetector.h"

// -----------------------------
// 간단한 TCP 송신 헬퍼
// -----------------------------
class SocketSender {
public:
    SocketSender(const std::string& host, uint16_t port)
        : host_(host), port_(port), sock_(-1) {}

    ~SocketSender() { closeSock(); }

    bool connectIfNeeded() {
        if (sock_ >= 0) return true;
        sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0) {
            std::perror("[SOCK] socket");
            return false;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port_);
        if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
            std::cerr << "[SOCK] invalid host: " << host_ << "\n";
            closeSock();
            return false;
        }
        if (::connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::perror("[SOCK] connect");
            closeSock();
            return false;
        }
        std::cout << "[SOCK] Connected to " << host_ << ":" << port_ << "\n";
        return true;
    }

    bool sendLine(const std::string& s) {
        if (!connectIfNeeded()) return false;
        const char* data = s.c_str();
        size_t rem = s.size();
        while (rem > 0) {
            ssize_t n = ::send(sock_, data, rem, MSG_NOSIGNAL);
            if (n < 0) {
                if (errno == EINTR) continue;
                std::perror("[SOCK] send");
                // 연결이 끊겼을 수 있으므로 닫고 재시도는 상위에서
                closeSock();
                return false;
            }
            data += n;
            rem  -= static_cast<size_t>(n);
        }
        return true;
    }

private:
    void closeSock() {
        if (sock_ >= 0) {
            ::close(sock_);
            sock_ = -1;
        }
    }

    std::string host_;
    uint16_t    port_;
    int         sock_;
};

// -----------------------------
// 앱 본체
// -----------------------------
class VirtualPianoApp {
public:
    VirtualPianoApp()
        : piano_(),
          handDetector_(),
          sender_(HOST, PORT) // 소켓 송신기
    {
        cv::namedWindow("Virtual Piano", cv::WINDOW_AUTOSIZE);
    }

    bool initialize() {
        if (!handDetector_.initialize()) {
            std::cerr << "Failed to initialize hand detector" << std::endl;
            return false;
        }
        handDetector_.setDebugMode(true);

        // 소켓 연결
        if (!sender_.connectIfNeeded()) {
            std::cerr << "Failed to connect socket" << std::endl;
            // 연결 실패해도 런타임에 재시도하길 원하면 여기서 false 리턴 안 해도 됨
        } else {
            // 🔽 접속 성공 시 한 번 로그인
            sendLoginOnce();
        }
        return true;
    }

    void run() {
        while (true) {
            handleEvents();
            update();
            if (cv::waitKey(1) == 27) { // ESC
                break;
            }
        }
    }

private:
    // ====== 환경 설정 (필요시 수정) ======
    static inline const char* HOST = "10.10.16.55";
    static constexpr uint16_t PORT = 5000;

    static inline const char* LOGIN_ID = "PIANO";
    static inline const char* LOGIN_PW = "PASSWD";

    OpenCVPiano  piano_;
    HandDetector handDetector_;
    SocketSender sender_;

    // 현재 누르고 있는 "화이트키(0~6)" 집합
    std::set<int> currentlyPlayingWhiteKeys_;

    // 크로매틱(0~11) → 화이트키(0~6) 매핑
    //  C  C# D  D# E  F  F# G  G# A  A# B
    //  0  1  2  3  4  5  6  7  8  9 10 11
    //  0     1     2  3     4     5     6
    int chromaticToWhite(int key12) const {
        switch (key12) {
            case 0:  return 0; // C -> 0
            case 2:  return 1; // D -> 1
            case 4:  return 2; // E -> 2
            case 5:  return 3; // F -> 3
            case 7:  return 4; // G -> 4
            case 9:  return 5; // A -> 5
            case 11: return 6; // B -> 6
            default: return -1; // 나머지(검은건반)는 무시
        }
    }

    bool sendLoginOnce() {
        // "id:pw\n" 형식 (서버가 이 포맷을 파싱함)
        std::string login = std::string(LOGIN_ID) + ":" + std::string(LOGIN_PW) + "\n";
        bool ok = sender_.sendLine(login);
        if (ok) {
            std::cout << "[SOCK] login sent\n";
        } else {
            std::cerr << "[SOCK] login send failed\n";
        }
        return ok;
    }

    void sendPianoMsg(int whiteIdx) {
        static const char* NOTE_NAME[7] = {"C","D","E","F","G","A","B"};
        if (whiteIdx < 0 || whiteIdx > 6) return;

        std::string msg = "[PIANO]" + std::string(NOTE_NAME[whiteIdx]) + "\n";
        if (!sender_.sendLine(msg)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            sender_.sendLine(msg);
        }

        std::cout << "[TX] " << msg; // 그대로 출력
    }

    void handleEvents() {
        // OpenCV 키 입력 (테스트용)
        int key = cv::waitKey(1) & 0xFF;
        if (key == 27) { // ESC
            cv::destroyAllWindows();
            exit(0);
        }
        handleKeyPress(key);
    }

    void handleKeyPress(int key) {
        // 화이트키만 테스트 매핑
        // A S D F G H J  → C D E F G A B
        int whiteIdx = -1;
        switch (key) {
            case 'a': whiteIdx = 0; break; // C
            case 's': whiteIdx = 1; break; // D
            case 'd': whiteIdx = 2; break; // E
            case 'f': whiteIdx = 3; break; // F
            case 'g': whiteIdx = 4; break; // G
            case 'h': whiteIdx = 5; break; // A
            case 'j': whiteIdx = 6; break; // B
            default: break;
        }
        if (whiteIdx >= 0) {
            // 새로 눌릴 때만 전송
            if (currentlyPlayingWhiteKeys_.find(whiteIdx) == currentlyPlayingWhiteKeys_.end()) {
                piano_.playKey(whiteIdx);       // 시각적 효과(가능하면 화이트키 인덱스를 그대로 사용)
                sendPianoMsg(whiteIdx);
                currentlyPlayingWhiteKeys_.insert(whiteIdx);
            }
        }
    }

    void update() {
        // 프레임 처리
        cv::Mat frame;
        handDetector_.processFrame(frame);
        if (frame.empty()) return;

        // 피아노 그리기 및 키 위치 넘기기
        piano_.drawOnFrame(frame);
        handDetector_.setKeyPositions(piano_.getKeys());

        // 손가락 포인트 읽기
        auto fingerPoints = handDetector_.getFingerPoints();

        std::set<int> newPlayingWhiteKeys;

        // 손가락으로 "새로 눌린" 화이트키만 전송
        for (const auto& finger : fingerPoints) {
            if (!finger.isActive) continue;
            int rawKeyIndex = -1;
            if (piano_.isPointOverKey(finger.position, rawKeyIndex)) {
                // rawKeyIndex가 0~11(크로매틱)일 가능성이 높다고 가정하고 화이트키로 변환
                int whiteIdx = chromaticToWhite(rawKeyIndex);
                if (whiteIdx >= 0) {
                    newPlayingWhiteKeys.insert(whiteIdx);
                    if (currentlyPlayingWhiteKeys_.find(whiteIdx) == currentlyPlayingWhiteKeys_.end()) {
                        piano_.playKey(rawKeyIndex); // UI는 기존 인덱스로 강조
                        sendPianoMsg(whiteIdx);
                    }
                }
            }
        }

        // 더 이상 눌리지 않은 화이트키는 해제(시각적 효과만)
        for (int w : currentlyPlayingWhiteKeys_) {
            if (newPlayingWhiteKeys.find(w) == newPlayingWhiteKeys.end()) {
                piano_.stopKey(w);
            }
        }
        currentlyPlayingWhiteKeys_ = std::move(newPlayingWhiteKeys);

        // 화면 표시
        cv::imshow("Virtual Piano", frame);
    }
};

int main() {
    try {
        VirtualPianoApp app;
        if (!app.initialize()) {
            std::cerr << "Failed to initialize application" << std::endl;
            return -1;
        }

        std::cout << "Virtual Piano (socket-only) started!\n";
        std::cout << "Controls:\n";
        std::cout << "  White keys: A S D F G H J  (C D E F G A B)\n";
        std::cout << "  Close window or press ESC to exit\n";

        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
