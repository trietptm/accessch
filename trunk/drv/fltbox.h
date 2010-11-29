#ifndef __fltbox_h
#define __fltbox_h

class FilterBox
{
public:
    FilterBox();
    ~FilterBox();

    __checkReturn
    NTSTATUS
    AddRef();

    void
    Release();

private:
    EX_RUNDOWN_REF      m_Ref;
};


class FilterBoxList
{
public:
    FilterBoxList();
    ~FilterBoxList();

    __checkReturn
    NTSTATUS
    GetOrCreateBox (
        LPGUID Guid
        );

private:

    EX_PUSH_LOCK    m_AccessLock;
};
#endif // __fltbox_h