#pragma once

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