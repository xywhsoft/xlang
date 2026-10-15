#ifndef XL6_TEXT_VALUE_H
#define XL6_TEXT_VALUE_H
#include <stdint.h>
#include "xl6_product_abi.h"
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xruntime_decl.h"
#endif
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_TEXT_API __declspec(dllexport)
#else
#define XL6_TEXT_API
#endif

struct xlir_closure;
struct xfuture;
struct XL6_NativeHandlePolicyV1;
XL6_TEXT_API void* xl6_xrt_rng_create(uint64_t,uint64_t);
XL6_TEXT_API void* xl6_xrt_rng_create_default(uint64_t);
XL6_TEXT_API void xl6_xrt_rng_destroy(void*);
XL6_TEXT_API void xl6_xrt_fs_file_destroy(void*);
XL6_TEXT_API void* xl6_xrt_vfs_create(void);
XL6_TEXT_API void xl6_xrt_vfs_destroy(void*);
XL6_TEXT_API xvalue* xl6_xrt_vfs_read(void*,xstrview,int64_t);
XL6_TEXT_API void xl6_xrt_vfs_mount_destroy(void*);
XL6_TEXT_API bool xl6_xrt_vfs_unmount(void*);
XL6_TEXT_API uint64_t xl6_xrt_vfs_mount_id(void*);
XL6_TEXT_API void* xl6_xrt_vfs_memory_create(void);
XL6_TEXT_API void xl6_xrt_vfs_memory_destroy(void*);
XL6_TEXT_API bool xl6_xrt_vfs_memory_put_bytes(void*,xstrview,const xvalue*,int64_t);
XL6_TEXT_API bool xl6_xrt_vfs_memory_put_string(void*,xstrview,xstrview,int64_t);
XL6_TEXT_API bool xl6_xrt_vfs_memory_seal(void*);
XL6_TEXT_API void* xl6_xrt_vfs_mount_memory(void*,void*,xstrview,int64_t,int64_t);
XL6_TEXT_API void* xl6_xrt_vfs_disk_create(xstrview,int64_t);
XL6_TEXT_API void xl6_xrt_vfs_disk_destroy(void*);
XL6_TEXT_API void* xl6_xrt_vfs_mount_disk(void*,void*,xstrview,int64_t,int64_t);
XL6_TEXT_API void* xl6_xrt_vfs_pack_create(xstrview,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API void xl6_xrt_vfs_pack_destroy(void*);
XL6_TEXT_API int64_t xl6_xrt_vfs_pack_format(void*);
XL6_TEXT_API void* xl6_xrt_vfs_mount_pack(void*,void*,xstrview,int64_t,int64_t);
XL6_TEXT_API void* xl6_xrt_fs_pack_path_create(xstrview,void*,uint64_t,int64_t);
XL6_TEXT_API void* xl6_xrt_fs_pack_take_create(void*,void*,uint64_t,int64_t);
XL6_TEXT_API void* xl6_xrt_fs_pack_options_create(int64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
XL6_TEXT_API void xl6_xrt_fs_pack_options_destroy(void*);
XL6_TEXT_API uint64_t xl6_xrt_fs_pack_options_value(void*,int64_t);
XL6_TEXT_API void* xl6_xrt_fs_pack_stats_create(void*);
XL6_TEXT_API void xl6_xrt_fs_pack_stats_destroy(void*);
XL6_TEXT_API uint64_t xl6_xrt_fs_pack_stats_value(void*,int64_t);
XL6_TEXT_API void* xl6_xrt_fs_vfs_open(void*,xstrview,int64_t,int64_t,int64_t);
XL6_TEXT_API void* xl6_xrt_fs_info_vfs(void*,xstrview,bool);
XL6_TEXT_API void* xl6_xrt_fs_vfs_dir_open(void*,xstrview,int64_t);
XL6_TEXT_API void xl6_xrt_fs_info_destroy(void*);
XL6_TEXT_API void xl6_xrt_fs_dir_destroy(void*);
/* Existing DynamicFields class: resident private C linkage. Names are exact
 * byte spans; adoption of a raw point transfers one native strong owner. */
XL6_TEXT_API void* xl6_xrt_dynamic_fields_create(void);
XL6_TEXT_API void* xl6_xrt_dynamic_fields_wrap(void*);
XL6_TEXT_API void* xl6_xrt_dynamic_fields_from_value(const xvalue*);
XL6_TEXT_API void xl6_xrt_dynamic_fields_destroy(void*);
XL6_TEXT_API void* xl6_xrt_dynamic_fields_native_handle(void*);
XL6_TEXT_API int64_t xl6_xrt_dynamic_fields_count(void*);
XL6_TEXT_API int64_t xl6_xrt_dynamic_fields_capacity(void*);
XL6_TEXT_API bool xl6_xrt_dynamic_fields_reserve(void*, int64_t);
XL6_TEXT_API bool xl6_xrt_dynamic_fields_trim(void*);
XL6_TEXT_API bool xl6_xrt_dynamic_fields_clear(void*);
XL6_TEXT_API bool xl6_xrt_dynamic_fields_has(void*, xstrview);
XL6_TEXT_API xvalue* xl6_xrt_dynamic_fields_get(void*, xstrview);
XL6_TEXT_API xvalue* xl6_xrt_dynamic_fields_copy(void*, xstrview);
XL6_TEXT_API bool xl6_xrt_dynamic_fields_set(void*, xstrview, const xvalue*);
XL6_TEXT_API bool xl6_xrt_dynamic_fields_remove(void*, xstrview);
XL6_TEXT_API xvalue* xl6_xrt_dynamic_fields_take(void*, xstrview);
XL6_TEXT_API xvalue* xl6_xrt_dynamic_fields_keys(void*);
XL6_TEXT_API xvalue* xl6_xrt_dynamic_fields_values(void*);
XL6_TEXT_API xvalue* xl6_xrt_dynamic_fields_items(void*);
XL6_TEXT_API xvalue* xl6_xrt_dynamic_fields_to_value(void*);
XL6_TEXT_API bool xl6_xrt_dynamic_fields_merge(void*, void*, bool);
XL6_TEXT_API void* xl6_xrt_dynamic_fields_clone(void*);
XL6_TEXT_API const struct XL6_NativeHandlePolicyV1* xl6DynamicFieldsHandlePolicyV1Get(void);
/* Immutable, allocation-free physical descriptors, selected independently.
 * They encode null presence, not raw padding or source-language TypeIds. */
XL6_TEXT_API const xrttype* xl6NullableBoolTypeV1(void);
XL6_TEXT_API const xrttype* xl6NullableI64TypeV1(void);
XL6_TEXT_API const xrttype* xl6NullableU64TypeV1(void);
XL6_TEXT_API const xrttype* xl6NullableF32TypeV1(void);
XL6_TEXT_API const xrttype* xl6NullableF64TypeV1(void);
XL6_TEXT_API const xrttype* xl6NullableTimeTypeV1(void);
XL6_TEXT_API const xrttype* xl6NullableCharTypeV1(void);
XL6_TEXT_API const xrttype* xl6NullableNativeI8TypeV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableNativeI8ConverterV1(void);
XL6_TEXT_API const xrttype* xl6NullableNativeU8TypeV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableNativeU8ConverterV1(void);
XL6_TEXT_API const xrttype* xl6NullableNativeI16TypeV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableNativeI16ConverterV1(void);
XL6_TEXT_API const xrttype* xl6NullableNativeU16TypeV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableNativeU16ConverterV1(void);
XL6_TEXT_API const xrttype* xl6NullableNativeI32TypeV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableNativeI32ConverterV1(void);
XL6_TEXT_API const xrttype* xl6NullableNativeU32TypeV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableNativeU32ConverterV1(void);
XL6_TEXT_API const xrttype* xl6NullableStringTypeV1(void);
XL6_TEXT_API const xrttype* xl6StringTypeV1(void);
XL6_TEXT_API const xrttype* xl6CharTypeV1(void);
XL6_TEXT_API const xvalueconverter* xl6CharConverterV1(void);
XL6_TEXT_API const xvalueconverter* xl6StringConverterV1(void);
/* Exact-length dynamic string addition; NULL is failure, never null data. */
XL6_TEXT_API xvalue* xl6ValueStringConcatV1(const xvalue*, const xvalue*);
XL6_TEXT_API bool xl6NullableStringEqualV1(xstrview, xstrview);
XL6_TEXT_API const xvalueconverter* xl6NullableStringConverterV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableBoolConverterV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableI64ConverterV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableU64ConverterV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableF32ConverterV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableF64ConverterV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableTimeConverterV1(void);
XL6_TEXT_API const xvalueconverter* xl6NullableCharConverterV1(void);
/* Exact Buffer value/range adapters; raw payload remains an XRT xbuffer. */
XL6_TEXT_API void* xl6BufferCreateV1(void);
XL6_TEXT_API void* xl6BufferCreateReserveV1(int64_t capacity);
XL6_TEXT_API void* xl6BufferAdoptV1(void* buffer);
XL6_TEXT_API void xl6BufferDestroyV1(void* buffer);
XL6_TEXT_API int64_t xl6BufferLengthV1(void* buffer);
XL6_TEXT_API int64_t xl6BufferCapacityV1(void* buffer);
XL6_TEXT_API bool xl6BufferReserveV1(void* buffer, int64_t capacity);
XL6_TEXT_API bool xl6BufferClearV1(void* buffer);
XL6_TEXT_API bool xl6BufferAppendRawV1(void* buffer, void* data, int64_t size);
XL6_TEXT_API bool xl6BufferInsertRawV1(void* buffer, int64_t offset, void* data, int64_t size);
XL6_TEXT_API bool xl6BufferAppendByteV1(void* buffer, int64_t value);
XL6_TEXT_API int64_t xl6BufferByteAtV1(void* buffer, int64_t index);
XL6_TEXT_API bool xl6BufferSetByteV1(void* buffer, int64_t index, int64_t value);
XL6_TEXT_API bool xl6BufferCopyFromV1(void* buffer, int64_t offset, void* source, int64_t sourceOffset, int64_t count);
XL6_TEXT_API bool xl6BufferCopyWithinV1(void* buffer, int64_t offset, int64_t sourceOffset, int64_t count);
XL6_TEXT_API bool xl6BufferFillV1(void* buffer, int64_t value, int64_t offset, int64_t count);
XL6_TEXT_API int64_t xl6BufferCompareV1(void* buffer, void* other, int64_t size, int64_t otherSize);
XL6_TEXT_API int64_t xl6BufferIndexOfV1(void* buffer, void* needle, int64_t start, int64_t size, int64_t needleSize);
XL6_TEXT_API void* xl6BufferDataV1(void* buffer);
XL6_TEXT_API void* xl6BufferHandleV1(void* buffer);
XL6_TEXT_API void* xl6BufferFromTextV1(xstrview text, int64_t size);
XL6_TEXT_API void* xl6BufferFromBytesV1(const xvalue* bytes, int64_t size);
XL6_TEXT_API void* xl6BufferCopyV1(void* source, int64_t size);
XL6_TEXT_API bool xl6BufferResizeV1(void* buffer, int64_t size, int64_t fill);
XL6_TEXT_API bool xl6BufferTrimV1(void* buffer);
XL6_TEXT_API bool xl6BufferAssignTextV1(void* buffer, xstrview text, int64_t size);
XL6_TEXT_API bool xl6BufferAssignBytesV1(void* buffer, const xvalue* bytes, int64_t size);
XL6_TEXT_API bool xl6BufferAssignBufferV1(void* buffer, void* source, int64_t size);
XL6_TEXT_API bool xl6BufferAppendTextV1(void* buffer, xstrview text, int64_t size);
XL6_TEXT_API bool xl6BufferAppendBytesV1(void* buffer, const xvalue* bytes, int64_t size);
XL6_TEXT_API bool xl6BufferAppendBufferV1(void* buffer, void* source, int64_t size);
XL6_TEXT_API bool xl6BufferInsertTextV1(void* buffer, int64_t offset, xstrview text, int64_t size);
XL6_TEXT_API bool xl6BufferInsertBytesV1(void* buffer, int64_t offset, const xvalue* bytes, int64_t size);
XL6_TEXT_API bool xl6BufferInsertBufferV1(void* buffer, int64_t offset, void* source, int64_t size);
XL6_TEXT_API bool xl6BufferRemoveV1(void* buffer, int64_t offset, int64_t count);
XL6_TEXT_API xvalue* xl6BufferBytesV1(void* buffer, int64_t size);
XL6_TEXT_API xstrview xl6BufferTextV1(void* buffer, int64_t size);
XL6_TEXT_API void* xl6BufferFromHexV1(xstrview text, int64_t flags, int64_t size);
XL6_TEXT_API xstrview xl6BufferHexV1(void* buffer, int64_t flags, int64_t size);
XL6_TEXT_API void* xl6BufferFromBase64V1(xstrview text, int64_t flags, xstrview alphabet, int64_t size, int64_t alphabetSize);
XL6_TEXT_API xstrview xl6BufferBase64V1(void* buffer, int64_t flags, xstrview alphabet, int64_t size, int64_t alphabetSize);
XL6_TEXT_API void* xl6BufferIteratorCreateV1(void* source, int64_t size);
XL6_TEXT_API void xl6BufferIteratorDestroyV1(void* iterator);
XL6_TEXT_API bool xl6BufferIteratorHasNextV1(void* iterator);
XL6_TEXT_API int64_t xl6BufferIteratorNextV1(void* iterator);
XL6_TEXT_API void xl6BufferIteratorCloseV1(void* iterator);
XL6_TEXT_API bool xl6BufferIteratorClosedV1(void* iterator);
/* Pipeline configuration snapshots and independently owned results. */
XL6_TEXT_API void* xl6ProcessPipelineCreateV1(void);
XL6_TEXT_API void xl6ProcessPipelineDestroyV1(void* pipeline);
XL6_TEXT_API void xl6ProcessPipelineAddV1(void* pipeline,void* command);
XL6_TEXT_API void xl6ProcessPipelineClearV1(void* pipeline);
XL6_TEXT_API int64_t xl6ProcessPipelineCountV1(void* pipeline);
XL6_TEXT_API void* xl6ProcessPipelineRunV1(void* pipeline,void* options,const xvalue* input,int64_t size);
XL6_TEXT_API void xl6PipelineResultDestroyV1(void* result);
XL6_TEXT_API bool xl6PipelineResultSuccessV1(void* result);
XL6_TEXT_API bool xl6PipelineResultTimedOutV1(void* result);
XL6_TEXT_API bool xl6PipelineResultCancelledV1(void* result);
XL6_TEXT_API int64_t xl6PipelineResultWaitV1(void* result);
XL6_TEXT_API int64_t xl6PipelineResultCountV1(void* result);
XL6_TEXT_API int64_t xl6PipelineResultCompletedV1(void* result);
XL6_TEXT_API int64_t xl6PipelineResultFailedIndexV1(void* result);
XL6_TEXT_API int64_t xl6PipelineResultExitCodeV1(void* result);
XL6_TEXT_API uint64_t xl6PipelineResultInputWrittenV1(void* result);
XL6_TEXT_API uint64_t xl6PipelineResultDurationV1(void* result);
XL6_TEXT_API bool xl6PipelineResultStdoutTruncatedV1(void* result);
XL6_TEXT_API xvalue* xl6PipelineResultStdoutV1(void* result,int64_t size);
XL6_TEXT_API xstrview xl6PipelineResultStdoutTextV1(void* result,int64_t size);
XL6_TEXT_API void* xl6PipelineResultStageV1(void* result,int64_t index);
XL6_TEXT_API void xl6ProcessStageDestroyV1(void* stage);
XL6_TEXT_API bool xl6ProcessStageSuccessV1(void* stage);
XL6_TEXT_API int64_t xl6ProcessStageExitCodeV1(void* stage);
XL6_TEXT_API bool xl6ProcessStageStderrTruncatedV1(void* stage);
XL6_TEXT_API xvalue* xl6ProcessStageStderrV1(void* stage,int64_t size);
XL6_TEXT_API xstrview xl6ProcessStageStderrTextV1(void* stage,int64_t size);
XL6_TEXT_API void* xl6ProcessStageStatusV1(void* stage);
/* Checked process resources and exact input/output views. */
XL6_TEXT_API void* xl6ProcessSpawnV1(void* command);
XL6_TEXT_API void xl6ProcessDestroyV1(void* process);
XL6_TEXT_API uint64_t xl6ProcessIdV1(void* process);
XL6_TEXT_API int64_t xl6ProcessStateV1(void* process);
XL6_TEXT_API bool xl6ProcessRunningV1(void* process);
XL6_TEXT_API bool xl6ProcessCloseV1(void* process, int64_t stream);
XL6_TEXT_API bool xl6ProcessResizeV1(void* process, int64_t columns, int64_t rows);
XL6_TEXT_API bool xl6ProcessInterruptV1(void* process);
XL6_TEXT_API bool xl6ProcessTerminateV1(void* process);
XL6_TEXT_API bool xl6ProcessKillV1(void* process);
XL6_TEXT_API bool xl6ProcessKillTreeV1(void* process);
XL6_TEXT_API bool xl6ProcessWaitReadyV1(void* process, int64_t timeoutMs);
XL6_TEXT_API struct xfuture* xl6ProcessWaitReadyAsyncV1(void* process);
XL6_TEXT_API void* xl6ProcessLiveStatusV1(void* process);
XL6_TEXT_API void* xl6ProcessReaderV1(void* process, int64_t stream);
XL6_TEXT_API void* xl6ProcessWriterV1(void* process);
XL6_TEXT_API bool xl6ProcessOpenV1(xstrview target, int64_t size);
XL6_TEXT_API bool xl6ProcessTerminalSupportedV1(void);
XL6_TEXT_API void* xl6ProcessCommandDirectV1(xstrview program, const xvalue* args, int64_t size, int64_t argSize);
XL6_TEXT_API void* xl6ProcessCommandShellV1(xstrview p0, int64_t p1);
XL6_TEXT_API void xl6ProcessCommandDestroyV1(void* p0);
XL6_TEXT_API void xl6ProcessCommandWorkDirV1(void* p0, xstrview p1, int64_t p2);
XL6_TEXT_API void xl6ProcessCommandArg0V1(void* p0, xstrview p1, int64_t p2);
XL6_TEXT_API void xl6ProcessCommandRemoveEnvV1(void* p0, xstrview p1, int64_t p2);
XL6_TEXT_API void xl6ProcessCommandEnvV1(void* p0, xstrview p1, xstrview p2, int64_t p3, int64_t p4);
XL6_TEXT_API void xl6ProcessCommandClearEnvV1(void* p0);
XL6_TEXT_API void xl6ProcessCommandInheritEnvV1(void* p0, bool p1);
XL6_TEXT_API void xl6ProcessCommandNewConsoleV1(void* p0, bool p1);
XL6_TEXT_API void xl6ProcessCommandProcessGroupV1(void* p0, bool p1);
XL6_TEXT_API void xl6ProcessCommandHideWindowV1(void* p0, bool p1);
XL6_TEXT_API void xl6ProcessCommandTerminalV1(void* p0, bool p1);
XL6_TEXT_API void xl6ProcessCommandTerminalSizeV1(void* p0, int64_t p1, int64_t p2);
XL6_TEXT_API void xl6ProcessCommandStdioV1(void* p0, int64_t p1, int64_t p2);
XL6_TEXT_API void xl6ProcessCommandFileV1(void* command, int64_t stream, void* file);
XL6_TEXT_API void* xl6ProcessRunOptionsCreateV1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, uint64_t p5);
XL6_TEXT_API void xl6ProcessRunOptionsDestroyV1(void* p0);
XL6_TEXT_API bool xl6ProcessRunOptionsCancelV1(void* p0);
XL6_TEXT_API bool xl6ProcessRunOptionsRequestedV1(void* p0);
XL6_TEXT_API void* xl6ProcessRunV1(void* command, void* options, const xvalue* input, int64_t size);
XL6_TEXT_API void xl6ProcessResultDestroyV1(void* p0);
XL6_TEXT_API bool xl6ProcessResultSuccessV1(void* p0);
XL6_TEXT_API bool xl6ProcessResultTimedOutV1(void* p0);
XL6_TEXT_API bool xl6ProcessResultCancelledV1(void* p0);
XL6_TEXT_API int64_t xl6ProcessResultWaitV1(void* p0);
XL6_TEXT_API uint64_t xl6ProcessResultInputWrittenV1(void* p0);
XL6_TEXT_API uint64_t xl6ProcessResultDurationV1(void* p0);
XL6_TEXT_API bool xl6ProcessResultStdoutTruncatedV1(void* p0);
XL6_TEXT_API bool xl6ProcessResultStderrTruncatedV1(void* p0);
XL6_TEXT_API int64_t xl6ProcessResultExitCodeV1(void* p0);
XL6_TEXT_API xvalue* xl6ProcessResultStdoutV1(void* p0, int64_t p1);
XL6_TEXT_API xvalue* xl6ProcessResultStderrV1(void* p0, int64_t p1);
XL6_TEXT_API xstrview xl6ProcessResultStdoutTextV1(void* p0, int64_t p1);
XL6_TEXT_API xstrview xl6ProcessResultStderrTextV1(void* p0, int64_t p1);
XL6_TEXT_API void* xl6ProcessResultStatusV1(void* p0);
XL6_TEXT_API void xl6ProcessStatusDestroyV1(void* p0);
XL6_TEXT_API int64_t xl6ProcessStatusKindV1(void* p0);
XL6_TEXT_API int64_t xl6ProcessStatusCodeV1(void* p0);
XL6_TEXT_API int64_t xl6ProcessStatusSignalV1(void* p0);
XL6_TEXT_API int64_t xl6ProcessStatusStopV1(void* p0);
XL6_TEXT_API bool xl6ProcessStatusCoreDumpedV1(void* p0);
/* Scalar signal foundation only. These are checked calls: lazy native
 * initialization can fail; restore/shutdown can wait for native callbacks.
 * name owns its exact result; count retains the unsigned native total. */
XL6_TEXT_API bool xl6SignalSupportedV1(int64_t code);
XL6_TEXT_API xstrview xl6SignalNameV1(int64_t code);
XL6_TEXT_API bool xl6SignalIgnoreV1(int64_t code);
XL6_TEXT_API bool xl6SignalRestoreV1(int64_t code);
XL6_TEXT_API bool xl6SignalRestoreAllV1(void);
XL6_TEXT_API bool xl6SignalRaiseV1(int64_t code);
XL6_TEXT_API uint64_t xl6SignalCountV1(int64_t code);
XL6_TEXT_API bool xl6SignalReceivedV1(int64_t code);
XL6_TEXT_API void xl6SignalClearV1(int64_t code);
XL6_TEXT_API bool xl6SignalHealthyV1(void);
XL6_TEXT_API void xl6SignalResetV1(void);
/* Owner-thread queue operations; immutable event snapshots own their storage.
 * Only the recorder runs on XRT's dispatcher, never a language callback. */
XL6_TEXT_API void* xl6SignalQueueCreateV1(int64_t code, bool once, int64_t capacity);
XL6_TEXT_API void xl6SignalQueueDestroyV1(void* queue);
XL6_TEXT_API void xl6SignalQueueCheckOwnerV1(void* queue);
XL6_TEXT_API bool xl6SignalQueueCloseV1(void* queue);
XL6_TEXT_API bool xl6SignalQueueActiveV1(void* queue);
XL6_TEXT_API int64_t xl6SignalQueueCodeV1(void* queue);
XL6_TEXT_API int64_t xl6SignalQueuePendingV1(void* queue);
XL6_TEXT_API bool xl6SignalQueueClosedV1(void* queue);
XL6_TEXT_API bool xl6SignalQueuePrepareV1(void* queue, int64_t timeoutMs);
XL6_TEXT_API void* xl6SignalQueueSnapshotV1(void* queue);
XL6_TEXT_API void xl6SignalQueueConsumeV1(void* queue);
XL6_TEXT_API void xl6SignalEventDestroyV1(void* event);
XL6_TEXT_API int64_t xl6SignalEventCodeV1(void* event);
XL6_TEXT_API int64_t xl6SignalEventSystemCodeV1(void* event);
XL6_TEXT_API uint64_t xl6SignalEventCountV1(void* event);
XL6_TEXT_API uint64_t xl6SignalEventTotalV1(void* event);
XL6_TEXT_API int64_t xl6SignalEventTimeV1(void* event);
XL6_TEXT_API bool xl6SignalEventCoalescedV1(void* event);
XL6_TEXT_API bool xl6SignalEventSaturatedV1(void* event);
XL6_TEXT_API xstrview xl6SignalEventNameV1(void* event);
XL6_TEXT_API void* xl6SignalTaskGroupCreateV1(void);
XL6_TEXT_API void xl6SignalTaskGroupDestroyV1(void* group);
XL6_TEXT_API bool xl6SignalTaskGroupCloseV1(void* group);
XL6_TEXT_API bool xl6SignalTaskGroupCancelV1(void* group);
XL6_TEXT_API void* xl6_xrt_task_group_wrap(void* group);
XL6_TEXT_API void* xl6_xrt_task_group_child(void* group);
XL6_TEXT_API bool xl6_xrt_task_group_add(void* group,const xvalue* future);
XL6_TEXT_API bool xl6_xrt_task_group_bind_parent(void* group,const xvalue* future);
XL6_TEXT_API int xl6_xrt_task_group_count(void* group);
XL6_TEXT_API int xl6_xrt_task_group_reap(void* group);
XL6_TEXT_API bool xl6_xrt_task_group_join(void* group);
XL6_TEXT_API bool xl6_xrt_task_group_join_timeout(void* group,int64_t milliseconds);
XL6_TEXT_API bool xl6_xrt_task_group_join_until(void* group,int64_t milliseconds);
XL6_TEXT_API bool xl6_xrt_task_group_wait(void* group);
XL6_TEXT_API bool xl6_xrt_task_group_wait_timeout(void* group,int64_t milliseconds);
XL6_TEXT_API bool xl6_xrt_task_group_wait_until(void* group,int64_t milliseconds);
XL6_TEXT_API xfuture* xl6_xrt_task_group_join_async(void* group);
XL6_TEXT_API xstrview xl6EnvGetV1(xstrview name, int64_t size);
XL6_TEXT_API bool xl6EnvHasV1(xstrview name, int64_t size);
XL6_TEXT_API void xl6EnvSetV1(xstrview name, xstrview value, int64_t nameSize, int64_t valueSize);
XL6_TEXT_API void xl6EnvRemoveV1(xstrview name, int64_t size);
XL6_TEXT_API xstrview xl6TimeFormatV1(int64_t value, xstrview pattern, int64_t offset, int64_t size);
XL6_TEXT_API xstrview xl6TimeRFC3339V1(int64_t value, int64_t offset);
XL6_TEXT_API xstrview xl6TimeHTTPDateV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeParsePatternV1(xstrview text, xstrview pattern, int64_t size, int64_t patternSize);
XL6_TEXT_API XL6_ProductBoolTimeV1 xl6TimeTryParsePatternV1(xstrview text, xstrview pattern, int64_t size, int64_t patternSize);
XL6_TEXT_API int64_t xl6TimeParseAnyV1(xstrview text, int64_t size);
XL6_TEXT_API XL6_ProductBoolTimeV1 xl6TimeTryParseAnyV1(xstrview text, int64_t size);
XL6_TEXT_API int64_t xl6TimeParseRFC3339V1(xstrview text, int64_t size);
XL6_TEXT_API XL6_ProductBoolTimeV1 xl6TimeTryParseRFC3339V1(xstrview text, int64_t size);
XL6_TEXT_API int64_t xl6TimeParseHTTPDateV1(xstrview text, int64_t size);
XL6_TEXT_API XL6_ProductBoolTimeV1 xl6TimeTryParseHTTPDateV1(xstrview text, int64_t size);
XL6_TEXT_API void* xl6DateTimeParseV1(xstrview text, xstrview pattern, int64_t size, int64_t patternSize);
XL6_TEXT_API xstrview xl6DateTimeFormatV1(void* handle, xstrview pattern, int64_t size);
XL6_TEXT_API int64_t xl6TimeYearV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeMonthV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeDayV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeHourV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeMinuteV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeSecondV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeWeekdayV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeDayOfYearV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeQuarterV1(int64_t value);
XL6_TEXT_API bool xl6TimeLeapV1(int64_t year);
XL6_TEXT_API int64_t xl6TimeDaysInYearV1(int64_t year);
XL6_TEXT_API int64_t xl6TimeDaysInMonthV1(int64_t year, int64_t month);
XL6_TEXT_API int64_t xl6TimeDatePartV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeTimePartV1(int64_t value);
XL6_TEXT_API bool xl6TimeSameDayV1(int64_t left, int64_t right);
XL6_TEXT_API bool xl6TimeSameMonthV1(int64_t left, int64_t right);
XL6_TEXT_API bool xl6TimeSameYearV1(int64_t left, int64_t right);
XL6_TEXT_API bool xl6TimeInV1(int64_t value, int64_t start, int64_t end);
XL6_TEXT_API bool xl6TimeOverlapV1(int64_t start1, int64_t end1, int64_t start2, int64_t end2);
XL6_TEXT_API double xl6TimeTimerV1(void);
XL6_TEXT_API void xl6TimeSleepV1(int64_t milliseconds);
XL6_TEXT_API XL6_ProductTimeX2V1 xl6TimeMonthRangeV1(int64_t value);
XL6_TEXT_API XL6_ProductTimeX2V1 xl6TimeYearRangeV1(int64_t value);
XL6_TEXT_API XL6_ProductTimeX2V1 xl6TimeWeekRangeV1(int64_t value, int64_t firstWeekday);
XL6_TEXT_API XL6_ProductI64x3V1 xl6TimeISOWeekV1(int64_t value);
XL6_TEXT_API void* xl6DateTimeSplitV1(int64_t value, int64_t offset);
XL6_TEXT_API void* xl6DateTimeFieldsV1(int64_t year, int64_t month, int64_t day, int64_t hour, int64_t minute, int64_t second, int64_t microsecond, int64_t offset);
XL6_TEXT_API void xl6DateTimeDestroyV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeYearV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeMonthV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeDayV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeHourV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeMinuteV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeSecondV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeMicrosecondV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeOffsetV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeWeekdayV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeDayOfYearV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeIsDSTV1(void* handle);
XL6_TEXT_API int64_t xl6DateTimeToTimeV1(void* handle);
XL6_TEXT_API void* xl6DateTimeLocalV1(int64_t value, bool local);
XL6_TEXT_API int64_t xl6TimeFromLocalV1(void* handle, int64_t fold);
XL6_TEXT_API int64_t xl6TimeTimezoneOffsetV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeFromDateTimeV1(int64_t year,int64_t month,int64_t day,int64_t hour,int64_t minute,int64_t second,int64_t microsecond,int64_t offset);
XL6_TEXT_API int64_t xl6TimeFromDateV1(int64_t year,int64_t month,int64_t day);
XL6_TEXT_API int64_t xl6TimeFromTimeV1(int64_t hour,int64_t minute,int64_t second,int64_t microsecond);
XL6_TEXT_API int64_t xl6TimeFromUnixV1(int64_t seconds);
XL6_TEXT_API int64_t xl6TimeFromUnixMsV1(int64_t milliseconds);
XL6_TEXT_API int64_t xl6TimeUnixV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeUnixMsV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeMicrosecondV1(int64_t value);
XL6_TEXT_API int64_t xl6TimeNowV1(void);
XL6_TEXT_API uint64_t xl6TimeMonotonicV1(void);
XL6_TEXT_API void xl6TimeSleepUsV1(uint64_t microseconds);
XL6_TEXT_API void xl6TimeSleepUntilV1(uint64_t deadline);
XL6_TEXT_API bool xl6TimeNearV1(int64_t left,int64_t right,uint64_t tolerance);
XL6_TEXT_API int64_t xl6TimeAddV1(int64_t value,int64_t amount,int64_t unit);
XL6_TEXT_API int64_t xl6TimeDiffV1(int64_t start,int64_t end,int64_t unit);
XL6_TEXT_API void* xl6PatternOptionsCreateV1(xstrview separators,int64_t bytes,int64_t patterns,int64_t captures,int64_t states,int64_t compiled,int64_t size);
XL6_TEXT_API void xl6PatternOptionsDestroyV1(void* handle);
XL6_TEXT_API xstrview xl6PatternOptionsSeparatorsV1(void* handle);
XL6_TEXT_API int64_t xl6PatternOptionsBytesV1(void* handle);
XL6_TEXT_API int64_t xl6PatternOptionsPatternsV1(void* handle);
XL6_TEXT_API int64_t xl6PatternOptionsCapturesV1(void* handle);
XL6_TEXT_API int64_t xl6PatternOptionsStatesV1(void* handle);
XL6_TEXT_API int64_t xl6PatternOptionsCompiledV1(void* handle);
XL6_TEXT_API void* xl6PatternCreateV1(xstrview text,int64_t size);
XL6_TEXT_API void* xl6PatternCreateOptionsV1(void* options,xstrview text,int64_t size);
XL6_TEXT_API void xl6PatternDestroyV1(void* handle);
XL6_TEXT_API int64_t xl6PatternCountV1(void* handle);
XL6_TEXT_API int64_t xl6PatternCompiledBytesV1(void* handle);
XL6_TEXT_API xstrview xl6PatternSeparatorsV1(void* handle);
XL6_TEXT_API xstrview xl6PatternSourceV1(void* handle,int64_t index);
XL6_TEXT_API uint64_t xl6PatternIdV1(void* handle,int64_t index);
XL6_TEXT_API int64_t xl6PatternTagKeyV1(void* handle,int64_t index);
XL6_TEXT_API int64_t xl6PatternCaptureCountV1(void* handle,int64_t index);
XL6_TEXT_API xstrview xl6PatternCaptureNameV1(void* handle,int64_t capture,int64_t index);
XL6_TEXT_API int64_t xl6PatternCaptureIndexV1(void* handle,xstrview name,int64_t index,int64_t size);
XL6_TEXT_API int64_t xl6PatternFirstV1(void* handle,xstrview text,int64_t size);
XL6_TEXT_API bool xl6PatternTestV1(void* handle,xstrview text,int64_t size);
XL6_TEXT_API int64_t xl6PatternErrorOffsetV1(const xerror* error);
XL6_TEXT_API int64_t xl6PatternErrorIndexV1(const xerror* error);
XL6_TEXT_API void* xl6PatternMatchCreateV1(void* handle,xstrview text,int64_t size);
XL6_TEXT_API void xl6PatternMatchDestroyV1(void* handle);
XL6_TEXT_API bool xl6PatternMatchMatchedV1(void* handle);
XL6_TEXT_API int64_t xl6PatternMatchCountV1(void* handle);
XL6_TEXT_API int64_t xl6PatternMatchPatternIndexV1(void* handle);
XL6_TEXT_API uint64_t xl6PatternMatchIdV1(void* handle);
XL6_TEXT_API xstrview xl6PatternMatchInputV1(void* handle);
XL6_TEXT_API xstrview xl6PatternMatchSourceV1(void* handle);
XL6_TEXT_API xstrview xl6PatternMatchTextV1(void* handle,int64_t index);
XL6_TEXT_API int64_t xl6PatternMatchStartV1(void* handle,int64_t index);
XL6_TEXT_API int64_t xl6PatternMatchEndV1(void* handle,int64_t index);
XL6_TEXT_API xstrview xl6PatternMatchNameV1(void* handle,int64_t index);
XL6_TEXT_API int64_t xl6PatternMatchIndexV1(void* handle,xstrview name,int64_t size);
XL6_TEXT_API void* xl6PatternBuilderCreateV1(void);
XL6_TEXT_API void* xl6PatternBuilderOptionsV1(void* options);
XL6_TEXT_API void xl6PatternBuilderDestroyV1(void* handle);
XL6_TEXT_API int64_t xl6PatternBuilderCountV1(void* handle);
XL6_TEXT_API uint64_t xl6PatternBuilderVersionV1(void* handle);
XL6_TEXT_API xvalue* xl6PatternBuilderIdentityV1(void* handle);
XL6_TEXT_API xvalue* xl6PatternStateCreateV1(const xvalue* identity);
XL6_TEXT_API void xl6PatternBuilderReserveV1(void* handle,int64_t capacity);
XL6_TEXT_API bool xl6PatternBuilderContainsV1(void* handle,void* key);
XL6_TEXT_API void* xl6PatternBuilderCompileV1(void* handle);
XL6_TEXT_API xvalue* xl6PatternBuilderTagsV1(void* handle,const xvalue* state);
XL6_TEXT_API void* xl6PatternEditAddV1(void* handle,xstrview expression,int64_t priority,int64_t size);
XL6_TEXT_API void* xl6PatternEditAddManyV1(void* handle,const xvalue* expressions,const xvalue* priorities,const xvalue* sizes);
XL6_TEXT_API void* xl6PatternEditSetV1(void* handle,void* key,xstrview expression,int64_t priority,int64_t size);
XL6_TEXT_API void* xl6PatternEditRemoveV1(void* handle,void* key);
XL6_TEXT_API void* xl6PatternEditClearV1(void* handle);
XL6_TEXT_API void xl6PatternEditDestroyV1(void* handle);
XL6_TEXT_API int64_t xl6PatternEditCountV1(void* handle);
XL6_TEXT_API void xl6PatternEditCancelV1(void* handle);
XL6_TEXT_API void xl6PatternEditApplyV1(void* handle,xvalue* state,const xvalue* payload);
XL6_TEXT_API void xl6PatternEditApplyManyV1(void* handle,xvalue* state,const xvalue* payload);
XL6_TEXT_API void* xl6PatternKeyCreateV1(void* handle,int64_t index);
XL6_TEXT_API void xl6PatternKeyDestroyV1(void* handle);
XL6_TEXT_API uint64_t xl6PatternKeyIdV1(void* handle);
XL6_TEXT_API void* xl6RegexReplacerCreateV1(void* regex,xstrview text,int64_t limit,int64_t size);
XL6_TEXT_API void xl6RegexReplacerDestroyV1(void* handle);
XL6_TEXT_API void* xl6RegexReplacerSnapshotV1(void* handle);
XL6_TEXT_API xstrview xl6RegexReplacerRunV1(void* handle,struct xlir_closure* callback);
/* Internal resident dependency, never a language declaration. */
XL6_TEXT_API bool xl6RegexReplacerRunOutputV1(void* handle,struct xlir_closure* callback,xstrbuf* output,size_t* count);
XL6_TEXT_API int64_t xl6RegexReplacerRunBufferV1(void* handle,void* target,struct xlir_closure* callback);
XL6_TEXT_API int64_t xl6RegexErrorOffsetV1(const xerror* error);
XL6_TEXT_API int64_t xl6RegexErrorIndexV1(const xerror* error);

/* Inputs borrow exact views for the call; textual operations validate UTF-8.
 * Every successful string result owns an independent XRT allocation, even
 * when empty; free only Data. Value/array/bytes results own one xvalue reference.
 * On failure string returns {NULL,0}, managed values return NULL, and scalar
 * operations return their documented sentinel, with an XRT thread error.
 * No strlen/heap-size inference or library implementation belongs in Cgen. */
XL6_TEXT_API xstrview xl6TextLowerV1(xstrview text, int64_t size);
XL6_TEXT_API xstrview xl6TextUpperV1(xstrview text, int64_t size);
XL6_TEXT_API xstrview xl6TextTrimV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize);
XL6_TEXT_API xstrview xl6TextLTrimV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize);
XL6_TEXT_API xstrview xl6TextRTrimV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize);
XL6_TEXT_API xstrview xl6TextFilterV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize);
XL6_TEXT_API xstrview xl6TextReplaceV1(xstrview text, xstrview part, xstrview replacement,
    int64_t size, int64_t partSize, int64_t replacementSize);
