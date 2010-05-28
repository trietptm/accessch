#ifndef __excludes_h

VOID
RegisterInvisibleProcess (
    HANDLE Process
    );

VOID
UnregisterInvisibleProcess (
    HANDLE Process
    );

__checkReturn
BOOLEAN
IsInvisibleProcess (
    HANDLE Process
    );

#endif // __excludes_h
