#ifndef XL_EXTLIB_ABI_H
#define XL_EXTLIB_ABI_H

/*
	XLang 原生扩展只应依赖本文件中的稳定句柄外壳，不应包含完整 xl_runtime.h。
	包含本文件前必须先包含 xruntime.h 或等价的 XRT/xruntime 声明头。
*/

struct xl6_type_desc;
struct xl6_typed_type_bridge;
struct xl6_func_sig;
struct xl6_call_frame;
struct xl6_call_result;
struct xerror;
struct xfuture;
struct xvalue;
struct xtlscontext;
struct xtlsverifier;
typedef struct xrttype xrttype;
typedef struct XL6_TlsLease XL6_TlsLease;



/*
	原生扩展按值接收函数句柄。布局属于稳定 ABI；扩展只持有 env，
	不得解释签名描述或调用帧的私有内容。
*/
#ifndef XL6_FUNCTION_TYPEDEF
#define XL6_FUNCTION_TYPEDEF
#define XL6_FUNCTION_DEFINED
typedef void (*XL6_NativeFunction)(void);

typedef struct XL6_Function {
	unsigned long long signature;
	const struct xl6_func_sig* sig;
	void* env;
	void* invoke_env;
	XL6_NativeFunction invoke;
	XL6_NativeFunction c_entry;
	struct xvalue* (*invoke_var)(
		void* env,
		int argc,
		struct xvalue** argv
	);
	bool (*invoke_xcall)(
		void* env,
		struct xl6_call_frame* frame,
		struct xl6_call_result* result
	);
	void (*retain)(void* env);
	void (*release)(void* env);
} XL6_Function;
#endif



/* 增加闭包环境引用；函数代码和签名描述均为进程期借用数据。 */
static inline XL6_Function xl6ExtlibFunctionRetain(XL6_Function Function)
{
	if ( (Function.retain != NULL) && (Function.env != NULL) ) {
		Function.retain(Function.env);
	}
	return Function;
}



/* 释放扩展持有的一份闭包环境引用。 */
static inline void xl6ExtlibFunctionRelease(XL6_Function Function)
{
	if ( (Function.release != NULL) && (Function.env != NULL) ) {
		Function.release(Function.env);
	}
}



/* 判断函数值是否包含任一可调用入口。 */
static inline bool xl6ExtlibFunctionValid(XL6_Function Function)
{
	return (Function.invoke != NULL) ||
		(Function.c_entry != NULL) ||
		(Function.invoke_var != NULL) ||
		(Function.invoke_xcall != NULL);
}



/* 返回当前生成模块共享的异步文件任务池；任务池由模块生命周期拥有。 */
void* xl6ExtlibAsyncFileTaskPool(void);



/* 原生扩展异步延续使用的稳定结果布局。 */
#ifndef XL6_FUTURE_RESULT_TYPEDEF
#define XL6_FUTURE_RESULT_TYPEDEF
typedef struct xl6_future_result {
	int iStatus;
	void* pValue;
	struct xerror* Error;
	unsigned int iFlags;
	void (*pfnFreeValue)(void* pValue);
} xl6_future_result;

typedef int (*xl6_future_continue_proc)(
	const xl6_future_result* pInput,
	void* pArgument,
	xl6_future_result* pOutput
);

typedef void (*xl6_future_arg_free_proc)(void* pArgument);
#endif



#define XL6_FUTURE_OWN_VALUE 0x00000001u
#define XL6_FUTURE_STATUS_OK 0
#define XL6_FUTURE_STATUS_FAILED (-1)
#define XL6_FUTURE_STATUS_CANCELLED (-2)
#define XL6_FUTURE_STATUS_CLOSED (-3)



/* 创建拥有延续参数的映射 Future；结果与取消链由语言 Future 桥统一处理。 */
struct xfuture* xl6FutureThenOwned(
	struct xfuture* pFuture,
	xl6_future_continue_proc pProc,
	void* pArgument,
	xl6_future_arg_free_proc pFreeArgument
);

bool xl6FutureLinkCancel(
	struct xfuture* pFuture,
	struct xfuture* pTarget
);

void xl6FutureDestroy(struct xfuture* pFuture);



/* 查询扩展回调是否留下了尚未处理的 xlang 异常。 */
int xl6ExtlibErrorPending(void);



/* 把当前 xlang 异常映射到 XRT 结构化错误槽，异常本身仍由语言运行时持有。 */
void xl6ExtlibErrorPublishXrt(void);



/* 外部异步回调已经保存 XRT 错误后，释放当前线程残留的 xlang 异常。 */
void xl6ExtlibErrorClear(void);



/*
	把可变 TlsConfig 快照为扩展库可持有的不可变租约。
	租约拥有 TLS Context，并可为异步客户端创建独立共享 Verifier。
*/
XL6_TlsLease* xl6ExtlibTlsLeaseCreate(void* pConfig);



/* 释放扩展库持有的一份 TLS 租约引用。 */
void xl6ExtlibTlsLeaseRelease(XL6_TlsLease* pLease);



/* 借用租约中的 TLS Context；返回值不得由扩展库释放。 */
struct xtlscontext* xl6ExtlibTlsLeaseContext(XL6_TlsLease* pLease);



/* 创建拥有租约引用的共享 Verifier；调用方负责释放返回值。 */
struct xtlsverifier* xl6ExtlibTlsLeaseShareVerifier(XL6_TlsLease* pLease);



/* XLang point 绑定使用的无类型租约适配；语义与上方强类型接口一致。 */
void* xl6ExtlibTlsLeaseCreateHandle(void* pConfig);
void xl6ExtlibTlsLeaseReleaseHandle(void* pLease);
void* xl6ExtlibTlsLeaseContextHandle(void* pLease);
void* xl6ExtlibTlsLeaseShareVerifierHandle(void* pLease);



#ifndef XL6_XRT_TYPED_CONTAINER_TYPEDEF
#define XL6_XRT_TYPED_CONTAINER_TYPEDEF
typedef struct xl6_typed_container* xtarray;
typedef struct xl6_typed_container* xtlist;
typedef struct xl6_typed_container* xtset;
typedef struct xl6_typed_container* xtdict;
#endif



#define XL6_TYPED_CONTAINER_ARRAY 1u
#define XL6_TYPED_CONTAINER_LIST 2u
#define XL6_TYPED_CONTAINER_SET 3u
#define XL6_TYPED_CONTAINER_DICT 4u



/* 类型化容器外壳持有 xruntime 容器；Value 的具体类型由 Kind 决定。 */
struct xl6_typed_container {
	volatile int32 RefCount;
	uint32 Kind;
	const struct xl6_type_desc* ItemType;
	const xrttype* XrtType;
	struct xl6_typed_type_bridge* Bridge;
	ptr Value;
};

#endif
