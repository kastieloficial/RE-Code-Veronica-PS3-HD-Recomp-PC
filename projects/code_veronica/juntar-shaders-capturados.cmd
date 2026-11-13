@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
rem Junta todas as listas capturadas (shader_cache\*\pipelines.list desta pasta, build-release\bin\shader_cache\*\pipelines.list
rem e as de qualquer pasta passada como argumento) com a lista embarcada atual
rem em shader-seed\pipelines-seed.list. Guarda a anterior como .bak.
set "SEED=shader-seed\pipelines-seed.list"
set "LISTS=%SEED%"
rem The launcher runs the game from this folder, so its recordings land in
rem shader_cache\ here; a game started straight from bin\ records there.
for /d %%D in ("shader_cache\*") do if exist "%%D\pipelines.list" set "LISTS=!LISTS! "%%D\pipelines.list""
for /d %%D in ("build-release\bin\shader_cache\*") do if exist "%%D\pipelines.list" set "LISTS=!LISTS! "%%D\pipelines.list""
for %%A in (%*) do (
 if exist "%%~A\pipelines.list" set "LISTS=!LISTS! "%%~A\pipelines.list""
 for /d %%D in ("%%~A\shader_cache\*") do if exist "%%D\pipelines.list" set "LISTS=!LISTS! "%%D\pipelines.list""
 if /i "%%~xA"==".list" set "LISTS=!LISTS! "%%~A""
)
copy /y "%SEED%" "%SEED%.bak" >nul
python tools\merge_pipeline_lists.py -o "%SEED%.new" %LISTS% || goto :erro
move /y "%SEED%.new" "%SEED%" >nul
python tools\merge_pipeline_lists.py --stats "%SEED%"
echo.
echo Pronto: %SEED% atualizado (anterior em %SEED%.bak).
echo Para entregar: envie esse arquivo ou faca commit dele. Nao contem dados do jogo.
if not "%CV_MERGE_NOPAUSE%"=="1" pause
exit /b 0
:erro
echo Falhou ao juntar as listas; a lista embarcada nao foi alterada.
if not "%CV_MERGE_NOPAUSE%"=="1" pause
exit /b 1
