#include "OpenCVPiano.h"

OpenCVPiano::OpenCVPiano() {
    keyRects_.resize(TOTAL_KEYS);
    keyStates_.resize(TOTAL_KEYS, false);
    keyColors_.resize(TOTAL_KEYS);
}

void OpenCVPiano::initializeKeys(int frameWidth, int frameHeight) {
    float keyWidth = static_cast<float>(frameWidth) / WHITE_KEYS;
    float keyHeight = frameHeight * 0.3f;  // 건반을 더 작게
    float startY = frameHeight * 0.7f;      // 화면 하단에 배치
    
    // 흰색 키 초기화
    for (int i = 0; i < WHITE_KEYS; ++i) {
        int keyIndex = WHITE_KEY_INDICES[i];
        keyRects_[keyIndex] = cv::Rect(
            static_cast<int>(i * keyWidth), 
            static_cast<int>(startY),
            static_cast<int>(keyWidth), 
            static_cast<int>(keyHeight)
        );
        keyColors_[keyIndex] = cv::Scalar(255, 255, 255); // 흰색
        // 건반 초기화 완료 (디버그 정보 제거)
    }
    
    // 검은색 키 초기화
    float blackKeyWidth = keyWidth * 0.6f;
    float blackKeyHeight = keyHeight * 0.6f;
    
    for (int i = 0; i < BLACK_KEYS; ++i) {
        int keyIndex = BLACK_KEY_INDICES[i];
        
        // 검은색 키 위치 계산
        float x = 0.0f;
        if (keyIndex == 1) x = keyWidth * 0.7f;      // C#
        else if (keyIndex == 3) x = keyWidth * 1.7f; // D#
        else if (keyIndex == 6) x = keyWidth * 3.7f; // F#
        else if (keyIndex == 8) x = keyWidth * 4.7f;  // G#
        else if (keyIndex == 10) x = keyWidth * 5.7f; // A#
        
        keyRects_[keyIndex] = cv::Rect(
            static_cast<int>(x), 
            static_cast<int>(startY),
            static_cast<int>(blackKeyWidth), 
            static_cast<int>(blackKeyHeight)
        );
        keyColors_[keyIndex] = cv::Scalar(0, 0, 0); // 검은색
    }
}

void OpenCVPiano::drawOnFrame(cv::Mat& frame) {
    // 프레임 크기가 변경되었으면 키 위치 재계산
    if (keyRects_.empty() || keyRects_[0].width == 0) {
        initializeKeys(frame.cols, frame.rows);
    }
    
    // 검은색 키 먼저 그리기 (위에 있으므로)
    for (int i = 0; i < BLACK_KEYS; ++i) {
        int keyIndex = BLACK_KEY_INDICES[i];
        drawKey(frame, keyIndex);
    }
    
    // 흰색 키 그리기
    for (int i = 0; i < WHITE_KEYS; ++i) {
        int keyIndex = WHITE_KEY_INDICES[i];
        drawKey(frame, keyIndex);
    }
}

void OpenCVPiano::drawKey(cv::Mat& frame, int index) {
    if (index < 0 || index >= TOTAL_KEYS) return;
    
    cv::Scalar color = keyColors_[index];
    
    // 키가 눌린 상태면 색상 변경
    if (keyStates_[index]) {
        if (keyColors_[index][0] == 255) { // 흰색 키
            color = cv::Scalar(0, 0, 255); // 빨간색
        } else { // 검은색 키
            color = cv::Scalar(0, 100, 255); // 어두운 빨간색
        }
    }
    
    // 키 그리기
    cv::rectangle(frame, keyRects_[index], color, -1); // 채워진 사각형
    cv::rectangle(frame, keyRects_[index], cv::Scalar(0, 0, 0), 2); // 검은색 테두리
}

void OpenCVPiano::playKey(int keyIndex) {
    if (keyIndex >= 0 && keyIndex < TOTAL_KEYS) {
        keyStates_[keyIndex] = true;
    }
}

void OpenCVPiano::stopKey(int keyIndex) {
    if (keyIndex >= 0 && keyIndex < TOTAL_KEYS) {
        keyStates_[keyIndex] = false;
    }
}

bool OpenCVPiano::isPointOverKey(const cv::Point2f& point, int& keyIndex) {
    keyIndex = -1;
    
    // 검은색 키부터 확인 (위에 있으므로 우선순위)
    for (int i = 0; i < BLACK_KEYS; ++i) {
        int index = BLACK_KEY_INDICES[i];
        if (keyRects_[index].contains(cv::Point(static_cast<int>(point.x), static_cast<int>(point.y)))) {
            keyIndex = index;
            return true;
        }
    }
    
    // 흰색 키 확인
    for (int i = 0; i < WHITE_KEYS; ++i) {
        int index = WHITE_KEY_INDICES[i];
        if (keyRects_[index].contains(cv::Point(static_cast<int>(point.x), static_cast<int>(point.y)))) {
            keyIndex = index;
            return true;
        }
    }
    
    return false;
}

