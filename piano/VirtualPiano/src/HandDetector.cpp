#include "HandDetector.h"
#include <iostream>
#include <algorithm>

HandDetector::HandDetector() 
    : debugMode_(false), backgroundCaptured_(false) {
    // 피부색 범위 설정 (HSV)
    lowerSkin_ = cv::Scalar(0, 20, 70);
    upperSkin_ = cv::Scalar(20, 255, 255);
    
    fingerPoints_.resize(12); // 12개 건반 (0-11)
    for (auto& finger : fingerPoints_) {
        finger.isActive = false;
        finger.keyIndex = -1;
    }
}

bool HandDetector::initialize() {
    // 웹캠 초기화
    cap_.open(0);
    if (!cap_.isOpened()) {
        std::cerr << "Error: Could not open webcam" << std::endl;
        return false;
    }
    
    // 웹캠 해상도 설정
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    
    std::cout << "Webcam initialized successfully" << std::endl;
    return true;
}

void HandDetector::processFrame(cv::Mat& frame) {
    if (!cap_.isOpened()) return;
    
    cap_ >> frame;
    if (frame.empty()) return;
    
    // 배경 캡처 (첫 번째 프레임)
    if (!backgroundCaptured_) {
        captureBackground();
        return;
    }
    
    // 손 감지
    detectHands(frame);
    
    // 디버그 정보 그리기
    if (debugMode_) {
        drawDebugInfo(frame);
    }
}

void HandDetector::captureBackground() {
    cv::Mat frame;
    cap_ >> frame;
    if (!frame.empty()) {
        cv::cvtColor(frame, background_, cv::COLOR_BGR2GRAY);
        backgroundCaptured_ = true;
        std::cout << "Background captured. Move your hand in front of the camera." << std::endl;
    }
}

void HandDetector::detectHands(cv::Mat& frame) {
    // 배경 제거
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::Mat diff;
    cv::absdiff(gray, background_, diff);
    
    // 더 높은 임계값으로 노이즈 제거 (더 엄격하게)
    cv::Mat binary;
    cv::threshold(diff, binary, 60, 255, cv::THRESH_BINARY);
    
    // 더 강한 노이즈 제거
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);
    
    // 추가적인 노이즈 제거 - 작은 영역 제거 (더 엄격하게)
    cv::Mat labels, stats, centroids;
    int numLabels = cv::connectedComponentsWithStats(binary, labels, stats, centroids);
    
    // 작은 영역들을 제거 (최소 영역 크기 대폭 증가)
    cv::Mat filtered = cv::Mat::zeros(binary.size(), CV_8UC1);
    for (int i = 1; i < numLabels; i++) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > 5000) { // 최소 영역 크기 대폭 증가 (2000 -> 5000)
            cv::Mat mask = (labels == i);
            mask.copyTo(filtered, mask);
        }
    }
    
    // 물체 감지 (손가락 대신 건반 영역에 물체가 있는지 확인)
    detectObjectsInKeys(frame, filtered);
}

void HandDetector::detectObjectsInKeys(cv::Mat& frame, cv::Mat& mask) {
    // 모든 손가락 비활성화
    for (auto& finger : fingerPoints_) {
        finger.isActive = false;
    }
    
    // 윤곽선 찾기
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        return;
    }
    
    // 실제 존재하는 건반들만 확인 (WHITE_KEY_INDICES + BLACK_KEY_INDICES)
    std::vector<int> allKeys = {0, 2, 4, 5, 7, 9, 11, 1, 3, 6, 8, 10}; // C,D,E,F,G,A,B + C#,D#,F#,G#,A#
    
    for (int keyIndex : allKeys) {
        bool hasObject = false;
        
        // 건반 영역이 유효한지 확인
        if (keyRects_.size() <= keyIndex || keyRects_[keyIndex].empty()) {
            continue;
        }
        
        // 건반 영역 내의 픽셀 수 계산
        cv::Mat keyMask = cv::Mat::zeros(mask.size(), CV_8UC1);
        cv::rectangle(keyMask, keyRects_[keyIndex], cv::Scalar(255), -1);
        
        cv::Mat keyArea;
        cv::bitwise_and(mask, keyMask, keyArea);
        
        // 건반 영역 내의 흰색 픽셀 수 계산
        int whitePixels = cv::countNonZero(keyArea);
        int totalPixels = keyRects_[keyIndex].area();
        double objectRatio = static_cast<double>(whitePixels) / totalPixels;
        
        // 디버그 정보 제거 (정상 작동 확인됨)
        
        // 더 쉽게 감지: 30% 이상이면 물체가 있다고 판단
        // 그리고 최소 1000픽셀 이상이면 됨
        if (objectRatio > 0.3 && whitePixels > 1000) {
            hasObject = true;
        }
        
        // 물체가 있으면 해당 키를 활성화
        if (hasObject) {
            // fingerPoints_ 배열에서 사용 가능한 슬롯 찾기
            for (auto& finger : fingerPoints_) {
                if (!finger.isActive) {
                    finger.isActive = true;
                    finger.keyIndex = keyIndex;
                    // 건반 중심점을 손가락 위치로 설정
                    cv::Point center = (keyRects_[keyIndex].tl() + keyRects_[keyIndex].br()) * 0.5;
                    finger.position = cv::Point2f(center);
                    
                    // 건반 감지 완료 (디버그 정보 제거)
                    break;
                }
            }
        }
    }
}

