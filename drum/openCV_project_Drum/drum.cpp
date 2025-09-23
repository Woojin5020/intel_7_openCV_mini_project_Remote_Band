#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace cv;
using namespace std;

/* ------------ ROI 정의 (원 2개 + 타원 3개) ------------ */
struct CircleROI { Point c; int r; string name; bool active=false; };
struct EllipseROI{ Point c; Size axes; double ang; string name; bool active=false; };

struct ROIState {
    bool active=false;
    int on_cnt=0, off_cnt=0;
    chrono::steady_clock::time_point last_fire = chrono::steady_clock::now();
};

static inline Point clampPt(Point p, Size sz){
    p.x = max(0, min(p.x, sz.width-1));
    p.y = max(0, min(p.y,  sz.height-1));
    return p;
}

static void putCenteredLabel(Mat& img, const string& text, Point center,
                             double scale=0.9, int thickness=2,
                             const Scalar& color=Scalar(0,255,0)) {
    int baseline = 0;
    Size ts = getTextSize(text, FONT_HERSHEY_SIMPLEX, scale, thickness, &baseline);
    Point org(center.x - ts.width/2, center.y + ts.height/2);
    putText(img, text, org, FONT_HERSHEY_SIMPLEX, scale, color, thickness, LINE_AA);
}

/* -------- 사용자가 맞춘 혼합 ROI (정규화 좌표) -------- */
static void makeMixedROIs(Size sz, vector<CircleROI>& circles, vector<EllipseROI>& ellipses){
    const int W=sz.width, H=sz.height, M=min(W,H);

    // 원(하이탐, 미들탐)
    struct Cn{ float cx,cy,r; const char* n; };
    const Cn cset[] = {
        {0.370f, 0.365f, 0.150f, "Tom-Hi"},
        {0.625f, 0.365f, 0.170f, "Tom-Mid"},
    };
    for (auto&p:cset){
        CircleROI R; R.c = clampPt(Point(int(p.cx*W+0.5f),int(p.cy*H+0.5f)), sz);
        R.r = max(5,int(p.r*M+0.5f)); R.name=p.n; circles.push_back(R);
    }

    // 타원(크래쉬L, 킥, 크래쉬R)
    struct En{ float cx,cy,ax,ay,ang; const char* n; };
    const En eset[] = {
        {0.100f, 0.205f, 0.33f, 0.16f, -1.0f, "Cymbal-L"},
        {0.490f, 0.975f, 0.25f, 0.20f,  0.0f, "Kick"},
        {0.950f, 0.260f, 0.37f, 0.23f, +6.0f, "Cymbal-R"}
    };
    for (auto&p:eset){
        EllipseROI E;
        E.c    = clampPt(Point(int(p.cx*W+0.5f),int(p.cy*H+0.5f)), sz);
        E.axes = Size(max(5,int(p.ax*M+0.5f)), max(5,int(p.ay*M+0.5f)));
        E.ang  = p.ang; E.name=p.n; ellipses.push_back(E);
    }
}

/* ------------- 카메라 열기 유틸(경로/인덱스/자동 스캔 + 폴백) ------------- */
static bool tryOpen(VideoCapture &cap, const string& target, int apiPreference)
{
    if (target.empty()) return false;
    if (cap.open(target, apiPreference)) {
        cerr << "[CAM] Opened " << target << " (API=" << apiPreference << ")\n";
        return true;
    }
    return false;
}

static bool tryOpenIndex(VideoCapture &cap, int idx, int apiPreference)
{
    if (idx < 0) return false;
    if (cap.open(idx, apiPreference)) {
        cerr << "[CAM] Opened index " << idx << " (API=" << apiPreference << ")\n";
        return true;
    }
    return false;
}

