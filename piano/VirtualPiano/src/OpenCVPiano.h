#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

class OpenCVPiano {
public:
    OpenCVPiano();
    ~OpenCVPiano() = default;

    void drawOnFrame(cv::Mat& frame);
    void playKey(int keyIndex);
    void stopKey(int keyIndex);
    
    // 키 위치 확인 함수
    bool isPointOverKey(const cv::Point2f& point, int& keyIndex);
    
    // 키 정보 가져오기
    const std::vector<cv::Rect>& getKeys() const { return keyRects_; }
    const std::vector<bool>& getKeyStates() const { return keyStates_; }

private:
    std::vector<cv::Rect> keyRects_;
    std::vector<bool> keyStates_;
    std::vector<cv::Scalar> keyColors_;
    
    void initializeKeys(int frameWidth, int frameHeight);
    void drawKey(cv::Mat& frame, int index);
    
    // 피아노 키 상수
    static constexpr int WHITE_KEYS = 7;  // C, D, E, F, G, A, B
    static constexpr int BLACK_KEYS = 5;  // C#, D#, F#, G#, A#
    static constexpr int TOTAL_KEYS = WHITE_KEYS + BLACK_KEYS;
    
    // 키 인덱스 (흰색 키: 0,2,4,5,7,9,11, 검은색 키: 1,3,6,8,10)
    static constexpr int WHITE_KEY_INDICES[WHITE_KEYS] = {0, 2, 4, 5, 7, 9, 11};
    static constexpr int BLACK_KEY_INDICES[BLACK_KEYS] = {1, 3, 6, 8, 10};
};

