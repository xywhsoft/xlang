/* Declarations for the full native ABI, never a runtime implementation.
 * Select either a genuine shared product or a separately linked source unit.
 * The observation layout is extracted from the identical full snapshot.
 * A test-owned waiter join is not a public wait/finalization guarantee. */
#ifndef XL6_TEST_PROCESS_PRODUCT_OBSERVER_H
#define XL6_TEST_PROCESS_PRODUCT_OBSERVER_H
#if defined(XL6_PROCESS_PRODUCT_OBSERVER) == defined(XL6_PROCESS_SOURCE_CONTRACTS)
#error "Select exactly one explicit native observation mode"
#endif
#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/types.h>
#endif
#include "process_observer_layout.h"
#include "src/runtime/xl6_process_live.h"
#include "src/runtime/xl6_io_handle.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_fs_file.h"
#endif
