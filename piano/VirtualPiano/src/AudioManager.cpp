#include "AudioManager.h"
#include <iostream>
#include <filesystem>
#include <cmath>

AudioManager::AudioManager() : masterVolume_(100.0f) {
    soundBuffers_.resize(12); // 12개 음계
    sounds_.resize(12);
    
    for (int i = 0; i < 12; ++i) {
        soundBuffers_[i] = std::make_unique<sf::SoundBuffer>();
        sounds_[i] = std::make_unique<sf::Sound>();
    }
}

bool AudioManager::initialize() {
    if (!loadAudioFiles()) {
        std::cerr << "Warning: Could not load audio files. Using generated tones." << std::endl;
        generateTones();
    }
    
    // 마스터 볼륨 설정
    setMasterVolume(masterVolume_);
    
    std::cout << "Audio manager initialized successfully" << std::endl;
    return true;
}

bool AudioManager::loadAudioFiles() {
    // build 디렉토리에서 실행되므로 상위 디렉토리로 이동
    std::string basePath = "../assets/sounds/";
    
    // assets/sounds 디렉토리 생성
    std::filesystem::create_directories(basePath);
    
    bool allLoaded = true;
    
    for (int i = 0; i < 12; ++i) {
        std::string filename = basePath + getNoteName(i) + ".wav";
        
        if (std::filesystem::exists(filename)) {
            if (!soundBuffers_[i]->loadFromFile(filename)) {
                std::cerr << "Failed to load: " << filename << std::endl;
                allLoaded = false;
            } else {
                sounds_[i]->setBuffer(*soundBuffers_[i]);
                sounds_[i]->setVolume(masterVolume_);
                std::cout << "Loaded: " << filename << std::endl;
            }
        } else {
            std::cout << "Audio file not found: " << filename << std::endl;
            allLoaded = false;
        }
    }
    
    return allLoaded;
}

void AudioManager::generateTones() {
    // 간단한 사인파 톤 생성 (실제 피아노 소리 대신)
    const int sampleRate = 44100;
    const float duration = 2.0f; // 2초
    const int sampleCount = static_cast<int>(sampleRate * duration);
    
    for (int i = 0; i < 12; ++i) {
        std::vector<sf::Int16> samples(sampleCount);
        
        // 각 음계의 주파수 계산 (A4 = 440Hz 기준)
        float frequency = 440.0f * pow(2.0f, (i - 9) / 12.0f);
        
        for (int j = 0; j < sampleCount; ++j) {
            float time = static_cast<float>(j) / sampleRate;
            
            // 사인파 + 약간의 하모닉스로 더 풍부한 소리 만들기
            float sample = 0.0f;
            sample += 0.6f * sin(2.0f * M_PI * frequency * time);
            sample += 0.3f * sin(2.0f * M_PI * frequency * 2.0f * time);
            sample += 0.1f * sin(2.0f * M_PI * frequency * 3.0f * time);
            
            // 감쇠 적용 (소리가 자연스럽게 사라지도록)
            float envelope = exp(-time * 2.0f);
            sample *= envelope;
            
            // 볼륨 조절
            sample *= 0.3f;
            
            samples[j] = static_cast<sf::Int16>(sample * 32767.0f);
        }
        
        if (soundBuffers_[i]->loadFromSamples(samples.data(), sampleCount, 1, sampleRate)) {
            sounds_[i]->setBuffer(*soundBuffers_[i]);
            sounds_[i]->setVolume(masterVolume_);
        }
    }
    
    std::cout << "Generated synthetic piano tones" << std::endl;
}

void AudioManager::playNote(int keyIndex) {
    if (keyIndex < 0 || keyIndex >= 12) return;
    
    std::cout << "Playing note: " << getNoteName(keyIndex) << " (index: " << keyIndex << ")" << std::endl;
    
    // 이미 재생 중인 음이면 중지 후 다시 재생
    if (sounds_[keyIndex]->getStatus() == sf::Sound::Playing) {
        sounds_[keyIndex]->stop();
    }
    
    sounds_[keyIndex]->play();
    
    // 재생 상태 확인
    if (sounds_[keyIndex]->getStatus() == sf::Sound::Playing) {
        std::cout << "Note " << getNoteName(keyIndex) << " is playing successfully" << std::endl;
    } else {
        std::cout << "Failed to play note " << getNoteName(keyIndex) << std::endl;
    }
}

void AudioManager::stopNote(int keyIndex) {
    if (keyIndex < 0 || keyIndex >= 12) return;
    
    sounds_[keyIndex]->stop();
}

void AudioManager::stopAllNotes() {
    for (auto& sound : sounds_) {
        sound->stop();
    }
}

void AudioManager::setMasterVolume(float volume) {
    masterVolume_ = std::max(0.0f, std::min(100.0f, volume));
    
    for (auto& sound : sounds_) {
        sound->setVolume(masterVolume_);
    }
}

std::string AudioManager::getNoteName(int keyIndex) {
    const std::string noteNames[] = {
        "C", "C#", "D", "D#", "E", "F", 
        "F#", "G", "G#", "A", "A#", "B"
    };
    
    if (keyIndex < 0 || keyIndex >= 12) {
        return "Unknown";
    }
    
    return noteNames[keyIndex];
}
