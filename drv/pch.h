#pragma prefast(disable:28252) // sal annotation compare h -> cpp
#pragma prefast(disable:28253) // sal annotation compare h -> cpp
#pragma prefast(disable:28107) // critical region checks
#pragma prefast(disable:28175) // access to DEVICE_OBJECT members

#include <fltKernel.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <ntdddisk.h>

#include "mm.h"
