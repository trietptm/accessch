#include "pch.h"
#include "proclist.h"

ULONG           ProcList::m_AllocTag = 'lpSA';
RTL_AVL_TABLE   ProcList::m_Tree;
EX_PUSH_LOCK    ProcList::m_TreeAccessLock;

EX_PUSH_LOCK    ProcList::m_CbAccessLock;
LIST_ENTRY      ProcList::m_ExitProcessCbList;


class ProcessInfo
{
public:
    ProcessInfo (
        )
    {
        ExInitializeRundownProtection( &m_Ref );
    }

    ~ProcessInfo (
        )
    {
        ExRundownCompleted( &m_Ref );
    }

    NTSTATUS
    Acquire (
        )
    {
        if ( ExAcquireRundownProtection( &m_Ref ) )
        {
            return STATUS_SUCCESS;
        }

        return STATUS_UNSUCCESSFUL;
    }

    void
    Release (
        )
    {
        ExReleaseRundownProtection( &m_Ref );
    }

    void
    WaitForRelease (
        )
    {
        ExWaitForRundownProtectionRelease( &m_Ref );
    }
    
private:
    EX_RUNDOWN_REF m_Ref;
};
typedef struct _ProcessItem
{
    HANDLE              m_ProcessId;
    ProcessInfo*        m_Info;
} ProcessItem, *PProcessItem;

typedef struct _ExitProcessCbList
{
    LIST_ENTRY          m_List;
    EX_RUNDOWN_REF      m_Ref;
    _tpProcessExitCb    m_Cb;
    PVOID               m_Opaque;
} ExitProcessCbList, *PExitProcessCbList;

//////////////////////////////////////////////////////////////////////////

void
ProcList::Initialize (
    )
{
    FltInitializePushLock( &m_TreeAccessLock );

    RtlInitializeGenericTableAvl (
        &m_Tree,
        ProcList::Compare,
        ProcList::Allocate,
        ProcList::Free,
        NULL
        );

    FltInitializePushLock( &m_CbAccessLock );
    InitializeListHead( &m_ExitProcessCbList );

#if ( NTDDI_VERSION < NTDDI_WIN7 )
    PsSetCreateProcessNotifyRoutineEx( CreateProcessNotifyExCb, FALSE );
#else
    PsSetCreateProcessNotifyRoutine( CreateProcessNotifyCb, FALSE );
#endif
}

void
ProcList::Destroy (
    )
{
#if ( NTDDI_VERSION < NTDDI_WIN7 )
    PsSetCreateProcessNotifyRoutineEx( CreateProcessNotifyExCb, TRUE );
#else
    PsSetCreateProcessNotifyRoutine( CreateProcessNotifyCb, TRUE );
#endif

    // not necessary synchronize queue and items
    PProcessItem pItem = NULL;
    do 
    {
        pItem = (PProcessItem) RtlEnumerateGenericTableAvl (
            &m_Tree,
            TRUE
            );

        if ( pItem )
        {
            ASSERT( pItem->m_Info );

            pItem->m_Info->ProcessInfo::~ProcessInfo();
            FREE_POOL( pItem->m_Info );

            RtlDeleteElementGenericTableAvl( &m_Tree, pItem );
        }

    } while ( pItem );

    ASSERT( IsListEmpty( &m_ExitProcessCbList ) );
}

NTSTATUS
ProcList::RegisterExitProcessCb (
    __in _tpProcessExitCb CbFunc,
    __in_opt PVOID Opaque
    )
{
    ASSERT( CbFunc );

    PExitProcessCbList pItem = ( PExitProcessCbList ) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( ExitProcessCbList ),
        m_AllocTag
        );

    if ( !pItem )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pItem->m_Cb = CbFunc;
    pItem->m_Opaque = Opaque;
    ExInitializeRundownProtection( &pItem->m_Ref );

    FltAcquirePushLockExclusive( &m_CbAccessLock );

    InsertHeadList( &m_ExitProcessCbList, &pItem->m_List );

    FltReleasePushLock( &m_CbAccessLock );

    return STATUS_SUCCESS;
}

void
ProcList::UnregisterExitProcessCb ( 
    __in _tpProcessExitCb CbFunc
    )
{
    ASSERT( CbFunc );

    PExitProcessCbList pItem = NULL;

    FltAcquirePushLockExclusive( &m_CbAccessLock );
    
    if ( !IsListEmpty( &m_ExitProcessCbList ) )
    {
        PLIST_ENTRY Flink;

        Flink = m_ExitProcessCbList.Flink;

        while ( Flink != &m_ExitProcessCbList )
        {
            pItem = CONTAINING_RECORD( Flink, ExitProcessCbList, m_List );
            Flink = Flink->Flink;

            if ( pItem->m_Cb == CbFunc )
            {
                RemoveEntryList( &pItem->m_List );
                
                break;
            }
            
            pItem = NULL;
        }
    }

    FltReleasePushLock( &m_CbAccessLock );

    if ( pItem )
    {
        ExWaitForRundownProtectionRelease( &pItem->m_Ref );
        ExRundownCompleted( &pItem->m_Ref );

        FREE_POOL( pItem );
    }
}

//////////////////////////////////////////////////////////////////////////

