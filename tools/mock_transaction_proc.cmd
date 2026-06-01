@echo off
REM Mock transaction_proc for Bear2Wave test-fp2.
REM stdin:  minimal VCD from trace_vcd_export_minimal
REM stdout: $name / #time value lines (GTKWave transaction filter subset)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0mock_transaction_proc.ps1"
exit /b %ERRORLEVEL%
