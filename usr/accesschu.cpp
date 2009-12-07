#include <windows.h>
#include <assert.h>
#include <strsafe.h>
#include <fltUser.h>

#define ACCESSCH_PORT_NAME          L"\\AccessCheckPort"
#define THREAD_MAXCOUNT_WAITERS     8

HRESULT
WaitForSingleMessage (
    __in HANDLE hCompletion
    )
{
    UNREFERENCED_PARAMETER( hCompletion );

    return S_OK;
}

DWORD
WINAPI
WaiterThread (
    __in  LPVOID lpParameter
    )
{
    HANDLE hCompletion = const_cast<HANDLE> (lpParameter);
    assert( hCompletion );

    HRESULT hResult;
    do
    {
        hResult = WaitForSingleMessage( hCompletion );
        if ( IS_ERROR( hResult ) )
        {
            break;
        }
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
            hThreads[thc] = CreateThread ( NULL, 0, WaiterThread, hCompletion, 0, &ThreadsId[thc] );

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
