#ifndef XL_CSV_NATIVE_H
#define XL_CSV_NATIVE_H
#include "xrt_csv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 语言绑定的物理 ABI；拥有的字符串和 Buffer 通过 point 移交给宿主。 */
void* xcsv_reader_open_memory(void* data, uint64 size, cstr delimiter, cstr quote,
    bool strict, uint64 maxInput, uint64 maxOutput, uint64 maxField, uint64 maxRows, uint64 maxFields);
void* xcsv_reader_open_text(cstr text, uint64 size, cstr delimiter, cstr quote,
    bool strict, uint64 maxInput, uint64 maxOutput, uint64 maxField, uint64 maxRows, uint64 maxFields);
void* xcsv_reader_open_file(cstr path, cstr delimiter, cstr quote, bool strict,
    uint64 maxInput, uint64 maxOutput, uint64 maxField, uint64 maxRows, uint64 maxFields);
void xcsv_reader_destroy(void* reader);
bool xcsv_reader_next(void* reader);
bool xcsv_reader_eof(void* reader);
int64 xcsv_reader_row(void* reader);
int64 xcsv_reader_count(void* reader);
bool xcsv_reader_close(void* reader);
void* xcsv_reader_field(void* reader, int64 index);

void* xcsv_writer_create_memory(cstr delimiter, cstr quote, int newline, bool quoteAll,
    bool writeBOM, uint64 maxInput, uint64 maxOutput, uint64 maxField, uint64 maxRows, uint64 maxFields);
void* xcsv_writer_open_file(cstr path, cstr delimiter, cstr quote, int newline, bool quoteAll,
    bool writeBOM, uint64 maxInput, uint64 maxOutput, uint64 maxField, uint64 maxRows, uint64 maxFields);
void xcsv_writer_destroy(void* writer);
bool xcsv_writer_begin(void* writer);
bool xcsv_writer_field(void* writer, cstr value);
bool xcsv_writer_end(void* writer);
bool xcsv_writer_close(void* writer);
void* xcsv_writer_take_buffer(void* writer);
/* 取走内存写入器的 UTF-8 结果；调用方使用 xrtFree 释放。 */
void* xcsv_writer_take_text(void* writer);

#ifdef __cplusplus
}
#endif
#endif
