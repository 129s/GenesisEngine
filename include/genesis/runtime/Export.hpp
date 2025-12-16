#pragma once

// Explicit DLL export/import surface for genesis_runtime.
// This replaces the temporary WINDOWS_EXPORT_ALL_SYMBOLS strategy on MSVC.

#if defined(_WIN32)
    #if defined(GENESIS_RUNTIME_EXPORTS)
        #define GENESIS_RUNTIME_API __declspec(dllexport)
    #else
        #define GENESIS_RUNTIME_API __declspec(dllimport)
    #endif
#else
    #if defined(__GNUC__) || defined(__clang__)
        #define GENESIS_RUNTIME_API __attribute__((visibility("default")))
    #else
        #define GENESIS_RUNTIME_API
    #endif
#endif

