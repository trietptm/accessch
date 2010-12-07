#pragma once

#ifndef FREE_POOL
#define FREE_POOL( _PoolPtr ) \
    if ( _PoolPtr ) \
    { \
        ExFreePool( _PoolPtr ); \
        _PoolPtr = NULL; \
    }
#endif // FREE_POOL

void* _cdecl operator new (
    size_t size,
    POOL_TYPE PoolType,
    ULONG Tag
    );
 
void _cdecl operator delete (
    void *p
    );
 
void* _cdecl operator new[] (
    size_t size,
    POOL_TYPE PoolType,
    ULONG Tag
    );

void _cdecl operator delete[] (
    void *p
    );


#ifndef FREE_OBJECT
#define FREE_OBJECT( _ObjPtr ) \
    if ( _ObjPtr ) \
    { \
        delete _ObjPtr; \
        _ObjPtr = NULL; \
    }

#endif // FREE_POOL