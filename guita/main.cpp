#include <opencv2/opencv.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdio>

// POSIX socket
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace cv;
using namespace std;

// ====== 네트워크/로그인 설정(필요시 수정) ======
static string SERVER_IP   = "127.0.0.1";
static int    SERVER_PORT = 5000;
static string CLIENT_ID   = "GUITA";
static string CLIENT_PW   = "PASSWD";

// ====== 카메라 설정 ======
static const vector<int> PREFERRED_INDEXES = {1, 2, 0}; // 당신 환경: 1,2가 실제 캠, 0은 Iriun
static const int MAX_INDEX_SCAN = 10;

static const int REQ_W = 1280;
static const int REQ_H = 720;
static const int REQ_FPS = 30;

// 모션/트리거 파라미터
static const int  MOTION_BIN_THR    = 18;           // 임계(0~255)
static const Size MORPH_KERNEL_SIZE = Size(5, 5);   // 모폴로지
static const double MOTION_AREA_THR = 0.004;        // ROI 면적 대비 모션 비율(0~1)
static const double COOLDOWN_SEC    = 1.0;          // 존별 쿨다운 (초)

// 라벨(존) 매핑: 0->G, 1->D, 2->C
static array<string,3> ZONE_LABELS = { "G", "D", "C" };

// -----------------------------------------------
// 유틸
// -----------------------------------------------
static VideoCapture open_first_working_camera(int &used_idx) {
    // 우선순위 인덱스
    for (int idx : PREFERRED_INDEXES) {
        VideoCapture cap(idx, CAP_V4L2);
        if (cap.isOpened()) {
            Mat f; cap.read(f);
            if (!f.empty()) { used_idx = idx; return cap; }
        }
    }
    // 자동 스캔
    for (int idx = 0; idx < MAX_INDEX_SCAN; ++idx) {
        if (find(PREFERRED_INDEXES.begin(), PREFERRED_INDEXES.end(), idx) != PREFERRED_INDEXES.end())
            continue;
        VideoCapture cap(idx, CAP_V4L2);
        if (cap.isOpened()) {
            Mat f; cap.read(f);
            if (!f.empty()) { used_idx = idx; return cap; }
        }
    }
    used_idx = -1;
    return VideoCapture();
}

static vector<Rect> layout_three_horizontal(int W, int H, int margin = 10) {
    int ww = (W - margin * 4) / 3;
    int hh = H - margin * 2;
    int y  = margin;
    int x1 = margin;
    int x2 = margin * 2 + ww;
    int x3 = margin * 3 + ww * 2;
    return { Rect(x1, y, ww, hh),  // 0: G (왼쪽)
             Rect(x2, y, ww, hh),  // 1: D (가운데)
             Rect(x3, y, ww, hh)   // 2: C (오른쪽)
    };
}

static double now_sec() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

// -----------------------------------------------
// TCP 클라이언트
// -----------------------------------------------
static int tcp_connect_and_login(const string& ip, int port, const string& id, const string& pw) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[ERR] socket");
        return -1;
    }
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        cerr << "[ERR] inet_pton: " << ip << endl;
        close(sock);
        return -1;
    }
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[ERR] connect");
        close(sock);
        return -1;
    }

    // 서버는 최초에 "id:pw" 를 읽어 인증
    string login = id + ":" + pw + "\n";
    ssize_t n = send(sock, login.c_str(), (int)login.size(), 0);
    if (n <= 0) {
        perror("[ERR] send(login)");
        close(sock);
        return -1;
    }
    cout << "[NET] Connected & sent login for ID=" << id << endl;
    return sock;
}

static bool tcp_send_guita(int sock, const string& label_one_char /* "G"|"D"|"C" */) {
    // 서버 파서가 "[:]" 를 구분자로 쓰므로, "[GUITA]G\n" 형태가 안전
    string msg = "[GUITA]" + label_one_char + "\n";
    ssize_t n = send(sock, msg.c_str(), (int)msg.size(), 0);
    if (n <= 0) {
        perror("[ERR] send([GUITA]X)");
        return false;
    }
    return true;
}

