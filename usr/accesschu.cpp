#include <windows.h>
#include <assert.h>
#include <strsafe.h>
#include <fltUser.h>

#include "../inc/accessch.h"

#define Add2Ptr(P,I) ((PVOID)((PUCHAR)(P) + (I)))

#define THREAD_MAXCOUNT_WAITERS     8
#define DRV_EVENT_CONTENT_SIZE      0x1000 

typedef struct _COMMUNICATIONS {
    HANDLE                  m_hPort;
    HANDLE                  m_hCompletion;
} COMMUNICATIONS, *PCOMMUNICATIONS;

#include <pshpack1.h>

typedef struct _DRVDATA {
    UCHAR			        m_Content[DRV_EVENT_CONTENT_SIZE];
} DRVDATA, *PDRVDATA;

typedef struct _DRVEVENT_OVLP {
    FILTER_MESSAGE_HEADER	m_Header;
    DRVDATA			    	m_Data;
    OVERLAPPED				m_Ovlp;
} DRVEVENT_OVLP, *PDRVEVENT_OVLP;

typedef struct _REPLY_MESSAGE {
    FILTER_REPLY_HEADER		m_ReplyHeader;
    REPLY_RESULT            m_Verdict;
} REPLY_MESSAGE, *PREPLY_MESSAGE;

#include <poppack.h>

//////////////////////////////////////////////////////////////////////////
typedef HRESULT (__cdecl * pfn_io_read) (
    HANDLE Context,
    LARGE_INTEGER Offset,
    PVOID Buffer,
    ULONG Size,
    PULONG Read
    );

typedef HRESULT (__cdecl * pfn_io_getsize) (
    HANDLE Context,
    PLARGE_INTEGER Size
    );

typedef HRESULT (__cdecl * pfn_InitEngineProvider) (
    PHANDLE Session,
    pfn_io_read IORead,
    pfn_io_getsize IOGetSize
    );

typedef HRESULT (__cdecl * pfn_DoneEngineProvider ) (
    HANDLE Handle
    );

typedef HRESULT (__cdecl * pfn_ScanIO ) (
    HANDLE Session,
    HANDLE ObjectToScan
    );

HANDLE gEngine = NULL;
pfn_InitEngineProvider InitEngineProvider = NULL;
pfn_DoneEngineProvider DoneEngineProvider = NULL;
pfn_ScanIO ScanIO = NULL;

typedef struct _IOScanContext
{
    PCOMMUNICATIONS CommPort;
    PMESSAGE_DATA   Data;

    PVOID           MemBasePtr;
    SIZE_T          IOSize;
} IOScanContext, *PIOScanContext;

HRESULT
__cdecl
CustomRead (
    HANDLE Context,
    LARGE_INTEGER Offset,
    PVOID Buffer,
    ULONG Size,
    PULONG Read
    )
{
    UNREFERENCED_PARAMETER( Context );
    UNREFERENCED_PARAMETER( Offset );
    UNREFERENCED_PARAMETER( Buffer );
    UNREFERENCED_PARAMETER( Size );
    UNREFERENCED_PARAMETER( Read );

    PIOScanContext pScanContext = (PIOScanContext) Context;
    
    HRESULT hResult = E_FAIL;
    __try
    {
        if ( Offset.QuadPart > pScanContext->IOSize )
        {
            __leave;
        }

        if ( Offset.QuadPart + Size > pScanContext->IOSize )
        {
            Size = (ULONG) (pScanContext->IOSize - Offset.QuadPart);
        }
        
        __try
        {
            RtlCopyMemory (
                Buffer,
                Add2Ptr( pScanContext->MemBasePtr, Offset.QuadPart ),
                Size
                );

            *Read = Size;
            hResult = S_OK;
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            hResult = E_FAIL;
            OutputDebugString( L"read exception\n" );
        }
    }
    __finally
    {
    	
    }
    
    return hResult;
}

