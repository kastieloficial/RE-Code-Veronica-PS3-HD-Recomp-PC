@echo off
setlocal
cd /d "%~dp0"
rem Medidor de controle: mostra se cada aperto do D-pad / START chega ao jogo.
rem Va a um menu, aperte para baixo rapido varias vezes, depois devagar.
rem Feche o jogo e mande o arquivo input-log.txt desta pasta.
rem   ok       = o jogo recebeu (e quantos ms depois do aperto)
rem   PERDIDO  = voce soltou antes de o jogo ler o controle
set "CV_INPUT_METER=1"
start "" /D "%~dp0build-release\bin" "%~dp0build-release\bin\CodeVeronicaPort.exe"
echo Launcher aberto com o medidor de controle. Esta janela fecha sozinha.
timeout /t 5 >nul
