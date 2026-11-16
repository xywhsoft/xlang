#ifndef XL6_TEST_FOREIGN_RETURN_OUTPUTS_H
#define XL6_TEST_FOREIGN_RETURN_OUTPUTS_H
#include <__xl_runtime/xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ReturnOutputReceiver ReturnOutputReceiver;
struct ReturnOutputReceiver {int (*query)(ReturnOutputReceiver*,int,double*,int*);};
typedef struct ReturnOutputPack {int first,second;} ReturnOutputPack;
extern const XL6_ForeignViewDescriptorV1 returnOutputReceiverPolicy;
int returnOutputQuery(int,double*,int*);
unsigned returnOutputCount(void);
int returnOutputReceiverQuery(ReturnOutputReceiver*,int,double*,int*);
int returnOutputReceiverCreate(ReturnOutputReceiver**);
void returnOutputReceiverDestroy(ReturnOutputReceiver*);
int returnOutputRawQuery(int,double*,int*);
int returnOutputMaybeQuery(int,double*,int*);
int returnOutputSpan(const uint8*,int,int*);
int returnOutputPacked(const ReturnOutputPack*,int*);
int returnOutputNoInputs(int*);
void returnOutputHeapMark(void);
bool returnOutputHeapCheck(void);
bool returnOutputHeapBaseline(void);
#ifdef __cplusplus
}
#endif
#endif
