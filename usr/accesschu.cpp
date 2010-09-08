#include <windows.h>
#include <assert.h>
#include <strsafe.h>
#include <fltUser.h>

#include "../inc/accessch.h"

#define Add2Ptr(P,I) ((PVOID)((PUCHAR)(P) + (I)))

//////////////////////////////////////////////////////////////////////////
#define FILE_SUPERSEDE                  0x00000000
#define FILE_OPEN                       0x00000001
#define FILE_CREATE                     0x00000002
#define FILE_OPEN_IF                    0x00000003
#define FILE_OVERWRITE                  0x00000004
#define FILE_OVERWRITE_IF               0x00000005
#define FILE_MAXIMUM_DISPOSITION        0x00000005

#define FILE_SUPERSEDED                 0x00000000
#define FILE_OPENED                     0x00000001
#define FILE_CREATED                    0x00000002
#define FILE_OVERWRITTEN                0x00000003
#define FILE_EXISTS                     0x00000004
#define FILE_DOES_NOT_EXIST             0x00000005
//////////////////////////////////////////////////////////////////////////

#define THREAD_MAXCOUNT_WAITERS     8
#define DRV_EVENT_CONTENT_SIZE      0x1000 

typedef struct _COMMUNICATIONS {
    HANDLE                  m_hPort;
    HANDLE                  m_hCompletion;
} COMMUNICATIONS, *PCOMMUNICATIONS;

#include <pshpack1.h>

typedef struct _DRVDATA {
    UCHAR                    m_Content[DRV_EVENT_CONTENT_SIZE];
} DRVDATA, *PDRVDATA;

typedef struct _DRVEVENT_OVLP {
    FILTER_MESSAGE_HEADER   m_Header;
    DRVDATA                 m_Data;
    OVERLAPPED              m_Ovlp;
} DRVEVENT_OVLP, *PDRVEVENT_OVLP;