XL6_TEXT_API xstrview xl6TextSliceV1(xstrview text, int64_t start, int64_t count, int64_t size);
XL6_TEXT_API xstrview xl6TextLeftV1(xstrview text, int64_t count, int64_t size);
XL6_TEXT_API xstrview xl6TextRightV1(xstrview text, int64_t count, int64_t size);
XL6_TEXT_API xstrview xl6TextRepeatV1(xstrview text, int64_t count, int64_t size);
XL6_TEXT_API xstrview xl6TextReverseV1(xstrview text, int64_t size);
XL6_TEXT_API xstrview xl6TextRemoveAtV1(xstrview text, int64_t start, int64_t count, int64_t size);
XL6_TEXT_API xstrview xl6TextInsertV1(xstrview text, int64_t index, xstrview part,
    int64_t size, int64_t partSize);
XL6_TEXT_API xstrview xl6TextRemoveV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API xstrview xl6TextPadLeftV1(xstrview text, int64_t width, xstrview pad, int64_t size, int64_t padSize);
XL6_TEXT_API xstrview xl6TextPadRightV1(xstrview text, int64_t width, xstrview pad, int64_t size, int64_t padSize);
XL6_TEXT_API xstrview xl6TextPadCenterV1(xstrview text, int64_t width, xstrview pad, int64_t size, int64_t padSize);
XL6_TEXT_API int64_t xl6TextLengthV1(xstrview text, int64_t size);
XL6_TEXT_API int64_t xl6TextByteLengthV1(xstrview text, int64_t size);
/* Bounded uint8 uses the language's i64 execution ABI, not foreign uint8_t. */
XL6_TEXT_API int64_t xl6TextByteAtV1(xstrview text, int64_t index, int64_t size);
XL6_TEXT_API uint32_t xl6TextCharAtV1(xstrview text, int64_t index, int64_t size);
XL6_TEXT_API xvalue* xl6TextBytesV1(xstrview text, int64_t size);
XL6_TEXT_API xvalue* xl6TextCharsV2(uint64_t ownerIdentity, uint64_t elementIdentity, xstrview text, int64_t size);
XL6_TEXT_API int64_t xl6TextCompareV1(xstrview left, xstrview right, bool ignoreCase, int64_t leftSize, int64_t rightSize);
XL6_TEXT_API bool xl6TextEqualsV1(xstrview left, xstrview right, bool ignoreCase, int64_t leftSize, int64_t rightSize);
XL6_TEXT_API bool xl6TextContainsV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API int64_t xl6TextIndexOfV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API int64_t xl6TextLastIndexOfV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API int64_t xl6TextCountV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API bool xl6TextStartsWithV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API bool xl6TextEndsWithV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API bool xl6TextHasAnyV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize);
XL6_TEXT_API bool xl6TextIsEmptyV1(xstrview text, int64_t size);
XL6_TEXT_API bool xl6TextIsBlankV1(xstrview text, int64_t size);
XL6_TEXT_API bool xl6TextIsUtf8V1(xstrview text, int64_t size);
XL6_TEXT_API bool xl6TextLikeV1(xstrview text, xstrview pattern, bool ignoreCase, int64_t size, int64_t patternSize);
XL6_TEXT_API bool xl6TextGlobV1(xstrview text, xstrview pattern, int64_t flags, int64_t size, int64_t patternSize);
XL6_TEXT_API int64_t xl6TextDistanceV1(xstrview left, xstrview right, int64_t limit, int64_t leftSize, int64_t rightSize);
XL6_TEXT_API double xl6TextSimilarityV1(xstrview left, xstrview right, int64_t leftSize, int64_t rightSize);
XL6_TEXT_API bool xl6TextApproxV1(xstrview left, xstrview right, double tolerance, int64_t leftSize, int64_t rightSize);
/* Arrays use the current xvalue carrier; every element is exactly STRING.
 * Split outputs own each item; join borrows input elements only for the call. */
