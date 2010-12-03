#ifndef __fltbox_h
#define __fltbox_h

class FilterBox
{
public:
    static ULONG    m_AllocTag;

public:
    FilterBox (
        __in LPGUID Guid
        );

    ~FilterBox();

    __checkReturn
    NTSTATUS
    AddRef();

    void
    Release();

    __checkReturn
    NTSTATUS
    AddParams (
        __in ULONG ParamsCount,
        __in PPARAM_ENTRY Params,
        __out PULONG Position
        );

    NTSTATUS
    MatchEvent (
        __in EventData *Event,
        __in PRTL_BITMAP Affecting
        );

public:
    LIST_ENTRY      m_List;
    LONG            m_RefCount;
    GUID            m_Guid;

private:
    ULONG           m_NextFreePosition;

    LIST_ENTRY      m_Items;
};

#define PFilterBox FilterBox*

//////////////////////////////////////////////////////////////////////////

class FilterBoxList
{
public:
    FilterBoxList();
    ~FilterBoxList();

    __checkReturn
    NTSTATUS
    GetOrCreateBox (
        __in LPGUID Guid,
        __deref_out_opt PFilterBox* FltBox
        );

    FilterBox*
    LookupBox (
        __in LPGUID Guid
        );

private:
    FilterBox*
    CreateNewp (
        __in LPGUID Guid
        );

    FilterBox*
    LookupBoxp (
        __in LPGUID Guid
        );

private:
    EX_PUSH_LOCK    m_AccessLock;
    LIST_ENTRY      m_List;
};

#define PFilterBoxList FilterBoxList*

#endif // __fltbox_h