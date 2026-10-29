@echo off
REM Requisitos: Visual Studio 2022 (carga "Desarrollo para el escritorio con C++") y CMake.
REM Ejecutar como ADMINISTRADOR para poder copiar a Program Files.
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :err
cmake --build build --config Release -j 4 || goto :err
xcopy /E /I /Y "build\RayaTune_artefacts\Release\VST3\RayaTune.vst3" "%CommonProgramFiles%\VST3\RayaTune.vst3" || goto :err
echo.
echo Listo: RayaTune.vst3 instalado en %CommonProgramFiles%\VST3
pause & exit /b 0
:err
echo ERROR en la compilacion.
pause & exit /b 1
