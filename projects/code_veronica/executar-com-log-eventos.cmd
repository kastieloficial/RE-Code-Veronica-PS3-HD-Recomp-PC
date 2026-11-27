@echo off
setlocal
cd /d "%~dp0"

rem Diagnostico dos eventos/transicoes que nao pulam (elevadores, submarino, etc.):
rem - CV_EVENT_LOG=1 grava event-log.txt (resumo automatico de cada cena + flags persistentes)
rem - Tecla F3 durante o jogo marca a cena atual (ou a que acabou de passar) em event-marks.txt
rem - CV_SMOOTH_LOG=1 grava smooth-log.txt (camera e controle) para comparar
rem - O pulo segue a opcao "Pular cenas de transicao" do launcher (aba Patches).

rem Faz backup automatico dos logs anteriores para nao sobrescrever nenhuma captura.
set "HAS_OLD="
if exist "event-log.txt" set "HAS_OLD=1"
if exist "event-marks.txt" set "HAS_OLD=1"
if defined HAS_OLD (
    for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "TS=%%i"
    if defined TS (
        mkdir "logs-eventos\%TS%" 2>nul
        if exist "event-log.txt" move /y "event-log.txt" "logs-eventos\%TS%\" >nul
        if exist "event-marks.txt" move /y "event-marks.txt" "logs-eventos\%TS%\" >nul
        if exist "smooth-log.txt" move /y "smooth-log.txt" "logs-eventos\%TS%\" >nul
        echo Backup dos logs anteriores salvo em: logs-eventos\%TS%\
    )
)

set "CV_EVENT_LOG=1"
set "CV_SMOOTH_LOG=1"
start "" /D "%~dp0build-release\bin" "%~dp0build-release\bin\CodeVeronicaPort.exe"

echo.
echo Launcher aberto com diagnostico de eventos aprimorado!
echo   - Log completo : event-log.txt  (com [SCENE-SUMMARY] e [SCENE-FLAGS])
echo   - Marcacoes F3 : event-marks.txt
echo   - Durante o jogo, aperte F3 na cena (ou logo apos) para marcar no log.
echo.
echo Esta janela fecha sozinha em 5 segundos...
timeout /t 5 >nul
