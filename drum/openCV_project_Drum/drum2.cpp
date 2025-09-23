#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

using namespace cv;
using namespace std;

struct DrumROI {
    Rect box;
    string name;
    string sound;

    // 상태
    bool active=false;                 // 시각화용(빨강)
    int on_cnt=0, off_cnt=0;

    // 스트라이크 검출용
    double areaEMA=0.0, prevEMA=0.0;   // 면적 지수평활
    Point  prevC = {-1,-1};            // 이전 프레임 손 중심
    chrono::steady_clock::time_point lastFire = chrono::steady_clock::now();
};

static inline Rect clampRect(const Rect& r, const Size& sz){
    int x=r.x, y=r.y, w=r.width, h=r.height;
    x = max(0, min(x, sz.width-1));
    y = max(0, min(y, sz.height-1));
    if (x + w > sz.width)  w = sz.width  - x;
    if (y + h > sz.height) h = sz.height - y;
    return Rect(x,y,w,h);
}

static inline void playSoundAsync(const string& wav){
    std::thread([wav](){
        string cmd = "aplay -q \"" + wav + "\" >/dev/null 2>&1";
        (void)system(cmd.c_str());
    }).detach();
}

// ---------- 드럼 이미지에서 ROI 자동 추출(요약형) ----------
static Rect circleToBox(const Vec3f& c, float scale=1.15f){
    Point center(cvRound(c[0]), cvRound(c[1]));
    int r = cvRound(c[2]*scale);
    return Rect(center.x - r, center.y - r, 2*r, 2*r);
}
static vector<Rect> detectROIs(const Mat& img){
    vector<Rect> result;
    int W=img.cols, H=img.rows;

    // 심벌(노란색)
    Mat hsv, ymask; cvtColor(img, hsv, COLOR_BGR2HSV);
    inRange(hsv, Scalar(15,60,60), Scalar(40,255,255), ymask);
    morphologyEx(ymask, ymask, MORPH_OPEN,  getStructuringElement(MORPH_ELLIPSE, Size(5,5)));
    morphologyEx(ymask, ymask, MORPH_CLOSE, getStructuringElement(MORPH_ELLIPSE, Size(7,7)));
    vector<vector<Point>> cnts; findContours(ymask, cnts, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    vector<Rect> cym;
    for (auto& c: cnts){
        if (contourArea(c) < (W*H*0.002)) continue;
        Rect r = boundingRect(c); if (r.y > H*0.65) continue;
        cym.push_back(r);
    }
    if (!cym.empty()){
        sort(cym.begin(), cym.end(), [](const Rect& a, const Rect& b){ return a.x < b.x; });
        result.push_back(cym.front());
        if ((int)cym.size()>=2) result.push_back(cym.back());
    }

    // 드럼헤드(원)
    Mat g; cvtColor(img, g, COLOR_BGR2GRAY); GaussianBlur(g, g, Size(7,7), 1.5);
    vector<Vec3f> circles;
    HoughCircles(g, circles, HOUGH_GRADIENT, 1.2, (double)H/10.0, 100, 30,
                 (int)(min(W,H)*0.07), (int)(min(W,H)*0.22));
    vector<Vec3f> mids;
    for (auto& c: circles){
        int cx=cvRound(c[0]), cy=cvRound(c[1]); int r=cvRound(c[2]);
        if (cy < (int)(H*0.20) || cy > (int)(H*0.75)) continue;
        Rect roi = clampRect(Rect(cx-r, cy-r, 2*r, 2*r), img.size());
        if (roi.width<10 || roi.height<10) continue;
        if (mean(g(roi))[0] < 140) continue;
        mids.push_back(c);
    }
    sort(mids.begin(), mids.end(), [](const Vec3f& a, const Vec3f& b){ return a[0] < b[0]; });
    for (int i=0; i<(int)mids.size() && (int)result.size()<5; ++i)
        result.push_back(clampRect(circleToBox(mids[i]), img.size()));

    if ((int)result.size() < 5){
        vector<Rect> fb = {
            Rect((int)(W*0.06),(int)(H*0.06),(int)(W*0.26),(int)(H*0.36)),
            Rect((int)(W*0.28),(int)(H*0.34),(int)(W*0.20),(int)(H*0.24)),
            Rect((int)(W*0.45),(int)(H*0.29),(int)(W*0.18),(int)(H*0.28)),
            Rect((int)(W*0.62),(int)(H*0.34),(int)(W*0.22),(int)(H*0.24)),
            Rect((int)(W*0.76),(int)(H*0.06),(int)(W*0.20),(int)(H*0.36))
        };
        for (auto& r: fb) result.push_back(clampRect(r, img.size()));
        if ((int)result.size()>5) result.resize(5);
    } else if ((int)result.size() > 5){
        sort(result.begin(), result.end(), [](const Rect& a, const Rect& b){ return a.y < b.y; });
        Rect top1=result[0], top2=result[1];
        vector<Rect> rest(result.begin()+2, result.end());
        sort(rest.begin(), rest.end(), [](const Rect& a, const Rect& b){ return a.x < b.x; });
        vector<Rect> picked{top1};
        for (int i=0; i<(int)rest.size() && (int)picked.size()<4; ++i) picked.push_back(rest[i]);
        picked.push_back(top2);
        if ((int)picked.size()>5) picked.resize(5);
        result = picked;
    }
    if ((int)result.size()>5) result.resize(5);
    return result;
}

// 가장 큰 컨투어 무게중심
static Point largestCentroid(const Mat& mask){
    vector<vector<Point>> cs; findContours(mask, cs, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    double best=0; Point out(-1,-1);
    for (auto& cc: cs){ double a=contourArea(cc); if (a>best){ Moments m=moments(cc); if (m.m00) out = Point((int)(m.m10/m.m00), (int)(m.m01/m.m00)); best=a; } }
    return out;
}

int main(int argc, char** argv){
    string img_path = (argc>=2)? argv[1] : "drum.jpg";
    Mat drum = imread(img_path);
    if (drum.empty()){ cerr<<"cannot read image\n"; return -1; }

    // ROI & 사운드
    vector<Rect> boxes = detectROIs(drum);
    vector<DrumROI> rois = {
        {boxes[0], "Cymbal-L", "sounds/cymbal1.wav"},
        {boxes[1], "Tom-Hi",   "sounds/tom1.wav"},
        {boxes[2], "Snare",    "sounds/snare.wav"},
        {boxes[3], "Tom-Low",  "sounds/tom2.wav"},
        {boxes[4], "Cymbal-R", "sounds/cymbal2.wav"}
    };

    VideoCapture cap; if (!cap.open(0, cv::CAP_V4L2)){ cerr<<"camera open fail\n"; return -1; }
    cap.set(CAP_PROP_FPS, 30);

    Ptr<BackgroundSubtractorMOG2> bg = createBackgroundSubtractorMOG2(500, 20, true);
    bg->setDetectShadows(false);

    // --- 스트라이크 튜닝 슬라이더 ---
    namedWindow("StrikeTune");
    int A_rise = 10;   createTrackbar("A_rise %", "StrikeTune", &A_rise, 50); // 상승률
    int A_fall = 12;   createTrackbar("A_fall %", "StrikeTune", &A_fall, 50); // 하강률
    int vy_thr = 12;   createTrackbar("vy_thr px", "StrikeTune", &vy_thr, 50);// 아래속도
    int coolms = 120;  createTrackbar("cool_ms",   "StrikeTune", &coolms, 500);

    int blurK=2, opK=1, dilK=1;
    createTrackbar("blur k", "StrikeTune", &blurK, 6);
    createTrackbar("open k", "StrikeTune", &opK, 6);
    createTrackbar("dil k",  "StrikeTune", &dilK, 6);

    bool showMask=false;
    cout<<"Keys: b=bg reset, m=mask toggle, q/ESC=quit\n";

    Mat cam, camRsz, fg, vis;
    while (true){
        if (!cap.read(cam) || cam.empty()) break;
        resize(cam, camRsz, drum.size());

        // 전처리 + 마스크
        int ksz = max(1, 2*blurK+1);
        Mat gray; cvtColor(camRsz, gray, COLOR_BGR2GRAY);
        GaussianBlur(gray, gray, Size(ksz,ksz), 0);
        bg->apply(gray, fg, -1);
        if (opK>0){ Mat k=getStructuringElement(MORPH_ELLIPSE, Size(2*opK+1,2*opK+1)); morphologyEx(fg, fg, MORPH_OPEN, k); }
        if (dilK>0){ Mat k=getStructuringElement(MORPH_ELLIPSE, Size(2*dilK+1,2*dilK+1)); dilate(fg, fg, k); }

        vis = drum.clone();
        camRsz.copyTo(vis, fg); // 손만 합성

        Point wholeC = largestCentroid(fg);
        if (wholeC.x>=0){ circle(vis, wholeC, 5, Scalar(0,255,255), FILLED); drawMarker(vis, wholeC, Scalar(0,0,0), MARKER_CROSS, 12, 2); }

        for (auto& r: rois){
            Rect R = clampRect(r.box, vis.size());
            Mat m = fg(R);
            int area = countNonZero(m);               // 현재 면적
            if (r.areaEMA==0.0) r.areaEMA = area;     // 초기화

            // 지수평활 + 도함수 근사
            r.prevEMA = r.areaEMA;
            r.areaEMA = 0.8*r.areaEMA + 0.2*area;
            double dA = (r.areaEMA - r.prevEMA);      // 상승(+)/하강(-)

            // 손 중심 속도(ROI 내부만)
            Point c = largestCentroid(m);
            double vy = 0.0;
            if (c.x>=0){
                Point cg = c + Point(R.x, R.y);       // 전역 좌표
                if (r.prevC.x>=0) vy = (double)(cg.y - r.prevC.y); // +면 아래로
                r.prevC = cg;
            } else {
                r.prevC = {-1,-1};
            }

            // 면적의 상대 임계 계산(ROI 크기 기준)
            double A_rise_thr = (A_rise/100.0) * (double)R.area();
            double A_fall_thr = (A_fall/100.0) * (double)R.area();

            // ----- 스트라이크 로직 -----
            bool approach = (vy > vy_thr);            // 아래로 빠르게 접근
            bool contact  = (dA >  A_rise_thr);       // 면적 급상승
            bool release  = (dA < -A_fall_thr);       // 면적 급하강

            // 시각화 활성(빨강) 조건: contact 직후 일정 프레임 유지
            if (contact) { r.active = true; r.on_cnt=4; }
            if (r.on_cnt>0) r.on_cnt--; else r.active=false;

            // '한 번만' 때리기: contact 뒤 release가 오면 히트 확정
            static const int DEBOUNCE = 3;
            if (contact && approach){
                // 후보 시작
                r.off_cnt = DEBOUNCE; // release 대기 카운터
            } else if (r.off_cnt>0){
                if (release){
                    // 쿨다운 체크
                    auto now = chrono::steady_clock::now();
                    int ms = (int)chrono::duration_cast<chrono::milliseconds>(now - r.lastFire).count();
                    if (ms >= coolms){
                        playSoundAsync(r.sound);
                        r.lastFire = now;
                    }
                    r.off_cnt = 0; // consume
                } else {
                    r.off_cnt--;
                }
            }

            // 그리기
            Scalar col = r.active ? Scalar(0,0,255) : Scalar(0,255,0);
            rectangle(vis, R, col, 3);
            char buf[64];
            snprintf(buf, sizeof(buf), "vy=%.0f dA=%.0f", vy, dA);
            putText(vis, r.name, Point(R.x+6, R.y+20), FONT_HERSHEY_SIMPLEX, 0.6, col, 2, LINE_AA);
            putText(vis, buf,    Point(R.x+6, R.y+40), FONT_HERSHEY_SIMPLEX, 0.5, col, 1, LINE_AA);
        }

        putText(vis, "b: reset bg | m: mask | q/ESC: quit",
                Point(10, vis.rows-10), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255,255,255), 1, LINE_AA);

        imshow("Drum Trigger (Strike)", vis);
        if (showMask) imshow("Mask", fg); else destroyWindow("Mask");

        int key = waitKey(1);
        if (key==27 || key=='q') break;
        if (key=='m') showMask = !showMask;
        if (key=='b'){ bg = createBackgroundSubtractorMOG2(500, 20, true); bg->setDetectShadows(false); cout<<"BG reset\n"; }
    }
    return 0;
}
