#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "../../inc/accessch.h"
#include "../inc/iosupport.h"

#include "../inc/filemgr.h"
#include "filestructs.h"
#include "filehlp.h"

__checkReturn
NTSTATUS
IoSupportCommand (
    __in PIO_SUPPORT IoCommand,
    __in ULONG IoCommandBufferSize,
    __deref_out PIO_SUPPORT_RESULT IoSupportResult,
    __deref_out PULONG IoResultSize
    )
{
    if ( Add2Ptr( IoCommand->m_Name, IoCommand->m_NameLengthCb )
         >
         Add2Ptr( IoCommand, IoCommandBufferSize )
         )
    {
        return STATUS_INVALID_PARAMETER_1;
    }

    RtlZeroMemory( IoSupportResult, sizeof( IO_SUPPORT_RESULT ) );
    *IoResultSize = sizeof( IO_SUPPORT_RESULT );

    UNICODE_STRING us;

    RtlInitEmptyUnicodeString (
        &us,
        IoCommand->m_Name,
        (USHORT) IoCommand->m_NameLengthCb
        );

    us.Length = us.MaximumLength;

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    HANDLE hFile = NULL;
    PFILE_OBJECT pFo = NULL;
    
    OBJECT_ATTRIBUTES objectAttributes;
    InitializeObjectAttributes( &objectAttributes,
        &us,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    IO_STATUS_BLOCK iosb;

    status = FltCreateFileEx2 (
        gFileMgr.m_FileFilter,
        NULL,
        &hFile,
        &pFo,
        SYNCHRONIZE | FILE_ANY_ACCESS,
        &objectAttributes,
        &iosb,
        NULL,
        0,
        FILE_SHARE_DELETE | FILE_SHARE_WRITE | FILE_SHARE_READ,
        FILE_OPEN,
        FILE_RANDOM_ACCESS | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REQUIRING_OPLOCK,
        NULL,
        0,
        IO_IGNORE_SHARE_ACCESS_CHECK,
        NULL
        );

    if ( !NT_SUCCESS( status ) )
    {
        return status;
    }

    status = QueryFileId( pFo, &IoSupportResult->m_FileId );
    if ( NT_SUCCESS( status ) )
    {
        SetFlag( IoSupportResult->m_FlagsReflect, _iosup_fileid );
    }
            
    ObDereferenceObject( pFo );
    FltClose( hFile );

    return STATUS_SUCCESS;
}

void
IoSupportCleanup (
    __in PIO_SUPPORT_RESULT IoSupportResult
    )
{
    /// \todo IoSupportCleanup
    UNREFERENCED_PARAMETER( IoSupportResult );
}
