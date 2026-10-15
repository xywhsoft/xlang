#include "xl6_io_private.h"
#include "xl6_fs_file.h"

static bool xlir_fs_stream_read(void* owner, void* data, size_t count, size_t* done)
{ xfile file = xlir_fs_file_get(owner); return file != NULL && xrtRead(file, data, count, done); }
static bool xlir_fs_stream_write(void* owner, const void* data, size_t count, size_t* done)
{ xfile file = xlir_fs_file_get(owner); return file != NULL && xrtWrite(file, data, count, done); }
static bool xlir_fs_stream_seek(void* owner, int64_t offset, xseek origin, uint64_t* position)
{ xfile file = xlir_fs_file_get(owner); return file != NULL && xrtSeek(file, offset, origin, position); }
static bool xlir_fs_stream_tell(void* owner, uint64_t* position)
{ xfile file = xlir_fs_file_get(owner); return file != NULL && xrtTell(file, position); }
static bool xlir_fs_stream_size(void* owner, uint64_t* size)
{ xfile file = xlir_fs_file_get(owner); return file != NULL && xrtFileSize(file, size); }
static bool xlir_fs_stream_flush(void* owner)
{ xfile file = xlir_fs_file_get(owner); return file != NULL && xrtFlush(file); }
static bool xlir_fs_stream_drop(void* owner)
{ xlir_fs_file_drop((xlir_fs_file*)owner); return xrtGetError() == NULL; }
XL6_TEXT_API void* xl6_xrt_fs_file_reader(void* handle)
{ xlir_fs_file* owner = (xlir_fs_file*)handle; xfile file = xlir_fs_file_get(handle); xreaderops ops = {0};
  uint64_t caps; xreader* reader; xerror* error;
  if (file == NULL) { return NULL; } caps = xrtFileCapabilities(file);
  if (!(caps & XFILE_CAP_READ)) { xrtSetErrorKind(XERR_UNSUPPORTED); return NULL; }
  ops.Read = xlir_fs_stream_read; ops.Close = xlir_fs_stream_drop;
  if (caps & XFILE_CAP_SEEK) { ops.Seek = xlir_fs_stream_seek; ops.Tell = xlir_fs_stream_tell; }
  if (caps & XFILE_CAP_STAT) ops.Size = xlir_fs_stream_size;
  if (!xlir_fs_file_ref(owner)) { return NULL; }
  reader = xrtReaderCreate(&ops, owner);
  if (reader == NULL) { error = xrtTakeError(); xlir_fs_file_drop(owner); xrtClearError();
    if (error != NULL) { xrtSetErrorTake(error); } return NULL; }
  return xlir_io_reader_take(reader); }
XL6_TEXT_API void* xl6_xrt_fs_file_writer(void* handle)
{ xlir_fs_file* owner = (xlir_fs_file*)handle; xfile file = xlir_fs_file_get(handle); xwriterops ops = {0};
  uint64_t caps; xwriter* writer; xerror* error;
  if (file == NULL) { return NULL; } caps = xrtFileCapabilities(file);
  if (!(caps & XFILE_CAP_WRITE)) { xrtSetErrorKind(XERR_UNSUPPORTED); return NULL; }
  ops.Write = xlir_fs_stream_write; ops.Close = xlir_fs_stream_drop;
  if (caps & XFILE_CAP_SEEK) { ops.Seek = xlir_fs_stream_seek; ops.Tell = xlir_fs_stream_tell; }
  if (caps & XFILE_CAP_STAT) ops.Size = xlir_fs_stream_size;
  if (caps & XFILE_CAP_FLUSH) ops.Flush = xlir_fs_stream_flush;
  if (!xlir_fs_file_ref(owner)) { return NULL; }
  writer = xrtWriterCreate(&ops, owner);
  if (writer == NULL) { error = xrtTakeError(); xlir_fs_file_drop(owner); xrtClearError();
    if (error != NULL) { xrtSetErrorTake(error); } return NULL; }
  return xlir_io_writer_take(writer, NULL, NULL); }