static bool openBestCamera(VideoCapture &cap, const string& devPath, int idxHint=-1)
{
    vector<int> apis = { cv::CAP_ANY, cv::CAP_V4L2 };

    // 1) 지정된 devPath 시도 (CAP_ANY → CAP_V4L2)
    if (!devPath.empty()) {
        for (int api : apis) {
            if (tryOpen(cap, devPath, api)) return true;
        }
        cerr << "[CAM] Failed to open explicitly specified device: " << devPath << "\n";
    }

    // 2) 지정된 인덱스 힌트 시도
    if (idxHint >= 0) {
        for (int api : apis) {
            if (tryOpenIndex(cap, idxHint, api)) return true;
        }
    }

    // 3) 자동 스캔: 경로 후보 → 인덱스 후보
    vector<string> candidatesPath = {
        "/dev/video1", "/dev/video2", "/dev/video3", "/dev/video0"
    };
    for (int api : apis) {
        for (const auto& path : candidatesPath) {
            if (tryOpen(cap, path, api)) return true;
        }
    }
    for (int api : apis) {
        for (int i=0; i<10; ++i) {
            if (tryOpenIndex(cap, i, api)) return true;
        }
    }
    return false;
}

/* ------------------------------- 메인 ------------------------------- */
int main(int argc, char** argv){
    // 사용법 안내
    // argv[1] : server_ip (기본 127.0.0.1)
    // argv[2] : server_port (기본 5000)
    // argv[3] : background image path (기본 "drum2.png")
    // argv[4] : camera device path or index 문자열 (예: "/dev/video1" 또는 "1") (기본 빈값 = 자동 탐색)
    string server_ip    = (argc>=2)? argv[1] : "10.10.16.55";
    int    server_port  = (argc>=3)? atoi(argv[2]) : 5000;
    string img_path     = (argc>=4)? argv[3] : "drum2.png";
    string cam_arg      = (argc>=5)? argv[4] : ""; // "/dev/video1" 권장
    int maxW = (argc>=6)? atoi(argv[5]) : 960;          // 가로 최대폭(축소)

    Mat drum = imread(img_path);
    if (drum.empty()){ cerr<<"cannot read "<<img_path<<"\n"; return -1; }
    if (drum.cols>maxW){ double s=double(maxW)/drum.cols; resize(drum,drum,Size(),s,s,INTER_AREA); }

    // TCP 연결
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    // 소켓 옵션 (지연/끊김 완화)
    {
        int flag = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
        int ka=1, idle=30, intvl=10, cnt=3;
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &ka, sizeof(ka));
#ifdef TCP_KEEPIDLE
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof(idle));
#endif
#ifdef TCP_KEEPINTVL
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
#endif
#ifdef TCP_KEEPCNT
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof(cnt));
#endif
    }

    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip.c_str(), &serv.sin_addr) != 1) {
        cerr << "Invalid server IP: " << server_ip << "\n";
        ::close(sock);
        return -1;
    }

    // 연결 재시도(최대 5회)
    bool connected=false;
    for (int i=0;i<5;i++){
        if (::connect(sock, (sockaddr*)&serv, sizeof(serv)) == 0) { connected=true; break; }
        perror("connect");
        this_thread::sleep_for(chrono::milliseconds(500));
    }
    if (!connected) {
        cerr << "[NET] Connect failed to " << server_ip << ":" << server_port << "\n";
        ::close(sock);
        return -1;
    }
    cout << "[NET] Connected to " << server_ip << ":" << server_port << endl;

    // 로그인 (개행 추가 필수!)
    {
        string login = "DRUM:PASSWD\n"; // 서버가 라인 파싱할 때 필요
        ssize_t n = ::send(sock, login.c_str(), login.size(), 0);
        if (n < 0) perror("send(login)");
    }


    // ROI 준비
    vector<CircleROI> cROIs; vector<EllipseROI> eROIs;
    makeMixedROIs(drum.size(), cROIs, eROIs);

    vector<Mat> masks; vector<int> areas;
    for (auto&r:cROIs){
        Mat m=Mat::zeros(drum.size(),CV_8U);
        circle(m,r.c,r.r,Scalar(255),FILLED,LINE_AA);
        masks.push_back(m); areas.push_back(countNonZero(m));
    }
    for (auto&e:eROIs){
        Mat m=Mat::zeros(drum.size(),CV_8U);
        ellipse(m,e.c,e.axes,e.ang,0,360,Scalar(255),FILLED,LINE_AA);
        masks.push_back(m); areas.push_back(countNonZero(m));
    }

    // ROI 상태 5개
    vector<ROIState> states(5);

    // 카메라 열기
    VideoCapture cap;
    int idxHint = -1;
    string devPath;
    if (!cam_arg.empty()) {
        // 숫자면 인덱스 힌트, 아니면 경로로 간주
        bool isNumber = !cam_arg.empty() && all_of(cam_arg.begin(), cam_arg.end(), ::isdigit);
        if (isNumber) idxHint = stoi(cam_arg);
        else devPath = cam_arg;
    }

    if (!openBestCamera(cap, devPath, idxHint)) {
        cerr<<"camera open fail (tried devPath='"<<devPath<<"', idxHint="<<idxHint<<")\n";
        ::close(sock);
        return -1;
    }
    cap.set(CAP_PROP_FPS,30);

    Ptr<BackgroundSubtractorMOG2> bg=createBackgroundSubtractorMOG2(500,20,true);
    bg->setDetectShadows(false);

    int varThr=20;
    const int enter_frames=2, exit_frames=3, cooldown_ms=120;
    int minArea=80, maxArea=5000;

    Mat cam,camRsz,fg,vis;

    cout << "[INFO] Press 'q' or ESC to quit.\n";
    while(true){
        if(!cap.read(cam)||cam.empty()) break;
        resize(cam,camRsz,drum.size());

        Mat gray; cvtColor(camRsz,gray,COLOR_BGR2GRAY);
        GaussianBlur(gray,gray,Size(5,5),0);
        bg->setVarThreshold(varThr);
        bg->apply(gray,fg);

        Mat pen; threshold(fg,pen,200,255,THRESH_BINARY);

        vis=drum.clone(); 
        camRsz.copyTo(vis,pen);

        double thr=0.03;
        size_t idx=0;

        // ROI 체크
        for(size_t i=0;i<masks.size();++i,++idx){
            Mat inter; bitwise_and(pen, masks[i], inter);
            double frac = (areas[i]>0)? (double)countNonZero(inter)/areas[i] : 0.0;
            bool hit = (frac >= thr);

            if (hit){
                states[idx].on_cnt++; 
                states[idx].off_cnt=0;
                if (!states[idx].active && states[idx].on_cnt>=enter_frames){
                    states[idx].active = true;
                    auto now=chrono::steady_clock::now();
                    int ms = (int)chrono::duration_cast<chrono::milliseconds>(now - states[idx].last_fire).count();
                    if (ms >= cooldown_ms){
                        string msg = "[DRUM]"+to_string(idx) + "\n"; // 개행 추가 권장
                        ssize_t n = ::send(sock, msg.c_str(), msg.size(), 0);
                        if (n < 0) perror("send([DRUM])");
                        else cout << "[NET] Sent: " << msg;
                        states[idx].last_fire = now;
                    }
                }
            } else {
                states[idx].off_cnt++; 
                states[idx].on_cnt=0;
                if (states[idx].active && states[idx].off_cnt>=exit_frames) 
                    states[idx].active=false;
            }

            Scalar col = states[idx].active? Scalar(0,0,255):Scalar(0,255,0);
            if (i<2) circle(vis, cROIs[i].c, cROIs[i].r, col, 4, LINE_AA);
            else     ellipse(vis, eROIs[i-2].c, eROIs[i-2].axes, eROIs[i-2].ang, 0, 360, col, 4, LINE_AA);
        }

        // (선택) 각 ROI 중앙에 라벨 표시
        for (int i=0;i<2;i++) putCenteredLabel(vis, cROIs[i].name, cROIs[i].c, 0.6, 2, Scalar(0,255,255));
        putCenteredLabel(vis, eROIs[0].name, eROIs[0].c, 0.6, 2, Scalar(0,255,255));
        putCenteredLabel(vis, eROIs[1].name, eROIs[1].c, 0.6, 2, Scalar(0,255,255));
        putCenteredLabel(vis, eROIs[2].name, eROIs[2].c, 0.6, 2, Scalar(0,255,255));

        imshow("Drum Client", vis);
        int key=waitKey(1);
        if(key==27||key=='q') break;
    }

    ::close(sock);
    return 0;
}
