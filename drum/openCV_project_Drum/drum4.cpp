#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
using namespace cv;
using namespace std;

/* ------------ ROI 정의 (원 2개 + 타원 3개) ------------ */
struct CircleROI { Point c; int r; string name; bool active=false; };
struct EllipseROI{ Point c; Size axes; double ang; string name; bool active=false; };

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

// 사용자가 조정한 정규화 좌표(기억해둔 값) → 픽셀 ROI 생성
// 원: cx,cy,r  (W,H,min(W,H) 비율)
// 타원: cx,cy,ax,ay,ang  (ax,ay는 반축을 min(W,H) 비율)
static void makeMixedROIs(Size sz, vector<CircleROI>& circles, vector<EllipseROI>& ellipses){
    const int W=sz.width, H=sz.height, M=min(W,H);

    // --- 원(하이탐, 미들탐) ---
    struct Cn{ float cx,cy,r; const char* n; };
    const Cn cset[] = {
        {0.370f, 0.365f, 0.150f, "Tom-Hi"},   // 하이탐
        {0.625f, 0.365f, 0.170f, "Tom-Mid"},  // 미들탐
    };
    for (auto&p:cset){
        CircleROI R; R.c = clampPt(Point(int(p.cx*W+0.5f),int(p.cy*H+0.5f)), sz);
        R.r = max(5,int(p.r*M+0.5f)); R.name=p.n; circles.push_back(R);
    }

    // --- 타원(크래쉬L, 킥, 크래쉬R) ---
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

/* -------------------------- 메인 -------------------------- */
int main(int argc, char** argv){
    string img_path = (argc>=2)? argv[1] : "drum2.png"; // 배경 이미지
    int maxW = (argc>=3)? atoi(argv[2]) : 960;          // 가로 최대폭(축소)

    Mat drum = imread(img_path);
    if (drum.empty()){ cerr<<"cannot read "<<img_path<<"\n"; return -1; }
    if (drum.cols>maxW){ double s=double(maxW)/drum.cols; resize(drum,drum,Size(),s,s,INTER_AREA); }

    // ROI 준비 + 마스크/면적 미리 계산 (마스크 순서: 원들 → 타원들)
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

    // 카메라/배경
    VideoCapture cap; if (!cap.open(0, cv::CAP_V4L2)){ cerr<<"camera open fail\n"; return -1; }
    cap.set(CAP_PROP_FPS,30);
    Ptr<BackgroundSubtractorMOG2> bg=createBackgroundSubtractorMOG2(500,20,true);
    bg->setDetectShadows(false);

    // 슬라이더(흰 뚜껑 전용)
    namedWindow("CapTune");
    int varThr=20;     createTrackbar("varThr","CapTune",&varThr,100);
    int lr1000=0;      createTrackbar("lr x1000","CapTune",&lr1000,100); // 0이면 자동
    int thPercent=3;   createTrackbar("thresh %","CapTune",&thPercent,50);
    int whiteSmax=55;  createTrackbar("white Smax","CapTune",&whiteSmax,255);
    int whiteVmin=185; createTrackbar("white Vmin","CapTune",&whiteVmin,255);
    int minArea=80;    createTrackbar("min area","CapTune",&minArea,10000);
    int maxArea=5000;  createTrackbar("max area","CapTune",&maxArea,60000);
    int blurK=2, openK=1, dilK=1;
    createTrackbar("blur k","CapTune",&blurK,6);
    createTrackbar("open k","CapTune",&openK,6);
    createTrackbar("dilate k","CapTune",&dilK,6);

    bool show_mask=false;
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

        // 겹침 계산/그리기 (원부터)
        double thr=thPercent/100.0; size_t idx=0;

        for(auto& r:cROIs){
            Mat inter; bitwise_and(pen,masks[idx],inter);
            double frac = (areas[idx]>0)? (double)countNonZero(inter)/areas[idx] : 0.0;
            r.active = (frac>=thr);
            Scalar col = r.active? Scalar(0,0,255):Scalar(0,255,0);
            circle(vis,r.c,r.r,col,4,LINE_AA);
            putCenteredLabel(vis, r.name, r.c, 0.9, 2, col);
            idx++;
        }
        for(auto& e:eROIs){
            Mat inter; bitwise_and(pen,masks[idx],inter);
            double frac = (areas[idx]>0)? (double)countNonZero(inter)/areas[idx] : 0.0;
            e.active = (frac>=thr);
            Scalar col = e.active? Scalar(0,0,255):Scalar(0,255,0);
            ellipse(vis,e.c,e.axes,e.ang,0,360,col,4,LINE_AA);
            // 킥처럼 하단에 붙은 건 살짝 위로 올려주면 보기 좋음
            Point labelPt = e.c;
            if (e.name=="Kick") labelPt.y -= 10;
            putCenteredLabel(vis, e.name, labelPt, 0.9, 2, col);
            idx++;
        }

        putText(vis,"b: reset bg | m: mask | q/ESC: quit",Point(10,vis.rows-10),
                FONT_HERSHEY_SIMPLEX,0.5,Scalar(255,255,255),1,LINE_AA);

        namedWindow("Drum (Mixed ROI Cap)", WINDOW_NORMAL);
        resizeWindow("Drum (Mixed ROI Cap)", drum.cols, drum.rows);
        imshow("Drum (Mixed ROI Cap)", vis);
        if(show_mask) imshow("Mask", pen); else destroyWindow("Mask");

        int key=waitKey(1);
        if(key==27||key=='q') break;
        if(key=='m') show_mask=!show_mask;
        if(key=='b'){ bg=createBackgroundSubtractorMOG2(500,varThr,true); bg->setDetectShadows(false); cout<<"BG reset\n"; }
    }
    return 0;
}
