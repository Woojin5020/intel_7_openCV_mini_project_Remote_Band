#include "Piano.h"
#include <algorithm>

Piano::Piano(sf::RenderWindow& window) : window_(window) {
    initializeKeys();
}

void Piano::initializeKeys() {
    keys_.resize(PianoKeys::TOTAL_KEYS);
    keyStates_.resize(PianoKeys::TOTAL_KEYS, false);
    keyColors_.resize(PianoKeys::TOTAL_KEYS);
    
    // 창 크기 가져오기 (웹캠 크기에 맞춤)
    sf::Vector2u windowSize = window_.getSize();
    float keyWidth = windowSize.x / PianoKeys::WHITE_KEYS;
    float keyHeight = windowSize.y * 0.3f;  // 건반을 더 작게
    float startY = windowSize.y * 0.7f;      // 화면 하단에 배치
    
    // 흰색 키 초기화 (반투명하게)
    for (int i = 0; i < PianoKeys::WHITE_KEYS; ++i) {
        int keyIndex = PianoKeys::WHITE_KEY_INDICES[i];
        keys_[keyIndex].setSize(sf::Vector2f(keyWidth, keyHeight));
        keys_[keyIndex].setPosition(i * keyWidth, startY);
        keys_[keyIndex].setFillColor(sf::Color(255, 255, 255, 150)); // 반투명 흰색
        keys_[keyIndex].setOutlineColor(sf::Color::Black);
        keys_[keyIndex].setOutlineThickness(2.0f);
        keyColors_[keyIndex] = sf::Color(255, 255, 255, 150);
    }
    
    // 검은색 키 초기화
    float blackKeyWidth = keyWidth * 0.6f;
    float blackKeyHeight = keyHeight * 0.6f;
    
    for (int i = 0; i < PianoKeys::BLACK_KEYS; ++i) {
        int keyIndex = PianoKeys::BLACK_KEY_INDICES[i];
        
        // 검은색 키 위치 계산
        float x = 0.0f;
        if (keyIndex == 1) x = keyWidth * 0.7f;      // C#
        else if (keyIndex == 3) x = keyWidth * 1.7f; // D#
        else if (keyIndex == 6) x = keyWidth * 3.7f; // F#
        else if (keyIndex == 8) x = keyWidth * 4.7f;  // G#
        else if (keyIndex == 10) x = keyWidth * 5.7f; // A#
        
        keys_[keyIndex].setSize(sf::Vector2f(blackKeyWidth, blackKeyHeight));
        keys_[keyIndex].setPosition(x, startY);
        keys_[keyIndex].setFillColor(sf::Color(0, 0, 0, 180)); // 반투명 검은색
        keys_[keyIndex].setOutlineColor(sf::Color(100, 100, 100));
        keys_[keyIndex].setOutlineThickness(1.0f);
        keyColors_[keyIndex] = sf::Color(0, 0, 0, 180);
    }
}

void Piano::draw() {
    // 흰색 키 먼저 그리기
    for (int i = 0; i < PianoKeys::WHITE_KEYS; ++i) {
        int keyIndex = PianoKeys::WHITE_KEY_INDICES[i];
        drawKey(keyIndex);
    }
    
    // 검은색 키 그리기
    for (int i = 0; i < PianoKeys::BLACK_KEYS; ++i) {
        int keyIndex = PianoKeys::BLACK_KEY_INDICES[i];
        drawKey(keyIndex);
    }
}

void Piano::drawKey(int index) {
    if (index < 0 || index >= PianoKeys::TOTAL_KEYS) return;
    
    // 키가 눌린 상태면 색상 변경
    if (keyStates_[index]) {
        sf::Color pressedColor = keyColors_[index];
        if (keyColors_[index].r == 255) { // 흰색 키
            pressedColor = sf::Color(255, 200, 200, 200); // 밝은 빨간색 (반투명)
        } else { // 검은색 키
            pressedColor = sf::Color(200, 100, 100, 200); // 어두운 빨간색 (반투명)
        }
        keys_[index].setFillColor(pressedColor);
    } else {
        keys_[index].setFillColor(keyColors_[index]);
    }
    
    window_.draw(keys_[index]);
}

void Piano::update() {
    // 애니메이션이나 다른 업데이트 로직이 필요하면 여기에 추가
}

void Piano::playKey(int keyIndex) {
    if (keyIndex >= 0 && keyIndex < PianoKeys::TOTAL_KEYS) {
        keyStates_[keyIndex] = true;
    }
}

void Piano::stopKey(int keyIndex) {
    if (keyIndex >= 0 && keyIndex < PianoKeys::TOTAL_KEYS) {
        keyStates_[keyIndex] = false;
    }
}

bool Piano::isPointOverKey(const sf::Vector2f& point, int& keyIndex) {
    keyIndex = -1;
    
    // 검은색 키부터 확인 (위에 있으므로 우선순위)
    for (int i = 0; i < PianoKeys::BLACK_KEYS; ++i) {
        int index = PianoKeys::BLACK_KEY_INDICES[i];
        if (keys_[index].getGlobalBounds().contains(point)) {
            keyIndex = index;
            return true;
        }
    }
    
    // 흰색 키 확인
    for (int i = 0; i < PianoKeys::WHITE_KEYS; ++i) {
        int index = PianoKeys::WHITE_KEY_INDICES[i];
        if (keys_[index].getGlobalBounds().contains(point)) {
            keyIndex = index;
            return true;
        }
    }
    
    return false;
}
