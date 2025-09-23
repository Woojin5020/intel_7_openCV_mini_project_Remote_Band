#!/bin/bash
set -e

# 스크립트 위치 기준 프로젝트 루트(CMakeLists.txt 있는 폴더)
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
TARGET_NAME="guita_client"

# CMakeLists.txt 확인
if [[ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]]; then
  echo "[ERR] CMakeLists.txt not found in: $PROJECT_ROOT"
  exit 1
fi

# 빌드 디렉토리 준비
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# CMake configure & build
cmake "$PROJECT_ROOT"
make -j"$(nproc)"

echo "========================================"
echo "[OK] 빌드 완료"
echo "실행 파일: $BUILD_DIR/$TARGET_NAME"
echo "========================================"

# 실행 (인자 있으면 그대로 전달)
echo "[RUN] 프로그램 실행..."
"$BUILD_DIR/$TARGET_NAME" "$@"
