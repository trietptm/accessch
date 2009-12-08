#include <windows.h>
#include <assert.h>
#include <strsafe.h>
#include <fltUser.h>

#include "../inc/accessch.h"

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

DWORD
WINAPI
WaiterThread (
    __in  LPVOID lpParameter
    )
{
    PCOMMUNICATIONS pComm = (PCOMMUNICATIONS) lpParameter;
    assert( pComm );

    HRESULT hResult;
    do
    {
        PDRVEVENT_OVLP pEvent;
        hResult = WaitForSingleMessage (
            pComm->m_hPort,
            pComm->m_hCompletion,
            &pEvent
            );

        if ( IS_ERROR( hResult ) )
        {
            break;
        }

        //check data
        PMESSAGE_DATA pData = (PMESSAGE_DATA) pEvent->m_Data.m_Content;
        printf( "[0x%x:%x]\n",
            pData->m_ProcessId,
            pData->m_ThreadId
            );

        // release
        
        REPLY_MESSAGE Reply;
        memset( &Reply, 0, sizeof( Reply) );

        Reply.m_ReplyHeader.Status = 0;
        Reply.m_ReplyHeader.MessageId = pEvent->m_Header.MessageId;

        hResult = FilterReplyMessage (
            pComm->m_hPort,
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

        COMMUNICATIONS Comm;
        Comm.m_hPort = hPort;
        Comm.m_hCompletion = hCompletion;

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
        Sleep( 1000 * 30 );
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
    }
}
