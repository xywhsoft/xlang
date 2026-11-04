#ifndef FOREIGN_OUTPUT_RECORD_H
#define FOREIGN_OUTPUT_RECORD_H
#include <stdint.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct OutputRecordNode {int refs,value,mode,holds;} OutputRecordNode;
typedef struct OutputRecordPoint {float x,y;} OutputRecordPoint;
typedef struct OutputRecordNative {
    uint32_t code;
    OutputRecordNode* first;
    OutputRecordPoint point;
    OutputRecordNode* second;
    int mode;
} OutputRecordNative;
extern const XL6_ForeignViewDescriptorV1 outputRecordPolicy;
int output_record_create(OutputRecordNode**,int);
int output_record_read(OutputRecordNode*,OutputRecordNative*);
int output_record_read_pod(OutputRecordNode*,OutputRecordNative*);
int output_record_read_one(OutputRecordNode*,OutputRecordNative*);
int output_record_read_owned(OutputRecordNode*,OutputRecordNative*);
void output_record_read_void(OutputRecordNode*,OutputRecordNative*);
int output_record_hold(OutputRecordNode*);
int output_record_drop(OutputRecordNode*);
int output_record_refs(OutputRecordNode*);
int output_record_live(void);
int output_record_producers(void);
void output_record_fail_after(int);
void output_record_reset(void);
#ifdef __cplusplus
}
#endif
#endif
