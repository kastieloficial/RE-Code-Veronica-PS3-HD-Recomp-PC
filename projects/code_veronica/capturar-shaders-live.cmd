@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
rem Captura de shaders em um clique (para a live):
rem  1. abre o launcher; jogue normalmente pelo tempo que quiser;
rem  2. feche o jogo E o launcher (o launcher volta sozinho depois do jogo);
rem  3. este atalho junta tudo o que foi capturado na lista embarcada
rem     shader-seed\pipelines-seed.list e abre a pasta com o arquivo marcado.
rem Quem grava os shaders e o proprio jogo (shader_cache\...\pipelines.list,
rem salvo a cada segundo). Se este atalho for fechado ou falhar, nada se
rem perde: rode-o de novo (ou juntar-shaders-capturados.cmd) para juntar.
set "CV_STATS=1"
set "EXE=%~dp0build-release\bin\CodeVeronicaPort.exe"
if not exist "%EXE%" (
 echo ERRO: nao achei %EXE%
 echo Compile o projeto primeiro ^(build-release.cmd^).
 pause & exit /b 1
)
start "" /D "%~dp0build-release\bin" "%EXE%"
echo.
echo  Abrindo o launcher...
rem Espera o launcher aparecer (ate 2 minutos), sem prazo fixo curto.
set /a tent=0
:aparece
call :rodando
if "!vivo!"=="1" goto :jogando
set /a tent+=1
if !tent! geq 60 (
 echo.
 echo  O launcher nao abriu em 2 minutos. Nada foi juntado.
 echo  Abra o atalho de novo. Os shaders ja capturados continuam salvos.
 pause & exit /b 1
)
timeout /t 2 /nobreak >nul
goto :aparece
:jogando
echo.
echo  Launcher aberto. Jogue normalmente, pelo tempo que quiser.
echo  Quando terminar, feche o jogo e depois o launcher.
echo  NAO feche esta janela: ela junta os shaders no final.
echo.
set /a seg=0
:espera
rem Duas checagens seguidas: o launcher reabre sozinho depois do jogo, e
rem nesse instante pode nao haver nenhum processo por um momento.
call :rodando
if "!vivo!"=="1" goto :continua
timeout /t 5 /nobreak >nul
call :rodando
if "!vivo!"=="1" goto :continua
goto :fim
:continua
timeout /t 5 /nobreak >nul
set /a seg+=5
set /a resto=seg %% 60
if !resto!==0 (set /a min=seg/60 & echo  ... jogando ha !min! min, aguardando o jogo e o launcher fecharem)
goto :espera
:fim
echo  Jogo e launcher fechados. Juntando os shaders capturados...
set "CV_MERGE_NOPAUSE=1"
call "%~dp0juntar-shaders-capturados.cmd"
if errorlevel 1 (pause & exit /b 1)
explorer /select,"%~dp0shader-seed\pipelines-seed.list"
echo.
echo Lista pronta: %~dp0shader-seed\pipelines-seed.list
pause
exit /b 0

:rodando
set "vivo=0"
for %%N in (CodeVeronicaPort.exe CodeVeronicaPort_US.exe CodeVeronicaPort_JP.exe) do (
 tasklist /nh /fi "imagename eq %%N" 2>nul | find /i "%%N" >nul && set "vivo=1"
)
exit /b 0
