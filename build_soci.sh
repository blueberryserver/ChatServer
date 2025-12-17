#!/bin/bash
set -e

# ========================
# 1. 환경 준비
# ========================
echo "[1/4] Homebrew 패키지 확인..."
brew install cmake postgresql boost || true

# ========================
# 2. 소스 다운로드
# ========================
echo "[2/4] SOCI 소스코드 클론..."
if [ ! -d "soci" ]; then
  git clone https://github.com/SOCI/soci.git
else
  echo "soci 디렉토리 이미 존재합니다. 최신 소스 pull..."
  (cd soci && git pull)
fi

# ========================
# 3. 빌드
# ========================
echo "[3/4] 빌드 시작..."
cd soci
mkdir -p build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/homebrew \
  -DWITH_POSTGRESQL=ON \
  -DWITH_BOOST=ON

make -j$(sysctl -n hw.ncpu)

# ========================
# 4. 설치
# ========================
echo "[4/4] 설치 진행..."
make install

echo "✅ SOCI 빌드 및 설치 완료!"
echo "헤더 경로: /opt/homebrew/include/soci"
echo "라이브러리: /opt/homebrew/lib/libsoci_*"