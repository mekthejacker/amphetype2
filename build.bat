@SETLOCAL
@ECHO OFF

SET ARCH=%1

IF "%ARCH%" == "32" (
  ECHO x86 build
  SET BUILD_DIR=build_32
  SET QT=D:\Qt\5.7\msvc2015
  CALL "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86
) ELSE IF "%ARCH%" == "64" (
  ECHO x64 build
  SET BUILD_DIR=build_64
  SET QT=D:\Qt\5.7\msvc2015_64
  CALL "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
) ELSE (
  ECHO error
  EXIT
)

SET PATH=%PATH%;%QT%\bin
GOTO build
GOTO test
GOTO deploy

:build

  if not exist %BUILD_DIR% md %BUILD_DIR%
  cd %BUILD_DIR%
  del src\ui_*.h
  cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DQTROOT=%QT%
  ninja

:test
  ctest

:deploy
  windeployqt src\amphetype2.exe --no-opengl-sw --no-translations --no-system-d3d-compiler
