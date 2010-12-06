#pragma once
#include  "../../inc/filters.h"
#include  "../inc/fltevents.h"


class FilteringSystem
{
public:
    FilteringSystem();
    ~FilteringSystem();

    __checkReturn
    NTSTATUS
    Attach (
        );

    void
    Detach (
        );

    __checkReturn
    NTSTATUS
    FilterEvent (
        );
};