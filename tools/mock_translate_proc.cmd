@echo off
REM Mock translate_proc for Bear2Wave test-fp0/fp1.
REM stdin:  full_name<TAB>time<TAB>raw_value
REM stdout: DEC:<raw_value>

setlocal EnableDelayedExpansion
set "LINE="
set /p LINE=
if not defined LINE exit /b 1

for /f "tokens=1,2,3 delims=	" %%a in ("!LINE!") do set "RAW=%%c"
if not defined RAW set "RAW="
echo DEC:!RAW!
exit /b 0
