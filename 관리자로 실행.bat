@echo off
setlocal
cd /d "%~dp0"

rem 게임이 관리자 권한으로 돌기 때문에, 사이드 버튼 메뉴를 쓰려면
rem TalesHelper도 같은 권한이어야 합니다. 이 파일은 그렇게 띄워줍니다.
rem 나머지 기능은 관리자 권한 없이도 그대로 동작합니다.
rem
rem 옆에 tales_helper.py 가 있으면 그 소스를 그대로 띄웁니다. 고친 내용이
rem 바로 반영되도록, 빌드해 둔 실행 파일보다 소스를 먼저 봅니다.
rem
rem 경로는 환경 변수로 넘깁니다. 따옴표가 cmd 와 PowerShell 을 거치며
rem 깨지지 않고, 폴더 이름에 공백이 있어도 그대로 전달됩니다.

set "TP_TARGET="
set "TP_ARGS="

if exist "%~dp0tales_helper.py" (
    rem pythonw 는 검은 창을 남기지 않습니다.
    for /f "delims=" %%P in ('where pythonw.exe 2^>nul') do if not defined TP_TARGET set "TP_TARGET=%%P"
    if not defined TP_TARGET for /f "delims=" %%P in ('where python.exe 2^>nul') do if not defined TP_TARGET set "TP_TARGET=%%P"
    if not defined TP_TARGET goto :nopython
    set "TP_ARGS=%~dp0tales_helper.py"
)

if not defined TP_TARGET if exist "%~dp0TalesHelper.exe" set "TP_TARGET=%~dp0TalesHelper.exe"
if not defined TP_TARGET if exist "%~dp0dist\TalesHelper.exe" set "TP_TARGET=%~dp0dist\TalesHelper.exe"
if not defined TP_TARGET goto :nothing

if defined TP_ARGS (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "try { Start-Process -FilePath $env:TP_TARGET -ArgumentList $env:TP_ARGS -Verb RunAs -ErrorAction Stop } catch { exit 1 }"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "try { Start-Process -FilePath $env:TP_TARGET -Verb RunAs -ErrorAction Stop } catch { exit 1 }"
)

if errorlevel 1 (
    echo.
    echo 권한 요청이 취소되어 TalesHelper 가 실행되지 않았습니다.
    echo.
    pause
)
exit /b

:nothing
echo.
echo tales_helper.py 도 TalesHelper.exe 도 찾지 못했습니다.
echo 이 파일을 둘 중 하나와 같은 폴더에 두세요.
echo.
pause
exit /b 1

:nopython
echo.
echo 파이썬을 찾지 못했습니다.
echo.
pause
exit /b 1
