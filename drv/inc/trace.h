#pragma once

#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(CtlGuid,(CFC1EC35, 85CC, 4DB9, AFF1, 8442957D549F),  \
        WPP_DEFINE_BIT(TB_CORE)       \
        WPP_DEFINE_BIT(TB_FILEMGR)    \
        WPP_DEFINE_BIT(TB_FILESRV)    \
        WPP_DEFINE_BIT(TB_FILTERS) )

#define WPP_LEVEL_FLAGS_LOGGER(level,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(level, flags) \
    (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= level)

#define WPP_CHECK_FOR_NULL_STRING  //to prevent exceptions due to NULL strings


