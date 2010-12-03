#include <windows.h>
#include <ntverp.h>

#undef VER_FILEFLAGS
#undef VER_COMPANYNAME_STR
#undef VER_LEGALTRADEMARKS_STR
#undef VER_PRODUCTMAJORVERSION
#undef VER_PRODUCTMINORVERSION
#undef VER_PRODUCTBUILD
#undef VER_PRODUCTBUILD_QFE
#undef VER_PRODUCTNAME_STR
                          

#define VER_INTERNALNAME_STR            "ACCESCH" 

#define str1( x )  #x
#define str2( x )  str1(x)

#define VER_FILEDESCRIPTION_STR         "Accesch Mini-Filter " str2(RC_BUILD_OPT)
#define VER_INTERNALEXTENSION_STR       "SYS"

#define VER_FILETYPE    VFT_DRV
#define VER_FILESUBTYPE VFT2_DRV_SYSTEM

#if DBG
#define VER_DEBUG                   VS_FF_DEBUG
#else
#define VER_DEBUG                   0
#endif

#define VER_FILEFLAGSMASK           VS_FFI_FILEFLAGSMASK
#define VER_FILEFLAGS               VER_DEBUG
#define VER_FILEOS                  VOS_NT_WINDOWS32

#define VER_FILEVERSION_HIGH            1
#define VER_FILEVERSION_LOW             0
#define VER_FILEVERSION_BUILD           0
#define VER_FILEVERSION_COMPILATION     0

#define VER_PRODUCTMAJORVERSION         VER_FILEVERSION_HIGH
#define VER_PRODUCTMINORVERSION         VER_FILEVERSION_LOW
#define VER_PRODUCTBUILD        VER_FILEVERSION_BUILD
#define VER_PRODUCTBUILD_QFE        VER_FILEVERSION_COMPILATION

#define VER_COMPANYNAME_STR      "Sobko"
#define VER_LEGALCOPYRIGHT_STR   "Copyright © " VER_COMPANYNAME_STR " 2009."
#define VER_TRADEMARKS_STR       "empty"
#define VER_LEGALTRADEMARKS_STR  VER_TRADEMARKS_STR " is registered trademark of " VER_COMPANYNAME_STR "."
#define VER_PRODUCTNAME_STR "test"

#include <common.ver>
