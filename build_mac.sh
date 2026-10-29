#!/bin/bash
# Requisitos: Xcode Command Line Tools (xcode-select --install) y CMake (brew install cmake)
set -e
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
mkdir -p ~/Library/Audio/Plug-Ins/VST3
rm -rf ~/Library/Audio/Plug-Ins/VST3/RayaTune.vst3
cp -R build/RayaTune_artefacts/Release/VST3/RayaTune.vst3 ~/Library/Audio/Plug-Ins/VST3/
xattr -cr ~/Library/Audio/Plug-Ins/VST3/RayaTune.vst3 || true
echo "Listo: RayaTune.vst3 instalado en ~/Library/Audio/Plug-Ins/VST3"
