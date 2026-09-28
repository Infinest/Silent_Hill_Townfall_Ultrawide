// proxy.cpp - Core of the system-DLL proxy. Every export of the real
// PROXY_TARGET_DLL is proxied through to the system copy; the mod ships as
// <target>.dll inside Townfall\Binaries\Win64 and Windows loads it instead of
// the system library.
//
// The real library is loaded by FULL SYSTEM PATH (GetSystemDirectoryW) so the
// loader can never confuse it with this proxy. Every call is verified not to
// resolve back into this module (which would recurse), and the first few calls
// are logged for diagnosis. The export stubs themselves live in a generated
// translation unit (dist\gen\<target>_stubs.cpp, see tools\gen_proxy.py).

#include <windows.h>

#include <cstring>

#include "log.h"
#include "proxy.h"

static HMODULE gReal = nullptr;

static HMODULE RealLibrary() {
    if (gReal) return gReal;
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 16) return nullptr;
    wchar_t wideName[64];
    size_t len = strlen(PROXY_TARGET_DLL);
    if (len == 0 || len >= 64) return nullptr;
    for (size_t i = 0; i <= len; i++) wideName[i] = (wchar_t)(unsigned char)PROXY_TARGET_DLL[i];
    wcscat_s(path, MAX_PATH, L"\\");
    wcscat_s(path, MAX_PATH, wideName);
    HMODULE m = LoadLibraryW(path);
    if (m) {
        // Paranoia: make sure we did not get our own proxy back.
        wchar_t loaded[MAX_PATH];
        GetModuleFileNameW(m, loaded, MAX_PATH);
        if (_wcsicmp(loaded, path) != 0) {
            LogLine("FATAL: real-%s load returned an unexpected module", PROXY_TARGET_DLL);
            FreeLibrary(m);
            return nullptr;
        }
        LogLine("proxy for %s: real library loaded from system32", PROXY_TARGET_DLL);
    } else {
        LogLine("FATAL: LoadLibraryW of system %s failed: %lu", PROXY_TARGET_DLL,
                GetLastError());
    }
    gReal = m;
    return m;
}

FARPROC ProxyResolve(const char* name) {
    HMODULE real = RealLibrary();
    if (!real) return nullptr;
    FARPROC fn = GetProcAddress(real, name);
    if (!fn) return nullptr;
    // Guard against resolving back into this proxy (would recurse forever).
    HMODULE self = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)fn, &self)) {
        HMODULE us = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&ProxyResolve, &us);
        if (self == us) {
            LogLine("FATAL: export %s resolved back into the proxy - refusing to call", name);
            return nullptr;
        }
    }
    return fn;
}

void ProxyTraceCall(const char* name) {
    static LONG count = 0;
    if (InterlockedIncrement(&count) <= 8) LogLine("stub called: %s", name);
}

FARPROC ProxyResolveOrdinal(unsigned short ordinal) {
    HMODULE real = RealLibrary();
    if (!real) return nullptr;
    FARPROC fn = GetProcAddress(real, MAKEINTRESOURCEA(ordinal));
    if (!fn) return nullptr;
    HMODULE self = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)fn, &self)) {
        HMODULE us = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&ProxyResolveOrdinal, &us);
        if (self == us) {
            LogLine("FATAL: ordinal %u resolved back into the proxy - refusing to call",
                    ordinal);
            return nullptr;
        }
    }
    return fn;
}
