#include "main.h "

void *operator new (
    size_t size
    )
{
    UNREFERENCED_PARAMETER( size );
    return NULL; 
}

void operator delete (
    void *p
    )
{
    UNREFERENCED_PARAMETER( p );
}

void* operator new[] (
    size_t size
    )
{
    UNREFERENCED_PARAMETER( size );
    return NULL;
}

void operator delete[] (
    void *p
    )
{
    UNREFERENCED_PARAMETER( p );
}
