#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>

class Piano {
public:
    Piano(sf::RenderWindow& window);
    ~Piano() = default;

    void draw();
    void update();
    void playKey(int keyIndex);
    void stopKey(int keyIndex);
    
    // 키 위치 확인 함수
    bool isPointOverKey(const sf::Vector2f& point, int& keyIndex);
    
    // 키 정보 가져오기
    const std::vector<sf::RectangleShape>& getKeys() const { return keys_; }
    const std::vector<bool>& getKeyStates() const { return keyStates_; }

private:
    sf::RenderWindow& window_;
    std::vector<sf::RectangleShape> keys_;
    std::vector<bool> keyStates_;
    std::vector<sf::Color> keyColors_;
    
    void initializeKeys();
    void drawKey(int index);
};

// 피아노 키 상수
namespace PianoKeys {
    constexpr int WHITE_KEYS = 7;  // C, D, E, F, G, A, B
    constexpr int BLACK_KEYS = 5;  // C#, D#, F#, G#, A#
    constexpr int TOTAL_KEYS = WHITE_KEYS + BLACK_KEYS;
    
    // 키 인덱스 (흰색 키: 0,2,4,5,7,9,11, 검은색 키: 1,3,6,8,10)
    constexpr int WHITE_KEY_INDICES[WHITE_KEYS] = {0, 2, 4, 5, 7, 9, 11};
    constexpr int BLACK_KEY_INDICES[BLACK_KEYS] = {1, 3, 6, 8, 10};
}

