#ifndef XL6_IO_PRIVATE_H
#define XL6_IO_PRIVATE_H
#include "xl6_text_value.h"
#include "xl6_io_handle.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_ownership_scope.h"
#include <string.h>
/* Private resident helpers. No generated callback or arbitrary host stream is
 * admitted by this ABI. Certified process contexts publish their actual owner
 * edge; unique stream moves and detaches participate in the graph freeze. */
static inline const char* xl6IoPath(xstrview path)
{
    if(!path.Data){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(memchr(path.Data,0,path.Size)){xrtSetErrorKind(XERR_VALUE);return NULL;}
    return path.Data;
}
static inline void xl6IoRestoreFailure(xerror* error)
{ xrtClearError();xrtSetErrorTake(error); }
static inline bool xlir_io_size(int64_t count, size_t* size)
{ if (count < 0 || (uint64_t)count > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
  *size = (size_t)count; return true; }
static inline bool xlir_io_count(int64_t limit, size_t actual, size_t* size)
{ if (limit < -1) { xrtSetErrorKind(XERR_RANGE); return false; }
  *size = limit >= 0 && (uint64_t)limit < actual ? (size_t)limit : actual; return true; }
static inline int64_t xlir_io_position(uint64_t position)
{ if (position > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return -1; } return (int64_t)position; }
static inline xreader* xlir_io_reader_get(void* handle)
{ xlir_io_reader* r = (xlir_io_reader*)handle;
  if (r == NULL || r->stream == NULL) { xrtSetErrorKind(XERR_STATE); return NULL; } return r->stream; }
static inline xwriter* xlir_io_writer_get(void* handle)
{ xlir_io_writer* w = (xlir_io_writer*)handle;
  if (w == NULL || w->stream == NULL) { xrtSetErrorKind(XERR_STATE); return NULL; } return w->stream; }

static inline void* xlir_io_reader_take(xreader* stream)
{ xlir_io_reader* r; xerror* error;
  if (stream == NULL) return NULL;
  r = (xlir_io_reader*)xrtCalloc(1, sizeof(*r));
  if (r == NULL) { error = xrtTakeError(); (void)xrtReaderDestroy(stream);
    xl6IoRestoreFailure(error); return NULL; }
  r->stream = stream; r->origin=XL6_IO_ORIGIN_BUILTIN; return r; }

static inline void* xlir_io_writer_take(xwriter* stream, void* owner, void (*drop)(void*))
{ xlir_io_writer* w; xerror* error;
  if (stream == NULL) { error = xrtTakeError(); if (drop != NULL) { drop(owner); }
    xl6IoRestoreFailure(error); return NULL; }
  w = (xlir_io_writer*)xrtCalloc(1, sizeof(*w));
  if (w == NULL) { error = xrtTakeError(); (void)xrtWriterDestroy(stream);
    if (drop != NULL) { drop(owner); } xl6IoRestoreFailure(error); return NULL; }
  w->stream = stream; w->owner = owner; w->drop = drop;
  w->origin=XL6_IO_ORIGIN_BUILTIN; return w; }

#endif
