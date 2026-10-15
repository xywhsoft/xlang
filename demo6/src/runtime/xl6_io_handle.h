#ifndef XL6_IO_HANDLE_H
#define XL6_IO_HANDLE_H
/* Closed private factory provenance. Unknown/host and process streams must not
 * inherit the native-only builtin resource's leaf certificate. */
enum { XL6_IO_ORIGIN_UNKNOWN=0, XL6_IO_ORIGIN_BUILTIN=1, XL6_IO_ORIGIN_PROCESS=2 };
typedef struct xlir_io_reader { xreader* stream; unsigned origin; } xlir_io_reader;
typedef struct xlir_io_writer { xwriter* stream; void* owner; void (*drop)(void*); unsigned origin; } xlir_io_writer;
#endif
