@echo off
REM Set the path to the EXE and the PRG file
set "EXE_PATH="
set "PRG_FILE=C:\EC-APPS\INPA\SGDAT\ms430ds0.prg"
set "LOG_FILE=%~dp0BimmerDis.log"

echo ================================================== >> "%LOG_FILE%"
echo Start time: %DATE% %TIME% >> "%LOG_FILE%"
echo Starting BimmerDis with: "%EXE_PATH%" "%PRG_FILE%" >> "%LOG_FILE%"

echo.
if not exist "%EXE_PATH%" (
    echo Error: File %EXE_PATH% was not found.
    echo Error: File %EXE_PATH% was not found. >> "%LOG_FILE%"
    pause
    exit /b 1
)

if not exist "%PRG_FILE%" (
    echo Error: File %PRG_FILE% was not found.
    echo Error: File %PRG_FILE% was not found. >> "%LOG_FILE%"
    pause
    exit /b 1
)

REM Run BimmerDis and save the output
"%EXE_PATH%" "%PRG_FILE%" >> "%LOG_FILE%" 2>&1

REM Save the result
if %ERRORLEVEL% neq 0 (
    echo Error: BimmerDis exited with error code %ERRORLEVEL%.
    echo Error: BimmerDis exited with error code %ERRORLEVEL% >> "%LOG_FILE%"
) else (
    echo BimmerDis executed successfully.
    echo BimmerDis executed successfully. >> "%LOG_FILE%"
)

echo End time: %DATE% %TIME% >> "%LOG_FILE%"
pause
