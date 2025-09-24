#pragma once

#include <SFML/Audio.hpp>
#include <vector>
#include <memory>
#include <string>

class AudioManager {
public:
    AudioManager();
    ~AudioManager() = default;

    bool initialize();
    void playNote(int keyIndex);
    void stopNote(int keyIndex);
    void stopAllNotes();
    
    // 볼륨 제어
    void setMasterVolume(float volume);
    float getMasterVolume() const { return masterVolume_; }
    
    // 톤 생성 함수
    void generateTones();

private:
    std::vector<std::unique_ptr<sf::SoundBuffer>> soundBuffers_;
    std::vector<std::unique_ptr<sf::Sound>> sounds_;
    
    float masterVolume_;
    
    bool loadAudioFiles();
    std::string getNoteName(int keyIndex);
    
    // 피아노 음계 상수
    static constexpr int OCTAVE_NOTES = 12;
    static constexpr int BASE_OCTAVE = 4;  // 중간 C가 4옥타브
};
