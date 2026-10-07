// Helpers shared by the platform layer's stubs.
#pragma once

#include <stdio.h>

#include <windows.h>

// Logs "not implemented: <function>" to stderr the first time a stub runs.
// Every stub of the platform layer starts with it, so grepping for
// PORT_UNIMPLEMENTED lists what is left to write.
#define PORT_UNIMPLEMENTED()                                                \
    do                                                                      \
    {                                                                       \
        static bool port_logged_ = false;                                   \
        if (!port_logged_)                                                  \
        {                                                                   \
            port_logged_ = true;                                            \
            fprintf(stderr, "[th16-port] not implemented: %s\n", __func__); \
        }                                                                   \
    } while (0)

// IUnknown for the stub COM objects: reference counted, deleted on the last
// Release, and no other interfaces.
template <typename Interface> struct PortComObject : public Interface
{
    uint32_t ref_count = 1;

    HRESULT QueryInterface(REFIID riid, void **ppvObject) override
    {
        if (ppvObject != NULL)
        {
            *ppvObject = NULL;
        }
        return E_NOINTERFACE;
    }
    uint32_t AddRef() override
    {
        return ++ref_count;
    }
    uint32_t Release() override
    {
        uint32_t count = --ref_count;
        if (count == 0)
        {
            delete this;
        }
        return count;
    }
};
