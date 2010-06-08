#ifndef __fltstore_h
#define __fltstore_h

typedef ULONG FilterId;

#define NumberOfBits 256

typedef enum OpertorId
{
    fltop_AND           = 0,
};

typedef struct FltData
{
    ULONG               m_DataSize;
    CHAR                m_Data[1];
};

typedef struct _FilterEntry
{
    LIST_ENTRY          m_List;
    OpertorId           m_Operation;
    ULONG               m_FiltersCount;
    FilterId*           m_Filters;
    FltData             m_Data;
};

class Filters
{
public:
    Filters();
    ~Filters();

    void
    Release();

private:
    EX_RUNDOWN_REF      m_Ref;
    EX_PUSH_LOCK        m_AccessLock;

    RTL_BITMAP          m_ActiveFilters;
    ULONG               m_ActiveFiltersBuffer[ NumberOfBits / sizeof(ULONG) ];
};

//////////////////////////////////////////////////////////////////////////

__checkReturn
Filters*
GetFiltersByOperation (
    __in Interceptors Interceptor,
    __in DriverOperationId Operation,
    __in_opt ULONG Minor,
    __in OperationPoint OperationType
    );

#endif //__fltstore_h