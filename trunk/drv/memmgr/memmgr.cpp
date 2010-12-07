#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"

void* _cdecl operator new (
    size_t size,
    POOL_TYPE PoolType,
    ULONG Tag
    )
{
    void* ptr = ExAllocatePoolWithTag (
        PoolType,
        size,
        Tag
        );

    return ptr; 
}

void _cdecl operator delete (
    void *p
    )
{
    ExFreePool( p );
}

void* _cdecl operator new[] (
    size_t size,
    POOL_TYPE PoolType,
    ULONG Tag
    )
{
    void* ptr = ExAllocatePoolWithTag (
        PoolType,
        size,
        Tag
        );

    return ptr; 
}

void _cdecl operator delete[] (
    void *p
    )
{
    ExFreePool( p );
}
