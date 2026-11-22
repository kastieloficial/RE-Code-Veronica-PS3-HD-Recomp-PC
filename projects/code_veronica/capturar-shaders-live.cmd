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
rem Se o jogo travar (imagem parada ou tela preta longa), o proprio jogo grava
rem o motivo em hang-log.txt e esta janela avisa.
set "CV_STATS=1"
set "CV_HANG_LOG=1"
rem Estado da camera/jogador em smooth-log.txt (diagnostico do cronometro no 60 FPS).
set "CV_SMOOTH_LOG=1"
rem Medidor de desempenho: onde vai o tempo de cada quadro (run-log.txt, resumo com analisar-desempenho.cmd).
set "PERF=1"
rem Perfil da thread de renderizacao (render-profile.txt; use tools\perfil_renderizacao.py).
rem (desligado: pausava a thread de desenho mil vezes por segundo e pode provocar um travamento ocasional na abertura; ligue so se precisar do render-profile.txt)
rem set "CV_PROFILE_RENDER=1"
rem Diagnostico de picos de draws (tiros): grava build-release\bin\draw-spike-log.txt. 1 = limite automatico; um numero maior = limite fixo de draws.
set "CV_DRAW_SPIKE=1"
rem Teste A/B do reaproveitamento de trabalho do prepass entre draws iguais: rode uma vez com a linha abaixo ativa (CV_PREP_REUSE2=0 desliga) e uma vez sem; compare as linhas "ms: total/prepass/record" no draw-spike-log.txt.
rem set "CV_PREP_REUSE2=0"
rem Verificacao (so dev): CV_PREP_VERIFY=1 recalcula cada reaproveitamento e compara; mande de volta as linhas [PREP-REUSE-MISMATCH] e "prepass reuse:" do log.
rem set "CV_PREP_VERIFY=1"
rem Juncao de draws consecutivos iguais na gravacao (liga por padrao). Teste A/B: rode uma vez com a linha abaixo ativa (CV_DRAW_MERGE=0 desliga) e uma vez sem; compare "ms: total/prepass/record" e as linhas "merge:" no draw-spike-log.txt.
rem set "CV_DRAW_MERGE=0"
rem Verificacao (so dev): CV_DRAW_MERGE_VERIFY=1 reconfere cada corrida juntada campo a campo; mande de volta as linhas [DRAW-MERGE-VIOLATION] (esperado: nenhuma) e "merge(actual...)" do log.
rem set "CV_DRAW_MERGE_VERIFY=1"
rem Prepass e gravacao por grupo (liga por padrao): decide o grupo de draws iguais uma vez e anda grupo a grupo. Teste A/B: rode uma vez com a linha abaixo ativa (CV_DRAW_RUNS=0 volta ao prepass por draw + juncao anterior) e uma vez sem; mesma sala e mesma rajada; compare "ms: total/prepass/record" e as linhas "runs:" no draw-spike-log.txt.
rem set "CV_DRAW_RUNS=0"
rem Verificacao (so dev, lento): CV_DRAW_RUNS_VERIFY=1 recalcula cada draw coberto pelo caminho antigo e compara; mande de volta as linhas [DRAW-RUNS-MISMATCH] (esperado: nenhuma) e "runs(actual" do log.
rem set "CV_DRAW_RUNS_VERIFY=1"
set "base="
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
ping -n 3 127.0.0.1 >nul
goto :aparece
:jogando
echo.
echo  Launcher aberto. Jogue normalmente, pelo tempo que quiser.
echo  Quando terminar, feche o jogo e depois o launcher.
echo  NAO feche esta janela: ela junta os shaders no final.
echo  Cada shader novo aparece abaixo assim que o jogo o grava.
echo.
call :contar
set /a base=!conta!, antes=!conta!
echo  Shaders na lista embarcada: !lista!
echo  Capturados e ainda nao juntados: !conta!
echo  TOTAL: !total!   ^(o total tambem aparece no topo desta janela^)
echo.
call :travou
set "hang0=!hsz!"
set /a seg=0
:espera
rem Duas checagens seguidas: o launcher reabre sozinho depois do jogo, e
rem nesse instante pode nao haver nenhum processo por um momento.
call :rodando
if "!vivo!"=="1" goto :continua
ping -n 6 127.0.0.1 >nul
call :rodando
if "!vivo!"=="1" goto :continua
goto :fim
:continua
ping -n 6 127.0.0.1 >nul
set /a seg+=5
call :contar
if !conta! gtr !antes! (
 set /a novos=conta-antes, sessao=conta-base
 echo  [!time:~0,8!] +!novos! shader^(s^) novo^(s^) capturado^(s^)  -  nesta captura: !sessao!  -  TOTAL: !total!
 set /a antes=conta
)
call :travou
if not "!hsz!"=="!hang0!" (
 echo.
 echo  [!time:~0,8!] *** O JOGO PARECE TRAVADO. Motivo gravado em:
 echo      %~dp0hang-log.txt
 echo      Mande esse arquivo. Pode fechar o jogo; os shaders ja estao salvos.
 echo.
 set "hang0=!hsz!"
)
set /a resto=seg %% 60
if !resto!==0 (set /a min=seg/60 & echo  ... jogando ha !min! min, aguardando o jogo e o launcher fecharem)
goto :espera
:fim
echo.
echo  Nesta sessao voce capturou !sessao! shader^(s^) novo^(s^). TOTAL: !total!
echo  Jogo e launcher fechados. Juntando os shaders capturados...
set "CV_MERGE_NOPAUSE=1"
call "%~dp0juntar-shaders-capturados.cmd"
if errorlevel 1 (pause & exit /b 1)
explorer /select,"%~dp0shader-seed\pipelines-seed.list"
echo.
echo Lista pronta: %~dp0shader-seed\pipelines-seed.list
echo.
echo Log de picos de draws: %~dp0build-release\bin\draw-spike-log.txt
echo   ^(o jogo grava ao lado do .exe; a linha [ARMED] aparece no primeiro quadro e a cada 30 s uma linha [STATUS] com o maior numero de draws visto^)
if exist "%~dp0build-release\bin\draw-spike-log.txt" echo   existe: mande este arquivo ^(tem os picos, ou o [STATUS] dizendo que nao houve^).
if not exist "%~dp0build-release\bin\draw-spike-log.txt" echo   NAO existe: o diagnostico nem chegou a ligar. Use o .exe recompilado e este atalho, sem mudar o CV_DRAW_SPIKE acima.
if exist "%~dp0draw-spike-log.txt" echo   Ha tambem um log antigo na pasta do projeto: %~dp0draw-spike-log.txt
pause
exit /b 0

:contar
set "conta=!antes!"
if not defined conta set "conta=0"
for /f "tokens=1,2" %%C in ('python "%~dp0tools\contar_shaders.py" --total "%~dp0shader-seed\pipelines-seed.list" "%~dp0build-release\bin\shader_cache" 2^>nul') do (set "conta=%%C" & set "lista=%%D")
if not defined lista set "lista=0"
set /a total=lista+conta
set "sessao=0"
if defined base set /a sessao=conta-base
title Shaders: TOTAL !total!  ^|  nesta sessao: +!sessao!  ^|  ainda nao juntados: !conta!
exit /b 0

:travou
set "hsz=0"
if exist "%~dp0hang-log.txt" for %%F in ("%~dp0hang-log.txt") do set "hsz=%%~zF"
exit /b 0

:rodando
set "vivo=0"
for %%N in (CodeVeronicaPort.exe CodeVeronicaPort_US.exe CodeVeronicaPort_JP.exe) do (
 tasklist /nh /fi "imagename eq %%N" 2>nul | find /i "%%N" >nul && set "vivo=1"
)
exit /b 0