void HandDetector::detectFingers(cv::Mat& frame, cv::Mat& mask) {
    // 윤곽선 찾기
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    // 모든 손가락 비활성화
    for (auto& finger : fingerPoints_) {
        finger.isActive = false;
    }
    
    if (contours.empty()) {
        return;
    }
    
    // 가장 큰 윤곽선 찾기 (손)
    auto largestContour = std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        });
    
    if (largestContour == contours.end()) return;
    
    // 손 영역이 너무 작으면 무시 (크기 증가)
    double area = cv::contourArea(*largestContour);
    if (area < 5000) return; // 최소 손 크기 대폭 증가
    
    // 윤곽선이 너무 작으면 무시
    if (largestContour->size() < 50) return;
    
    // 간단한 손가락 감지: 윤곽선의 극값 점들 찾기
    std::vector<cv::Point> fingerTips = findSimpleFingerTips(*largestContour);
    
    // 손가락 개수 제한 (너무 많으면 무시)
    if (fingerTips.size() > 5 || fingerTips.size() < 1) return;
    
    // 감지된 손가락 개수만큼 업데이트
    int detectedFingers = std::min(static_cast<int>(fingerTips.size()), 5);
    for (int i = 0; i < detectedFingers; ++i) {
        fingerPoints_[i].position = cv::Point2f(fingerTips[i]);
        fingerPoints_[i].isActive = true;
    }
}

std::vector<cv::Point> HandDetector::findFingerTips(const std::vector<cv::Point>& contour) {
    std::vector<cv::Point> fingerTips;
    
    // 윤곽선이 너무 작으면 무시
    if (contour.size() < 5) return fingerTips;
    
    // 윤곽선 단순화
    std::vector<cv::Point> approx;
    cv::approxPolyDP(contour, approx, 0.02 * cv::arcLength(contour, true), true);
    
    // 단순화된 윤곽선이 너무 작으면 무시
    if (approx.size() < 5) return fingerTips;
    
    // 볼록 껍질 찾기
    std::vector<int> hull;
    cv::convexHull(approx, hull);
    
    // 볼록 껍질이 충분하지 않으면 무시
    if (hull.size() < 3) return fingerTips;
    
    // 볼록 결함 찾기
    std::vector<cv::Vec4i> defects;
    try {
        cv::convexityDefects(approx, hull, defects);
    } catch (const cv::Exception& e) {
        // convex hull 오류 발생 시 빈 결과 반환
        return fingerTips;
    }
    
    // 손가락 끝점 추출
    for (const auto& defect : defects) {
        int startIdx = defect[0];
        int endIdx = defect[1];
        int farIdx = defect[2];
        float depth = defect[3] / 256.0f;
        
        // 깊이가 충분한 경우만 손가락으로 인식
        if (depth > 20) {
            fingerTips.push_back(approx[startIdx]);
            fingerTips.push_back(approx[endIdx]);
        }
    }
    
    // 중복 제거 및 정렬
    std::sort(fingerTips.begin(), fingerTips.end(), 
        [](const cv::Point& a, const cv::Point& b) {
            return a.y < b.y; // Y 좌표 기준 정렬
        });
    
    // 너무 가까운 점들 제거
    std::vector<cv::Point> filteredTips;
    for (const auto& tip : fingerTips) {
        bool tooClose = false;
        for (const auto& existing : filteredTips) {
            if (cv::norm(tip - existing) < 30) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose) {
            filteredTips.push_back(tip);
        }
    }
    
    return filteredTips;
}