RTL_GENERIC_COMPARE_RESULTS
NTAPI
ProcList::Compare (
    __in struct _RTL_AVL_TABLE *Table,
    __in PVOID FirstStruct,
    __in PVOID SecondStruct
    )
{
    UNREFERENCED_PARAMETER( Table );

    PProcessItem Struct1 = (PProcessItem) FirstStruct;
    PProcessItem Struct2 = (PProcessItem) SecondStruct;

    if ( Struct1->m_ProcessId == Struct2->m_ProcessId )
    {
        return GenericEqual;
    }

    if ( Struct1->m_ProcessId > Struct2->m_ProcessId )
    {
        return GenericGreaterThan;
    }

    return GenericLessThan;
}

PVOID
NTAPI
ProcList::Allocate (
    __in struct _RTL_AVL_TABLE *Table,
    __in CLONG ByteSize
    )
{
    UNREFERENCED_PARAMETER( Table );

    PVOID ptr = ExAllocatePoolWithTag (
        PagedPool,
        ByteSize,
        m_AllocTag
        );

    return ptr;
}

void
NTAPI
ProcList::Free (
    __in struct _RTL_AVL_TABLE *Table,
    __in __drv_freesMem(Mem) __post_invalid PVOID Buffer
    )
{
    UNREFERENCED_PARAMETER( Table );

    FREE_POOL( Buffer );
}

//////////////////////////////////////////////////////////////////////////

void
ProcList::CreateProcessNotifyExCb (
    __inout PVOID Process,
    __in HANDLE ProcessId,
    __in_opt PVOID CreateInfo
    )
{
    PEPROCESS process =  ( PEPROCESS ) Process;
    PPS_CREATE_NOTIFY_INFO createInfo = ( PPS_CREATE_NOTIFY_INFO ) CreateInfo;

    if ( createInfo )
    {
        ProcList::RegisterNewProcess( ProcessId );
    }
    else
    {
        ProcList::UnregisterProcess( ProcessId );
    }
}

void
ProcList::CreateProcessNotifyCb (
    __in HANDLE ParentId,
    __in HANDLE ProcessId,
    __in BOOLEAN Create
    )
{
    if ( Create )
    {
        ProcList::RegisterNewProcess( ProcessId );
    }
    else
    {
        ProcList::UnregisterProcess( ProcessId );
    }
}

__checkReturn
NTSTATUS
ProcList::RegisterNewProcess (
    HANDLE ProcessId
    )
{
    NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;

    ProcessItem item;
    item.m_ProcessId = ProcessId;
    item.m_Info = 0;

    BOOLEAN newElement = FALSE;

    FltAcquirePushLockExclusive( &m_TreeAccessLock );

    PProcessItem pItem = (PProcessItem) RtlInsertElementGenericTableAvl (
        &m_Tree,
        &item,
        sizeof( item ),
        &newElement
        );

    if ( pItem )
    {
        if ( newElement )
        {
            pItem->m_Info = (ProcessInfo*) ExAllocatePoolWithTag (
                PagedPool,
                sizeof( ProcessInfo ),
                m_AllocTag
                );

            if ( pItem->m_Info )
            {
                pItem->m_Info->ProcessInfo::ProcessInfo();

                status = STATUS_SUCCESS;
            }
            else
            {
                RtlDeleteElementGenericTableAvl ( &m_Tree, pItem );
            }
        }
        else
        {
            ASSERT( FALSE );
        }
    }

    FltReleasePushLock( &m_TreeAccessLock );

    return status;
}

void
ProcList::UnregisterProcess (
    HANDLE ProcessId
    )
{

    PExitProcessCbList pItemCb = NULL;

    FltAcquirePushLockExclusive( &m_CbAccessLock );

    if ( !IsListEmpty( &m_ExitProcessCbList ) )
    {
        PLIST_ENTRY Flink;

        Flink = m_ExitProcessCbList.Flink;

        while ( Flink != &m_ExitProcessCbList )
        {
            pItemCb = CONTAINING_RECORD( Flink, ExitProcessCbList, m_List );
            Flink = Flink->Flink;

            NTSTATUS status = ExAcquireRundownProtection( &pItemCb->m_Ref );
            if ( !NT_SUCCESS( status ) )
            {
                pItemCb = NULL;
                
                continue;
            }

            break;
        }
    }

    FltReleasePushLock( &m_CbAccessLock );

    if ( pItemCb )
    {
        pItemCb->m_Cb( ProcessId, pItemCb->m_Opaque );
        ExReleaseRundownProtection( &pItemCb->m_Ref );
    }

//////////////////////////////////////////////////////////////////////////

    ProcessItem item;
    item.m_ProcessId = ProcessId;
    item.m_Info = 0;

    ProcessInfo* pInfo = NULL;

    FltAcquirePushLockExclusive( &m_TreeAccessLock );

    PProcessItem pItem = ( PProcessItem) RtlLookupElementGenericTableAvl (
        &m_Tree,
        &item
        );

    if ( pItem )
    {
        pInfo = pItem->m_Info;
        RtlDeleteElementGenericTableAvl ( &m_Tree, pItem );
    }

    FltReleasePushLock( &m_TreeAccessLock );

    /// \todo move to service thread to release current thread
    if ( pInfo )
    {
        pInfo->WaitForRelease();
        pInfo->ProcessInfo::~ProcessInfo();
        FREE_POOL( pInfo );
    }
}