XL6_TEXT_API xvalue* xl6TextSplitV2(uint64_t ownerIdentity, uint64_t elementIdentity, xstrview text, xstrview separator, int64_t size, int64_t separatorSize);
XL6_TEXT_API xvalue* xl6TextLinesV2(uint64_t ownerIdentity, uint64_t elementIdentity, xstrview text, int64_t size);
XL6_TEXT_API xvalue* xl6TextFieldsV2(uint64_t ownerIdentity, uint64_t elementIdentity, xstrview text, int64_t size);
XL6_TEXT_API xstrview xl6TextJoinV1(const xvalue* values, xstrview separator, int64_t separatorSize, int64_t itemSize);
/* Thread-affine owned input snapshot; mode 0 split / 1 lines / 2 fields.
 * Next owns its output, NULL means EOF unless the thread error is set.
 * Close is idempotent; EOF releases inputs. Destroy is no-raise and preserves
 * an existing error. No raw handle is exposed by the language facade. */
XL6_TEXT_API void* xl6TextIteratorCreateV1(xstrview text, xstrview separator, int64_t mode, int64_t size, int64_t separatorSize);
XL6_TEXT_API void xl6TextIteratorDestroyV1(void* handle);
XL6_TEXT_API xstrview xl6TextIteratorNextV1(void* handle);
XL6_TEXT_API void xl6TextIteratorCloseV1(void* handle);
XL6_TEXT_API bool xl6TextIteratorClosedV1(void* handle);
/* Numeric format is ASCII grammar over an exact byte view, not arbitrary
 * Unicode text. size selects a clamped prefix. Results may contain U+0000. */
