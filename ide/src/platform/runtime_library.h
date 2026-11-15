#ifndef XIDE_RUNTIME_LIBRARY_H
#define XIDE_RUNTIME_LIBRARY_H

/* The IDE owns this matched SDK runtime. The engine's xge.dll is independent. */
#define XIDE_RUNTIME_DLL_BASENAME "xlang_ide_xge.dll"
#define XIDE_RUNTIME_WIDEN_IMPL(value) L##value
#define XIDE_RUNTIME_WIDEN(value) XIDE_RUNTIME_WIDEN_IMPL(value)
#define XIDE_RUNTIME_DLL_BASENAME_W XIDE_RUNTIME_WIDEN(XIDE_RUNTIME_DLL_BASENAME)

#endif
