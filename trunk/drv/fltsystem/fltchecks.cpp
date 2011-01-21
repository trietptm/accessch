#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "fltbox.h"
#include "fltchecks.h"

#include "fltchecks.tmh"

ULONG ParamCheckEntry::m_AllocTag = 'ecSA';

ParamCheckEntry::ParamCheckEntry (
    )
{
    m_Flags = 0;
    m_PosCount = 0;
    m_FilterPosList = NULL;
    m_Type = CheckEntryInvalid;
}

ParamCheckEntry::~ParamCheckEntry (
    )
{
    FREE_POOL( m_FilterPosList );

    switch ( m_Type )
    {
    case CheckEntryInvalid:
        break;

    case CheckEntryGeneric:
        FREE_POOL( Generic.m_CheckData );
        break;

    case CheckEntryBox:
        Container.m_Box->Release();
        FREE_POOL( Container.m_Affecting );
        break;

    default:
        {
            __debugbreak();
        }
    }
}

__checkReturn
NTSTATUS
ParamCheckEntry::Attach (
    __in ULONG DataSize,
    __in ULONG Count,
    __in PUCHAR Data
    )
{
     Generic.m_CheckData = (PFltCheckData) ExAllocatePoolWithTag(
        PagedPool,
        sizeof( FltCheckData ) + DataSize,
        m_AllocTag
        );

    if ( !Generic.m_CheckData )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Generic.m_CheckData->m_DataSize = DataSize;
    Generic.m_CheckData->m_Count = Count;
    
    RtlCopyMemory(
        Generic.m_CheckData->m_Data,
        Data,
        DataSize
        );
        
    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
CheckMask (
    PWCHAR PatternStart,
    PWCHAR PatternEnd,
    PWCHAR StringStart,
    PWCHAR StringEnd
    )
{
    PWCHAR asterisk_ptr = NULL;

    while ( PatternStart <= PatternEnd && StringStart <= StringEnd )
    {
        if ( '*' == *PatternStart )
        {
            long ask_cnt = 0;
            asterisk_ptr = PatternStart;

            while (
                PatternStart <= PatternEnd
                &&
                ( *PatternStart == '?' || *PatternStart == '*' )
                )
            {
                if ( *PatternStart++ == '?' )
                {
                    ask_cnt++;
                }
            }

            if ( PatternStart > PatternEnd )
            {
                if ( !ask_cnt ) 
                {
                    return STATUS_SUCCESS;
                }

                if ( ( StringEnd - StringStart ) < ask_cnt )
                {
                    return STATUS_UNSUCCESSFUL;
                }

                return STATUS_SUCCESS;
            }
            else
            {
                StringStart += ask_cnt;

                while ( StringStart <= StringEnd && *StringStart != *PatternStart )
                {
                    StringStart++;
                }

                if ( StringStart > StringEnd )
                {
                    return STATUS_UNSUCCESSFUL;
                }
            }
        }
        else
        {
            if ( '?' != *PatternStart && *StringStart != *PatternStart )
            {
                if ( !asterisk_ptr )
                {
                    return STATUS_UNSUCCESSFUL;
                }

                PatternStart = asterisk_ptr;

                continue;
            }
        }

        StringStart++;
        PatternStart++;
    }

    if ( PatternStart > PatternEnd && StringStart <= StringEnd )
    {
        return STATUS_UNSUCCESSFUL;
    }

    if ( PatternStart <= PatternEnd && StringStart > StringEnd )
    {
        while ( PatternStart <= PatternEnd && *PatternStart == '*' )
        {
           PatternStart++;
        }

        if ( PatternStart <= PatternEnd )
        {
            return STATUS_UNSUCCESSFUL;
        }
    }

    return STATUS_SUCCESS;
}

__checkReturn
__drv_valueIs( STATUS_SUCCESS; STATUS_UNSUCCESSFUL; STATUS_NOT_FOUND )
NTSTATUS
CheckGeneric (
    __in ParamCheckEntry* Entry,
    __in EventData *Event
    )
{
    PVOID pData;
    ULONG datasize;
    NTSTATUS status = Event->QueryParameter(
        Entry->Generic.m_Parameter,
        &pData,
        &datasize
        );

    if ( !NT_SUCCESS( status ) )
    {
        if ( FlagOn( Entry->m_Flags, FltFlags_BePresent ) )
        {
            return STATUS_NOT_FOUND;
        }
        
        return STATUS_SUCCESS;
    }
    
    status = STATUS_UNSUCCESSFUL;

    FltCheckData* pCheck = Entry->Generic.m_CheckData;

    PVOID ptr = pCheck->m_Data;

    ULONG item;
    switch( Entry->Generic.m_Operation )
    {
    case FltOp_equ:
        if ( datasize
            != 
            pCheck->m_DataSize / pCheck->m_Count )
        {
            __debugbreak();
            break;
        }
        
        for ( item = 0; item < pCheck->m_Count; item++ )
        {
            if ( datasize == RtlCompareMemory(
                ptr,
                pData,
                datasize
                ) )
            {
                status = STATUS_SUCCESS;    
                break;
            }

            ptr = Add2Ptr( ptr, datasize );
        }

        break;

    case FltOp_and:
        ASSERT( pCheck->m_Count == 1 );

        if ( datasize
            !=
            pCheck->m_DataSize / pCheck->m_Count )
        {
            __debugbreak();
            break;
        }

        if ( datasize == sizeof( ULONG ) )
        {
            PULONG pu1 = (PULONG) ptr;
            PULONG pu2 = (PULONG) pData;
            if ( *pu1 & *pu2 )
            {
                status = STATUS_SUCCESS;
                break;
            }
        }
        else
        {
            __debugbreak(); //not impelemented
        }

        break;

    case FltOp_pattern:
        {
            /// \todo upper cased string have to be acquired as QueryParameter with UPPER_CASE flag

            UNICODE_STRING ussrc;
            RtlInitEmptyUnicodeString( &ussrc, ( PWCHAR ) pData, (USHORT) datasize );
            ussrc.Length = ussrc.MaximumLength;

            UNICODE_STRING usdest;
            RtlInitEmptyUnicodeString( &usdest, NULL, 0 );

            status = RtlUpcaseUnicodeString( &usdest, &ussrc, TRUE );

            if ( NT_SUCCESS( status ) )
            {
                status = CheckMask(
                    ( PWCHAR ) pCheck->m_Data,
                    ( PWCHAR ) Add2Ptr( pCheck->m_Data, pCheck->m_DataSize - sizeof( WCHAR ) ),
                    ( PWCHAR ) usdest.Buffer,
                    ( PWCHAR ) Add2Ptr( usdest.Buffer, datasize - sizeof( WCHAR ) )
                    );

                RtlFreeUnicodeString( &usdest );
            }
        }
        break;
    
    default:
        break;
    }

    return status;
}

__checkReturn
__drv_valueIs( STATUS_SUCCESS; STATUS_UNSUCCESSFUL; STATUS_NOT_FOUND )
NTSTATUS
CheckContainer (
    __in ParamCheckEntry* Entry,
    __in EventData *Event
    )
{
    NTSTATUS status = Entry->Container.m_Box->MatchEvent(
        Event,
        Entry->Container.m_Affecting
        );

    return status;
}

__checkReturn
NTSTATUS
CheckEntry (
    __in ParamCheckEntry* Entry,
    __in EventData *Event
    )
{
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;

    switch ( Entry->m_Type )
    {
    case CheckEntryGeneric:
        status = CheckGeneric( Entry, Event );
        break;

    case CheckEntryBox:
        status = CheckContainer( Entry, Event );
        break;

    default:
        __debugbreak();
        return STATUS_NOT_IMPLEMENTED;
    }

    switch ( status )
    {
    case STATUS_SUCCESS:
    case STATUS_UNSUCCESSFUL:
        if ( FlagOn( Entry->m_Flags, FltFlags_Negation ) )
        {
            if ( NT_SUCCESS( status ) )
            {
                status = STATUS_UNSUCCESSFUL;
            }
            else
            {
                status = STATUS_SUCCESS;
            }
        }
        break;

    default:
        break;
    }

    return status;
}
