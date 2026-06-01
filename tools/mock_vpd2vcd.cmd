@echo off
REM Mock vpd2vcd for Bear2Wave test-e4: copies sample VCD to output path.
if "%~2"=="" exit /b 1
set "SRC=%~dp0..\tests\traces\bear2wave_sample.vcd"
if not exist "%SRC%" exit /b 2
copy /Y "%SRC%" "%~2" >nul
exit /b 0
