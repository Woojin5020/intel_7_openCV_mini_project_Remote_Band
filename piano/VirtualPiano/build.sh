#!/bin/bash

# Virtual Piano 빌드 스크립트

echo "=== Virtual Piano 빌드 스크립트 ==="

# 필요한 패키지 확인
echo "필요한 패키지 확인 중..."

# OpenCV 확인
if ! pkg-config --exists opencv4; then
    echo "OpenCV가 설치되지 않았습니다. 설치하시겠습니까? (y/n)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        sudo apt update
        sudo apt install libopencv-dev libopencv-contrib-dev
    else
        echo "OpenCV 설치가 필요합니다."
        exit 1
    fi
fi

# SFML 확인
if ! pkg-config --exists sfml-all; then
    echo "SFML이 설치되지 않았습니다. 설치하시겠습니까? (y/n)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        sudo apt update
        sudo apt install libsfml-dev
    else
        echo "SFML 설치가 필요합니다."
        exit 1
    fi
fi

# CMake 확인
if ! command -v cmake &> /dev/null; then
    echo "CMake가 설치되지 않았습니다. 설치하시겠습니까? (y/n)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        sudo apt update
        sudo apt install cmake build-essential
    else
        echo "CMake 설치가 필요합니다."
        exit 1
    fi
fi

echo "모든 필요한 패키지가 설치되어 있습니다."

# 빌드 디렉토리 생성
echo "빌드 디렉토리 생성 중..."
mkdir -p build
cd build

# CMake 설정
echo "CMake 설정 중..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# 컴파일
echo "컴파일 중..."
make -j$(nproc)

# 빌드 결과 확인
if [ -f "VirtualPiano" ]; then
    echo "✅ 빌드 성공!"
    echo "실행하려면: ./VirtualPiano"
    echo ""
    echo "실행하시겠습니까? (y/n)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        ./VirtualPiano
    fi
else
    echo "❌ 빌드 실패!"
    exit 1
fi

