#ifndef XLANG_IDE_APP_XRT_H
#define XLANG_IDE_APP_XRT_H
/* The IDE uses its matched XGE-owned runtime. The standalone language server
 * is rebuilt with the compiler XRT profile and its own single implementation.
 * Never link analysis/runtime.c into the XGE-owned IDE. */
#if defined(XLS_COMPILER_XRT)
#if !defined(XRT_MODULE_ALL)
#error "The standalone language server requires its compiler XRT build profile"
#endif
#include <xrt.h>
#else
#include <xrt_config.h>
#include <xrt.h>
#endif
#endif