XL6_TEXT_API xstrview xl6NumberIntFormatV1(int64_t value, xstrview format, int64_t size);
XL6_TEXT_API xstrview xl6NumberUIntFormatV1(uint64_t value, xstrview format, int64_t size);
XL6_TEXT_API xstrview xl6NumberFloatFormatV1(double value, xstrview format, int64_t size);
XL6_TEXT_API bool xl6NumberBufferIntFormatV1(void* buffer, int64_t value, xstrview format, int64_t size);
XL6_TEXT_API bool xl6NumberBufferUIntFormatV1(void* buffer, uint64_t value, xstrview format, int64_t size);
XL6_TEXT_API bool xl6NumberBufferFloatFormatV1(void* buffer, double value, xstrview format, int64_t size);
/* Scalar parse failures return zero plus an XRT thread error, never a partial
 * value. Small integer types use i64 execution ABI with closed logical widths.
 * Default integer flags=0; float flags=8 preserves explicit NaN/Inf support. */
XL6_TEXT_API int64_t xl6NumberIntParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberInt64ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API uint64_t xl6NumberUIntParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberInt8ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberInt16ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberInt32ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberUInt8ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberUInt16ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberUInt32ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API double xl6NumberFloatParseV1(xstrview text, int64_t flags, int64_t size);
XL6_TEXT_API xstrview xl6NumberIntStringV1(int64_t value, int64_t base, int64_t flags);
XL6_TEXT_API xstrview xl6NumberUIntStringV1(uint64_t value, int64_t base, int64_t flags);
XL6_TEXT_API xstrview xl6NumberFloatStringV1(double value, int64_t flags);
XL6_TEXT_API bool xl6NumberBufferIntStringV1(void* buffer, int64_t value, int64_t base, int64_t flags);
XL6_TEXT_API bool xl6NumberBufferUIntStringV1(void* buffer, uint64_t value, int64_t base, int64_t flags);
XL6_TEXT_API bool xl6NumberBufferFloatStringV1(void* buffer, double value, int64_t flags);
XL6_TEXT_API bool xl6UnicodeScalarV1(int64_t code);
XL6_TEXT_API xstrview xl6UnicodeCharStringV1(uint32_t scalar);
XL6_TEXT_API int64_t xl6UnicodeInvalidTextV1(xstrview text, int64_t size);
XL6_TEXT_API int64_t xl6UnicodeInvalidBytesV1(const xvalue* data, int64_t size);
XL6_TEXT_API int64_t xl6UnicodeByteOffsetV1(xstrview text, int64_t index, int64_t size);
XL6_TEXT_API int64_t xl6UnicodeScalarIndexV1(xstrview text, int64_t offset, int64_t size);
/* Encoding ids 0 unknown / 1 UTF8 / 2 UTF16LE / 3 BE / 4 UTF32LE / 5 BE.
 * Policy 0 strict / 1 replacement. Decode BOM modes: 0 literal, 1 optional
 * matching strip, 2 require matching strip, 3 reject any BOM. Transcode adds
 * bit 4 to emit a target BOM. Caps are bytes, not code units; all results own
 * exact independent data and failed results expose no partial allocation. */
