#ifndef __fltstore_h
#define __fltstore_h

#define FilterId ULONG

#define NumberOfBits 256

typedef enum _OpertorId
{
    fltop_AND           = 0,
} OpertorId;

typedef struct _FltData
{
    ULONG               m_DataSize;
    UCHAR               m_Data[1];
} FltData;

typedef struct _FilterEntry
{
    LIST_ENTRY          m_List;
    OpertorId           m_Operation;
    ULONG               m_NumbersCount;
    PULONG              m_FilterNumbers;
    FltData             m_Data;
} FilterEntry;

//////////////////////////////////////////////////////////////////////////
class Filters
{
public:
    Filters();
    ~Filters();

    void
    Release();

    VERDICT
    GetVerdict (
        __in EventData *Event,
        __out PARAMS_MASK *ParamsMask
        );

private:
    EX_RUNDOWN_REF      m_Ref;
    EX_PUSH_LOCK        m_AccessLock;

    RTL_BITMAP          m_ActiveFilters;
    ULONG               m_ActiveFiltersBuffer[ NumberOfBits / sizeof(ULONG) ];
    LIST_ENTRY          m_FilteringHead;
};

//////////////////////////////////////////////////////////////////////////

class FiltersTree
{
public:
    static
    VOID
    Initialize (
        );

    static
    VOID
    Destroy (
        );

    static RTL_AVL_COMPARE_ROUTINE Compare;
    static RTL_AVL_ALLOCATE_ROUTINE Allocate;
    static RTL_AVL_FREE_ROUTINE Free;
  
    __checkReturn
    static
    Filters*
    GetFiltersByOperation (
        __in Interceptors Interceptor,
        __in DriverOperationId Operation,
        __in_opt ULONG Minor,
        __in OperationPoint OperationType
        );
private:
    static RTL_AVL_TABLE    m_Tree;
    static EX_PUSH_LOCK     m_AccessLock;

public:
    FiltersTree();
    ~FiltersTree();
};


#endif //__fltstore_h