#!/usr/bin/env bash
# One-shot Linux desktop build for qdomyos-zwift (feat/sb20-buttons).
# Mirrors the green GitHub-Actions recipe, using system Qt 5.15 from apt.
# Target: Ubuntu 22.04 / 24.04. Run from the repo root:  bash build-linux-desktop.sh
set -euo pipefail
cd "$(dirname "$0")"

echo ">>> [1/4] Installing Qt5 + BLE + build deps (sudo apt)..."
sudo apt-get update
sudo apt-get install -y \
  qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools qtdeclarative5-dev qtquickcontrols2-5-dev \
  libqt5bluetooth5 libqt5positioning5 libqt5xml5 qtconnectivity5-dev qtpositioning5-dev \
  libqt5charts5-dev libqt5networkauth5-dev libqt5websockets5-dev libqt5texttospeech5-dev \
  qtlocation5-dev qtmultimedia5-dev libqt5sql5-sqlite libqt5sql5 \
  libxcb-randr0-dev libxcb-xtest0-dev libxcb-xinerama0-dev libxcb-shape0-dev libxcb-xkb-dev \
  build-essential git bluez

# QML runtime modules needed to actually LAUNCH the UI (best-effort; names vary a little by release)
sudo apt-get install -y \
  qml-module-qtquick-controls2 qml-module-qtquick-layouts qml-module-qtquick-window2 \
  qml-module-qtquick2 qml-module-qtcharts qml-module-qtmultimedia qml-module-qtpositioning \
  qml-module-qtquick-dialogs qml-module-qtquick-extras || true

echo ">>> [2/4] Fetching submodules (smtpclient, googletest, qthttpserver)..."
git submodule update --init src/smtpclient tst/googletest src/qthttpserver

echo ">>> [3/4] Building qthttpserver (small prereq)..."
cp qHttpServerBin/5.15.2/headers/* src/qthttpserver/src/3rdparty/http-parser/ 2>/dev/null || true
( cd src/qthttpserver && qmake && make -j"$(nproc)" && sudo make install )

echo ">>> [4/4] Building qdomyos-zwift..."
qmake
make -j"$(nproc)"

echo ""
echo ">>> Build complete.  Binary: ./src/qdomyos-zwift"
echo ">>> Run with debug capture:  ./src/qdomyos-zwift 2>&1 | tee /tmp/qz.log"