XL6_TEXT_API xvalue* xl6EncodingEncodeV1(xstrview text, int64_t encoding, bool bom, int64_t policy, int64_t size);
XL6_TEXT_API xstrview xl6EncodingDecodeV1(const xvalue* data, int64_t encoding, int64_t policy, int64_t bom, int64_t size);
XL6_TEXT_API xvalue* xl6EncodingTranscodeV1(const xvalue* data, int64_t source, int64_t target, int64_t policy, int64_t bom, int64_t size);
XL6_TEXT_API int64_t xl6EncodingBomV1(const xvalue* data, int64_t size);
XL6_TEXT_API int64_t xl6EncodingBomSizeV1(const xvalue* data, int64_t size);
XL6_TEXT_API xvalue* xl6EncodingBomBytesV1(int64_t encoding);
XL6_TEXT_API int64_t xl6EncodingUnitSizeV1(int64_t encoding);
XL6_TEXT_API xstrview xl6EncodingNameV1(int64_t encoding);
XL6_TEXT_API xstrview xl6EncodingRepairTextV1(xstrview text, int64_t size);
/* Guess is not validation: e0 encoding / e1 BOM bytes / e2 confidence 0..100.
 * One XRT guess, a trivial stack result, no retained input or allocation on
 * valid arguments. A BOM gets confidence 100 even with an invalid body. */
