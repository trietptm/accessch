@echo off
@rem "!build.cmd" chk WLH x86 E:\OpenSource\accessch\drv
@echo parameters: %1 %2 %3 '%4'
call C:\WinDDK\7600.16385.1\bin\setenv.bat C:\WinDDK\7600.16385.1 %1 %2 %3
cd /D %4
rem buildprefast.cmd 
build -cC
