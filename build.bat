@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
cd /d "%~dp0"
set "ROOT=%~dp0"

echo ============================================================
echo   剪切板 ShearPlate  一键编译脚本  (Qt 6 + MinGW)
echo ============================================================
echo.

rem ---------- 1. 定位 Qt ----------
set "QT_ROOT="
if defined QT_DIR set "QT_ROOT=%QT_DIR%"

if not defined QT_ROOT (
  for %%D in (C D E F) do (
    if not defined QT_ROOT (
      if exist "%%D:\Qt" (
        for /f "delims=" %%V in ('dir /b /ad "%%D:\Qt" 2^>nul') do (
          if exist "%%D:\Qt\%%V\mingw_64\bin\qmake.exe" (
            set "QT_ROOT=%%D:\Qt\%%V\mingw_64"
          )
        )
      )
    )
  )
)

if not defined QT_ROOT (
  echo [错误] 未找到 Qt 6 MinGW 版。
  echo.
  echo   请先安装 Qt 6.x.x（安装时勾选 MinGW 64-bit 组件）,
  echo   或手动指定后重跑：
  echo       set QT_DIR=C:\Qt\6.x.x\mingw_64
  echo.
  pause
  exit /b 1
)
echo [1/6] Qt      : %QT_ROOT%

rem ---------- 2. 定位 MinGW 工具链 (gcc) ----------
set "MINGW_BIN="
set "QT_BASE=%QT_ROOT%\..\.."
if exist "%QT_BASE%\Tools" (
  for /f "delims=" %%M in ('dir /b /ad "%QT_BASE%\Tools" 2^>nul ^| findstr /i "mingw"') do (
    if exist "%QT_BASE%\Tools\%%M\bin\gcc.exe" (
      if not defined MINGW_BIN set "MINGW_BIN=%QT_BASE%\Tools\%%M\bin"
    )
  )
)
if not defined MINGW_BIN (
  echo [错误] 未找到 MinGW 工具链（gcc.exe）。
  echo   请在 Qt 安装时勾选 "MinGW" 组件，或手动把 gcc.exe 所在目录加入 PATH。
  pause
  exit /b 1
)
echo [2/6] MinGW    : %MINGW_BIN%

rem ---------- 3. 定位 CMake / Ninja ----------
set "CMAKE=cmake"
if exist "%QT_BASE%\Tools\CMake_64\bin\cmake.exe" set "CMAKE=%QT_BASE%\Tools\CMake_64\bin\cmake.exe"
set "NINJA="
if exist "%QT_BASE%\Tools\Ninja\ninja.exe" set "NINJA=%QT_BASE%\Tools\Ninja\ninja.exe"
echo [3/6] CMake    : %CMAKE%

rem ---------- 4. 配置环境 ----------
set "PATH=%QT_ROOT%\bin;%MINGW_BIN%;%PATH%"
if defined NINJA set "PATH=%QT_BASE%\Tools\Ninja;%PATH%"
if not "%CMAKE%"=="cmake" set "PATH=%CMAKE%\..;%PATH%"

rem 工作区路径含空格时，windres（图标资源编译器）会报错，改用不含空格的构建目录
set "BUILD_DIR=%ROOT%build"
echo "%ROOT%" | findstr /c:" " >nul
if not errorlevel 1 set "BUILD_DIR=%SystemDrive%\ShearPlateBuild"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo [4/6] 配置构建...
if defined NINJA (
  "%CMAKE%" -S "%ROOT%" -B "%BUILD_DIR%" -G "Ninja" -DCMAKE_PREFIX_PATH="%QT_ROOT%" -DCMAKE_BUILD_TYPE=Release
) else (
  "%CMAKE%" -S "%ROOT%" -B "%BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="%QT_ROOT%" -DCMAKE_BUILD_TYPE=Release
)
if errorlevel 1 goto :fail

echo [5/6] 编译中...
"%CMAKE%" --build "%BUILD_DIR%" --config Release
if errorlevel 1 goto :fail

rem ---------- 5. 部署 Qt 运行库 ----------
set "EXE=%BUILD_DIR%ShearPlate.exe"
if not exist "%EXE%" (
  echo [错误] 未找到生成的可执行文件: %EXE%
  goto :fail
)
echo [6/6] 部署 Qt 运行库...
"%QT_ROOT%\bin\windeployqt.exe" --release --no-translations "%EXE%"
if errorlevel 1 goto :fail

rem 若构建目录在工作区外（因路径含空格），复制产物回工作区 build
if not "%BUILD_DIR%"=="%ROOT%build" (
    if exist "%ROOT%build" rmdir /s /q "%ROOT%build"
    xcopy "%BUILD_DIR%\*" "%ROOT%build\" /e /i /q /y >nul
    set "EXE=%ROOT%build\ShearPlate.exe"
)

echo.
echo ============================================================
echo   编译成功！
echo.
echo   可执行文件: %EXE%
echo   该目录已包含 Qt 运行库，可直接双击运行（绿色免安装）。
echo   把整个 build 文件夹打包即可分发使用。
echo ============================================================
pause
exit /b 0

:fail
echo.
echo [失败] 编译出错，请查看上方日志。
pause
exit /b 1
