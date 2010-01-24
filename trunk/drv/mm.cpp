#include "main.h "

void* _cdecl operator new (
    size_t size
    )
{
    UNREFERENCED_PARAMETER( size );
    return NULL; 
}

void _cdecl operator delete (
    void *p
    )
{
    UNREFERENCED_PARAMETER( p );
}

void* _cdecl operator new[] (
    size_t size
    )
{
    UNREFERENCED_PARAMETER( size );
    return NULL;
}

void _cdecl operator delete[] (
    void *p
    )
{
    UNREFERENCED_PARAMETER( p );
}
