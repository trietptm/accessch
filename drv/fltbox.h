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

#endif // __fltbox_h