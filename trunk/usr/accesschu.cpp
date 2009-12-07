#include <windows.h>
#include <assert.h>
#include <strsafe.h>
#include <fltUser.h>

#define ACCESSCH_PORT_NAME          L"\\AccessCheckPort"
#define THREAD_MAXCOUNT_WAITERS     8
#define DRV_EVENT_CONTENT_SIZE      0x1000 

#include <pshpack1.h>
typedef struct _DRVEVENT {
    UCHAR			m_Content[DRV_EVENT_CONTENT_SIZE];
} DRVEVENT, *PDRVEVENT;

typedef struct _DRVEVENT_OVLP {
    FILTER_MESSAGE_HEADER	m_Header;
    DRVEVENT				m_Event;
    OVERLAPPED				m_Ovlp;
} DRVEVENT_OVLP, *PDRVEVENT_OVLP;
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
    HANDLE hPort = const_cast<HANDLE> (lpParameter);
    assert( hPort );

    HRESULT hResult;
    do
    {
        PDRVEVENT_OVLP pEvent;
        hResult = WaitForSingleMessage( hPort, 0, &pEvent );
        if ( IS_ERROR( hResult ) )
        {
            break;
        }
        
        //todo: reply
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
            __leave;
        }

        for ( int thc = 0; thc < THREAD_MAXCOUNT_WAITERS; thc++ )
        {
            hThreads[thc] = CreateThread ( NULL, 0, WaiterThread, hPort, 0, &ThreadsId[thc] );

            if ( !hThreads[thc] )
            {
                __leave;
            }

        }
    }
    __finally
    {
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
