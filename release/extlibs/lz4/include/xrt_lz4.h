#ifndef XRT_LZ4_H
#define XRT_LZ4_H

#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_LZ4_OPTIONS_VERSION 1u

typedef enum {
	XRT_LZ4_OK = 0,
	XRT_LZ4_ERROR_ARGUMENT = 1,
	XRT_LZ4_ERROR_MEMORY = 2,
	XRT_LZ4_ERROR_DATA = 3,
	XRT_LZ4_ERROR_TRUNCATED = 4,
	XRT_LZ4_ERROR_LIMIT = 5,
	XRT_LZ4_ERROR_IO = 6,
	XRT_LZ4_ERROR_STATE = 7,
	XRT_LZ4_ERROR_CANCELLED = 8
} xrtlz4status;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtlz4error;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iAcceleration;
	uint8 sReserved[4];
	/* maxEntries 限制帧数；单项、节点、深度和标志必须为零。 */
	xrtresourcelimits tLimits;
} xrtlz4options;

typedef struct xrtlz4compressor xrtlz4compressor;
typedef struct xrtlz4decompressor xrtlz4decompressor;

/* 标准 LZ4 Frame。压缩使用独立 64 KiB 块，并开启块/整帧校验。
 * 解压接受关联块、标准块大小、串联帧和 skippable 帧，不支持外部字典。
 * tLimits: 输入/输出为累计字节；ratio 为每帧已消费输入对应的输出上限；
 * maxEntries 包含 skippable 帧。C 的限额 0 表示不限，XL 使用正数硬上限。
 * 输入只借用到调用返回。对象须串行使用，不能从其 writer 回调重入或销毁。
 * 构造失败返回 NULL。参数错误可纠正；数据、限额、IO 失败进入终态。
 * 所有失败同时写结构化 XRT error，Error() 保存对象最近一次操作的快照。 */
XRT_API void xrtLz4OptionsInit(xrtlz4options* pOptions);
XRT_API void xrtLz4ErrorInit(xrtlz4error* pError);

XRT_API xrtlz4compressor* xrtLz4CompressorCreate(const xrtlz4options* pOptions);
XRT_API bool xrtLz4CompressorUpdate(xrtlz4compressor* pCompressor, const void* pData, size_t iSize);
XRT_API bool xrtLz4CompressorFinish(xrtlz4compressor* pCompressor);
XRT_API bool xrtLz4CompressorDrain(xrtlz4compressor* pCompressor, xwriter* pWriter);
XRT_API xbuffer* xrtLz4CompressorTakeBuffer(xrtlz4compressor* pCompressor);
/* drain 转移已有缓冲，不复制全文；新空缓冲分配失败时原输出仍可重试取出。 */
XRT_API xbuffer* xrtLz4CompressorDrainBuffer(xrtlz4compressor* pCompressor);
XRT_API uint64 xrtLz4CompressorInputBytes(const xrtlz4compressor* pCompressor);
XRT_API uint64 xrtLz4CompressorOutputBytes(const xrtlz4compressor* pCompressor);
XRT_API const xrtlz4error* xrtLz4CompressorError(const xrtlz4compressor* pCompressor);
XRT_API void xrtLz4CompressorDestroy(xrtlz4compressor* pCompressor);

XRT_API xrtlz4decompressor* xrtLz4DecompressorCreate(const xrtlz4options* pOptions);
XRT_API bool xrtLz4DecompressorUpdate(xrtlz4decompressor* pDecompressor, const void* pData, size_t iSize);
XRT_API bool xrtLz4DecompressorFinish(xrtlz4decompressor* pDecompressor);
XRT_API bool xrtLz4DecompressorDrain(xrtlz4decompressor* pDecompressor, xwriter* pWriter);
XRT_API xbuffer* xrtLz4DecompressorTakeBuffer(xrtlz4decompressor* pDecompressor);
XRT_API xbuffer* xrtLz4DecompressorDrainBuffer(xrtlz4decompressor* pDecompressor);
XRT_API uint64 xrtLz4DecompressorInputBytes(const xrtlz4decompressor* pDecompressor);
XRT_API uint64 xrtLz4DecompressorOutputBytes(const xrtlz4decompressor* pDecompressor);
XRT_API bool xrtLz4DecompressorFinished(const xrtlz4decompressor* pDecompressor);
/* 只计已验证的普通数据帧，不计 skippable；finished 也接受完整 skippable 尾部。 */
XRT_API uint64 xrtLz4DecompressorMembers(const xrtlz4decompressor* pDecompressor);
XRT_API const xrtlz4error* xrtLz4DecompressorError(const xrtlz4decompressor* pDecompressor);
XRT_API void xrtLz4DecompressorDestroy(xrtlz4decompressor* pDecompressor);

XRT_API bool xrtLz4Compress(const void* pData, size_t iSize, const xrtlz4options* pOptions,
	xbuffer** ppOutput, xrtlz4error* pError);
XRT_API bool xrtLz4Decompress(const void* pData, size_t iSize, const xrtlz4options* pOptions,
	xbuffer** ppOutput, xrtlz4error* pError);
/* 流接口借用 reader/writer，不关闭或 flush；进度回调同步执行，可取消。
 * 总量从 reader 当前游标计算；最终回调失败仍算失败。
 * 已交给 writer 的前缀无法撤回，只有完整成功才能将解压结果视为已验证。 */
XRT_API bool xrtLz4CompressStream(xreader* pReader, xwriter* pWriter, const xrtlz4options* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtlz4error* pError);
XRT_API bool xrtLz4DecompressStream(xreader* pReader, xwriter* pWriter, const xrtlz4options* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtlz4error* pError);

/* 同目录临时文件，完整验证、最终回调和关闭成功后才提交；支持原地变换。
 * 失败保留原目标并删除临时文件，错误从 xrtGetError 读取。 */
XRT_API bool xrtLz4CompressFile(const char* source, const char* destination,
    const xrtlz4options* options, bool overwrite, xrtprogressproc progress, ptr user);
XRT_API bool xrtLz4DecompressFile(const char* source, const char* destination,
    const xrtlz4options* options, bool overwrite, xrtprogressproc progress, ptr user);

#ifdef __cplusplus
}
#endif

#endif
