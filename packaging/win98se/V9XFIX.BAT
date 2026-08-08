@ECHO OFF
ECHO Velocity9x phase3-matrix-v2 normal-mode repair
ECHO.
IF NOT EXIST D:\ACTIVE\V9XFIX.INF GOTO MISSING
IF NOT EXIST D:\ACTIVE\V9XDISP.DRV GOTO MISSING
IF NOT EXIST D:\ACTIVE\V9XMINI.VXD GOTO MISSING
IF NOT EXIST C:\WINDOWS\RUNDLL.EXE GOTO NORUNDLL
IF NOT EXIST C:\WINDOWS\SYSTEM\SETUPX.DLL GOTO NORUNDLL

ECHO Asking Windows Setup to replace the installed driver on reboot...
C:\WINDOWS\RUNDLL.EXE C:\WINDOWS\SYSTEM\SETUPX.DLL,InstallHinfSection DefaultInstall 132 D:\ACTIVE\V9XFIX.INF
IF ERRORLEVEL 1 GOTO FAILED
ECHO.
ECHO Repair request completed. Shut down Windows normally, then cold-start once.
GOTO DONE

:MISSING
ECHO FAIL: D:\ACTIVE does not contain the complete repair package.
GOTO DONE

:NORUNDLL
ECHO FAIL: Windows 98 RUNDLL.EXE or SETUPX.DLL was not found.
GOTO DONE

:FAILED
ECHO FAIL: Windows Setup could not stage the corrected driver files.

:DONE
PAUSE
