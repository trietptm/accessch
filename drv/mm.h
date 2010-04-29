#ifndef __mm_h
#define __mm_h

#define _ALLOC_TAG                  'hcSA'

#ifndef FREE_POOL
#define FREE_POOL( _PoolPtr ) \
    if ( _PoolPtr ) \
    { \
        ExFreePool( _PoolPtr ); \
        _PoolPtr = NULL; \
    }
#endif // FREE_POOL

void* _cdecl operator new (
    size_t size
    );
 
void _cdecl operator delete (
    void *p
    );
 
void* _cdecl operator new[] (
    size_t size
    );

void _cdecl operator delete[] (
    void *p
    );
    
#endif //__mm_h