#ifndef XL6_IO_HANDLE_H
#define XL6_IO_HANDLE_H
/* Fixed resource projection ABI, shared by generated IO and resident users. */
typedef struct xlir_io_writer { xwriter* stream; void* owner; void (*drop)(void*); } xlir_io_writer;
#endif
