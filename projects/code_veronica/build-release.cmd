@echo off
rem Optimized build of the Code Veronica recomp into build-release\bin.
rem The Debug build (build-x64) compiles the lifted game code with /Od, which
rem leaves the guest at ~98%% of frame time and makes FMV playback stutter.
setlocal
cd /d "%~dp0"
rem Absolute repo root (projects\code_veronica\..\..) with links resolved: the
rem menu hook script compares resolved paths, so RECOMP_DIR must not go
rem through a symlink or junction or the patched units are linked twice.
set "CV_ROOT="
for /f "delims=" %%I in ('python -c "import os,sys;print(os.path.realpath(sys.argv[1]))" "%~dp0..\.." 2^>nul') do set "CV_ROOT=%%I"
if not defined CV_ROOT for %%I in ("%~dp0..\..") do set "CV_ROOT=%%~fI"
cd /d "%CV_ROOT%\projects\code_veronica" || exit /b 1
set "CV_ROOT=%CV_ROOT:\=/%"
rem Visual Studio (any edition or version with the C++ tools), found by vswhere.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VCVARS="
if exist "%VSWHERE%" for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat`) do set "VCVARS=%%I"
if not defined VCVARS (
 echo Visual Studio with the C++ tools was not found. See "Building" in the README.
 exit /b 1
)
if not exist "C:\Program Files\LLVM\bin\clang-cl.exe" (
 echo LLVM ^(clang-cl^) was not found in C:\Program Files\LLVM. See "Building" in the README.
 exit /b 1
)
call "%VCVARS%" >nul || exit /b 1
rem Generate only the keyboard contact sheet when reviewing artwork.
if "%CV_KEYCAP_PREVIEW_ONLY%"=="1" (
 cmake --build build-release --target code_veronica_keycap_preview -j 6 || exit /b 1
 build-release\bin\code_veronica_keycap_preview.exe "%~dp0build-release\keyboard-icons-preview.png" || exit /b 1
 exit /b 0
)
rem Offline archive extraction and real map compatibility checks.
if "%CV_TEXTURE_EXPORT_TESTS_ONLY%"=="1" (
 cmake --build build-release --target code_veronica_texture_export_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_texture_export_tests.exe "%~dp0build-release/texture-export-check" "%CV_ROOT%/JOGO/NPEB00553/USRDIR/BHCV/nativePS3/biocv_disc/eng/rdx_lnk/rm_0030.arc" || exit /b 1
 if defined CV_TEXTURE_EXPORT_SOURCE (
  build-release\bin\code_veronica_texture_export_tests.exe "%~dp0build-release/texture-export-check-full" "%CV_TEXTURE_EXPORT_SOURCE%" || exit /b 1
 )
 exit /b 0
)
rem Focused offline checks/exports, using the actual native HD icon compositor.
if "%CV_HD_BUTTON_TESTS_ONLY%"=="1" (
 cmake --build build-release --target code_veronica_hd_button_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_hd_button_tests.exe "%CV_ROOT%/JOGO/NPEB00553/USRDIR/BHCV/nativePS3/biocv_disc/eng" "%~dp0build-release/hd-button-check" || exit /b 1
 for %%L in (fra ger ita spa) do (
  build-release\bin\code_veronica_hd_button_tests.exe "%CV_ROOT%/JOGO/NPEB00553/USRDIR/BHCV/nativePS3/biocv_disc/%%L" "%~dp0build-release/hd-button-check-%%L" || exit /b 1
 )
 build-release\bin\code_veronica_hd_button_tests.exe "D:/Jogos/rpcs3-v0.0.33-17018-0a57c459_win64/dev_hdd0/game/NPJB00135/USRDIR/BHCV/nativePS3/biocv_disc/jpn" "%~dp0build-release/hd-button-check-jp" || exit /b 1
 exit /b 0
)
if not exist build-release\CMakeCache.txt (
 cmake -S . -B build-release -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_C_COMPILER=C:/Program Files/LLVM/bin/clang-cl.exe" ^
  "-DCMAKE_CXX_COMPILER=C:/Program Files/LLVM/bin/clang-cl.exe" ^
  "-DCMAKE_LINKER=C:/Program Files/LLVM/bin/lld-link.exe" ^
  "-DCMAKE_C_FLAGS_RELEASE=/O2 /Ob2" ^
  "-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2" ^
  -DPS3RECOMP_DIR=%CV_ROOT% ^
  -DRECOMP_DIR=%CV_ROOT%/src/recomp ^
  -DSPU_DIR=%CV_ROOT%/projects/code_veronica/spu ^
  -DPORT_ENABLE_SPU=ON ^
  -DPS3RECOMP_BUILD_TESTS=OFF ^
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=%CV_ROOT%/projects/code_veronica/build-release/bin || exit /b 1
)
rem The lifted PPU units are ~45 MB each; cap parallel jobs to bound memory.
cmake --build build-release -j 6 || exit /b 1
rem Optional DXC testing: use the matched compiler/validator from the Windows
rem SDK already selected by vcvars64; these are NOT required for FXC.
if "%CV_DXC_TESTS%"=="1" (
 if not exist "%WindowsSdkVerBinPath%x64\dxcompiler.dll" exit /b 1
 if not exist "%WindowsSdkVerBinPath%x64\dxil.dll" exit /b 1
 copy /y "%WindowsSdkVerBinPath%x64\dxcompiler.dll" "build-release\bin\dxcompiler.dll" >nul || exit /b 1
 copy /y "%WindowsSdkVerBinPath%x64\dxil.dll" "build-release\bin\dxil.dll" >nul || exit /b 1
 cmake --build build-release --target code_veronica_dxc_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_dxc_tests.exe || exit /b 1
)
rem Optional focused shader regression tests; the normal game build is unchanged.
if "%CV_FP_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_fp_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_fp_tests.exe || exit /b 1
)
rem Keep build-release.cmd as the single entry point when validating both
rem supported regions; the Japanese executable shares the same runtime.
if "%CV_BUILD_JP%"=="1" call "%~dp0build-release-jp.cmd" || exit /b 1

rem Save/load checks run only on request, in isolated temporary folders.
if "%CV_SAVE_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_save_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_save_tests.exe || exit /b 1
)
rem Native menu checks use synthetic input without a game or GPU.
if "%CV_MENU_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_menu_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_menu_tests.exe || exit /b 1
)
rem Optional American executable; separate lift and build, same shared runtime.
if "%CV_BUILD_US%"=="1" (
 if not exist build-release-us\CMakeCache.txt (
  cmake -S . -B build-release-us -G Ninja ^
   -DCMAKE_BUILD_TYPE=Release ^
   "-DCMAKE_C_COMPILER=C:/Program Files/LLVM/bin/clang-cl.exe" ^
   "-DCMAKE_CXX_COMPILER=C:/Program Files/LLVM/bin/clang-cl.exe" ^
   "-DCMAKE_LINKER=C:/Program Files/LLVM/bin/lld-link.exe" ^
   "-DCMAKE_C_FLAGS_RELEASE=/O2 /Ob2" ^
   "-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2" ^
   -DPS3RECOMP_DIR=%CV_ROOT% ^
   -DRECOMP_DIR=%CV_ROOT%/src/recomp_us ^
   -DCV_REGION=US ^
   -DSPU_DIR=%CV_ROOT%/projects/code_veronica/spu ^
   -DPORT_ENABLE_SPU=ON ^
   -DPS3RECOMP_BUILD_TESTS=OFF ^
   -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=%CV_ROOT%/projects/code_veronica/build-release/bin || exit /b 1
 )
 cmake --build build-release-us -j 6 || exit /b 1
)

rem SRT memory/layout regression tests, isolated from real saves/settings.
if "%CV_SPU_FAULT_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_spu_fault_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_spu_fault_tests.exe || exit /b 1
)
if "%CV_SPU_POOL_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_spu_pool_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_spu_pool_tests.exe || exit /b 1
)
if "%CV_SPU_SCHEDULE_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_spu_schedule_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_spu_schedule_tests.exe || exit /b 1
)

if "%CV_SRT_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_srt_tests -j 6 || exit /b 1
 build-release\bin\cv_srt_tests_eu.exe codex_diag_srt || exit /b 1
 if "%CV_BUILD_JP%"=="1" (
  cmake --build build-release-jp --target code_veronica_srt_tests -j 6 || exit /b 1
  build-release\bin\cv_srt_tests_jp.exe || exit /b 1
 )
 if "%CV_BUILD_US%"=="1" (
  cmake --build build-release-us --target code_veronica_srt_tests -j 6 || exit /b 1
  build-release\bin\cv_srt_tests_us.exe || exit /b 1
 )
)

rem Map test tools: isolated guest-memory and input/dispatch checks.
if "%CV_MAP_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_map_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_map_tests.exe || exit /b 1
 if "%CV_BUILD_JP%"=="1" (
  cmake --build build-release-jp --target code_veronica_map_tests -j 6 || exit /b 1
  build-release\bin\code_veronica_map_tests_jp.exe || exit /b 1
 )
 if "%CV_BUILD_US%"=="1" (
  cmake --build build-release-us --target code_veronica_map_tests -j 6 || exit /b 1
  build-release\bin\code_veronica_map_tests_us.exe || exit /b 1
 )
)

if "%CV_FPS_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_fps_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_fps_tests.exe off || exit /b 1
 build-release\bin\code_veronica_fps_tests.exe on || exit /b 1
 build-release\bin\code_veronica_fps_tests.exe raw || exit /b 1
)

if "%CV_POSE_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_pose_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_pose_tests.exe || exit /b 1
)

if "%CV_SHADOW_FILTER_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_shadow_filter_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_shadow_filter_tests.exe || exit /b 1
)
if "%CV_POSE_GPU_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_pose_gpu_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_pose_gpu_tests.exe || exit /b 1
)

rem F6 item menu: rules and guest write on synthetic memory, per region;
rem with CV_ITEMS_PREVIEW=<folder> the EU run also draws the menu there.
if "%CV_ITEMS_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_items_tests -j 6 || exit /b 1
 if defined CV_ITEMS_PREVIEW (
  build-release\bin\cv_items_tests_eu.exe "%CV_ITEMS_PREVIEW%" "%CV_ROOT%/JOGO/NPEB00553" || exit /b 1
 ) else (
  build-release\bin\cv_items_tests_eu.exe || exit /b 1
 )
 if "%CV_BUILD_JP%"=="1" (
  cmake --build build-release-jp --target code_veronica_items_tests -j 6 || exit /b 1
  build-release\bin\cv_items_tests_jp.exe || exit /b 1
 )
 if "%CV_BUILD_US%"=="1" (
  cmake --build build-release-us --target code_veronica_items_tests -j 6 || exit /b 1
  build-release\bin\cv_items_tests_us.exe || exit /b 1
 )
)

if "%CV_CHEAT_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_cheat_tests -j 6 || exit /b 1
 build-release\bin\cv_cheats_tests_eu.exe || exit /b 1
 build-release\bin\cv_cheats_tests_jp.exe || exit /b 1
 build-release\bin\cv_cheats_tests_us.exe || exit /b 1
)

rem Launcher updater: no network, a throwaway folder in %%TEMP%%.
if "%CV_UPDATER_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_updater_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_updater_tests.exe || exit /b 1
)

rem Pause on focus loss: the decision logic, no game or window needed.
if "%CV_FOCUS_PAUSE_TESTS%"=="1" (
 cmake --build build-release --target code_veronica_focus_pause_tests -j 6 || exit /b 1
 build-release\bin\code_veronica_focus_pause_tests.exe || exit /b 1
)

rem Optional visual preview of the shared keyboard icon artwork.
if "%CV_KEYCAP_PREVIEW%"=="1" (
 cmake --build build-release --target code_veronica_keycap_preview -j 6 || exit /b 1
 build-release\bin\code_veronica_keycap_preview.exe "%~dp0build-release\keyboard-icons-preview.png" || exit /b 1
)
