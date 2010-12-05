#pragma once

class FilteringSystem
{
public:
    FilteringSystem();
    ~FilteringSystem();

    NTSTATUS
    Attach (
        );

    void
    Detach (
        );

    FilterEvent (
        );
};