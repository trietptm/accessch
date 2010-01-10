#ifndef __mm_h
#define __mm_h

void *operator new (
    size_t size
    );
 
void operator delete (
    void *p
    );
 
void* operator new[] (
    size_t size
    );

void operator delete[] (
    void *p
    );
    
#endif //__mm_h