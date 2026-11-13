@echo off
setlocal
if "%~4"=="" exit /b 2
set "module=%~f1"
set "pugi=%~f2"
set "out=%~f3"
set "vcvars=%~f4"
if exist "%out%" exit /b 3
call "%vcvars%"
if errorlevel 1 exit /b %errorlevel%
mkdir "%out%" || exit /b 4
pushd "%out%" || exit /b 4
cl /nologo /LD /W4 /WX /EHsc /std:c++20 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"%pugi%" "%module%\trainer_module.cpp" "%pugi%\pugixml.cpp" /link /out:gtavxr_trainer.dll
if errorlevel 1 exit /b %errorlevel%
dumpbin /dependents gtavxr_trainer.dll > imports.log
if errorlevel 1 exit /b %errorlevel%
dumpbin /exports gtavxr_trainer.dll > exports.log
if errorlevel 1 exit /b %errorlevel%
if "%~5"=="" exit /b 0
if not "%~6"=="" call "%~f6" x86_amd64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"%module%" "%~f5" /Fe"%out%\controls.exe"
if errorlevel 1 exit /b %errorlevel%
"%out%\controls.exe" "%out%\gtavxr_trainer.dll" "%out%\settings.ini"
exit /b %errorlevel%
