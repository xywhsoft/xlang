#ifndef FOREIGN_FIELD_RECEIVER_H
#define FOREIGN_FIELD_RECEIVER_H
#include <xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct ReceiverTable ReceiverTable;
typedef struct ReceiverPayload { ReceiverTable* table; int bias; } ReceiverPayload;
typedef struct ReceiverScope { ReceiverTable* table; } ReceiverScope;
typedef struct ReceiverInfo { int width,height; } ReceiverInfo;
struct ReceiverTable {
 void* pUser;
 void* unused;
 const ReceiverPayload* constUser;
 int (*add)(void*,int);
 int (*read)(const ReceiverPayload*);
 size_t (*count)(void*,const uint8_t*,size_t);
 void (*pair)(void*,int*,float*);
 int (*checked)(void*,int);
 xstrview (*echo)(void*,xstrview);
 int (*apply)(void*,int,int(*)(int,void*),void*);
 int (*missing)(void*);
 bool (*isNull)(void*);
 int (*info)(void*,ReceiverInfo*);
 int (*readPixels)(void*,void*,int);
 unsigned depth;
 bool closing;
 ReceiverPayload payload;
};
extern const XL6_ForeignViewDescriptorV1 receiverPolicy;
int receiver_create(ReceiverTable**);
void receiver_destroy(ReceiverTable*);
ReceiverTable* receiver_null(void);
void receiver_heap_begin(void);
void receiver_heap_end(void);
void receiver_native_samples(ReceiverTable*,int);
int receiver_live_count(void);
#endif
