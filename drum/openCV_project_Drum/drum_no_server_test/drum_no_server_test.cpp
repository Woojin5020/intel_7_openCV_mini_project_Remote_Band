#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>

using namespace cv;
using namespace std;

/* ------------ ROI 정의 (원 2개 + 타원 3개) ------------ */
struct CircleROI { Point c; int r; string name; bool active=false; };
struct EllipseROI{ Point c; Size axes; double ang; string name; bool active=false; };

struct ROIState {
    bool active=false;
    int on_cnt=0, off_cnt=0;
    chrono::steady_clock::time_point last_fire = chrono::steady_clock::now();
    string sound;   // 파일 경로 (mp3/mp4)
    // 볼륨은 슬라이더 변수로 runtime 계산 (여기엔 들고있지 않음)
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

/* --------- MP3/MP4 재생기 (ffplay → gstreamer → vlc) + 볼륨 --------- */
/* gain: 선형배수 (1.0=100%, 0.5=50%, 2.0=200%) */
static inline void playSoundAsync(const string& path, double gain) {
    std::thread([path, gain](){
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
            "if command -v ffplay >/dev/null 2>&1; then "
            "  ffplay -nodisp -autoexit -loglevel quiet -af \"volume=%f\" \"%s\" >/dev/null 2>&1; "
            "elif command -v gst-play-1.0 >/dev/null 2>&1; then "
            "  gst-play-1.0 -q --volume=%f \"%s\" >/dev/null 2>&1; "
            "elif command -v cvlc >/dev/null 2>&1; then "
            "  cvlc --intf dummy --play-and-exit --gain=%f \"%s\" >/dev/null 2>&1; "
            "else "
            "  echo 'No audio player found (install ffmpeg or gstreamer or vlc)' 1>&2; "
            "fi",
            gain, path.c_str(),
            gain, path.c_str(),
            gain, path.c_str()
        );
        (void)system(cmd);
    }).detach();
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

/* ------------------------------- 메인 ------------------------------- */
int main(int argc, char** argv){
    /* 배경 이미지를 바꿀려면 파일 이름 지정*/
    string img_path = (argc>=2)? argv[1] : "drum2.png"; // 배경 이미지
    /* 이미지의 최대 가로폭을 지정*/
    int maxW = (argc>=3)? atoi(argv[2]) : 960;          // 가로 최대폭(축소)

    Mat drum = imread(img_path);
    if (drum.empty()){ cerr<<"cannot read "<<img_path<<"\n"; return -1; }
    if (drum.cols>maxW){ double s=double(maxW)/drum.cols; resize(drum,drum,Size(),s,s,INTER_AREA); }

    // ROI 준비 + 마스크/면적 미리 계산 (순서: 원들 → 타원들)
    vector<CircleROI> cROIs; vector<EllipseROI> eROIs;
    makeMixedROIs(drum.size(), cROIs, eROIs);

    vector<Mat> masks; vector<int> areas; vector<string> names;
    for (auto&r:cROIs){
        Mat m=Mat::zeros(drum.size(),CV_8U);
        circle(m,r.c,r.r,Scalar(255),FILLED,LINE_AA);
        masks.push_back(m); areas.push_back(countNonZero(m)); names.push_back(r.name);
    }
    for (auto&e:eROIs){
        Mat m=Mat::zeros(drum.size(),CV_8U);
        ellipse(m,e.c,e.axes,e.ang,0,360,Scalar(255),FILLED,LINE_AA);
        masks.push_back(m); areas.push_back(countNonZero(m)); names.push_back(e.name);
    }

    // 🔊 파일 매핑 (왼심벌=mp3, 나머지=mp4)
    vector<ROIState> states = {
        {false,0,0, chrono::steady_clock::now(), "sounds/tom_hi.mp4"},        // 0: Tom-Hi
        {false,0,0, chrono::steady_clock::now(), "sounds/tom_mid.mp4"},       // 1: Tom-Mid
        {false,0,0, chrono::steady_clock::now(), "sounds/cymbal_left.mp3"},   // 2: Cymbal-L
        {false,0,0, chrono::steady_clock::now(), "sounds/kick.mp4"},          // 3: Kick
        {false,0,0, chrono::steady_clock::now(), "sounds/cymbal_right.mp4"}   // 4: Cymbal-R
    };

    // 카메라/배경
    VideoCapture cap; if (!cap.open(0, cv::CAP_V4L2)){ cerr<<"camera open fail\n"; return -1; }
    cap.set(CAP_PROP_FPS,30);
    Ptr<BackgroundSubtractorMOG2> bg=createBackgroundSubtractorMOG2(500,20,true);
    bg->setDetectShadows(false);

    // 슬라이더(흰 뚜껑 전용) + 볼륨 슬라이더
    namedWindow("CapTune");
    int varThr=20;     createTrackbar("varThr","CapTune",&varThr,100);
    int lr1000=0;      createTrackbar("lr x1000","CapTune",&lr1000,100); // 0이면 자동
    int thPercent=3;   createTrackbar("thresh %","CapTune",&thPercent,50);
    int whiteSmax=55;  createTrackbar("white Smax","CapTune",&whiteSmax,255);
    int whiteVmin=185; createTrackbar("white Vmin","CapTune",&whiteVmin,255);
    int minArea=80;    createTrackbar("min area","CapTune",&minArea,10000);
    int maxArea=5000;  createTrackbar("max area","CapTune",&maxArea,60000);
    int blurK=2, openK=1, dilK=4;
    createTrackbar("blur k","CapTune",&blurK,6);
    createTrackbar("open k","CapTune",&openK,6);
    createTrackbar("dilate k","CapTune",&dilK,6);

    // 🔊 볼륨 슬라이더 (0~200%)
    int masterVol100 = 100; createTrackbar("master %","CapTune",&masterVol100,200);
    int volTomHi100  = 100; createTrackbar("vol TomHi %","CapTune",&volTomHi100,200);
    int volTomMid100 = 100; createTrackbar("vol TomMid %","CapTune",&volTomMid100,200);
    int volCymL100   = 5; createTrackbar("vol CymL %","CapTune",&volCymL100,200);
    int volKick100   = 100; createTrackbar("vol Kick %","CapTune",&volKick100,200);
    int volCymR100   = 100; createTrackbar("vol CymR %","CapTune",&volCymR100,200);

    // 트리거 튜닝
    const int enter_frames = 2;   // 연속 n프레임 이상 겹치면 '들어옴'
    const int exit_frames  = 3;   // 연속 n프레임 미만이면 '나감'
    const int cooldown_ms  = 120; // 중복 방지 쿨다운(ms)

    bool show_mask=false, mask_open=false;
    cout<<"Keys: b=bg reset, m=mask toggle, q/ESC: quit\n";

    Mat cam,camRsz,fg,vis;
    while(true){
        if(!cap.read(cam)||cam.empty()) break;
        resize(cam,camRsz,drum.size());

        // BG subtract
        int ksz=max(1,2*blurK+1);
        Mat gray; cvtColor(camRsz,gray,COLOR_BGR2GRAY);
        GaussianBlur(gray,gray,Size(ksz,ksz),0);
        bg->setVarThreshold(max(1,varThr));
        double lr=(lr1000==0)? -1.0 : (lr1000/1000.0);
        bg->apply(gray,fg,lr);

        // 흰 뚜껑 HSV
        Mat hsv; cvtColor(camRsz,hsv,COLOR_BGR2HSV);
        Mat white; inRange(hsv,Scalar(0,0,whiteVmin),Scalar(180,whiteSmax,255),white);

        // 교집합 + 보정 + 컴포넌트 필터
        Mat capMask; bitwise_and(white,fg,capMask);
        if(openK>0){ Mat k=getStructuringElement(MORPH_ELLIPSE,Size(2*openK+1,2*openK+1)); morphologyEx(capMask,capMask,MORPH_OPEN,k); }
        if(dilK>0){ Mat k=getStructuringElement(MORPH_ELLIPSE,Size(2*dilK+1,2*dilK+1)); dilate(capMask,capMask,k); }
        Mat lab,st,ce; int n=connectedComponentsWithStats(capMask,lab,st,ce,8,CV_32S);
        Mat pen=Mat::zeros(capMask.size(),CV_8U);
        for(int i=1;i<n;++i){ int a=st.at<int>(i,CC_STAT_AREA); if(a>=minArea&&a<=maxArea) pen.setTo(255, lab==i); }

        // 합성
        vis=drum.clone(); camRsz.copyTo(vis,pen);

        // 현재 볼륨(배수) 계산
        double master = std::max(0, masterVol100) / 100.0;
        double vol[5] = {
            (std::max(0, volTomHi100 ) / 100.0) * master,  // idx 0
            (std::max(0, volTomMid100) / 100.0) * master,  // idx 1
            (std::max(0, volCymL100  ) / 100.0) * master,  // idx 2
            (std::max(0, volKick100  ) / 100.0) * master,  // idx 3
            (std::max(0, volCymR100  ) / 100.0) * master   // idx 4
        };

        // 겹침 계산 & 상태 업데이트 & 그리기
        double thr=thPercent/100.0;
        size_t idx=0;

        // 원 2개
        for(size_t i=0;i<cROIs.size();++i,++idx){
            Mat inter; bitwise_and(pen, masks[idx], inter);
            double frac = (areas[idx]>0)? (double)countNonZero(inter)/areas[idx] : 0.0;
            bool hit = (frac >= thr);

            // 히스테리시스 & 사운드
            if (hit){ states[idx].on_cnt++; states[idx].off_cnt=0;
                if (!states[idx].active && states[idx].on_cnt>=enter_frames){
                    states[idx].active = true;
                    auto now=chrono::steady_clock::now();
                    int ms = (int)chrono::duration_cast<chrono::milliseconds>(now - states[idx].last_fire).count();
                    if (ms >= cooldown_ms && !states[idx].sound.empty()){
                        playSoundAsync(states[idx].sound, vol[idx]); // 🔊 볼륨 반영
                        states[idx].last_fire = now;
                    }
                }
            } else { states[idx].off_cnt++; states[idx].on_cnt=0;
                if (states[idx].active && states[idx].off_cnt>=exit_frames) states[idx].active=false;
            }

            Scalar col = states[idx].active? Scalar(0,0,255):Scalar(0,255,0);
            circle(vis, cROIs[i].c, cROIs[i].r, col, 4, LINE_AA);
            putCenteredLabel(vis, cROIs[i].name, cROIs[i].c, 0.9, 2, col);
        }

        // 타원 3개
        for(size_t i=0;i<eROIs.size();++i,++idx){
            Mat inter; bitwise_and(pen, masks[idx], inter);
            double frac = (areas[idx]>0)? (double)countNonZero(inter)/areas[idx] : 0.0;
            bool hit = (frac >= thr);

            if (hit){ states[idx].on_cnt++; states[idx].off_cnt=0;
                if (!states[idx].active && states[idx].on_cnt>=enter_frames){
                    states[idx].active = true;
                    auto now=chrono::steady_clock::now();
                    int ms = (int)chrono::duration_cast<chrono::milliseconds>(now - states[idx].last_fire).count();
                    if (ms >= cooldown_ms && !states[idx].sound.empty()){
                        playSoundAsync(states[idx].sound, vol[idx]); // 🔊 볼륨 반영
                        states[idx].last_fire = now;
                    }
                }
            } else { states[idx].off_cnt++; states[idx].on_cnt=0;
                if (states[idx].active && states[idx].off_cnt>=exit_frames) states[idx].active=false;
            }

            Scalar col = states[idx].active? Scalar(0,0,255):Scalar(0,255,0);
            ellipse(vis, eROIs[i].c, eROIs[i].axes, eROIs[i].ang, 0, 360, col, 4, LINE_AA);
            Point labelPt = eROIs[i].c;
            if (eROIs[i].name=="Kick") labelPt.y -= 10; // 하단 잘림 방지
            putCenteredLabel(vis, eROIs[i].name, labelPt, 0.9, 2, col);
        }

        putText(vis,"b: reset bg | m: mask | q/ESC: quit",Point(10,vis.rows-10),
                FONT_HERSHEY_SIMPLEX,0.5,Scalar(255,255,255),1,LINE_AA);

        // 메인 창
        namedWindow("Drum (Mixed ROI + Volume)", WINDOW_NORMAL);
        resizeWindow("Drum (Mixed ROI + Volume)", drum.cols, drum.rows);
        imshow("Drum (Mixed ROI + Volume)", vis);

        // Mask 창 (토글-세이프)
        if (show_mask) {
            if (!mask_open) { namedWindow("Mask", WINDOW_NORMAL); mask_open=true; }
            imshow("Mask", pen);
        } else if (mask_open) {
            destroyWindow("Mask"); mask_open=false;
        }

        int key=waitKey(1);
        if(key==27||key=='q') break;
        if(key=='m') show_mask=!show_mask;
        if(key=='b'){ bg=createBackgroundSubtractorMOG2(500,varThr,true); bg->setDetectShadows(false); cout<<"BG reset\n"; }

        // 강제 재생(경로/볼륨 체크용) 1~5
        if(key=='1') playSoundAsync("sounds/tom_hi.mp4",      vol[0]);
        if(key=='2') playSoundAsync("sounds/tom_mid.mp4",     vol[1]);
        if(key=='3') playSoundAsync("sounds/cymbal_left.mp3", vol[2]);
        if(key=='4') playSoundAsync("sounds/kick.mp4",        vol[3]);
        if(key=='5') playSoundAsync("sounds/cymbal_right.mp4",vol[4]);
    }

    // 종료 시 정리
    destroyAllWindows();
    return 0;
}

