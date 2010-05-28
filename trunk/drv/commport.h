#ifndef __commport_h
#define __commport_h

// создание общего порта
NTSTATUS
PortCreate (
    __in PFLT_FILTER Filter,
    __deref_out_opt PFLT_PORT* Port
    );

// cb-функция установки соединения с юзером
NTSTATUS
PortConnect (
    __in PFLT_PORT ClientPort,
    __in_opt PVOID ServerPortCookie,
    __in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    );

// cb-функция отключение юзера от драйвера
void
PortDisconnect (
    __in PVOID ConnectionCookie
    );

// cb-функция запроса из юзера
NTSTATUS
PortMessageNotify (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    );

//+ взаимодействие с юзером

// захват порта
__checkReturn
NTSTATUS
PortQueryConnected (
    __deref_out_opt PFLT_PORT* Port
    );

// освобождение порта
void
PortRelease (
    __deref_in PFLT_PORT* Port
    );

// создание события
__checkReturn
NTSTATUS
PortAllocateMessage (
    __in EventData *Event,
    __in QueuedItem* QueuedItem,
    __deref_out_opt PVOID* Message,
    __out_opt PULONG MessageSize,
    __in PARAMS_MASK ParamsMask
    );

// освобождение события
void
PortReleaseMessage (
    __deref_in PVOID* Message
    );

// посылка сообщения в юзер
__checkReturn
NTSTATUS
PortAskUser (
    __in EventData *Event,
    __in PARAMS_MASK ParamsMask
    );

#endif // __commport_h