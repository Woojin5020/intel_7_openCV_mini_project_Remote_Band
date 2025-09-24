#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

struct FingerPoint {
    cv::Point2f position;
    bool isActive;
    int keyIndex;  // 현재 가리키고 있는 키의 인덱스
};

class HandDetector {
public:
    HandDetector();
    ~HandDetector() = default;

    bool initialize();
    void processFrame(cv::Mat& frame);
    std::vector<FingerPoint> getFingerPoints() const { return fingerPoints_; }
    
    void setKeyPositions(const std::vector<cv::Rect>& keyRects);
    
    // 디버그용 함수
    void drawDebugInfo(cv::Mat& frame);
    void setDebugMode(bool enabled) { debugMode_ = enabled; }

private:
    cv::VideoCapture cap_;
    std::vector<FingerPoint> fingerPoints_;
    std::vector<cv::Rect> keyRects_;
    
    bool debugMode_;
    cv::Mat background_;
    bool backgroundCaptured_;
    
    // 색상 범위 설정
    cv::Scalar lowerSkin_;
    cv::Scalar upperSkin_;
    
    void detectHands(cv::Mat& frame);
    void detectFingers(cv::Mat& frame, cv::Mat& mask);
    void detectObjectsInKeys(cv::Mat& frame, cv::Mat& mask);
    void updateFingerPositions(cv::Mat& frame);
    void captureBackground();
    
    // 색상 기반 손 감지
    cv::Mat createSkinMask(cv::Mat& frame);
    
    // 모양 기반 손 감지
    std::vector<cv::Point> findHandContour(cv::Mat& mask);
    std::vector<cv::Point> findFingerTips(const std::vector<cv::Point>& contour);
    std::vector<cv::Point> findSimpleFingerTips(const std::vector<cv::Point>& contour);
    
    // 좌표 변환
    cv::Point2f screenToPiano(const cv::Point2f& screenPoint);
};
