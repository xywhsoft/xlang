/* Selected IO adapters own only language results and close their XRT streams.
 * XRT supplies file modes, streaming, limits, short-operation handling and errors. */
static void xl6IrCgenEmitIoRuntime(XL6_Output* out, uint64_t operations, int fileReader, int fileWriter)
{
    if (operations & 1u)
        xl6OutputPuts(
            "xvalue* xl6_xrt_io_read_file(xlir_string path, int64_t limit)\n"
            "{ const char* name; xreader* reader; xbuffer* buffer; unsigned char* data;\n"
            "  size_t size = 0; xvalue* result; bool closed; xerror* error;\n"
            "  if (limit < 0 || (uint64_t)limit > SIZE_MAX)\n"
            "    { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
            "  name = xlir_string_cstr(path); if (name == NULL) return NULL;\n"
            "  reader = xrtReaderOpen(name); if (reader == NULL) return NULL;\n"
            "  buffer = xrtReaderReadAll(reader, (size_t)limit);\n"
            "  error = buffer == NULL ? xrtTakeError() : NULL;\n"
            "  closed = xrtReaderDestroy(reader);\n"
            "  if (error != NULL) { xrtSetErrorTake(error); }\n"
            "  if (buffer == NULL || !closed) { xrtBufferDestroy(buffer); return NULL; }\n"
            "  data = xrtBufferTake(buffer, &size, NULL); xrtBufferDestroy(buffer);\n"
            "  result = xrtValueBytesTake(&data, size); if (data != NULL) xrtFree(data);\n"
            "  return result; }\n\n", out);
    if (operations & (UINT64_C(6) | XL6_IO_BIT(WRITE_BUFFER_FILE))) {
        xl6OutputPuts(
            "static bool xlir_io_write_file(xlir_string path, const void* data, size_t actual, int64_t mode, int64_t limit)\n"
            "{ const char* name; xfile file; xwriter* writer; uint32_t flags; size_t count;\n"
            "  bool written, closed; xerror* error;\n"
            "  if (mode < 0 || mode > 2 || limit < -1)\n"
            "    { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "  if (data == NULL && actual != 0)\n"
            "    { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
            "  count = limit >= 0 && (uint64_t)limit < actual ? (size_t)limit : actual;\n"
            "  name = xlir_string_cstr(path); if (name == NULL) return false;\n"
            "  flags = XFILE_WRITE | XFILE_CREATE | (mode == 0 ? XFILE_EXCLUSIVE :\n"
            "    mode == 1 ? XFILE_TRUNCATE : XFILE_APPEND);\n"
            "  file = xrtOpen(name, flags); if (file == NULL) return false;\n"
            "  writer = xrtWriterTakeFile(&file);\n"
            "  if (writer == NULL) { error = xrtTakeError(); (void)xrtClose(file);\n"
            "    if (error != NULL) { xrtSetErrorTake(error); } return false; }\n"
            "  written = xrtWriterWriteFull(writer, data, count, NULL) && xrtWriterFlush(writer);\n"
            "  error = !written ? xrtTakeError() : NULL; closed = xrtWriterDestroy(writer);\n"
            "  if (error != NULL) { xrtSetErrorTake(error); } return written && closed; }\n\n", out);
    }
    if (operations & 2u)
        xl6OutputPuts(
            "bool xl6_xrt_io_write_file(xlir_string path, const xvalue* data, int64_t mode, int64_t limit)\n"
            "{ xbytesview view = {0}; if (data == NULL || !xrtValueGetBytes(data, &view))\n"
            "    { xrtSetErrorKind(XERR_TYPE); return false; }\n"
            "  return xlir_io_write_file(path, view.Data, view.Size, mode, limit); }\n\n", out);
    if (operations & 4u)
        xl6OutputPuts(
            "bool xl6_xrt_io_write_text_file(xlir_string path, xlir_string data, int64_t mode, int64_t limit)\n"
            "{ return xlir_io_write_file(path, data.Data, data.Size, mode, limit); }\n\n", out);
    if (operations & (~UINT64_C(7))) xl6OutputPuts(
        "typedef struct xlir_io_reader { xreader* stream; } xlir_io_reader;\n"
        "#include <__xl_runtime/xl6_io_handle.h>\n"
        "static bool xlir_io_size(int64_t count, size_t* size)\n"
        "{ if (count < 0 || (uint64_t)count > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  *size = (size_t)count; return true; }\n"
        "static bool xlir_io_count(int64_t limit, size_t actual, size_t* size)\n"
        "{ if (limit < -1) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  *size = limit >= 0 && (uint64_t)limit < actual ? (size_t)limit : actual; return true; }\n"
        "static int64_t xlir_io_position(uint64_t position)\n"
        "{ if (position > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return -1; } return (int64_t)position; }\n"
        "static xreader* xlir_io_reader_get(void* handle)\n"
        "{ xlir_io_reader* r = (xlir_io_reader*)handle;\n"
        "  if (r == NULL || r->stream == NULL) { xrtSetErrorKind(XERR_STATE); return NULL; } return r->stream; }\n"
        "static xwriter* xlir_io_writer_get(void* handle)\n"
        "{ xlir_io_writer* w = (xlir_io_writer*)handle;\n"
        "  if (w == NULL || w->stream == NULL) { xrtSetErrorKind(XERR_STATE); return NULL; } return w->stream; }\n"
        "\n"
        , out);
    if (fileReader || (operations & (XL6_IO_BIT(READER_OPEN) | XL6_IO_BIT(READER_BYTES) | XL6_IO_BIT(READER_BUFFER) | XL6_IO_BIT(READER_STDIN)))) xl6OutputPuts(
        "static void* xlir_io_reader_take(xreader* stream)\n"
        "{ xlir_io_reader* r; xerror* error;\n"
        "  if (stream == NULL) return NULL;\n"
        "  r = (xlir_io_reader*)xrtCalloc(1, sizeof(*r));\n"
        "  if (r == NULL) { error = xrtTakeError(); (void)xrtReaderDestroy(stream);\n"
        "    if (error != NULL) { xrtSetErrorTake(error); } return NULL; }\n"
        "  r->stream = stream; return r; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_OPEN))) xl6OutputPuts(
        "void* xl6_xrt_io_reader_open(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL ? xlir_io_reader_take(xrtReaderOpen(name)) : NULL; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_BYTES) | XL6_IO_BIT(READER_BUFFER))) xl6OutputPuts(
        "static void* xlir_io_reader_snapshot(xbytesview view)\n"
        "{ xbuffer* buffer = xrtBufferFrom(view); xreader* stream; xerror* error;\n"
        "  if (buffer == NULL) return NULL;\n"
        "  stream = xrtReaderTakeBuffer(&buffer);\n"
        "  if (stream == NULL) { error = xrtTakeError(); xrtBufferDestroy(buffer);\n"
        "    if (error != NULL) { xrtSetErrorTake(error); } return NULL; }\n"
        "  return xlir_io_reader_take(stream); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_BYTES))) xl6OutputPuts(
        "void* xl6_xrt_io_reader_bytes(const xvalue* data, int64_t limit)\n"
        "{ xbytesview view = {0}; if (!xrtValueGetBytes(data, &view)) { xrtSetErrorKind(XERR_TYPE); return NULL; }\n"
        "  if (!xlir_io_count(limit, view.Size, &view.Size)) return NULL;\n"
        "  return xlir_io_reader_snapshot(view); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_BUFFER))) xl6OutputPuts(
        "void* xl6_xrt_io_reader_buffer(void* data, int64_t limit)\n"
        "{ if (data == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }\n"
        "  xbytesview view = xrtBufferView((xbuffer*)data);\n"
        "  if (!xlir_io_count(limit, view.Size, &view.Size)) return NULL;\n"
        "  return xlir_io_reader_snapshot(view); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_STDIN))) xl6OutputPuts(
        "void* xl6_xrt_io_reader_stdin(void) { return xlir_io_reader_take(xrtReaderStdin()); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_DESTROY) | XL6_IO_BIT(READER_CLOSE))) xl6OutputPuts(
        "static bool xlir_io_reader_close(void* handle)\n"
        "{ xlir_io_reader* r = (xlir_io_reader*)handle; xreader* stream;\n"
        "  if (r == NULL || r->stream == NULL) return true;\n"
        "  stream = r->stream; r->stream = NULL; return xrtReaderDestroy(stream); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_DESTROY))) xl6OutputPuts(
        "void xl6_xrt_io_reader_destroy(void* handle)\n"
        "{ xerror* error = xrtTakeError(); (void)xlir_io_reader_close(handle); xrtFree(handle); xrtClearError();\n"
        "  if (error != NULL) { xrtSetErrorTake(error); } }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_CLOSE))) xl6OutputPuts(
        "bool xl6_xrt_io_reader_close(void* handle) { return xlir_io_reader_close(handle); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_HANDLE))) xl6OutputPuts(
        "void* xl6_xrt_io_reader_handle(void* handle) { return handle; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_READ) | XL6_IO_BIT(READER_EXACT))) xl6OutputPuts(
        "static xvalue* xlir_io_read(void* handle, int64_t max, bool exact)\n"
        "{ size_t count, done = 0; unsigned char* data = NULL; xvalue* result; xreader* r;\n"
        "  if (!xlir_io_size(max, &count) || (r = xlir_io_reader_get(handle)) == NULL) return NULL;\n"
        "  if (count != 0 && (data = (unsigned char*)xrtMalloc(count)) == NULL) return NULL;\n"
        "  if (!(exact ? xrtReaderReadFull(r, data, count, &done) : xrtReaderRead(r, data, count, &done)))\n"
        "    { xrtFree(data); return NULL; }\n"
        "  result = xrtValueBytesTake(&data, done); xrtFree(data); return result; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_READ))) xl6OutputPuts(
        "xvalue* xl6_xrt_io_reader_read(void* r, int64_t n) { return xlir_io_read(r, n, false); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_EXACT))) xl6OutputPuts(
        "xvalue* xl6_xrt_io_reader_exact(void* r, int64_t n) { return xlir_io_read(r, n, true); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_ALL))) xl6OutputPuts(
        "xvalue* xl6_xrt_io_reader_all(void* handle, int64_t limit)\n"
        "{ xreader* r; xbuffer* buffer; size_t max, size = 0; unsigned char* data; xvalue* value;\n"
        "  if (!xlir_io_size(limit, &max) || (r = xlir_io_reader_get(handle)) == NULL) return NULL;\n"
        "  buffer = xrtReaderReadAll(r, max); if (buffer == NULL) return NULL;\n"
        "  data = xrtBufferTake(buffer, &size, NULL); xrtBufferDestroy(buffer);\n"
        "  value = xrtValueBytesTake(&data, size); xrtFree(data); return value; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_INTO))) xl6OutputPuts(
        "int64_t xl6_xrt_io_reader_into(void* handle, void* target, int64_t limit)\n"
        "{ xreader* r; xbuffer* buffer = (xbuffer*)target; size_t max, old, done = 0; xbytesview view;\n"
        "  if (!xlir_io_size(limit, &max) || (r = xlir_io_reader_get(handle)) == NULL) return -1;\n"
        "  if (buffer == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }\n"
        "  old = xrtBufferView(buffer).Size;\n"
        "  if (max > SIZE_MAX - old) { xrtSetErrorKind(XERR_RANGE); return -1; }\n"
        "  if (!xrtBufferResize(buffer, old + max)) return -1;\n"
        "  view = xrtBufferView(buffer);\n"
        "  if (!xrtReaderRead(r, max != 0 ? (unsigned char*)view.Data + old : NULL, max, &done))\n"
        "    { (void)xrtBufferResize(buffer, old); return -1; }\n"
        "  (void)xrtBufferResize(buffer, old + done); return (int64_t)done; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_TELL))) xl6OutputPuts(
        "int64_t xl6_xrt_io_reader_tell(void* handle)\n"
        "{ xreader* r = xlir_io_reader_get(handle); uint64_t value = 0;\n"
        "  return r != NULL && xrtReaderTell(r, &value) ? xlir_io_position(value) : -1; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_SIZE))) xl6OutputPuts(
        "int64_t xl6_xrt_io_reader_size(void* handle)\n"
        "{ xreader* r = xlir_io_reader_get(handle); uint64_t value = 0;\n"
        "  return r != NULL && xrtReaderSize(r, &value) ? xlir_io_position(value) : -1; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_SEEK))) xl6OutputPuts(
        "int64_t xl6_xrt_io_reader_seek(void* handle, int64_t offset, int64_t origin)\n"
        "{ xreader* r = xlir_io_reader_get(handle); uint64_t value = 0;\n"
        "  if (origin < 0 || origin > 2) { xrtSetErrorKind(XERR_RANGE); return -1; }\n"
        "  return r != NULL && xrtReaderSeek(r, offset, (xseek)origin, &value) ? xlir_io_position(value) : -1; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_EOF))) xl6OutputPuts(
        "bool xl6_xrt_io_reader_eof(void* handle) { return xlir_io_reader_get(handle) != NULL && xrtReaderEOF(((xlir_io_reader*)handle)->stream); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_CAN_SEEK))) xl6OutputPuts(
        "bool xl6_xrt_io_reader_can_seek(void* handle) { return xlir_io_reader_get(handle) != NULL && xrtReaderCanSeek(((xlir_io_reader*)handle)->stream); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_CAN_SIZE))) xl6OutputPuts(
        "bool xl6_xrt_io_reader_can_size(void* handle) { return xlir_io_reader_get(handle) != NULL && xrtReaderCanSize(((xlir_io_reader*)handle)->stream); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(READER_CLOSED))) xl6OutputPuts(
        "bool xl6_xrt_io_reader_closed(void* handle) { return handle == NULL || ((xlir_io_reader*)handle)->stream == NULL; }\n"
        "\n"
        , out);
    if (fileWriter || (operations & (XL6_IO_BIT(WRITER_OPEN) | XL6_IO_BIT(WRITER_BUFFER) | XL6_IO_BIT(WRITER_STANDARD)))) xl6OutputPuts(
        "static void* xlir_io_writer_take(xwriter* stream, void* owner, void (*drop)(void*))\n"
        "{ xlir_io_writer* w; xerror* error;\n"
        "  if (stream == NULL) { error = xrtTakeError(); if (drop != NULL) { drop(owner); }\n"
        "    if (error != NULL) { xrtSetErrorTake(error); } return NULL; }\n"
        "  w = (xlir_io_writer*)xrtCalloc(1, sizeof(*w));\n"
        "  if (w == NULL) { error = xrtTakeError(); (void)xrtWriterDestroy(stream);\n"
        "    if (drop != NULL) { drop(owner); } if (error != NULL) { xrtSetErrorTake(error); } return NULL; }\n"
        "  w->stream = stream; w->owner = owner; w->drop = drop; return w; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_OPEN))) xl6OutputPuts(
        "void* xl6_xrt_io_writer_open(xlir_string path, int64_t mode)\n"
        "{ const char* name; xfile file; xwriter* stream; xerror* error; uint32_t flags;\n"
        "  if (mode < 0 || mode > 2) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  name = xlir_string_cstr(path); if (name == NULL) return NULL;\n"
        "  flags = XFILE_WRITE | XFILE_CREATE | (mode == 0 ? XFILE_EXCLUSIVE : mode == 1 ? XFILE_TRUNCATE : XFILE_APPEND);\n"
        "  file = xrtOpen(name, flags); if (file == NULL) return NULL;\n"
        "  stream = xrtWriterTakeFile(&file);\n"
        "  if (stream == NULL) { error = xrtTakeError(); (void)xrtClose(file);\n"
        "    if (error != NULL) { xrtSetErrorTake(error); } return NULL; }\n"
        "  return xlir_io_writer_take(stream, NULL, NULL); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_BUFFER))) xl6OutputPuts(
        "static void xlir_io_buffer_drop(void* owner) { xrtBufferDestroy((xbuffer*)owner); }\n"
        "void* xl6_xrt_io_writer_buffer(void)\n"
        "{ xbuffer* buffer = xrtBufferCreate(); if (buffer == NULL) return NULL;\n"
        "  return xlir_io_writer_take(xrtWriterFromBuffer(buffer), buffer, xlir_io_buffer_drop); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_STANDARD))) xl6OutputPuts(
        "void* xl6_xrt_io_writer_standard(int64_t stream)\n"
        "{ if (stream < 1 || stream > 3) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  return xlir_io_writer_take(stream == 1 ? xrtWriterStdout() : stream == 2 ? xrtWriterStderr() : xrtWriterDiscard(), NULL, NULL); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_DESTROY) | XL6_IO_BIT(WRITER_CLOSE))) xl6OutputPuts(
        "static bool xlir_io_writer_close(void* handle)\n"
        "{ xlir_io_writer* w = (xlir_io_writer*)handle; xwriter* stream;\n"
        "  if (w == NULL || w->stream == NULL) return true;\n"
        "  stream = w->stream; w->stream = NULL; return xrtWriterDestroy(stream); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_DESTROY))) xl6OutputPuts(
        "void xl6_xrt_io_writer_destroy(void* handle)\n"
        "{ xlir_io_writer* w = (xlir_io_writer*)handle; xerror* error = xrtTakeError();\n"
        "  (void)xlir_io_writer_close(handle);\n"
        "  if (w != NULL && w->drop != NULL) { w->drop(w->owner); } xrtFree(w); xrtClearError();\n"
        "  if (error != NULL) { xrtSetErrorTake(error); } }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_CLOSE))) xl6OutputPuts(
        "bool xl6_xrt_io_writer_close(void* handle) { return xlir_io_writer_close(handle); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_HANDLE))) xl6OutputPuts(
        "void* xl6_xrt_io_writer_handle(void* handle) { return handle; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_BYTES) | XL6_IO_BIT(WRITER_TEXT) | XL6_IO_BIT(WRITER_WRITE_BUFFER) | XL6_IO_BIT(WRITER_ALL_BYTES) | XL6_IO_BIT(WRITER_ALL_TEXT) | XL6_IO_BIT(WRITER_ALL_BUFFER))) xl6OutputPuts(
        "static int64_t xlir_io_write(void* handle, xbytesview view, int64_t limit, bool all)\n"
        "{ xwriter* w; size_t count, done = 0;\n"
        "  if (!xlir_io_count(limit, view.Size, &count) || (w = xlir_io_writer_get(handle)) == NULL) return -1;\n"
        "  if (!(all ? xrtWriterWriteFull(w, view.Data, count, &done) : xrtWriterWrite(w, view.Data, count, &done))) return -1;\n"
        "  return xlir_io_position(done); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_BYTES))) xl6OutputPuts(
        "int64_t xl6_xrt_io_writer_bytes(void* handle, const xvalue* data, int64_t limit)\n"
        "{ xbytesview view = {0}; if (!xrtValueGetBytes(data, &view)) { xrtSetErrorKind(XERR_TYPE); return -1; } return xlir_io_write(handle, view, limit, false); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_ALL_BYTES))) xl6OutputPuts(
        "bool xl6_xrt_io_writer_all_bytes(void* handle, const xvalue* data, int64_t limit)\n"
        "{ xbytesview view = {0}; if (!xrtValueGetBytes(data, &view)) { xrtSetErrorKind(XERR_TYPE); return false; } return xlir_io_write(handle, view, limit, true) >= 0; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_TEXT))) xl6OutputPuts(
        "int64_t xl6_xrt_io_writer_text(void* handle, xlir_string data, int64_t limit)\n"
        "{ xbytesview view = { (const unsigned char*)data.Data, data.Size }; return xlir_io_write(handle, view, limit, false); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_ALL_TEXT))) xl6OutputPuts(
        "bool xl6_xrt_io_writer_all_text(void* handle, xlir_string data, int64_t limit)\n"
        "{ xbytesview view = { (const unsigned char*)data.Data, data.Size }; return xlir_io_write(handle, view, limit, true) >= 0; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_WRITE_BUFFER))) xl6OutputPuts(
        "int64_t xl6_xrt_io_writer_write_buffer(void* handle, void* data, int64_t limit)\n"
        "{ xbytesview view; if (data == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return -1; } view = xrtBufferView((xbuffer*)data); return xlir_io_write(handle, view, limit, false); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_ALL_BUFFER))) xl6OutputPuts(
        "bool xl6_xrt_io_writer_all_buffer(void* handle, void* data, int64_t limit)\n"
        "{ xbytesview view; if (data == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; } view = xrtBufferView((xbuffer*)data); return xlir_io_write(handle, view, limit, true) >= 0; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_FLUSH))) xl6OutputPuts(
        "bool xl6_xrt_io_writer_flush(void* handle)\n"
        "{ xwriter* w = xlir_io_writer_get(handle); return w != NULL && xrtWriterFlush(w); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_TELL))) xl6OutputPuts(
        "int64_t xl6_xrt_io_writer_tell(void* handle)\n"
        "{ xwriter* w = xlir_io_writer_get(handle); uint64_t value = 0;\n"
        "  return w != NULL && xrtWriterTell(w, &value) ? xlir_io_position(value) : -1; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_SIZE))) xl6OutputPuts(
        "int64_t xl6_xrt_io_writer_size(void* handle)\n"
        "{ xwriter* w = xlir_io_writer_get(handle); uint64_t value = 0;\n"
        "  return w != NULL && xrtWriterSize(w, &value) ? xlir_io_position(value) : -1; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_SEEK))) xl6OutputPuts(
        "int64_t xl6_xrt_io_writer_seek(void* handle, int64_t offset, int64_t origin)\n"
        "{ xwriter* w = xlir_io_writer_get(handle); uint64_t value = 0;\n"
        "  if (origin < 0 || origin > 2) { xrtSetErrorKind(XERR_RANGE); return -1; }\n"
        "  return w != NULL && xrtWriterSeek(w, offset, (xseek)origin, &value) ? xlir_io_position(value) : -1; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_CAN_SEEK))) xl6OutputPuts(
        "bool xl6_xrt_io_writer_can_seek(void* handle) { return xlir_io_writer_get(handle) != NULL && xrtWriterCanSeek(((xlir_io_writer*)handle)->stream); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_CAN_SIZE))) xl6OutputPuts(
        "bool xl6_xrt_io_writer_can_size(void* handle) { return xlir_io_writer_get(handle) != NULL && xrtWriterCanSize(((xlir_io_writer*)handle)->stream); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_CLOSED))) xl6OutputPuts(
        "bool xl6_xrt_io_writer_closed(void* handle) { return handle == NULL || ((xlir_io_writer*)handle)->stream == NULL; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITER_TO_BYTES))) xl6OutputPuts(
        "xvalue* xl6_xrt_io_writer_to_bytes(void* handle)\n"
        "{ xlir_io_writer* w = (xlir_io_writer*)handle;\n"
        "  if (w == NULL || w->owner == NULL || w->drop == NULL) { xrtSetErrorKind(XERR_STATE); return NULL; }\n"
        "  return xrtValueBytes(xrtBufferView((xbuffer*)w->owner)); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(LINES_CREATE) | XL6_IO_BIT(LINES_DESTROY) | XL6_IO_BIT(LINES_CLOSE) | XL6_IO_BIT(LINES_ADVANCE) | XL6_IO_BIT(LINES_BYTES) | XL6_IO_BIT(LINES_ENDING))) xl6OutputPuts(
        "typedef struct xlir_io_lines { xlinereader* stream; xlineview view; bool hasLine; } xlir_io_lines;\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(LINES_CREATE))) xl6OutputPuts(
        "void* xl6_xrt_io_lines_create(void* handle, int64_t limit)\n"
        "{ xlir_io_reader* r = (xlir_io_reader*)handle; xlir_io_lines* lines; size_t max;\n"
        "  if (!xlir_io_size(limit, &max) || xlir_io_reader_get(handle) == NULL) return NULL;\n"
        "  lines = (xlir_io_lines*)xrtCalloc(1, sizeof(*lines)); if (lines == NULL) return NULL;\n"
        "  lines->stream = xrtLineReaderTake(&r->stream, max);\n"
        "  if (lines->stream == NULL) { xrtFree(lines); return NULL; } return lines; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(LINES_DESTROY) | XL6_IO_BIT(LINES_CLOSE))) xl6OutputPuts(
        "static bool xlir_io_lines_close(void* handle)\n"
        "{ xlir_io_lines* l = (xlir_io_lines*)handle; xlinereader* stream;\n"
        "  if (l == NULL || l->stream == NULL) return true;\n"
        "  stream = l->stream; l->stream = NULL; l->hasLine = false; return xrtLineReaderDestroy(stream); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(LINES_DESTROY))) xl6OutputPuts(
        "void xl6_xrt_io_lines_destroy(void* handle)\n"
        "{ xerror* error = xrtTakeError(); (void)xlir_io_lines_close(handle); xrtFree(handle); xrtClearError();\n"
        "  if (error != NULL) { xrtSetErrorTake(error); } }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(LINES_CLOSE))) xl6OutputPuts(
        "bool xl6_xrt_io_lines_close(void* handle) { return xlir_io_lines_close(handle); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(LINES_ADVANCE))) xl6OutputPuts(
        "bool xl6_xrt_io_lines_advance(void* handle)\n"
        "{ xlir_io_lines* l = (xlir_io_lines*)handle; xlinenext status;\n"
        "  if (l == NULL || l->stream == NULL) { xrtSetErrorKind(XERR_STATE); return false; }\n"
        "  l->hasLine = false; status = xrtLineReaderNext(l->stream, &l->view);\n"
        "  l->hasLine = status == XLINE_NEXT_LINE; return l->hasLine; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(LINES_BYTES))) xl6OutputPuts(
        "xvalue* xl6_xrt_io_lines_bytes(void* handle)\n"
        "{ xlir_io_lines* l = (xlir_io_lines*)handle; xbytesview view;\n"
        "  if (l == NULL || !l->hasLine) { xrtSetErrorKind(XERR_STATE); return NULL; }\n"
        "  view.Data = (const unsigned char*)l->view.Text.Data; view.Size = l->view.Text.Size;\n"
        "  return xrtValueBytes(view); }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(LINES_ENDING))) xl6OutputPuts(
        "int64_t xl6_xrt_io_lines_ending(void* handle)\n"
        "{ xlir_io_lines* l = (xlir_io_lines*)handle;\n"
        "  if (l == NULL || !l->hasLine) { xrtSetErrorKind(XERR_STATE); return -1; } return (int64_t)l->view.End; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(COPY))) xl6OutputPuts(
        "int64_t xl6_xrt_io_copy(void* source, void* target, int64_t limit, int64_t mode)\n"
        "{ xreader* r = xlir_io_reader_get(source); xwriter* w = xlir_io_writer_get(target);\n"
        "  uint64_t done = 0; bool ok;\n"
        "  if (r == NULL || w == NULL) return -1;\n"
        "  if (mode < 0 || mode > 2 || (mode == 0 ? limit != -1 : limit < 0))\n"
        "    { xrtSetErrorKind(XERR_RANGE); return -1; }\n"
        "  ok = mode == 1 ? xrtReaderCopyN(r, w, (uint64_t)limit, &done) :\n"
        "    mode == 2 ? xrtReaderCopyUpTo(r, w, (uint64_t)limit, &done) :\n"
        "    xrtReaderCopy(r, w, &done);\n"
        "  return ok ? xlir_io_position(done) : -1; }\n"
        "\n"
        , out);
    if (operations & (XL6_IO_BIT(WRITE_BUFFER_FILE))) xl6OutputPuts(
        "bool xl6_xrt_io_write_buffer_file(xlir_string path, void* data, int64_t mode, int64_t limit)\n"
        "{ xbytesview view; if (data == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
        "  view = xrtBufferView((xbuffer*)data); return xlir_io_write_file(path, view.Data, view.Size, mode, limit); }\n"
        "\n"
        , out);
}
