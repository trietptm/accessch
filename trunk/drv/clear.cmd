for /f %%X in (%1) do (rd %%X\objfre_wlh_amd64 /S /Q)
for /f %%X in (%1) do (rd %%X\objfre_win7_x86 /S /Q)

exit /b 0

rem rd channel\objchk_win7_x86 /S /Q
rem rd filemgr\objchk_win7_x86 /S /Q
rem rd fltsystem\objchk_win7_x86 /S /Q
rem rd main\objchk_win7_x86 /S /Q
rem rd memmgr\objchk_win7_x86 /S /Q
rem rd osspec\objchk_win7_x86 /S /Q
