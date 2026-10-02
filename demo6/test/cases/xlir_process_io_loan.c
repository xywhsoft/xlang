#if defined(XRT_PROCESS_IO_LOAN_RUNTIME)
#define XRUNTIME_IMPLEMENTATION
#define XRT_PROCESS_IO_LOAN_HEADER "lib/xruntime/xruntime.h"
#else
#define XRT_PROCESS_IO_LOAN_HEADER "lib/xrt/xrt.h"
#endif
#include "test/support/process_io_loan_probe.h"