typedef struct _REPLY_MESSAGE {
    FILTER_REPLY_HEADER        m_ReplyHeader;
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
    pFilter->m_OperationId = OP_FILE_CREATE;
    pFilter->m_OperationType = PostProcessing;
    pFilter->m_Verdict = VERDICT_ASK;
    pFilter->m_RequestTimeout = 0;
    pFilter->m_ParamsCount = 3;
    pFilter->m_WishMask = Id2Bit( PARAMETER_FILE_NAME )
        | Id2Bit( PARAMETER_VOLUME_NAME )
        | Id2Bit( PARAMETER_REQUESTOR_PROCESS_ID )
        | Id2Bit( PARAMETER_CREATE_MODE )
        | Id2Bit( PARAMETER_RESULT_INFORMATION )
        | Id2Bit( PARAMETER_DESIRED_ACCESS );

    //first param
    PPARAM_ENTRY pEntry = pFilter->m_Params;
    pEntry->m_Id = PARAMETER_DESIRED_ACCESS;
    pEntry->m_Operation = _fltop_and;
    pEntry->m_FltData.m_Size = sizeof( ACCESS_MASK );
    pEntry->m_FltData.m_Count = 1;
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
    pEntry->m_FltData.m_Count = 1;
    pEntry->m_Flags = _PARAM_ENTRY_FLAG_NEGATION;
    PULONG pFlags = (PULONG) pEntry->m_FltData.m_Data;
    *pFlags = _STREAM_FLAGS_DIRECTORY | _STREAM_FLAGS_CASHE1;

    /*
    FILE_SUPERSEDE If the file already exists, replace it with the given file. If it does not, create the given file.  
    FILE_CREATE  If the file already exists, fail the request and do not create or open the given file. If it does not, create the given file. 
    -FILE_OPEN  If the file already exists, open it instead of creating a new file. If it does not, fail the request and do not create a new file. 
    ? FILE_OPEN_IF If the file already exists, open it. If it does not, create the given file. 
    FILE_OVERWRITE If the file already exists, open it and overwrite it. If it does not, fail the request. 
    FILE_OVERWRITE_IF If the file already exists, open it and overwrite it. If it does not, create the given file. 
    */
    
    //// forth param
    //pEntry = (PPARAM_ENTRY) Add2Ptr( 
    //    pEntry,
    //    sizeof( PARAM_ENTRY ) + pEntry->m_FltData.m_Size
    //    );
    //pEntry->m_Id = PARAMETER_CREATE_MODE;
    //pEntry->m_Operation = _fltop_equ;
    //pEntry->m_Flags = _PARAM_ENTRY_FLAG_NEGATION;
    //pEntry->m_FltData.m_Count = 4;
    //pEntry->m_FltData.m_Size = sizeof( ULONG ) * pEntry->m_FltData.m_Count;
    //PULONG pMode = (PULONG) pEntry->m_FltData.m_Data;
    //*pMode = FILE_SUPERSEDE;
    //*(pMode + 1) = FILE_CREATE;
    //*(pMode + 2) = FILE_OVERWRITE;
    //*(pMode + 3) = FILE_OVERWRITE_IF;

    // third
    pEntry = (PPARAM_ENTRY) Add2Ptr( 
        pEntry,
        sizeof( PARAM_ENTRY ) + pEntry->m_FltData.m_Size
        );
    pEntry->m_Id = PARAMETER_RESULT_INFORMATION;
    pEntry->m_Operation = _fltop_equ;
    pEntry->m_FltData.m_Size = sizeof( ULONG );
    pEntry->m_FltData.m_Count = 1;
    PULONG pInformation = (PULONG) pEntry->m_FltData.m_Data;
    *pInformation = FILE_OPENED;
     
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
    pFilter->m_OperationId = OP_FILE_CLEANUP;
    pFilter->m_OperationType = PreProcessing;
    pFilter->m_Verdict = VERDICT_ASK;
    pFilter->m_RequestTimeout = 0;
    pFilter->m_ParamsCount = 2;
    pFilter->m_WishMask = Id2Bit( PARAMETER_FILE_NAME )
        | Id2Bit( PARAMETER_VOLUME_NAME )
        | Id2Bit( PARAMETER_REQUESTOR_PROCESS_ID )
        | Id2Bit( PARAMETER_OBJECT_STREAM_FLAGS );

    // first param
    PPARAM_ENTRY pEntry = pFilter->m_Params;
    pEntry->m_Id = PARAMETER_OBJECT_STREAM_FLAGS;
    pEntry->m_Operation = _fltop_and;
    pEntry->m_FltData.m_Size = sizeof( ULONG );
    pEntry->m_FltData.m_Count = 1;
    pEntry->m_Flags = _PARAM_ENTRY_FLAG_NEGATION;
    PULONG pFlags = (PULONG) pEntry->m_FltData.m_Data;
    *pFlags = _STREAM_FLAGS_DIRECTORY | _STREAM_FLAGS_CASHE1 | _STREAM_FLAGS_DELONCLOSE;

    // second param
    pEntry = (PPARAM_ENTRY) Add2Ptr( 
        pEntry,
        sizeof( PARAM_ENTRY ) + pEntry->m_FltData.m_Size
        );
    pEntry->m_Id = PARAMETER_OBJECT_STREAM_FLAGS;
    pEntry->m_Operation = _fltop_and;
    pEntry->m_FltData.m_Size = sizeof( ULONG );
    pEntry->m_FltData.m_Count = 1;
    pEntry->m_Flags = 0;
    pFlags = (PULONG) pEntry->m_FltData.m_Data;
    *pFlags = _STREAM_FLAGS_MODIFIED;

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
        sizeof( DRVEVENT_OVLP )
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

        if ( Queued )
        {
            PDRVEVENT_OVLP pEventTmp = CONTAINING_RECORD (
                pOvlp,
                DRVEVENT_OVLP,
                m_Ovlp
                );

            if ( NumbersOfByte == FIELD_OFFSET( DRVEVENT_OVLP, m_Data ) )
            {
                HeapFree( GetProcessHeap(), 0, pEventTmp );

                return E_ABORT;
            }

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
Nc_Command (
    __in PCOMMUNICATIONS CommPort,
    __in NOTIFY_ID CommandId
    )
{
    NOTIFY_COMMAND command;
    ZeroMemory( &command, sizeof( NOTIFY_COMMAND) );
    command.m_Command = CommandId;

    DWORD returned = 0;
    HRESULT hResult = FilterSendMessage (
        CommPort->m_hPort,
        &command,
        sizeof( NOTIFY_COMMAND),
        NULL,
        0,
        &returned
        );

    return hResult;
}

__checkReturn
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
        //OutputDebugString( L"create io failed\n" );

        return false;
    }
    
    //if ( scancontext.IOSize  > 1024 * 1024 * 10 ) //10 mb
    //{
    //    printf( "skip huge file %d\n", scancontext.IOSize / ( 1024 * 1024 ) );
    //}
    //else
    {
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

    __try
    {
        PEVENT_PARAMETER pParam = &Data->m_Parameters[0];
        for ( ULONG cou = 0; cou < Data->m_ParametersCount; cou++ )
        {
            if ( ParameterId == pParam->m_Id )
            {
                return pParam;
            }

            pParam = (PEVENT_PARAMETER) Add2Ptr (
                pParam,
                FIELD_OFFSET( EVENT_PARAMETER, m_Data ) + pParam->m_Size
                );
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        __debugbreak();
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

    WCHAR wchOut[MAX_PATH * 2 ];

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
            /*StringCbPrintf (
                wchOut,
                sizeof( wchOut ),
                L"-> %s> %.*s\n",
                pData->m_OperationId == OP_FILE_CREATE ?
                L"create(post)" : L"cleanup(pre)",
                pParam->m_Size / sizeof( WCHAR ),
                pParam->m_Data
                );

            OutputDebugString( wchOut );*/
        }
        PEVENT_PARAMETER pParamSFlags = GetEventParam (
            pData, PARAMETER_OBJECT_STREAM_FLAGS );

        ULONG sflags = 0;
        if ( pParamSFlags )
        {
            sflags = *(PULONG) pParamSFlags->m_Data;
        }
        
        if ( OP_FILE_CREATE == pData->m_OperationId )
        {
            PEVENT_PARAMETER pParamDesiredAccess = GetEventParam (
                pData, PARAMETER_DESIRED_ACCESS );
            
            if ( pParamDesiredAccess )
            {
                ULONG desired_access = *(PULONG) pParamDesiredAccess->m_Data;
                assert( desired_access | FILE_READ_DATA );
                if ( desired_access )
                {
                }
            }

            PEVENT_PARAMETER pParamInformation = GetEventParam (
                pData, PARAMETER_RESULT_INFORMATION );

            PEVENT_PARAMETER pParamCreateMode = GetEventParam (
                pData, PARAMETER_CREATE_MODE );

            if ( pParamInformation && pParamCreateMode )
            {
                ULONG Mode = *(PULONG) pParamCreateMode->m_Data;
                ULONG Information = *(PULONG) pParamInformation->m_Data;

                if ( FILE_OPENED == Information )
                {
                   
                }
                else
                {
                    StringCbPrintf (
                        wchOut,
                        sizeof( wchOut ),
                        L"\t\tcreated, mode 0x%x\n",
                        Mode
                        );

                    OutputDebugString( wchOut );
                    __debugbreak();
                }
            }
        }

        if ( OP_FILE_CLEANUP == pData->m_OperationId )
        {
            assert( !( sflags & _STREAM_FLAGS_DELONCLOSE ) );
            assert( sflags & _STREAM_FLAGS_MODIFIED );
        }

        bool bBlock = ScanObject( pCommPort, pData );
        
        if ( pParam && bBlock )
        {
            WCHAR wchOut[MAX_PATH * 2 ];
            StringCbPrintf (
                wchOut,
                sizeof( wchOut ),
                L"<- %s (0x%x)> %.*s - sflags 0x%x\n",
                pData->m_OperationId == OP_FILE_CREATE ?
                L"create(post)" : L"cleanup(pre)",
                pData->m_OperationId,
                pParam->m_Size / sizeof( WCHAR ),
                pParam->m_Data,
                sflags
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
            sizeof( Reply )
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

    HANDLE hPort = 0;
    HANDLE hCompletion = 0;

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
            hPort = 0;
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

        Nc_Command( &Comm, ntfcom_Activate );

        // just wait
        MessageBox( NULL, L"stop?", L"RTP prototype", NULL );
        
        Nc_Command( &Comm, ntfcom_Pause );
    }
    __finally
    {
        printf( "cleanup...\n" );
        for ( int thc = 0; thc < THREAD_MAXCOUNT_WAITERS; thc++ )
        {
            if ( hThreads[thc] )
            {
                WaitForSingleObject( hThreads[thc], INFINITE );
                CloseHandle( hThreads[thc] );
            }
        }

        if ( hCompletion )
        {
            CloseHandle( hCompletion );
        }

        if ( hPort )
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
