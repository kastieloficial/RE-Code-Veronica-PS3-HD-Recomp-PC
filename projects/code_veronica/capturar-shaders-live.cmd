@echo off
setlocal
cd /d "%~dp0"
rem Captura de shaders em um clique (para a live):
rem  1. abre o launcher; jogue normalmente pelo tempo que quiser;
rem  2. feche o jogo E o launcher (o launcher volta sozinho depois do jogo);
rem  3. este atalho junta sozinho tudo o que foi capturado na lista embarcada
rem     shader-seed\pipelines-seed.list e abre a pasta com o arquivo marcado.
rem Pode repetir quantas vezes quiser: a lista so cresce, nada se perde.
rem Deixe esta janela aberta enquanto joga.
set "CV_STATS=1"
start "" /D "%~dp0build-release\bin" "%~dp0build-release\bin\CodeVeronicaPort.exe"
echo.
echo  Jogue normalmente. Quando terminar, feche o jogo e o launcher.
echo  Os shaders serao juntados automaticamente. Nao feche esta janela.
echo.
timeout /t 10 >nul
:espera
tasklist /fi "imagename eq CodeVeronicaPort.exe" 2>nul | find /i "CodeVeronicaPort" >nul && (timeout /t 3 >nul & goto :espera)
tasklist /fi "imagename eq CodeVeronicaPort_US.exe" 2>nul | find /i "CodeVeronicaPort" >nul && (timeout /t 3 >nul & goto :espera)
tasklist /fi "imagename eq CodeVeronicaPort_JP.exe" 2>nul | find /i "CodeVeronicaPort" >nul && (timeout /t 3 >nul & goto :espera)
echo Jogo fechado. Juntando os shaders capturados...
set "CV_MERGE_NOPAUSE=1"
call "%~dp0juntar-shaders-capturados.cmd"
if errorlevel 1 (pause & exit /b 1)
explorer /select,"%~dp0shader-seed\pipelines-seed.list"
echo.
echo Lista pronta: %~dp0shader-seed\pipelines-seed.list
pause