std::vector<cv::Point> HandDetector::findSimpleFingerTips(const std::vector<cv::Point>& contour) {
    std::vector<cv::Point> fingerTips;
    
    if (contour.size() < 20) return fingerTips; // 최소 윤곽선 크기 증가
    
    // 윤곽선의 중심점 계산
    cv::Moments moments = cv::moments(contour);
    if (moments.m00 == 0) return fingerTips;
    
    cv::Point center(moments.m10 / moments.m00, moments.m01 / moments.m00);
    
    // 중심점에서 가장 먼 점들을 손가락 끝으로 간주
    std::vector<std::pair<double, cv::Point>> distances;
    
    for (const auto& point : contour) {
        double dist = cv::norm(point - center);
        distances.push_back({dist, point});
    }
    
    // 거리순으로 정렬
    std::sort(distances.begin(), distances.end(), 
        [](const std::pair<double, cv::Point>& a, const std::pair<double, cv::Point>& b) {
            return a.first > b.first;
        });
    
    // 평균 거리 계산
    double avgDistance = 0;
    int count = std::min(10, static_cast<int>(distances.size()));
    for (int i = 0; i < count; ++i) {
        avgDistance += distances[i].first;
    }
    avgDistance /= count;
    
    // 상위 점들을 손가락 끝으로 선택 (더 엄격한 조건)
    double minDistance = avgDistance * 0.7; // 평균 거리의 70% 이상
    double minSeparation = 60.0; // 최소 분리 거리 증가
    
    for (const auto& distPoint : distances) {
        if (distPoint.first > minDistance && fingerTips.size() < 5) {
            bool tooClose = false;
            for (const auto& existing : fingerTips) {
                if (cv::norm(distPoint.second - existing) < minSeparation) {
                    tooClose = true;
                    break;
                }
            }
            if (!tooClose) {
                fingerTips.push_back(distPoint.second);
            }
        }
    }
    
    return fingerTips;
}

void HandDetector::updateFingerPositions(cv::Mat& frame) {
    // 좌표 변환 및 키 인덱스 업데이트
    for (auto& finger : fingerPoints_) {
        if (finger.isActive) {
            // 화면 좌표를 피아노 좌표로 변환
            cv::Point2f pianoPoint = screenToPiano(finger.position);
            
            // 키 인덱스 찾기 (이 부분은 Piano 클래스와 연동 필요)
            finger.keyIndex = -1; // 임시로 -1 설정
        }
    }
}

cv::Point2f HandDetector::screenToPiano(const cv::Point2f& screenPoint) {
    // 웹캠 좌표를 피아노 좌표로 변환
    // 이 부분은 실제 피아노 창 크기에 맞게 조정 필요
    float scaleX = 1200.0f / 640.0f;  // 피아노 창 너비 / 웹캠 너비
    float scaleY = 800.0f / 480.0f;   // 피아노 창 높이 / 웹캠 높이
    
    return cv::Point2f(screenPoint.x * scaleX, screenPoint.y * scaleY);
}

void HandDetector::setKeyPositions(const std::vector<cv::Rect>& keyRects) {
    keyRects_ = keyRects;
}

void HandDetector::drawDebugInfo(cv::Mat& frame) {
    // 손가락 위치 표시
    for (const auto& finger : fingerPoints_) {
        if (finger.isActive) {
            cv::circle(frame, finger.position, 10, cv::Scalar(0, 255, 0), 2);
            cv::putText(frame, std::to_string(finger.keyIndex), 
                       finger.position + cv::Point2f(15, 0),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        }
    }
    
    // 프레임 정보 표시
    cv::putText(frame, "Virtual Piano - Hand Detection", 
               cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7, 
               cv::Scalar(255, 255, 255), 2);
    
    int activeFingers = std::count_if(fingerPoints_.begin(), fingerPoints_.end(),
        [](const FingerPoint& f) { return f.isActive; });
    
    cv::putText(frame, "Active Fingers: " + std::to_string(activeFingers),
               cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.5,
               cv::Scalar(255, 255, 255), 1);
}