HRESULT
__cdecl
CustomGetSize (
    HANDLE Context,
    PLARGE_INTEGER Size
    )
{
    PIOScanContext pScanContext = (PIOScanContext) Context;
    Size->QuadPart = pScanContext->IOSize;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////

HRESULT
CreateFilter_PostCreate (
    __in PCOMMUNICATIONS CommPort
    )
{
    assert( CommPort );

    HRESULT hResult = E_FAIL;

    // create filters manually
    char buffer[0x1000];

    memset( buffer, 0, sizeof( buffer) );

    PNOTIFY_COMMAND pCommand = (PNOTIFY_COMMAND) buffer;
    pCommand->m_Command = ntfcom_FiltersChain;

    PFILTERS_CHAIN pChain = (PFILTERS_CHAIN) pCommand->m_Data;

    pChain->m_Count = 1;
    pChain->m_Entry[0].m_Operation = _fltchain_add;
    
    PFILTER pFilter = pChain->m_Entry[0].m_Filter;
    pFilter->m_Interceptor = FILE_MINIFILTER;
    pFilter->m_FunctionMj = OP_FILE_CREATE;
    pFilter->m_OperationType = PostProcessing;
    pFilter->m_Verdict = VERDICT_ASK;
    pFilter->m_RequestTimeout = 0;
    pFilter->m_ParamsCount = 2;
    pFilter->m_WishMask = Id2Bit( PARAMETER_FILE_NAME )
        | Id2Bit( PARAMETER_VOLUME_NAME )
        | Id2Bit( PARAMETER_REQUESTOR_PROCESS_ID );

    //first param
    PPARAM_ENTRY pEntry = pFilter->m_Params;
    pEntry->m_Id = PARAMETER_DESIRED_ACCESS;
    pEntry->m_Operation = _fltop_and;
    pEntry->m_FltData.m_Size = sizeof( ACCESS_MASK );
    ACCESS_MASK *pMask = (ACCESS_MASK*) pEntry->m_FltData.m_Data;
    *pMask = FILE_READ_DATA | FILE_EXECUTE;

    // second param
    pEntry = (PPARAM_ENTRY) Add2Ptr( 
        pEntry,
        sizeof( PARAM_ENTRY ) + pEntry->m_FltData.m_Size
        );
    pEntry->m_Id = PARAMETER_OBJECT_STREAM_FLAGS;
    pEntry->m_Operation = _fltop_and;
    pEntry->m_FltData.m_Size = sizeof( ULONG );
    pEntry->m_Flags = _PARAM_ENTRY_FLAG_NEGATION;
    PULONG pFlags = (PULONG) pEntry->m_FltData.m_Data;
    *pFlags = _STREAM_FLAGS_DIRECTORY | _STREAM_FLAGS_CASHE1;

    // result size
    ULONG requestsize = (ULONG) ((char*)pFlags - buffer) + sizeof( ULONG );

    DWORD retsize;
    hResult = FilterSendMessage (
        CommPort->m_hPort,
        pCommand,
        requestsize,
        NULL,
        0,
        &retsize
        );

    return hResult;
}

HRESULT
    CreateFilter_PreCleanup (
    __in PCOMMUNICATIONS CommPort
    )
{
    assert( CommPort );

    HRESULT hResult = E_FAIL;

    // create filters manually
    char buffer[0x1000];

    memset( buffer, 0, sizeof( buffer) );

    PNOTIFY_COMMAND pCommand = (PNOTIFY_COMMAND) buffer;
    pCommand->m_Command = ntfcom_FiltersChain;

    PFILTERS_CHAIN pChain = (PFILTERS_CHAIN) pCommand->m_Data;

    pChain->m_Count = 1;
    pChain->m_Entry[0].m_Operation = _fltchain_add;

    PFILTER pFilter = pChain->m_Entry[0].m_Filter;
    pFilter->m_Interceptor = FILE_MINIFILTER;
    pFilter->m_FunctionMj = OP_FILE_CLEANUP;
    pFilter->m_OperationType = PreProcessing;
    pFilter->m_Verdict = VERDICT_ASK;
    pFilter->m_RequestTimeout = 0;
    pFilter->m_ParamsCount = 1;
    pFilter->m_WishMask = Id2Bit( PARAMETER_FILE_NAME )
        | Id2Bit( PARAMETER_VOLUME_NAME )
        | Id2Bit( PARAMETER_REQUESTOR_PROCESS_ID );

    // first param
    PPARAM_ENTRY pEntry = pFilter->m_Params;
    pEntry->m_Id = PARAMETER_OBJECT_STREAM_FLAGS;
    pEntry->m_Operation = _fltop_and;
    pEntry->m_FltData.m_Size = sizeof( ULONG );
    pEntry->m_Flags = _PARAM_ENTRY_FLAG_NEGATION;
    PULONG pFlags = (PULONG) pEntry->m_FltData.m_Data;
    *pFlags = _STREAM_FLAGS_DIRECTORY | _STREAM_FLAGS_CASHE1;

    // result size
    ULONG requestsize = (ULONG) ((char*)pFlags - buffer) + sizeof( ULONG );

    DWORD retsize;
    hResult = FilterSendMessage (
        CommPort->m_hPort,
        pCommand,
        requestsize,
        NULL,
        0,
        &retsize
        );

    return hResult;
}

HRESULT
CreateFilters (
    __in PCOMMUNICATIONS CommPort
    )
{
    HRESULT hResult = CreateFilter_PostCreate( CommPort );
    if ( SUCCEEDED( hResult ) )
    {
        hResult = CreateFilter_PreCleanup( CommPort );
    }
    
    return hResult;
}

HRESULT
WaitForSingleMessage (
    __in HANDLE hPort,
    __in HANDLE hCompletion,
    __deref_out_opt PDRVEVENT_OVLP *ppEvent
    )
{
    assert( hPort );
    assert( hCompletion );
    assert( ppEvent );

    *ppEvent = 0;

    PDRVEVENT_OVLP pEvent = (PDRVEVENT_OVLP) HeapAlloc (
        GetProcessHeap(),
        0,
        sizeof(DRVEVENT_OVLP)
        );

    if ( !pEvent )
    {
        return E_OUTOFMEMORY;
    }

    memset( &pEvent->m_Ovlp, 0, sizeof( OVERLAPPED ) );

    HRESULT hResult = FilterGetMessage (
        hPort,
        &pEvent->m_Header,
        FIELD_OFFSET( DRVEVENT_OVLP, m_Ovlp ),
        &pEvent->m_Ovlp
        );

    assert( !SUCCEEDED( hResult ) );
   
    if ( HRESULT_FROM_WIN32( ERROR_IO_PENDING ) == hResult )
    {
        ULONG_PTR key = 0;
        LPOVERLAPPED pOvlp = NULL;
        DWORD NumbersOfByte = 0;

        BOOL Queued = GetQueuedCompletionStatus (
            hCompletion,
            &NumbersOfByte,
            &key,
            &pOvlp,
            INFINITE
            );

        PDRVEVENT_OVLP pEventTmp = CONTAINING_RECORD( pOvlp, DRVEVENT_OVLP, m_Ovlp );

        if ( Queued )
        {
            *ppEvent = pEventTmp;
            return S_OK;
        }
        else
        {
            assert( pOvlp );
            hResult = E_FAIL;
        }
    }

    HeapFree( GetProcessHeap(), 0, pEvent );

    return hResult;
}

HRESULT
Nc_ExecuteObjectRequest (
    __in PCOMMUNICATIONS CommPort,
    __in PNOTIFY_COMMAND Command,
    __in NOTIFY_ID CommandId,
    __in ULONG EventId,
    __out PVOID OutBuffer,
    __inout PULONG Size
    )
{
    ZeroMemory( Command, sizeof( NOTIFY_COMMAND) );
    Command->m_Command = CommandId;
    Command->m_EventId = EventId;

   HRESULT hResult = FilterSendMessage (
       CommPort->m_hPort,
       Command,
       sizeof( NOTIFY_COMMAND),
       OutBuffer,
       *Size,
       Size
       );

   return hResult;
}

HRESULT
PrepareIo (
    __in PCOMMUNICATIONS CommPort,
    __in PMESSAGE_DATA pData,
    __deref_out_opt PVOID* MemPtr,
    __out SIZE_T* IoSize
    )
{
    HRESULT hResult;

    NOTIFY_COMMAND command;
    NC_IOPREPARE prepare;
    ULONG size = sizeof( NC_IOPREPARE );
    hResult = Nc_ExecuteObjectRequest (
        CommPort,
        &command,
        ntfcom_PrepareIO,
        pData->m_EventId,
        &prepare,
        &size
        );

    if ( !SUCCEEDED( hResult ) )
    {
        return hResult;
    }

    *MemPtr = MapViewOfFile (
        prepare.m_Section,
        FILE_MAP_READ,
        0,
        0,
        (SIZE_T) prepare.m_IoSize.QuadPart
        );

    if ( *MemPtr )
    {
        CloseHandle( prepare.m_Section );
        *IoSize = (SIZE_T) prepare.m_IoSize.QuadPart;
        
        return S_OK;
    }

    /// \todo: prepare real error code

    return E_FAIL;
};

bool
ScanObject (
    __in PCOMMUNICATIONS CommPort,
    __in PMESSAGE_DATA Data
    )
{
    bool bBlock = FALSE;
    assert( Data );

    IOScanContext scancontext;
    scancontext.CommPort = CommPort;
    scancontext.Data = Data;

    HRESULT hResult = PrepareIo (
        CommPort,
        Data,
        &scancontext.MemBasePtr,
        &scancontext.IOSize
        );

    if ( !SUCCEEDED( hResult ) )
    {
        return false;
    }
    
    __try
    {
        if ( gEngine )
        {
            HRESULT hResult = ScanIO( gEngine, &scancontext );
            if ( FAILED( hResult ) )
            {
                printf( "detect\n" );
                bBlock = true;
            }
        }
        else
        {
            for (SIZE_T cou = 0; cou < scancontext.IOSize % 0x1000; cou++ )
            {
                char buf = ((char*) scancontext.MemBasePtr + cou * 0x1000 )[0];
                if ( buf )
                {
                    // simulate
                }
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {

    }

    UnmapViewOfFile( scancontext.MemBasePtr ); 

    return bBlock;
}

__checkReturn
PEVENT_PARAMETER
GetEventParam (
    __in PMESSAGE_DATA Data,
    __in Parameters ParameterId
    )
{
    assert( Data );

    PEVENT_PARAMETER pParam = &Data->m_Parameters[0];
    for ( ULONG cou = 0; cou < Data->m_ParametersCount; cou++ )
    {
        if ( ParameterId == pParam->m_Id )
        {
            return pParam;
        }

        pParam = (PEVENT_PARAMETER) Add2Ptr (
            pParam,
            pParam->m_Size + sizeof( EVENT_PARAMETER)
            );
    }
    return NULL;
}
DWORD
WINAPI
WaiterThread (
    __in  LPVOID lpParameter
    )
{
    PCOMMUNICATIONS pCommPort = (PCOMMUNICATIONS) lpParameter;
    assert( pCommPort );

    HRESULT hResult;
    do
    {
        PDRVEVENT_OVLP pEvent;
        hResult = WaitForSingleMessage (
            pCommPort->m_hPort,
            pCommPort->m_hCompletion,
            &pEvent
            );

        if ( IS_ERROR( hResult ) )
        {
            break;
        }

        //check data
        PMESSAGE_DATA pData = (PMESSAGE_DATA) pEvent->m_Data.m_Content;
        PEVENT_PARAMETER pParam = GetEventParam( pData, PARAMETER_FILE_NAME );

        if ( pParam )
        {
            WCHAR wchOut[MAX_PATH * 2 ];
            StringCbPrintf (
                wchOut,
                sizeof(wchOut),
                L"-> %s> %.*s\n",
                pData->m_OperationType == OP_FILE_CREATE ?
                L"create(post)" : L"cleanup(pre)",
                pParam->m_Size / sizeof( WCHAR ),
                pParam->m_Data
                );

            OutputDebugString( wchOut );
        }

        bool bBlock = ScanObject( pCommPort, pData );
        
        if ( pParam )
        {
            WCHAR wchOut[MAX_PATH * 2 ];
            StringCbPrintf (
                wchOut,
                sizeof(wchOut),
                L"<- %s> %.*s\n",
                pData->m_OperationType == OP_FILE_CREATE ?
                L"create(post)" : L"cleanup(pre)",
                pParam->m_Size / sizeof( WCHAR ),
                pParam->m_Data
                );
            
            OutputDebugString( wchOut );
        }

        // release
        
        REPLY_MESSAGE Reply;
        memset( &Reply, 0, sizeof( Reply) );

        Reply.m_Verdict.m_Flags = VERDICT_CACHE1;
        if ( bBlock )
        {
            Reply.m_Verdict.m_Flags |= VERDICT_DENY;
        }

        Reply.m_ReplyHeader.Status = 0;
        Reply.m_ReplyHeader.MessageId = pEvent->m_Header.MessageId;

        hResult = FilterReplyMessage (
            pCommPort->m_hPort,
            (PFILTER_REPLY_HEADER) &Reply,
            sizeof(Reply)
            );

        HeapFree( GetProcessHeap(), 0, pEvent );

    } while ( SUCCEEDED( hResult ) );

    return 0;
}

void
__cdecl
main (
    int Argc,
    __in_ecount( Argc ) char** Argv
	)
{
    UNREFERENCED_PARAMETER( Argc );
    UNREFERENCED_PARAMETER( Argv );

    HANDLE hPort = INVALID_HANDLE_VALUE;
    HANDLE hCompletion = INVALID_HANDLE_VALUE;

    HANDLE hThreads[ THREAD_MAXCOUNT_WAITERS ] = { NULL };
    DWORD ThreadsId[ THREAD_MAXCOUNT_WAITERS ] = { 0 };

    HMODULE hEngine = NULL;

    __try
    {
        HRESULT hResult = FilterConnectCommunicationPort (
            ACCESSCH_PORT_NAME,
            0,
            NULL,
            0,
            NULL,
            &hPort
            );

        if ( IS_ERROR( hResult ) )
        {
            printf( "Connect failed. Error 0x%x\n", hResult );
            hPort = INVALID_HANDLE_VALUE;
            __leave;
        }

        hCompletion = CreateIoCompletionPort (
            hPort,
            NULL,
            0,
            THREAD_MAXCOUNT_WAITERS
            );

        if ( !hCompletion )
        {
            printf( "Create completion failed. Error 0x%x\n", GetLastError() );
            __leave;
        }

        hEngine = LoadLibrary( L"scanengine.dll" );
        if ( hEngine )
        {
            InitEngineProvider = (pfn_InitEngineProvider) GetProcAddress (
                hEngine,
                "InitEngineProvider"
                );

            DoneEngineProvider = (pfn_DoneEngineProvider) GetProcAddress (
                hEngine,
                "DoneEngineProvider"
                );

            ScanIO = (pfn_ScanIO) GetProcAddress (
                hEngine,
                "ScanIO"
                );
            
            if ( InitEngineProvider && DoneEngineProvider && ScanIO )
            {
                printf( "Engine loading...\n", GetLastError() );
                hResult = InitEngineProvider (
                    &gEngine,
                    CustomRead,
                    CustomGetSize
                    );
                
                if ( SUCCEEDED( hResult ) )
                {
                    printf( "Engine loaded\n" );
                }
                else
                {
                    gEngine = NULL;
                }
            }
        }
  
        COMMUNICATIONS Comm;
        Comm.m_hPort = hPort;
        Comm.m_hCompletion = hCompletion;

        hResult = CreateFilters( &Comm );
        if ( IS_ERROR( hResult ) )
        {
            printf( "Add filters failed. Error 0x%x\n", hResult );
            __leave;
        }

        for ( int thc = 0; thc < THREAD_MAXCOUNT_WAITERS; thc++ )
        {
            hThreads[thc] = CreateThread ( NULL, 0, WaiterThread, &Comm, 0, &ThreadsId[thc] );

            if ( !hThreads[thc] )
            {
                printf( "Create thread failed. Error 0x%x\n", GetLastError() );
                __leave;
            }
        }

        // just wait
        MessageBox( NULL, L"stop?", L"RTP prototype", NULL );
    }
    __finally
    {
        printf( "cleanup...\n" );
        for ( int thc = 0; thc < THREAD_MAXCOUNT_WAITERS; thc++ )
        {
            if ( hThreads[thc] )
            {
                if (TerminateThread( hThreads[thc], 0 ) )
                {
                    CloseHandle( hThreads[thc] );
                }
            }
        }

        if ( INVALID_HANDLE_VALUE != hCompletion )
        {
            CloseHandle( hCompletion );
        }

        if ( INVALID_HANDLE_VALUE != hPort )
        {
            CloseHandle( hPort );
        }
        
        if ( gEngine )
        {
            DoneEngineProvider( gEngine );
        }

        if ( hEngine )
        {
            FreeLibrary( hEngine );
        }
    }
}