XL6_TEXT_API XL6_ProductI64x3V1 xl6EncodingGuessTextV1(xstrview text, int64_t size);
XL6_TEXT_API XL6_ProductI64x3V1 xl6EncodingGuessBytesV1(const xvalue* data, int64_t size);
/* Validator chunks are synchronous borrows. Data-invalid is an ordinary
 * XUTF_INVALID result, not a raised error; argument/lifetime/overflow faults
 * raise. final seals the stream; reset starts a new stream on its owner thread. */
XL6_TEXT_API void* xl6UnicodeValidatorCreateV1(void);
XL6_TEXT_API void xl6UnicodeValidatorDestroyV1(void* handle);
XL6_TEXT_API int64_t xl6UnicodeValidatorFeedV1(void* handle, const xvalue* data, bool final, int64_t size);
XL6_TEXT_API void xl6UnicodeValidatorResetV1(void* handle);
XL6_TEXT_API int64_t xl6UnicodeValidatorErrorV1(void* handle);
XL6_TEXT_API int64_t xl6UnicodeValidatorTotalV1(void* handle);
XL6_TEXT_API bool xl6UnicodeValidatorFinishedV1(void* handle);
XL6_TEXT_API xstrview xl6HtmlEscapeTextV1(xstrview text, int64_t size);
XL6_TEXT_API xstrview xl6HtmlEscapeAttributeV1(xstrview text, int64_t size);
/* Regex and match snapshots are immutable managed native fields. All strings
 * returned here own exact independent spans. Optional group text NULL means
 * unmatched (an empty matched group instead owns a one-byte allocation).
 * mode 0 find / 1 at / 2 full; offsets are UTF-8 boundaries in bytes. */
