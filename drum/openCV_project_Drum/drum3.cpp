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
    bool active = false;
    int on_cnt = 0, off_cnt = 0;
    chrono::steady_clock::time_point last_fire = chrono::steady_clock::now();
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
    thread([wav](){
        string cmd = "aplay -q \"" + wav + "\" >/dev/null 2>&1";
        (void)system(cmd.c_str());
    }).detach();
}

// --- 드럼 이미지에서 ROI 자동 추출 (노랑 HSV + HoughCircles) ---
static Rect circleToBox(const Vec3f& c, float scale=1.15f){
    Point center(cvRound(c[0]), cvRound(c[1]));
    int r = cvRound(c[2]*scale);
    return Rect(center.x - r, center.y - r, 2*r, 2*r);
}
static vector<Rect> detectROIsFromImage(const Mat& img){
    vector<Rect> result;
    int W = img.cols, H = img.rows;

    Mat hsv, ymask; cvtColor(img, hsv, COLOR_BGR2HSV);
    inRange(hsv, Scalar(15,60,60), Scalar(40,255,255), ymask);
    morphologyEx(ymask, ymask, MORPH_OPEN,  getStructuringElement(MORPH_ELLIPSE, Size(5,5)));
    morphologyEx(ymask, ymask, MORPH_CLOSE, getStructuringElement(MORPH_ELLIPSE, Size(7,7)));

    vector<vector<Point>> cnts; findContours(ymask, cnts, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    vector<Rect> cym;
    for (auto& c: cnts){
        double a = contourArea(c);
        if (a < (W*H*0.002)) continue;
        Rect r = boundingRect(c);
        if (r.y > H*0.65) continue;
        cym.push_back(r);
    }
    if (!cym.empty()){
        sort(cym.begin(), cym.end(), [](const Rect& a, const Rect& b){ return a.x < b.x; });
        result.push_back(cym.front());
        if ((int)cym.size()>=2) result.push_back(cym.back());
    }

    Mat gray; cvtColor(img, gray, COLOR_BGR2GRAY);
    GaussianBlur(gray, gray, Size(7,7), 1.5);
    vector<Vec3f> circles;
    HoughCircles(gray, circles, HOUGH_GRADIENT, 1.2, (double)H/10.0, 100, 30,
                 (int)(min(W,H)*0.07), (int)(min(W,H)*0.22));
    vector<Vec3f> mids;
    for (auto& c: circles){
        int cx=cvRound(c[0]), cy=cvRound(c[1]); int r=cvRound(c[2]);
        if (cy < (int)(H*0.20) || cy > (int)(H*0.75)) continue;
        Rect roi = clampRect(Rect(cx-r, cy-r, 2*r, 2*r), img.size());
        if (roi.width<10 || roi.height<10) continue;
        if (mean(gray(roi))[0] < 140) continue;
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

static Point largestCentroid(const Mat& mask){
    vector<vector<Point>> cs; findContours(mask, cs, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    double best=0; Point out(-1,-1);
    for (auto& c: cs){ double a=contourArea(c); if (a>best){ Moments m=moments(c); if (m.m00) out=Point((int)(m.m10/m.m00),(int)(m.m01/m.m00)); best=a; } }
    return out;
}

int main(int argc, char** argv){
    string img_path = (argc>=2)? argv[1] : "drum.jpg";
    Mat drum = imread(img_path);
    if (drum.empty()){ std::cerr<<"cannot read image\n"; return -1; }

    // ROI
    vector<Rect> boxes = detectROIsFromImage(drum);
    vector<DrumROI> rois = {
        {boxes[0], "Cymbal-L", "sounds/cymbal1.wav"},
        {boxes[1], "Tom-Hi",   "sounds/tom1.wav"   },
        {boxes[2], "Snare",    "sounds/snare.wav"  },
        {boxes[3], "Tom-Low",  "sounds/tom2.wav"   },
        {boxes[4], "Cymbal-R", "sounds/cymbal2.wav"}
    };

    // 카메라
    VideoCapture cap; if (!cap.open(0, cv::CAP_V4L2)){ cerr<<"camera open fail\n"; return -1; }
    cap.set(CAP_PROP_FPS, 30);

    // 배경 분리기
    Ptr<BackgroundSubtractorMOG2> bg = createBackgroundSubtractorMOG2(500, 20, true);
    bg->setDetectShadows(false);

    // ---- 튜닝 슬라이더 (뚜껑=흰색만) ----
    namedWindow("CapTune", WINDOW_AUTOSIZE);
    int varThr = 20;   createTrackbar("varThr",   "CapTune", &varThr, 100); // MOG2 감도
    int lr1000 = 0;    createTrackbar("lr x1000", "CapTune", &lr1000, 100); // 0=auto, >0 => lr/1000
    int thPercent= 3;  createTrackbar("thresh %", "CapTune", &thPercent, 50);

    // HSV: 흰 = S 낮고 V 높음
    int whiteSmax = 60;  createTrackbar("white Smax", "CapTune", &whiteSmax, 255);
    int whiteVmin = 180; createTrackbar("white Vmin", "CapTune", &whiteVmin, 255);

    // 연결요소 면적 필터(픽셀) : 너무 작은 잡음/너무 큰 종이 제거
    int minArea = 80;   createTrackbar("min area", "CapTune", &minArea, 10000);
    int maxArea = 4000; createTrackbar("max area", "CapTune", &maxArea, 50000);

    int blurK=2;  createTrackbar("blur k",   "CapTune", &blurK, 6);   // 0..6 -> (2k+1)
    int openK=1;  createTrackbar("open k",   "CapTune", &openK, 6);
    int dilK=1;   createTrackbar("dilate k", "CapTune", &dilK,  6);

    bool show_mask=false;
    const int enter_frames=2, exit_frames=3, cooldown_ms=120;

    cout<<"Tips: 펜 치우고 b로 배경 리셋 → whiteSmax↓ / whiteVmin↑ 로 흰뚜껑만 잡히게 조정\n"
          "Keys: b=bg reset, m=mask toggle, q/ESC=quit\n";

    Mat cam, camRsz, fg, vis;
    while (true){
        if (!cap.read(cam) || cam.empty()) break;
        resize(cam, camRsz, drum.size(), 0,0, INTER_LINEAR);

        // --- 전처리 & 배경분리 ---
        int ksz = max(1, 2*blurK+1);
        Mat gray; cvtColor(camRsz, gray, COLOR_BGR2GRAY);
        GaussianBlur(gray, gray, Size(ksz, ksz), 0);
        bg->setVarThreshold(max(1, varThr));
        double lr = (lr1000==0)? -1.0 : (lr1000/1000.0);
        bg->apply(gray, fg, lr);

        // --- 흰 뚜껑 마스크 (HSV) ---
        Mat hsv; cvtColor(camRsz, hsv, COLOR_BGR2HSV);
        Mat whiteMask;
        inRange(hsv,
                Scalar(0, 0,           whiteVmin),   // H:any, S:0..whiteSmax, V:whiteVmin..255
                Scalar(180, whiteSmax, 255),
                whiteMask);

        // ---- 움직임과 교집합(정적 하이라이트 제거) ----
        Mat capMask;
        bitwise_and(whiteMask, fg, capMask);

        // ---- 형태학 보정 ----
        if (openK>0){
            Mat k = getStructuringElement(MORPH_ELLIPSE, Size(2*openK+1, 2*openK+1));
            morphologyEx(capMask, capMask, MORPH_OPEN, k);
        }
        if (dilK>0){
            Mat k = getStructuringElement(MORPH_ELLIPSE, Size(2*dilK+1, 2*dilK+1));
            dilate(capMask, capMask, k);
        }

        // ---- 면적 필터로 '뚜껑 크기'만 통과 ----
        Mat labels, stats, cents;
        int n = connectedComponentsWithStats(capMask, labels, stats, cents, 8, CV_32S);
        Mat capMaskFiltered = Mat::zeros(capMask.size(), CV_8U);
        for (int i=1; i<n; ++i){ // 0은 배경
            int area = stats.at<int>(i, CC_STAT_AREA);
            if (area >= minArea && area <= maxArea){
                Mat sel = (labels == i);
                capMaskFiltered.setTo(255, sel);
            }
        }

        // --- 드럼 이미지에 '뚜껑만' 합성 ---
        vis = drum.clone();
        camRsz.copyTo(vis, capMaskFiltered);

        // 뚜껑 중심(가장 큰 컴포넌트) 표시
        Point c = largestCentroid(capMaskFiltered);
        if (c.x>=0){
            circle(vis, c, 6, Scalar(0,255,255), FILLED);
            drawMarker(vis, c, Scalar(0,0,0), MARKER_CROSS, 12, 2);
        }

        // --- ROI 판정/시각화 (흰 뚜껑 기준) ---
        double thresh_motion = thPercent/100.0;
        for (auto& r: rois){
            Rect R = clampRect(r.box, vis.size());
            if (R.width<5 || R.height<5) continue;

            Mat roiMask = capMaskFiltered(R);
            int nonz = countNonZero(roiMask);
            double motion = (double)nonz / (double)R.area();
            bool hit = (motion >= thresh_motion);

            if (hit){ r.on_cnt++; r.off_cnt=0;
                if (!r.active && r.on_cnt>=enter_frames){
                    r.active = true;
                    auto now = chrono::steady_clock::now();
                    if (chrono::duration_cast<chrono::milliseconds>(now - r.last_fire).count() >= cooldown_ms){
                        playSoundAsync(r.sound);
                        r.last_fire = now;
                    }
                }
            }else{ r.off_cnt++; r.on_cnt=0;
                if (r.active && r.off_cnt>=exit_frames) r.active=false;
            }

            Scalar col = r.active? Scalar(0,0,255) : Scalar(0,255,0);
            rectangle(vis, R, col, 3);
            char buf[32]; snprintf(buf, sizeof(buf), "%.1f%%", motion*100.0);
            putText(vis, r.name + " " + buf, Point(R.x+6, R.y+20),
                    FONT_HERSHEY_SIMPLEX, 0.55, col, 2, LINE_AA);
        }

        putText(vis, "b: reset bg | m: mask | q/ESC: quit",
                Point(10, vis.rows-10), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255,255,255), 1, LINE_AA);

        imshow("Drum Trigger (Cap Only)", vis);
        if (show_mask) imshow("Mask", capMaskFiltered); else destroyWindow("Mask");

        int key = waitKey(1);
        if (key==27 || key=='q') break;
        if (key=='m') show_mask=!show_mask;
        if (key=='b'){ bg = createBackgroundSubtractorMOG2(500, max(1,varThr), true); bg->setDetectShadows(false); cout<<"BG reset\n"; }
    }
    return 0;
}
