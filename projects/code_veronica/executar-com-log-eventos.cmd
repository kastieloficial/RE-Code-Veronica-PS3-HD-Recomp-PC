@echo off
setlocal
cd /d "%~dp0"
rem Diagnostico dos eventos que nao pulam (elevador, submarino...):
rem CV_EVENT_LOG=1 grava event-log.txt a cada mudanca de estado do jogo.
rem CV_SMOOTH_LOG=1 grava smooth-log.txt (camera e controle) para comparar.
rem O pulo do elevador segue a opcao "Pular cenas de transicao" do launcher (aba Patches).
set "CV_EVENT_LOG=1"
set "CV_SMOOTH_LOG=1"
start "" /D "%~dp0build-release\bin" "%~dp0build-release\bin\CodeVeronicaPort.exe"
echo Launcher aberto com o log de eventos. Esta janela fecha sozinha.
timeout /t 4 >nul
