@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ========================================
echo   直达 - 编译脚本
echo ========================================
echo.

set "PROJ_DIR=%~dp0直达"
set "VCXPROJ=%PROJ_DIR%\直达.vcxproj"

where msbuild >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [信息] 已找到 MSBuild
    goto :build
)

echo [信息] 正在查找 Visual Studio 2022 安装路径...

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [错误] 未找到 vswhere，请确认已安装 Visual Studio 2022
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
    set "MSBUILD_PATH=%%i"
)

if not defined MSBUILD_PATH (
    echo [错误] 未找到 MSBuild，请确认已安装 Visual Studio 2022 及 C++ 桌面开发工作负载
    pause
    exit /b 1
)

echo [信息] 找到 MSBuild: !MSBUILD_PATH!
path !MSBUILD_PATH!\..;!PATH!

:build
echo.
echo [步骤1] 开始编译 Release^|x64...
msbuild "%VCXPROJ%" /p:Configuration=Release /p:Platform=x64 /t:Rebuild /m /v:minimal /nologo

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [错误] Release 编译失败！尝试 Debug 编译...
    echo.
    echo [步骤2] 开始编译 Debug^|x64...
    msbuild "%VCXPROJ%" /p:Configuration=Debug /p:Platform=x64 /t:Rebuild /m /v:minimal /nologo
    
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo [错误] 编译失败！请检查代码错误。
        pause
        exit /b 1
    )
    
    echo.
    echo [成功] Debug 编译完成！
    echo   输出: %PROJ_DIR%\bin\Debug\x64\直达.exe
) else (
    echo.
    echo [成功] Release 编译完成！
    echo   输出: %PROJ_DIR%\bin\Release\x64\直达.exe
)

echo.
echo ========================================
echo   编译完成
echo ========================================
pause