// -----------------------------------------------
// 메인
// -----------------------------------------------
int main(int argc, char** argv) {
    // 명령행 인자: ip port id pw
    if (argc >= 2) SERVER_IP   = argv[1];
    if (argc >= 3) SERVER_PORT = atoi(argv[2]);
    if (argc >= 4) CLIENT_ID   = argv[3];
    if (argc >= 5) CLIENT_PW   = argv[4];

    // ---- 서버 접속 & 로그인 ----
    int sock = tcp_connect_and_login(SERVER_IP, SERVER_PORT, CLIENT_ID, CLIENT_PW);
    if (sock < 0) {
        cerr << "[FATAL] 서버 접속 실패" << endl;
        return 1;
    }

    // ---- 카메라 열기 ----
    int used_idx = -1;
    VideoCapture cap = open_first_working_camera(used_idx);
    if (!cap.isOpened()) {
        cerr << "[ERR] 카메라를 열 수 없습니다." << endl;
        close(sock);
        return 1;
    }
    cout << "[OK] Using /dev/video" << used_idx << endl;

    cap.set(CAP_PROP_FRAME_WIDTH,  REQ_W);
    cap.set(CAP_PROP_FRAME_HEIGHT, REQ_H);
    cap.set(CAP_PROP_FPS,          REQ_FPS);

    int W = (int)cap.get(CAP_PROP_FRAME_WIDTH);
    int H = (int)cap.get(CAP_PROP_FRAME_HEIGHT);
    if (W <= 0) W = REQ_W;
    if (H <= 0) H = REQ_H;

    // ---- 3개 존 배치 ----
    vector<Rect> zones = layout_three_horizontal(W, H, 10);

    // ---- 모션 비교용 이전 프레임(그레이) ----
    Mat prev_gray;
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, MORPH_KERNEL_SIZE);

    array<double, 3> last_ts = {0.0, 0.0, 0.0};

    cout << "[INFO] q: 종료 | r: 배경(비교 기준) 리셋" << endl;
    cout << "[INFO] 서버: " << SERVER_IP << ":" << SERVER_PORT
         << "  ID=" << CLIENT_ID << endl;

    for (;;) {
        Mat frame;
        if (!cap.read(frame) || frame.empty()) {
            cerr << "[WARN] 프레임 읽기 실패…" << endl;
            waitKey(20);
            continue;
        }

        Mat gray_full; cvtColor(frame, gray_full, COLOR_BGR2GRAY);
        GaussianBlur(gray_full, gray_full, Size(5,5), 0);

        if (prev_gray.empty())
            prev_gray = gray_full.clone();

        double tnow = now_sec();

        // 각 존 처리
        for (int i = 0; i < 3; ++i) {
            Rect r = zones[i];
            r &= Rect(0,0,W,H); // 안전 보정

            Mat cur_roi  = gray_full(r);
            Mat prev_roi = prev_gray(r);

            Mat diff; absdiff(cur_roi, prev_roi, diff);
            Mat motion;
            threshold(diff, motion, MOTION_BIN_THR, 255, THRESH_BINARY);
            morphologyEx(motion, motion, MORPH_OPEN, kernel, Point(-1,-1), 1);
            dilate(motion, motion, kernel, Point(-1,-1), 1);

            double motion_ratio = (double)countNonZero(motion) / max(1, motion.rows * motion.cols);
            bool cooled = (tnow - last_ts[i]) >= COOLDOWN_SEC;

            // 사각형/라벨(디스플레이용 오버레이만 유지)
            Scalar rectColor = (motion_ratio < MOTION_AREA_THR) ? Scalar(0,255,0) : Scalar(0,150,255);
            rectangle(frame, r, rectColor, 3);

            int font = FONT_HERSHEY_SIMPLEX;
            double scale = 2.0;
            int thickness = 4;
            int baseline = 0;
            Size tsz = getTextSize(ZONE_LABELS[i], font, scale, thickness, &baseline);
            Point center(r.x + r.width/2 - tsz.width/2, r.y + r.height/2 + tsz.height/2);
            putText(frame, ZONE_LABELS[i], center, font, scale, Scalar(255,255,255), thickness);

            string info = ZONE_LABELS[i] + "  m=" + to_string(motion_ratio).substr(0,5);
            putText(frame, info, Point(r.x+12, r.y+36), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(240,240,240), 2);

            // 트리거 → 서버로 [GUITA]X 전송
            if (motion_ratio >= MOTION_AREA_THR && cooled) {
                last_ts[i] = tnow;
                putText(frame, "TRIGGER " + ZONE_LABELS[i], Point(r.x+12, r.y+66),
                        FONT_HERSHEY_SIMPLEX, 0.9, Scalar(0,215,255), 2);

                if (!tcp_send_guita(sock, ZONE_LABELS[i])) {
                    cerr << "[WARN] 서버 전송 실패. 재접속 시도…" << endl;
                    close(sock);
                    sock = tcp_connect_and_login(SERVER_IP, SERVER_PORT, CLIENT_ID, CLIENT_PW);
                    if (sock >= 0) {
                        // 재접속 성공 시 한번 더 시도(실패해도 진행)
                        tcp_send_guita(sock, ZONE_LABELS[i]);
                    }
                } else {
                    cout << "[NET] Sent: [GUITA]" << ZONE_LABELS[i] << endl;
                }
            }
        }

        // 하단 정보
        string info1 = "/dev/video" + to_string(used_idx) + "  " + to_string(W) + "x" + to_string(H) +
                       "  cooldown=" + to_string(COOLDOWN_SEC).substr(0,4) + "s";
        string info2 = "thr=" + to_string(MOTION_AREA_THR).substr(0,6) + "  bin_thr=" + to_string(MOTION_BIN_THR);
        putText(frame, info1, Point(10, H-40), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(50,230,50), 2);
        putText(frame, info2, Point(10, H-12), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(180,180,180), 2);

        imshow("G D C Motion -> [GUITA]X to Server (q/r)", frame);

        int k = waitKey(1) & 0xFF;
        if (k == 'q') break;
        else if (k == 'r') prev_gray.release();

        prev_gray = gray_full.clone();
    }

    // 정리
    if (sock >= 0) close(sock);
    return 0;
}
