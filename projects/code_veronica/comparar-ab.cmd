@echo off
setlocal
cd /d "%~dp0"
rem Comparacao A/B das melhorias do branch gow-melhorias.
rem   A = tudo novo DESLIGADO (igual a v1.5.3)
rem   B = tudo novo LIGADO
rem Os dois ligam a linha [STATS] (a cada 5 s no run-log.txt): fps, pior
rem quadro, quadros >= 20 ms, draws pulados por shader faltando, preload.
rem Jogue o mesmo trecho nos dois, feche o jogo e copie o run-log.txt antes
rem de abrir o outro (o log e sobrescrito a cada abertura).
echo.
echo  A - tudo novo desligado (v1.5.3)
echo  B - tudo novo ligado
echo.
choice /c AB /n /m "Escolha A ou B: "
set "CV_STATS=1"
if errorlevel 2 goto :ligado
set "CV_SHADER_SEED=0"
set "CV_AUDIO_FOLLOW_DEVICE=0"
set "CV_PIN=0"
set "CV_PRIORITY=0"
set "CV_FRAME_PACE=0"
set "CV_AUDIO_PRIO=0"
set "CV_AUDIO_SMOOTH=0"
set "CV_NO_GHOSTING=0"
set "CV_FG_GPU_CHECK=0"
set "SPU_TASK_POOL=0"
goto :abrir
:ligado
rem CV_PIN=auto so age em CPU de 6 nucleos (como no port do GoW).
set "CV_PIN=auto"
set "CV_PRIORITY=1"
set "CV_FRAME_PACE=1"
set "CV_AUDIO_PRIO=1"
set "CV_AUDIO_SMOOTH=1"
set "CV_NO_GHOSTING=1"
set "CV_FG_GPU_CHECK=1"
rem Reaproveita threads das tarefas SPU (inicio 110 -> 15 us na tela inicial).
set "SPU_TASK_POOL=1"
:abrir
start "" /D "%~dp0build-release\bin" "%~dp0build-release\bin\CodeVeronicaPort.exe"
echo Launcher aberto. Esta janela fecha sozinha.
timeout /t 4 >nul
