#ifndef FOREIGN_PACK_CALLBACK_H
#define FOREIGN_PACK_CALLBACK_H
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PcDesc {uint32_t size;int seed,count;int (*callback)(int,void*);void* user;} PcDesc;
typedef struct PcFirst {uint32_t size;int seed,count;int (*callback)(void*,int);void* user;} PcFirst;
typedef struct PcPair {PcDesc left;PcFirst right;} PcPair;
typedef struct PcAction {int count;void (*callback)(void*);void* user;} PcAction;
typedef struct PcEvent {int value;} PcEvent;
typedef struct PcEventDesc {int seed;int (*callback)(const PcEvent*,void*);void* user;} PcEventDesc;
typedef struct PcRoot PcRoot;
struct PcRoot {int value;int (*apply)(PcRoot*,const PcDesc*,int*);};
typedef struct PcReceipt {void* payload;int (*callback)(int,void*);void* user;} PcReceipt;
extern const XL6_ForeignViewDescriptorV1 pcPolicy;
extern const XL6_ForeignViewDescriptorV1 pcReceiptPolicy;
int pc_call(const PcDesc*,int*);
int pc_scalar(const PcDesc*);
int pc_pair(const PcPair*);
int pc_two(const PcDesc*,const PcFirst*);
void pc_action(const PcAction*);
int pc_event(const PcEventDesc*);
int pc_worker(const PcDesc*);
int pc_make(const PcDesc*,PcRoot**);
void pc_drop(PcRoot*);
int pc_value(PcRoot*);
int pc_live(void);
void pc_heap(size_t*,size_t*);
void pc_receipt(PcReceipt*,const PcDesc*);
int pc_receipt_adopt(PcReceipt*);
void pc_receipt_rollback(PcReceipt*);
int pc_tail_calls(void);
#ifdef __cplusplus
}
#endif
#endif
