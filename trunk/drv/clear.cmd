for /f %%X in (%1) do (rd %%X\objfre_wlh_amd64 /S /Q)

for /f %%X in (%1) do (rd %%X\objfre_win7_amd64 /S /Q)
for /f %%X in (%1) do (rd %%X\objchk_win7_amd64 /S /Q)
for /f %%X in (%1) do (rd %%X\objfre_win7_x86 /S /Q)
for /f %%X in (%1) do (rd %%X\objchk_win7_x86 /S /Q)

exit /b 0

