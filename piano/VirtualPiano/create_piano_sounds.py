#!/usr/bin/env python3
import numpy as np
import wave
import os

def create_piano_tone(frequency, duration=1.5, sample_rate=44100):
    """자연스러운 피아노 톤 생성"""
    t = np.linspace(0, duration, int(sample_rate * duration), False)
    
    # 피아노 특성의 복합 파형
    wave_data = np.zeros_like(t)
    
    # 기본 톤과 하모닉스
    wave_data += 0.5 * np.sin(2 * np.pi * frequency * t)  # 기본 톤
    wave_data += 0.3 * np.sin(2 * np.pi * frequency * 2 * t)  # 옥타브
    wave_data += 0.2 * np.sin(2 * np.pi * frequency * 3 * t)  # 5도
    wave_data += 0.1 * np.sin(2 * np.pi * frequency * 4 * t)  # 옥타브
    wave_data += 0.05 * np.sin(2 * np.pi * frequency * 5 * t)  # 3도
    
    # 피아노 특성 추가 (어택과 감쇠)
    attack_time = 0.01  # 어택 시간
    attack_samples = int(attack_time * sample_rate)
    
    # 어택 부분 (빠른 상승)
    attack_envelope = np.linspace(0, 1, attack_samples)
    wave_data[:attack_samples] *= attack_envelope
    
    # 감쇠 부분 (자연스러운 감쇠)
    decay_envelope = np.exp(-t[attack_samples:] * 1.5)
    wave_data[attack_samples:] *= decay_envelope
    
    # 전체 볼륨 조절
    wave_data *= 0.3
    
    # 16비트로 변환
    wave_data = (wave_data * 32767).astype(np.int16)
    
    return wave_data

def save_wav(filename, wave_data, sample_rate=44100):
    """WAV 파일로 저장"""
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)  # 모노
        wav_file.setsampwidth(2)  # 16비트
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(wave_data.tobytes())

# 피아노 음계 주파수 (4옥타브)
notes = {
    'C': 261.63,   # 도
    'C#': 277.18,  # 도#
    'D': 293.66,   # 레
    'D#': 311.13,  # 레#
    'E': 329.63,   # 미
    'F': 349.23,   # 파
    'F#': 369.99,  # 파#
    'G': 392.00,   # 솔
    'G#': 415.30,  # 솔#
    'A': 440.00,   # 라
    'A#': 466.16,  # 라#
    'B': 493.88    # 시
}

print("자연스러운 피아노 소리 파일 생성 중...")

# 각 음계별로 WAV 파일 생성
for note_name, frequency in notes.items():
    filename = f"assets/sounds/{note_name}.wav"
    print(f"생성 중: {filename} ({frequency:.2f} Hz)")
    
    wave_data = create_piano_tone(frequency, duration=1.5)
    save_wav(filename, wave_data)

print("모든 피아노 소리 파일 생성 완료!")
print("이제 실제 피아노 소리가 재생될 것입니다!")

