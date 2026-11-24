#ifndef FOREIGN_CALLBACK_WRITABLE_SPAN_H
#define FOREIGN_CALLBACK_WRITABLE_SPAN_H
#include <stddef.h>
typedef int (*writable_span_proc)(char*, int, void*);
typedef int (*writable_span_size_proc)(void*, size_t, void*);
int xl_test_writable_span(writable_span_proc, void*, int);
int xl_test_writable_span_size(writable_span_size_proc, void*);
#endif
