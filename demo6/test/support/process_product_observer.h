/* Declarations for the ordinary full DLL/SO, not a runtime implementation.
 * Only the observation layout is staged from that exact product snapshot.
 * The host keeps products loaded; a private waiter join is not a public wait
 * or module-finalization guarantee. Source-only mapper/plan units stay separate. */
#ifndef XL6_TEST_PROCESS_PRODUCT_OBSERVER_H
#define XL6_TEST_PROCESS_PRODUCT_OBSERVER_H
#if !defined(XL6_PROCESS_PRODUCT_OBSERVER)
#error "Product observer must be selected explicitly"
#endif
#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
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
