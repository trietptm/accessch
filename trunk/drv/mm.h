#ifndef __mm_h
#define __mm_h

#define _ALLOC_TAG                  'hcca'

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