XL6_TEXT_API void* xl6RegexCreateV1(xstrview pattern, int64_t flags, int64_t maxPatternBytes, int64_t maxCaptures, int64_t size);
XL6_TEXT_API void xl6RegexDestroyV1(void* handle);
XL6_TEXT_API xstrview xl6RegexPatternV1(void* handle);
XL6_TEXT_API int64_t xl6RegexFlagsV1(void* handle);
XL6_TEXT_API int64_t xl6RegexCaptureCountV1(void* handle);
XL6_TEXT_API xstrview xl6RegexCaptureNameV1(void* handle, int64_t index);
XL6_TEXT_API int64_t xl6RegexCaptureIndexV1(void* handle, xstrview name, int64_t size);
XL6_TEXT_API xstrview xl6RegexEscapeV1(xstrview text, int64_t size);
XL6_TEXT_API void* xl6RegexMatchCreateV1(void* regex, xstrview text, int64_t mode, int64_t start, int64_t size);
XL6_TEXT_API void xl6RegexMatchDestroyV1(void* handle);
XL6_TEXT_API bool xl6RegexTestV1(void* regex, xstrview text, bool full, int64_t size);
XL6_TEXT_API bool xl6RegexMatchMatchedV1(void* handle);
XL6_TEXT_API int64_t xl6RegexMatchCountV1(void* handle);
XL6_TEXT_API xstrview xl6RegexMatchTextV1(void* handle, int64_t index);
XL6_TEXT_API bool xl6RegexMatchGroupMatchedV1(void* handle, int64_t index);
XL6_TEXT_API int64_t xl6RegexMatchStartV1(void* handle, int64_t index);
XL6_TEXT_API int64_t xl6RegexMatchEndV1(void* handle, int64_t index);
XL6_TEXT_API xstrview xl6RegexMatchNameV1(void* handle, int64_t index);
XL6_TEXT_API int64_t xl6RegexMatchIndexV1(void* handle, xstrview name, int64_t size);
XL6_TEXT_API xstrview xl6RegexMatchInputV1(void* handle);
/* Mutable iterators are owner-thread only. Preparation does not consume a
 * pending match/part: the language facade commits only after construction of
 * the managed snapshot succeeds. Matcher execution failure closes the input;
 * snapshot allocation failure leaves the pending item available for retry. */
