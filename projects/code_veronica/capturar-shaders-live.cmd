@echo off
setlocal
cd /d "%~dp0"
rem Captura de shaders para a live: abra, jogue normalmente, feche o jogo.
rem O jogo ja grava sozinho cada pipeline novo em
rem build-release\bin\shader_cache\<pasta>\pipelines.list (programas do PS3,
rem nao HLSL: serve para NVIDIA, AMD e Intel). Este atalho so liga a linha
rem leve de estatisticas para voce ver quantos draws ainda pulam.
rem Depois da live, rode juntar-shaders-capturados.cmd.
set "CV_STATS=1"
start "" /D "%~dp0build-release\bin" "%~dp0build-release\bin\CodeVeronicaPort.exe"
echo Launcher aberto. Jogue a vontade; ao terminar feche o jogo e rode juntar-shaders-capturados.cmd
timeout /t 5 >nul
