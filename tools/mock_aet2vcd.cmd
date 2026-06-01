@echo off

REM Mock aet2vcd for Bear2Wave test-e5: copies sample VCD to output path (IBM AET2 -> VCD wrapper).

if "%~2"=="" exit /b 1

set "SRC=%~dp0..\tests\traces\bear2wave_sample.vcd"

if not exist "%SRC%" exit /b 2

copy /Y "%SRC%" "%~2" >nul

exit /b 0
