@echo off
rem Optimized build of the Japanese version (NPJB00135) into
rem build-release\bin\CodeVeronicaPort_JP.exe, beside the European one.
rem Needs the JP lift in src\recomp_jp (see cv_version.h for how it is made);
rem the European build is build-release.cmd.
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
if not exist build-release-jp\CMakeCache.txt (
 cmake -S . -B build-release-jp -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_C_COMPILER=C:/Program Files/LLVM/bin/clang-cl.exe" ^
  "-DCMAKE_CXX_COMPILER=C:/Program Files/LLVM/bin/clang-cl.exe" ^
  "-DCMAKE_LINKER=C:/Program Files/LLVM/bin/lld-link.exe" ^
  "-DCMAKE_C_FLAGS_RELEASE=/O2 /Ob2" ^
  "-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2" ^
  -DPS3RECOMP_DIR=%CV_ROOT% ^
  -DRECOMP_DIR=%CV_ROOT%/src/recomp_jp ^
  -DCV_REGION=JP ^
  -DSPU_DIR=%CV_ROOT%/projects/code_veronica/spu ^
  -DPORT_ENABLE_SPU=ON ^
  -DPS3RECOMP_BUILD_TESTS=OFF ^
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=%CV_ROOT%/projects/code_veronica/build-release/bin || exit /b 1
)
rem The lifted PPU units are ~45 MB each; cap parallel jobs to bound memory.
cmake --build build-release-jp -j 6 || exit /b 1
