@echo off
if "%1"=="" goto _already
@echo parameters: %1 %2 %3 '%4'
call C:\WinDDK\7600.16385.1\bin\setenv.bat C:\WinDDK\7600.16385.1 %1 %2 %3 no_oacr
cd /D %4
rem buildprefast.cmd 
build %5
goto _exit

:_already
build -cC

:_exit