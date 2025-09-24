# Virtual Piano - 웹캠 기반 가상 피아노

웹캠을 사용하여 손가락을 감지하고 가상 피아노를 연주할 수 있는 C++ 애플리케이션입니다.

## 기능

- 🎹 12개 음계의 가상 피아노 키보드
- 📷 웹캠을 통한 손가락 감지
- 🎵 실시간 음악 재생
- ⌨️ 키보드로도 연주 가능 (테스트용)
- 🎨 시각적 피드백 (키 눌림 표시)

## 시스템 요구사항

- Ubuntu 20.04 이상 (또는 다른 Linux 배포판)
- C++17 지원 컴파일러
- 웹캠
- OpenCV 4.x
- SFML 2.5+

## 설치 및 빌드

### 1. 필요한 패키지 설치

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake
sudo apt install libopencv-dev libsfml-dev

# 또는 개별 설치
sudo apt install libopencv-contrib-dev
sudo apt install libsfml-audio-dev libsfml-graphics-dev libsfml-window-dev libsfml-system-dev
```

### 2. 프로젝트 빌드

```bash
# 프로젝트 디렉토리로 이동
cd VirtualPiano

# 빌드 디렉토리 생성
mkdir build
cd build

# CMake 설정
cmake ..

# 컴파일
make -j4
```

### 3. 실행

```bash
# 빌드 디렉토리에서
./VirtualPiano
```

## 사용법

### 웹캠 연주
1. 애플리케이션을 실행하면 웹캠이 활성화됩니다
2. 첫 번째 프레임이 배경으로 캡처됩니다
3. 손을 웹캠 앞에 가져가면 손가락이 감지됩니다
4. 손가락을 가상 피아노 키 위에 올리면 해당 음이 재생됩니다

### 키보드 연주 (테스트용)
- **흰색 키**: A, S, D, F, G, H, J
- **검은색 키**: W, E, T, Y, U
- **종료**: 창 닫기

## 프로젝트 구조

```
VirtualPiano/
├── CMakeLists.txt          # 빌드 설정
├── README.md              # 이 파일
├── src/                   # 소스 코드
│   ├── main.cpp          # 메인 애플리케이션
│   ├── Piano.h/cpp       # 피아노 클래스
│   ├── HandDetector.h/cpp # 손가락 감지 클래스
│   └── AudioManager.h/cpp # 오디오 관리 클래스
└── assets/               # 리소스 파일
    └── sounds/           # 오디오 파일 (선택사항)
```

## 클래스 설명

### Piano 클래스
- 가상 피아노 키보드 렌더링
- 키 상태 관리 (눌림/해제)
- 마우스/터치 좌표를 키 인덱스로 변환

### HandDetector 클래스
- 웹캠 초기화 및 프레임 캡처
- 배경 제거를 통한 손 감지
- 손가락 끝점 추출
- 좌표 변환 (웹캠 → 피아노)

### AudioManager 클래스
- 오디오 파일 로딩 및 재생
- 사인파 톤 생성 (오디오 파일이 없을 경우)
- 볼륨 제어
- 동시 음 재생 관리

## 문제 해결

### 웹캠이 인식되지 않는 경우
```bash
# 웹캠 장치 확인
ls /dev/video*

# 권한 확인
sudo usermod -a -G video $USER
```

### 오디오가 재생되지 않는 경우
```bash
# 오디오 시스템 확인
sudo apt install alsa-utils
aplay -l
```

### 컴파일 오류가 발생하는 경우
- OpenCV와 SFML이 올바르게 설치되었는지 확인
- CMake 버전이 3.16 이상인지 확인
- C++17 지원 컴파일러 사용 확인

## 향후 개선 사항

- [ ] 더 정확한 손가락 감지 알고리즘
- [ ] 실제 피아노 샘플 오디오 파일 지원
- [ ] 다중 손가락 동시 연주
- [ ] 음악 녹음 및 재생 기능
- [ ] 다양한 악기 사운드 지원
- [ ] 설정 메뉴 및 사용자 인터페이스 개선

## 라이선스

이 프로젝트는 MIT 라이선스 하에 배포됩니다.

## 기여하기

버그 리포트, 기능 요청, 또는 풀 리퀘스트를 환영합니다!

## 연락처

프로젝트에 대한 질문이나 제안이 있으시면 이슈를 생성해 주세요.

