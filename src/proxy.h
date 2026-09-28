// proxy.h - shared between the proxy core (proxy.cpp) and the generated
// export stubs (dist\gen\<target>_stubs.cpp, produced by tools\gen_proxy.py).
//
// PROXY_TARGET_DLL names the system DLL being proxied (e.g. "dxgi.dll",
// "winmm.dll"). It is defined via /D on the build.bat command line; the
// default keeps the mod a dxgi.dll proxy.

#pragma once

#include <windows.h>

#ifndef PROXY_TARGET_DLL
#define PROXY_TARGET_DLL "dxgi.dll"
#endif

FARPROC ProxyResolve(const char* name);
FARPROC ProxyResolveOrdinal(unsigned short ordinal);
void ProxyTraceCall(const char* name);

// x64 has a single calling convention and the caller cleans the stack, so a
// 4-void*-argument trampoline forwards every export of the real DLL safely;
// a callee that takes fewer arguments (or none) simply ignores the registers.
// The stub functions use internal names (ProxyStub_<name>) and are exported
// under the real export name by the generated .def file - several system
// headers (wingdi.h, mmsystem.h, ...) declare the real prototypes, so the
// exported names cannot be defined directly in C.
#define PROXY_STUB(name)                                                                  \
    extern "C" HRESULT WINAPI ProxyStub_##name(void* a, void* b, void* c, void* d) {     \
        static FARPROC fn = ProxyResolve(#name);                                          \
        if (!fn) return 0x80004001L; /* E_NOTIMPL */                                      \
        ProxyTraceCall(#name);                                                            \
        return reinterpret_cast<HRESULT(WINAPI*)(void*, void*, void*, void*)>(fn)(a, b,  \
                                                                                   c, d); \
    }

// Ordinal-only exports have no name to resolve; they are exported under the
// same ordinal via the generated .def file and forwarded by ordinal.
#define PROXY_ORDINAL_STUB(ord)                                                           \
    extern "C" HRESULT WINAPI ProxyOrdinal_##ord(void* a, void* b, void* c, void* d) {   \
        static FARPROC fn = ProxyResolveOrdinal(ord);                                     \
        if (!fn) return 0x80004001L; /* E_NOTIMPL */                                      \
        ProxyTraceCall("ordinal " #ord);                                                  \
        return reinterpret_cast<HRESULT(WINAPI*)(void*, void*, void*, void*)>(fn)(a, b,  \
                                                                                   c, d); \
    }
