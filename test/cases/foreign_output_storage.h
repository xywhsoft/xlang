#ifndef FOREIGN_OUTPUT_STORAGE_H
#define FOREIGN_OUTPUT_STORAGE_H
#include <__xl_runtime/xrt_decl.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct StorageValue {size_t size;uint32_t tag;uint64_t words[4];} StorageValue;
int storage_read(int code,int partial,const StorageValue* input,StorageValue* output);
int storage_pair(int code,const StorageValue* input,StorageValue* first,StorageValue* second);
unsigned storage_calls(void);
void storage_heap_begin(void);
void storage_heap_end(void);
#ifdef __cplusplus
}
#endif
#endif