XL6_TEXT_API void* xl6RegexIteratorCreateV1(void* regex, xstrview text, int64_t start, int64_t limit, int64_t size);
XL6_TEXT_API void xl6RegexIteratorDestroyV1(void* handle);
XL6_TEXT_API bool xl6RegexIteratorHasNextV1(void* handle);
XL6_TEXT_API void* xl6RegexIteratorSnapshotV1(void* handle);
XL6_TEXT_API void xl6RegexIteratorConsumeV1(void* handle);
XL6_TEXT_API void xl6RegexIteratorCloseV1(void* handle);
XL6_TEXT_API bool xl6RegexIteratorClosedV1(void* handle);
XL6_TEXT_API xstrview xl6RegexReplaceV1(void* regex, xstrview text, xstrview replacement, int64_t limit, int64_t size, int64_t replacementSize);
XL6_TEXT_API int64_t xl6RegexReplaceBufferV1(void* regex, void* target, xstrview text, xstrview replacement, int64_t limit, int64_t size, int64_t replacementSize);
XL6_TEXT_API void* xl6RegexSplitCreateV1(void* regex, xstrview text, int64_t limit, int64_t flags, int64_t size);
XL6_TEXT_API void xl6RegexSplitDestroyV1(void* handle);
XL6_TEXT_API bool xl6RegexSplitHasNextV1(void* handle);
XL6_TEXT_API void* xl6RegexSplitSnapshotV1(void* handle);
XL6_TEXT_API void xl6RegexSplitConsumeV1(void* handle);
XL6_TEXT_API void xl6RegexSplitCloseV1(void* handle);
XL6_TEXT_API bool xl6RegexSplitClosedV1(void* handle);
XL6_TEXT_API void xl6RegexSplitPartDestroyV1(void* handle);
XL6_TEXT_API xstrview xl6RegexSplitPartTextV1(void* handle);
XL6_TEXT_API bool xl6RegexSplitPartMatchedV1(void* handle);
XL6_TEXT_API int64_t xl6RegexSplitPartCaptureV1(void* handle);
XL6_TEXT_API int64_t xl6RegexSplitPartStartV1(void* handle);
XL6_TEXT_API int64_t xl6RegexSplitPartEndV1(void* handle);
XL6_TEXT_API void* xl6RegexSetCreateV1(const xvalue* patterns, int64_t flags, int64_t maxPatternBytes, int64_t maxCaptures, int64_t maxPatterns, int64_t itemSize);
XL6_TEXT_API void xl6RegexSetDestroyV1(void* handle);
XL6_TEXT_API int64_t xl6RegexSetCountV1(void* handle);
XL6_TEXT_API xstrview xl6RegexSetPatternV1(void* handle, int64_t index);
XL6_TEXT_API xvalue* xl6RegexSetHitsV2(uint64_t ownerIdentity, uint64_t elementIdentity, void* handle, xstrview text, int64_t start, int64_t size);
XL6_TEXT_API int64_t xl6RegexSetFirstV1(void* handle, xstrview text, int64_t start, int64_t size);
XL6_TEXT_API bool xl6RegexSetTestV1(void* handle, xstrview text, int64_t start, int64_t size);

/* Resident synchronous IO physical ABI. */
XL6_TEXT_API void xl6_xrt_fs_map_destroy(void* handle);
XL6_TEXT_API void xl6_xrt_fs_lock_destroy(void* handle);
XL6_TEXT_API void xl6_xrt_fs_root_destroy(void* handle);
XL6_TEXT_API int64_t xl6_xrt_io_copy(void* source, void* target, int64_t limit, int64_t mode);
XL6_TEXT_API void* xl6_xrt_fs_file_reader(void* handle);
XL6_TEXT_API void* xl6_xrt_fs_file_writer(void* handle);
XL6_TEXT_API void* xl6_xrt_io_lines_create(void* handle, int64_t limit);
XL6_TEXT_API void xl6_xrt_io_lines_destroy(void* handle);
XL6_TEXT_API bool xl6_xrt_io_lines_close(void* handle);
XL6_TEXT_API bool xl6_xrt_io_lines_advance(void* handle);
XL6_TEXT_API xvalue* xl6_xrt_io_lines_bytes(void* handle);
XL6_TEXT_API int64_t xl6_xrt_io_lines_ending(void* handle);
XL6_TEXT_API xvalue* xl6_xrt_io_read_file(xstrview path, int64_t limit);
XL6_TEXT_API void* xl6_xrt_io_reader_open(xstrview path);
XL6_TEXT_API void* xl6_xrt_io_reader_bytes(const xvalue* data, int64_t limit);
XL6_TEXT_API void* xl6_xrt_io_reader_buffer(void* data, int64_t limit);
XL6_TEXT_API void* xl6_xrt_io_reader_stdin(void);
XL6_TEXT_API void xl6_xrt_io_reader_destroy(void* handle);
XL6_TEXT_API bool xl6_xrt_io_reader_close(void* handle);
XL6_TEXT_API void* xl6_xrt_io_reader_handle(void* handle);
XL6_TEXT_API xvalue* xl6_xrt_io_reader_read(void* r, int64_t n);
XL6_TEXT_API xvalue* xl6_xrt_io_reader_exact(void* r, int64_t n);
XL6_TEXT_API xvalue* xl6_xrt_io_reader_all(void* handle, int64_t limit);
XL6_TEXT_API int64_t xl6_xrt_io_reader_into(void* handle, void* target, int64_t limit);
XL6_TEXT_API int64_t xl6_xrt_io_reader_tell(void* handle);
XL6_TEXT_API int64_t xl6_xrt_io_reader_size(void* handle);
XL6_TEXT_API int64_t xl6_xrt_io_reader_seek(void* handle, int64_t offset, int64_t origin);
XL6_TEXT_API bool xl6_xrt_io_reader_eof(void* handle);
XL6_TEXT_API bool xl6_xrt_io_reader_can_seek(void* handle);
XL6_TEXT_API bool xl6_xrt_io_reader_can_size(void* handle);
XL6_TEXT_API bool xl6_xrt_io_reader_closed(void* handle);
XL6_TEXT_API bool xl6_xrt_io_write_file(xstrview path, const xvalue* data, int64_t mode, int64_t limit);
XL6_TEXT_API bool xl6_xrt_io_write_text_file(xstrview path, xstrview data, int64_t mode, int64_t limit);
XL6_TEXT_API bool xl6_xrt_io_write_buffer_file(xstrview path, void* data, int64_t mode, int64_t limit);
XL6_TEXT_API void* xl6_xrt_io_writer_open(xstrview path, int64_t mode);
XL6_TEXT_API void* xl6_xrt_io_writer_buffer(void);
XL6_TEXT_API void* xl6_xrt_io_writer_standard(int64_t stream);
XL6_TEXT_API void xl6_xrt_io_writer_destroy(void* handle);
XL6_TEXT_API bool xl6_xrt_io_writer_close(void* handle);
XL6_TEXT_API void* xl6_xrt_io_writer_handle(void* handle);
XL6_TEXT_API xvalue* xl6_xrt_io_writer_to_bytes(void* handle);
XL6_TEXT_API int64_t xl6_xrt_io_writer_bytes(void* handle, const xvalue* data, int64_t limit);
XL6_TEXT_API bool xl6_xrt_io_writer_all_bytes(void* handle, const xvalue* data, int64_t limit);
XL6_TEXT_API int64_t xl6_xrt_io_writer_text(void* handle, xstrview data, int64_t limit);
XL6_TEXT_API bool xl6_xrt_io_writer_all_text(void* handle, xstrview data, int64_t limit);
XL6_TEXT_API int64_t xl6_xrt_io_writer_write_buffer(void* handle, void* data, int64_t limit);
XL6_TEXT_API bool xl6_xrt_io_writer_all_buffer(void* handle, void* data, int64_t limit);
XL6_TEXT_API bool xl6_xrt_io_writer_flush(void* handle);
XL6_TEXT_API int64_t xl6_xrt_io_writer_tell(void* handle);
XL6_TEXT_API int64_t xl6_xrt_io_writer_size(void* handle);
XL6_TEXT_API int64_t xl6_xrt_io_writer_seek(void* handle, int64_t offset, int64_t origin);
XL6_TEXT_API bool xl6_xrt_io_writer_can_seek(void* handle);
XL6_TEXT_API bool xl6_xrt_io_writer_can_size(void* handle);
XL6_TEXT_API bool xl6_xrt_io_writer_closed(void* handle);
#endif
