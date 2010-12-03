#ifndef __excludes_h

void
RegisterInvisibleProcess (
    HANDLE Process
    );

void
UnregisterInvisibleProcess (
    HANDLE Process
    );

__checkReturn
BOOLEAN
IsInvisibleProcess (
    HANDLE Process
    );

#endif // __excludes_h
