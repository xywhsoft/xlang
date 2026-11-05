/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xruntime requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XRUNTIME_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XRUNTIME_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XRUNTIME_SINGLE_HEADER_H
#define XRUNTIME_SINGLE_HEADER_H
#define XRUNTIME_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xruntime/include/xruntime/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XRUNTIME_FEATURES_H
#define XRUNTIME_FEATURES_H

/* runtime_dynamic_field 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD)
#ifndef XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD
#define XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_FIELD
#define XRUNTIME_MODULE_RUNTIME_FIELD
#endif
#ifndef XRUNTIME_MODULE_TYPED_DICT
#define XRUNTIME_MODULE_TYPED_DICT
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_OBJECT_GRAPH
#define XRUNTIME_MODULE_RUNTIME_OBJECT_GRAPH
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_TRACE
#define XRUNTIME_MODULE_RUNTIME_VALUE_TRACE
#endif
#endif

/* runtime_value_roots 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_VALUE_ROOTS)
#ifndef XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS
#define XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_TRACE
#define XRUNTIME_MODULE_RUNTIME_VALUE_TRACE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_OBJECT_GRAPH
#define XRUNTIME_MODULE_RUNTIME_OBJECT_GRAPH
#endif
#endif

/* runtime_value_trace 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_VALUE_TRACE)
#ifndef XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE
#define XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_OBJECT
#define XRUNTIME_MODULE_RUNTIME_VALUE_OBJECT
#endif
#endif

/* runtime_value_type 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_VALUE_TYPE)
#ifndef XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE
#define XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#endif

/* runtime_value_future 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE)
#ifndef XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE
#define XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#endif

/* typed_dict_value 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_DICT_VALUE)
#ifndef XRUNTIME_FEATURE_TYPED_DICT_VALUE
#define XRUNTIME_FEATURE_TYPED_DICT_VALUE
#endif
#ifndef XRUNTIME_MODULE_TYPED_VALUE
#define XRUNTIME_MODULE_TYPED_VALUE
#endif
#ifndef XRUNTIME_MODULE_TYPED_DICT
#define XRUNTIME_MODULE_TYPED_DICT
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#endif

/* typed_set_value 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_SET_VALUE)
#ifndef XRUNTIME_FEATURE_TYPED_SET_VALUE
#define XRUNTIME_FEATURE_TYPED_SET_VALUE
#endif
#ifndef XRUNTIME_MODULE_TYPED_VALUE
#define XRUNTIME_MODULE_TYPED_VALUE
#endif
#ifndef XRUNTIME_MODULE_TYPED_SET
#define XRUNTIME_MODULE_TYPED_SET
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#endif

/* typed_list_value 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_LIST_VALUE)
#ifndef XRUNTIME_FEATURE_TYPED_LIST_VALUE
#define XRUNTIME_FEATURE_TYPED_LIST_VALUE
#endif
#ifndef XRUNTIME_MODULE_TYPED_VALUE
#define XRUNTIME_MODULE_TYPED_VALUE
#endif
#ifndef XRUNTIME_MODULE_TYPED_LIST
#define XRUNTIME_MODULE_TYPED_LIST
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#endif

/* typed_array_value 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_ARRAY_VALUE)
#ifndef XRUNTIME_FEATURE_TYPED_ARRAY_VALUE
#define XRUNTIME_FEATURE_TYPED_ARRAY_VALUE
#endif
#ifndef XRUNTIME_MODULE_TYPED_VALUE
#define XRUNTIME_MODULE_TYPED_VALUE
#endif
#ifndef XRUNTIME_MODULE_TYPED_ARRAY
#define XRUNTIME_MODULE_TYPED_ARRAY
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#endif

/* typed_dict 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_DICT)
#ifndef XRUNTIME_FEATURE_TYPED_DICT
#define XRUNTIME_FEATURE_TYPED_DICT
#endif
#ifndef XRT_MODULE_MAP
#define XRT_MODULE_MAP
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* typed_set 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_SET)
#ifndef XRUNTIME_FEATURE_TYPED_SET
#define XRUNTIME_FEATURE_TYPED_SET
#endif
#ifndef XRT_MODULE_SET
#define XRT_MODULE_SET
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* typed_list 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_LIST)
#ifndef XRUNTIME_FEATURE_TYPED_LIST
#define XRUNTIME_FEATURE_TYPED_LIST
#endif
#ifndef XRT_MODULE_INT_MAP
#define XRT_MODULE_INT_MAP
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* typed_tree 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_TREE)
#ifndef XRUNTIME_FEATURE_TYPED_TREE
#define XRUNTIME_FEATURE_TYPED_TREE
#endif
#ifndef XRT_MODULE_AVL_TREE
#define XRT_MODULE_AVL_TREE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* typed_stack 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_STACK)
#ifndef XRUNTIME_FEATURE_TYPED_STACK
#define XRUNTIME_FEATURE_TYPED_STACK
#endif
#ifndef XRUNTIME_MODULE_TYPED_ARRAY
#define XRUNTIME_MODULE_TYPED_ARRAY
#endif
#endif

/* typed_queue_mpmc 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_QUEUE_MPMC)
#ifndef XRUNTIME_FEATURE_TYPED_QUEUE_MPMC
#define XRUNTIME_FEATURE_TYPED_QUEUE_MPMC
#endif
#ifndef XRUNTIME_MODULE_TYPED_QUEUE
#define XRUNTIME_MODULE_TYPED_QUEUE
#endif
#ifndef XRT_MODULE_QUEUE_MPMC
#define XRT_MODULE_QUEUE_MPMC
#endif
#endif

/* typed_queue_mpsc 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_QUEUE_MPSC)
#ifndef XRUNTIME_FEATURE_TYPED_QUEUE_MPSC
#define XRUNTIME_FEATURE_TYPED_QUEUE_MPSC
#endif
#ifndef XRUNTIME_MODULE_TYPED_QUEUE
#define XRUNTIME_MODULE_TYPED_QUEUE
#endif
#ifndef XRT_MODULE_QUEUE_MPSC
#define XRT_MODULE_QUEUE_MPSC
#endif
#ifndef XRT_MODULE_QUEUE_MPMC
#define XRT_MODULE_QUEUE_MPMC
#endif
#endif

/* typed_queue_spsc 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_QUEUE_SPSC)
#ifndef XRUNTIME_FEATURE_TYPED_QUEUE_SPSC
#define XRUNTIME_FEATURE_TYPED_QUEUE_SPSC
#endif
#ifndef XRUNTIME_MODULE_TYPED_QUEUE
#define XRUNTIME_MODULE_TYPED_QUEUE
#endif
#ifndef XRT_MODULE_QUEUE_SPSC
#define XRT_MODULE_QUEUE_SPSC
#endif
#endif

/* typed_queue 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_QUEUE)
#ifndef XRUNTIME_FEATURE_TYPED_QUEUE
#define XRUNTIME_FEATURE_TYPED_QUEUE
#endif
#ifndef XRT_MODULE_QUEUE
#define XRT_MODULE_QUEUE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* typed_array 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_ARRAY)
#ifndef XRUNTIME_FEATURE_TYPED_ARRAY
#define XRUNTIME_FEATURE_TYPED_ARRAY
#endif
#ifndef XRT_MODULE_ARRAY
#define XRT_MODULE_ARRAY
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* runtime_field 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_FIELD)
#ifndef XRUNTIME_FEATURE_RUNTIME_FIELD
#define XRUNTIME_FEATURE_RUNTIME_FIELD
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* runtime_type_future 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE)
#ifndef XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* runtime_type_string_value 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE)
#ifndef XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE
#define XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE_STRING
#define XRUNTIME_MODULE_RUNTIME_TYPE_STRING
#endif
#ifndef XRUNTIME_MODULE_TYPED_VALUE
#define XRUNTIME_MODULE_TYPED_VALUE
#endif
#endif

/* typed_value 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_TYPED_VALUE)
#ifndef XRUNTIME_FEATURE_TYPED_VALUE
#define XRUNTIME_FEATURE_TYPED_VALUE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#endif

/* value_convert_string 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_VALUE_CONVERT_STRING)
#ifndef XRUNTIME_FEATURE_VALUE_CONVERT_STRING
#define XRUNTIME_FEATURE_VALUE_CONVERT_STRING
#endif
#ifndef XRUNTIME_MODULE_VALUE_CONVERT
#define XRUNTIME_MODULE_VALUE_CONVERT
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_CONVERT_STRING
#define XRUNTIME_MODULE_RUNTIME_CONVERT_STRING
#endif
#endif

/* value_convert 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_VALUE_CONVERT)
#ifndef XRUNTIME_FEATURE_VALUE_CONVERT
#define XRUNTIME_FEATURE_VALUE_CONVERT
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_CONVERT
#define XRUNTIME_MODULE_RUNTIME_CONVERT
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#endif

/* runtime_convert_string 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_CONVERT_STRING)
#ifndef XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING
#define XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_CONVERT
#define XRUNTIME_MODULE_RUNTIME_CONVERT
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE_STRING
#define XRUNTIME_MODULE_RUNTIME_TYPE_STRING
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#ifndef XRT_MODULE_TIME_TEXT
#define XRT_MODULE_TIME_TEXT
#endif
#endif

/* runtime_type_string 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_TYPE_STRING)
#ifndef XRUNTIME_FEATURE_RUNTIME_TYPE_STRING
#define XRUNTIME_FEATURE_RUNTIME_TYPE_STRING
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* runtime_convert 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_CONVERT)
#ifndef XRUNTIME_FEATURE_RUNTIME_CONVERT
#define XRUNTIME_FEATURE_RUNTIME_CONVERT
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* runtime_object_graph 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_OBJECT_GRAPH)
#ifndef XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH
#define XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_OBJECT
#define XRUNTIME_MODULE_RUNTIME_OBJECT
#endif
#endif

/* runtime_value_weak 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_VALUE_WEAK)
#ifndef XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK
#define XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_OBJECT
#define XRUNTIME_MODULE_RUNTIME_OBJECT
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#endif

/* runtime_value_callable 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE)
#ifndef XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_CALL
#endif
#endif

/* runtime_call 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_CALL)
#ifndef XRUNTIME_FEATURE_RUNTIME_CALL
#define XRUNTIME_FEATURE_RUNTIME_CALL
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#endif

/* runtime_value_object 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_VALUE_OBJECT)
#ifndef XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT
#define XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_OBJECT
#define XRUNTIME_MODULE_RUNTIME_OBJECT
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#endif

/* runtime_object 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_OBJECT)
#ifndef XRUNTIME_FEATURE_RUNTIME_OBJECT
#define XRUNTIME_FEATURE_RUNTIME_OBJECT
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#endif
#endif

/* runtime_type 及其直接依赖。 */
#if defined(XRUNTIME_MODULE_ALL) || defined(XRUNTIME_MODULE_RUNTIME_TYPE)
#ifndef XRUNTIME_FEATURE_RUNTIME_TYPE
#define XRUNTIME_FEATURE_RUNTIME_TYPE
#endif
#endif

#endif /* XRUNTIME_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/runtime_type.h */
/* ========================================================================== */

#ifndef XRT_RUNTIME_TYPE_H
#define XRT_RUNTIME_TYPE_H




#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_OBJECT requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE)

/*
	运行时类型描述只表达 C、XRT 和上层语言都能解释的事实。
	描述对象由声明方持有，注册表只借用，不绑定任何语言语法。
*/
typedef struct xrttype xrttype;
typedef struct xrttypeops xrttypeops;
typedef struct xrtinstanceops xrtinstanceops;
typedef struct xrtobject xrtobject;
typedef struct xrtfunctionsig xrtfunctionsig;
typedef struct xrtparamdesc xrtparamdesc;
typedef struct xrtfielddesc xrtfielddesc;
typedef struct xrtfieldtable xrtfieldtable;
typedef struct xrtmethoddesc xrtmethoddesc;
typedef struct xrtmethodtable xrtmethodtable;
typedef struct xrtprotocol xrtprotocol;
typedef struct xrtprotocolwitness xrtprotocolwitness;
typedef struct xrtprotocolregistry xrtprotocolregistry;
typedef struct xrtenum xrtenum;
typedef struct xrtenumvariant xrtenumvariant;
typedef struct xrttyperegistry xrttyperegistry;



/* 对象访问器接收一个借用的强引用；返回 false 会立即终止本次遍历。 */
typedef bool (*xrtobjectvisitor)(xrtobject* pObject, ptr pContext);



/*
	类型格式化器通过该回调同步写出借用 UTF-8 分块。
	分块只在本次调用期间有效；返回 false 要求格式化器立即停止。
*/
typedef bool (*xrttypewriter)(xstrview Text, ptr pContext);



typedef enum xrttypekind {
	XRT_TYPE_INVALID = 0,
	XRT_TYPE_NULL,
	XRT_TYPE_BOOL,
	XRT_TYPE_SIGNED_INT,
	XRT_TYPE_UNSIGNED_INT,
	XRT_TYPE_FLOAT,
	XRT_TYPE_STRING,
	XRT_TYPE_BYTES,
	XRT_TYPE_TIME,
	XRT_TYPE_POINTER,
	XRT_TYPE_CALLABLE,
	XRT_TYPE_ARRAY,
	XRT_TYPE_LIST,
	XRT_TYPE_SET,
	XRT_TYPE_DICT,
	XRT_TYPE_RECORD,
	XRT_TYPE_HANDLE,
	XRT_TYPE_TYPE,
	XRT_TYPE_FUTURE,
	XRT_TYPE_CLASS,
	XRT_TYPE_ENUM,
	XRT_TYPE_PROTOCOL,
	XRT_TYPE_OPTIONAL,
	XRT_TYPE_WEAK
} xrttypekind;



/* 运行时类型模块稳定错误代码。 */
typedef enum xtypeerror {
	XTYPE_ERROR_DESCRIPTOR = 1,
	XTYPE_ERROR_SIGNATURE,
	XTYPE_ERROR_OPERATION,
	XTYPE_ERROR_REGISTRY,
	XTYPE_ERROR_PROTOCOL,
	XTYPE_ERROR_ENUM
} xtypeerror;



/* 类型标志用于统一生命周期和 ABI 判断，不承担语言级访问控制。 */
#define XRT_TYPE_FLAG_TRIVIAL_COPY	UINT32_C(0x00000001)
#define XRT_TYPE_FLAG_TRIVIAL_DROP	UINT32_C(0x00000002)
#define XRT_TYPE_FLAG_COPYABLE		UINT32_C(0x00000004)
#define XRT_TYPE_FLAG_REFERENCE		UINT32_C(0x00000008)
#define XRT_TYPE_FLAG_NULLABLE		UINT32_C(0x00000010)
#define XRT_TYPE_FLAG_FINAL			UINT32_C(0x00000020)
#define XRT_TYPE_FLAG_RELOCATABLE	UINT32_C(0x00000040)



typedef enum xrtparammode {
	XRT_PARAM_DEFAULT = 0,
	XRT_PARAM_BYVAL,
	XRT_PARAM_BYREF
} xrtparammode;



#define XRT_PARAM_FLAG_OPTIONAL		UINT32_C(0x00000001)
#define XRT_PARAM_FLAG_NAMED_ONLY	UINT32_C(0x00000002)
#define XRT_FUNCTION_FLAG_VARARGS	UINT32_C(0x00000001)
#define XRT_FUNCTION_FLAG_KWARGS	UINT32_C(0x00000002)
#define XRT_METHOD_FLAG_STATIC		UINT32_C(0x00000001)
#define XRT_METHOD_FLAG_VIRTUAL		UINT32_C(0x00000002)
#define XRT_METHOD_FLAG_FINAL		UINT32_C(0x00000004)



/*
	所有值操作只处理 Size 字节的 C ABI 值，不得访问 InstanceSize 负载。
	Init 失败时必须释放已经取得的资源并设置 XRT 错误，调用方不会执行 Drop。
	其他类型操作返回 false 时必须保留目标原值并设置 XRT 错误。
	Move 成功后源值处于已初始化的空状态，仍允许 Drop。
	Format 只读值并同步调用 writer；writer 失败后必须立即停止并返回 false。
*/
struct xrttypeops {
	bool (*Init)(ptr pValue, const xrttype* pType);
	bool (*Copy)(ptr pTarget, const void* pSource, const xrttype* pType);
	bool (*Move)(ptr pTarget, ptr pSource, const xrttype* pType);
	void (*Drop)(ptr pValue, const xrttype* pType);
	bool (*Clone)(ptr pTarget, const void* pSource, const xrttype* pType);
	int (*Compare)(
		const void* pLeft,
		const void* pRight,
		const xrttype* pType
	);
	uint64 (*Hash)(const void* pValue, const xrttype* pType);
	bool (*Format)(
		const void* pValue,
		const xrttype* pType,
		xrttypewriter pWrite,
		ptr pContext
	);
	bool (*Trace)(
		const void* pValue,
		const xrttype* pType,
		xrtobjectvisitor pVisit,
		ptr pContext
	);
};



/* 实例操作只处理引用对象的堆负载，不处理 C ABI 中的对象指针值。 */
struct xrtinstanceops {
	bool (*Init)(ptr pInstance, const xrttype* pType);
	void (*Drop)(ptr pInstance, const xrttype* pType);
	bool (*Trace)(
		const void* pInstance,
		const xrttype* pType,
		xrtobjectvisitor pVisit,
		ptr pContext
	);
};



struct xrtparamdesc {
	xstrview Name;
	const xrttype* Type;
	xrtparammode Mode;
	uint32 Flags;
};



struct xrtfunctionsig {
	uint64 Id;
	xstrview Name;
	size_t ParamCount;
	const xrtparamdesc* Params;
	size_t ReturnCount;
	const xrttype* const* ReturnTypes;
	uint32 Flags;
	/* 扩展元数据不参与签名身份，生命周期由描述符所有者管理。 */
	uint64 UserTag;
	ptr UserData;
};



struct xrtmethoddesc {
	xstrview Name;
	const xrtfunctionsig* Signature;
	ptr Entry;
	uint32 Flags;
};



struct xrtmethodtable {
	size_t Count;
	const xrtmethoddesc* Methods;
};



struct xrttype {
	uint64 Id;
	xrttypekind Kind;
	uint32 Flags;
	xstrview Name;
	xstrview AbiName;
	/* Size/Align 描述值在 C ABI 中的存储形态。 */
	size_t Size;
	size_t Align;
	/* InstanceSize/InstanceAlign 描述引用类型在堆上的数据负载。 */
	size_t InstanceSize;
	size_t InstanceAlign;
	/* Ops 处理 Size 字节的 C ABI 值，InstanceOps 处理堆对象负载。 */
	const xrttypeops* Ops;
	const xrtinstanceops* InstanceOps;
	const xrttype* Base;
	size_t ArgumentCount;
	const xrttype* const* Arguments;
	const xrtfieldtable* Fields;
	const xrtmethodtable* Methods;
	const void* Metadata;
};



typedef struct xrtprotocolrequirement {
	xstrview Name;
	const xrtfunctionsig* Signature;
} xrtprotocolrequirement;



struct xrtprotocol {
	const xrttype* Type;
	size_t RequirementCount;
	const xrtprotocolrequirement* Requirements;
};



typedef struct xrtprotocolentry {
	xstrview Name;
	const xrtfunctionsig* Signature;
	ptr Entry;
} xrtprotocolentry;



struct xrtprotocolwitness {
	const xrtprotocol* Protocol;
	const xrttype* ConcreteType;
	size_t EntryCount;
	const xrtprotocolentry* Entries;
};



struct xrtenumvariant {
	xstrview Name;
	int64 Tag;
	const xrttype* PayloadType;
};



struct xrtenum {
	const xrttype* Type;
	size_t VariantCount;
	const xrtenumvariant* Variants;
};



XRT_EXTERN_C_BEGIN



/* 按规范 ABI 名生成稳定的非零类型 ID。 */
XRT_API uint64 xrtTypeId(xstrview AbiName);



/* 按调用形态生成稳定签名 ID；所有非空参数名都参与身份。 */
XRT_API uint64 xrtFunctionSigId(const xrtfunctionsig* pSignature);



/* 检查参数、返回值、标志、名称唯一性和显式签名 ID 是否自洽。 */
XRT_API bool xrtFunctionSigValidate(const xrtfunctionsig* pSignature);



/* 检查类型 ID、结构、生命周期标志和完整继承链是否自洽。 */
XRT_API bool xrtTypeValidate(const xrttype* pType);



/* 类型相等以稳定 ID 和 ABI 名共同判断，指针相同是快速路径。 */
XRT_API bool xrtTypeSame(const xrttype* pLeft, const xrttype* pRight);



/* 判断类型是否等于目标类型或从目标类型派生。 */
XRT_API bool xrtTypeIsA(const xrttype* pType, const xrttype* pTarget);



/* 查询类型值是否支持复制、字节重定位、比较或散列。 */
XRT_API bool xrtTypeIsCopyable(const xrttype* pType);
XRT_API bool xrtTypeIsRelocatable(const xrttype* pType);
XRT_API bool xrtTypeIsComparable(const xrttype* pType);
XRT_API bool xrtTypeIsHashable(const xrttype* pType);



/* 返回指定下标的泛型参数，越界返回空并设置范围错误。 */
XRT_API const xrttype* xrtTypeArgument(const xrttype* pType, size_t iIndex);



/* 沿当前类型和基类查找方法；签名 ID 为零时返回首个同名重载。 */
XRT_API const xrtmethoddesc* xrtTypeFindMethod(
	const xrttype* pType,
	xstrview Name,
	uint64 iSignatureId
);



/* 使用类型操作初始化、复制、移动、销毁或克隆一个 Size 字节值。 */
XRT_API bool xrtTypeInitValue(const xrttype* pType, ptr pValue);
XRT_API bool xrtTypeCopyValue(
	const xrttype* pType,
	ptr pTarget,
	const void* pSource
);
XRT_API bool xrtTypeMoveValue(
	const xrttype* pType,
	ptr pTarget,
	ptr pSource
);
XRT_API void xrtTypeDropValue(const xrttype* pType, ptr pValue);
XRT_API bool xrtTypeCloneValue(
	const xrttype* pType,
	ptr pTarget,
	const void* pSource
);



/* 枚举值直接持有的全部强对象引用；每一个所有权槽位必须访问一次。 */
XRT_API bool xrtTypeTraceValue(
	const xrttype* pType,
	const void* pValue,
	xrtobjectvisitor pVisit,
	ptr pContext
);



/* 初始化、销毁或追踪引用对象的堆实例负载。 */
XRT_API bool xrtTypeInitInstance(const xrttype* pType, ptr pInstance);
XRT_API void xrtTypeDropInstance(const xrttype* pType, ptr pInstance);
XRT_API bool xrtTypeTraceInstance(
	const xrttype* pType,
	const void* pInstance,
	xrtobjectvisitor pVisit,
	ptr pContext
);



/* 使用类型操作比较或散列一个值；成功才写入输出。 */
XRT_API bool xrtTypeCompareValue(
	const xrttype* pType,
	const void* pLeft,
	const void* pRight,
	int* pResult
);
XRT_API bool xrtTypeHashValue(
	const xrttype* pType,
	const void* pValue,
	uint64* pHash
);



/* 注册表只借用唯一且不可变的描述指针；除销毁外允许并发调用。 */
XRT_API xrttyperegistry* xrtTypeRegistryCreate(void);
XRT_API void xrtTypeRegistryDestroy(xrttyperegistry* pRegistry);
XRT_API bool xrtTypeRegistryAdd(
	xrttyperegistry* pRegistry,
	const xrttype* pType
);
XRT_API bool xrtTypeRegistryRemove(
	xrttyperegistry* pRegistry,
	const xrttype* pType
);
XRT_API size_t xrtTypeRegistryCount(const xrttyperegistry* pRegistry);



/* 按稳定类型 ID 顺序返回指定位置的借用描述；并发修改时下标只代表本次调用快照。 */
XRT_API const xrttype* xrtTypeRegistryAt(
	const xrttyperegistry* pRegistry,
	size_t iIndex
);
XRT_API const xrttype* xrtTypeRegistryFindId(
	const xrttyperegistry* pRegistry,
	uint64 iTypeId
);
XRT_API const xrttype* xrtTypeRegistryFindName(
	const xrttyperegistry* pRegistry,
	xstrview AbiName
);



/* 验证协议类型、要求和重载身份完整且唯一。 */
XRT_API bool xrtProtocolValidate(const xrtprotocol* pProtocol);



/* 验证协议见证表完整且每个要求只实现一次。 */
XRT_API bool xrtProtocolWitnessValidate(
	const xrtprotocolwitness* pWitness
);
XRT_API const xrtprotocolentry* xrtProtocolWitnessFind(
	const xrtprotocolwitness* pWitness,
	xstrview Name,
	uint64 iSignatureId
);
XRT_API xrtprotocolregistry* xrtProtocolRegistryCreate(void);
XRT_API void xrtProtocolRegistryDestroy(xrtprotocolregistry* pRegistry);
XRT_API bool xrtProtocolRegistryAdd(
	xrtprotocolregistry* pRegistry,
	const xrtprotocolwitness* pWitness
);
XRT_API bool xrtProtocolRegistryRemove(
	xrtprotocolregistry* pRegistry,
	const xrtprotocolwitness* pWitness
);
XRT_API const xrtprotocolwitness* xrtProtocolRegistryFind(
	const xrtprotocolregistry* pRegistry,
	uint64 iProtocolTypeId,
	uint64 iConcreteTypeId
);
XRT_API size_t xrtProtocolRegistryCount(
	const xrtprotocolregistry* pRegistry
);



/* 按协议类型 ID 和具体类型 ID 顺序返回借用见证；并发修改时下标只代表本次调用快照。 */
XRT_API const xrtprotocolwitness* xrtProtocolRegistryAt(
	const xrtprotocolregistry* pRegistry,
	size_t iIndex
);



/* 验证枚举标签和名称唯一，并查询变体。 */
XRT_API bool xrtEnumValidate(const xrtenum* pEnum);
XRT_API const xrtenumvariant* xrtEnumFindTag(
	const xrtenum* pEnum,
	int64 iTag
);
XRT_API const xrtenumvariant* xrtEnumFindName(
	const xrtenum* pEnum,
	xstrview Name
);



/* XRT 内建标量类型描述在进程期保持稳定。 */
XRT_API const xrttype* xrtTypeNull(void);
XRT_API const xrttype* xrtTypeBool(void);
XRT_API const xrttype* xrtTypeBool32(void);
XRT_API const xrttype* xrtTypeInt8(void);
XRT_API const xrttype* xrtTypeUInt8(void);
XRT_API const xrttype* xrtTypeInt16(void);
XRT_API const xrttype* xrtTypeUInt16(void);
XRT_API const xrttype* xrtTypeInt32(void);
XRT_API const xrttype* xrtTypeUInt32(void);
XRT_API const xrttype* xrtTypeInt64(void);
XRT_API const xrttype* xrtTypeUInt64(void);
XRT_API const xrttype* xrtTypeFloat32(void);
XRT_API const xrttype* xrtTypeFloat64(void);
XRT_API const xrttype* xrtTypeTime(void);
XRT_API const xrttype* xrtTypePointer(void);
XRT_API const xrttype* xrtTypeType(void);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/runtime_object.h */
/* ========================================================================== */

#ifndef XRT_RUNTIME_OBJECT_H
#define XRT_RUNTIME_OBJECT_H




#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_OBJECT requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)

typedef struct xrtobject xrtobject;



/* 运行时对象模块稳定错误代码。 */
typedef enum xobjecterror {
	XOBJECT_ERROR_TYPE = 1,
	XOBJECT_ERROR_SIZE,
	XOBJECT_ERROR_REFERENCE,
	XOBJECT_ERROR_WEAK,
	XOBJECT_ERROR_INITIALIZE
} xobjecterror;



/* 弱引用是一个可复制、可移动的控制块引用，不拥有对象的强生命周期。 */
typedef struct xrtweak {
	ptr Control;
} xrtweak;



XRT_EXTERN_C_BEGIN

/* Full physical ownership trace for native payloads, as distinct from the
 * older object-only Trace that may flatten Value shells/backing. Bind once
 * before publication while uniquely owned. An unbound payload is opaque to
 * the full graph inspector and fails closed. No lifetime pin is acquired. */
XRT_API xrtownershipref xrtObjectOwnership(const xrtobject* pObject);
XRT_API bool xrtObjectOwnershipTraceBind(xrtobject* pObject, xrtownershiptrace pTrace);



/* 返回对象强引用槽使用的进程期稳定值操作表。 */
XRT_API const xrttypeops* xrtObjectValueOps(void);



/* 按类型声明的负载大小创建堆对象，并执行类型初始化操作。 */
XRT_API xrtobject* xrtObjectCreate(const xrttype* pType);



/* 创建至少容纳指定字节数的对象，用于尾随数据和 native-backed 对象。 */
XRT_API xrtobject* xrtObjectCreateSized(
	const xrttype* pType,
	size_t iSize
);



/* 增加对象强引用；调用方必须已经持有一个有效强引用。 */
XRT_API xrtobject* xrtObjectRef(xrtobject* pObject);



/* 释放一个强引用；最后一个强引用负责执行一次 Drop。 */
XRT_API void xrtObjectUnref(xrtobject* pObject);



/* 借用对象类型、负载地址和真实负载长度。 */
XRT_API const xrttype* xrtObjectType(const xrtobject* pObject);
XRT_API ptr xrtObjectData(xrtobject* pObject);
XRT_API const void* xrtObjectConstData(const xrtobject* pObject);
XRT_API size_t xrtObjectSize(const xrtobject* pObject);



/* 返回瞬时强引用数量，并判断调用方是否持有唯一强引用。 */
XRT_API size_t xrtObjectRefCount(const xrtobject* pObject);
XRT_API bool xrtObjectUnique(const xrtobject* pObject);



/* 初始化、复制、移动、替换和销毁弱引用值。 */
XRT_API bool xrtWeakInit(xrtweak* pWeak, xrtobject* pObject);
XRT_API bool xrtWeakCopy(xrtweak* pTarget, const xrtweak* pSource);
XRT_API bool xrtWeakMove(xrtweak* pTarget, xrtweak* pSource);
XRT_API bool xrtWeakSet(xrtweak* pWeak, xrtobject* pObject);
XRT_API void xrtWeakUnit(xrtweak* pWeak);



/* 判断弱引用当前是否为空或已经过期；结果只是瞬时状态。 */
XRT_API bool xrtWeakExpired(const xrtweak* pWeak);



/* 成功时返回一个新的强引用；对象已销毁时返回空而不设置错误。 */
XRT_API xrtobject* xrtWeakLock(const xrtweak* pWeak);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/runtime_object_graph.h */
/* ========================================================================== */

#ifndef XRT_RUNTIME_OBJECT_GRAPH_H
#define XRT_RUNTIME_OBJECT_GRAPH_H




#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH) && !defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)
	#error "XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH requires XRUNTIME_FEATURE_RUNTIME_OBJECT"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)

typedef struct xrtobjectgraph xrtobjectgraph;



/* 对象图模块稳定错误代码。 */
typedef enum xobjectgrapherror {
	XOBJECT_GRAPH_ERROR_ARGUMENT = 1,
	XOBJECT_GRAPH_ERROR_TRACK,
	XOBJECT_GRAPH_ERROR_TRACE,
	XOBJECT_GRAPH_ERROR_STATE,
	XOBJECT_GRAPH_ERROR_ROOTS
} xobjectgrapherror;



/* 根枚举器可补充运行时栈、生成器和宿主状态中的借用根。 */
typedef bool (*xrtobjectrootproc)(
	xrtobjectvisitor pVisit,
	ptr pVisitContext,
	ptr pContext
);



/* 一次成功收集的稳定统计结果。 */
typedef struct xrtobjectgraphresult {
	size_t TrackedCount;
	size_t EdgeCount;
	size_t RootCount;
	size_t CollectedCount;
} xrtobjectgraphresult;

/* The physical snapshot includes Value shells, shared backing, callable
 * environments and native payloads; TrackedCount/CollectedCount count only
 * members of this graph. Ownership.EdgeCount includes temporary snapshot
 * reference slots, which are internal, never external roots. */
typedef struct xrtobjectgraphownedresult {
	xrtownershipresult Ownership;
	size_t TrackedCount;
	size_t CollectedCount;
} xrtobjectgraphownedresult;



XRT_EXTERN_C_BEGIN



/* 创建和销毁一个不拥有对象强引用的独立对象图。 */
XRT_API xrtobjectgraph* xrtObjectGraphCreate(void);
XRT_API void xrtObjectGraphDestroy(xrtobjectgraph* pGraph);



/* 幂等跟踪对象；同一对象在任意时刻只能属于一个对象图。 */
XRT_API bool xrtObjectGraphTrack(
	xrtobjectgraph* pGraph,
	xrtobject* pObject
);



/* 停止跟踪对象；对象不属于该图时返回 false 且不设置错误。 */
XRT_API bool xrtObjectGraphUntrack(
	xrtobjectgraph* pGraph,
	xrtobject* pObject
);



/* 查询借用对象是否属于图以及图当前跟踪的对象数量。 */
XRT_API bool xrtObjectGraphContains(
	const xrtobjectgraph* pGraph,
	const xrtobject* pObject
);
XRT_API size_t xrtObjectGraphCount(const xrtobjectgraph* pGraph);



/* 在调用方保证对象图静止的安全点收集不可达强引用环。 */
XRT_API bool xrtObjectGraphCollect(
	xrtobjectgraph* pGraph,
	xrtobjectgraphresult* pResult
);



/* 使用可选的宿主根枚举器收集不可达强引用环。 */
XRT_API bool xrtObjectGraphCollectRoots(
	xrtobjectgraph* pGraph,
	xrtobjectrootproc pRoots,
	ptr pContext,
	xrtobjectgraphresult* pResult
);

/* Collect native object cycles using COMPLETE PHYSICAL ownership, not the
 * legacy object-only InstanceOps.Trace. Every visited native payload and
 * opaque handle/context must have its complete adapter bound; absent or
 * inconsistent metadata rejects the whole operation without finalization.
 * Actual external strong references (including native Value Retain and COW
 * aliases) are automatic roots. No host reference is silently discounted.
 * Native objects outside this graph are conservative roots: this collector
 * never partially finalizes another domain's weakly observable cycle.
 * Caller guarantees the ENTIRE transitive graph is quiescent and all Type,
 * Trace and Drop code/data remain resident until this call has returned.
 * Snapshot pins protect all candidate payload allocations through all Drops.
 * Validation and claim finish before the first Drop; failure before commit
 * preserves graph membership, payloads, counts and output. Drop obeys the
 * existing non-failing object destruction contract. This does not establish
 * a safepoint, acquire module code pins, or retire language module globals. */
XRT_API bool xrtObjectGraphCollectOwned(
	xrtobjectgraph* pGraph, xrtobjectgraphownedresult* pResult);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/runtime_call.h */
/* ========================================================================== */

#ifndef XRT_RUNTIME_CALL_H
#define XRT_RUNTIME_CALL_H




#if defined(XRUNTIME_FEATURE_RUNTIME_CALL) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_CALL requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_CALL) && !defined(XRT_FEATURE_VALUE)
	#error "XRUNTIME_FEATURE_RUNTIME_CALL requires XRT_FEATURE_VALUE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_CALL)

#define XRT_CALL_RESULT_INLINE_COUNT 4u



typedef struct xrtcallable xrtcallable;
typedef struct xrtcallframe xrtcallframe;
typedef struct xrtcallresult xrtcallresult;



/* 动态调用模块稳定错误代码。 */
typedef enum xcallerror {
	XCALL_ERROR_CALLABLE = 1,
	XCALL_ERROR_SIGNATURE,
	XCALL_ERROR_FRAME,
	XCALL_ERROR_RESULT,
	XCALL_ERROR_ENTRY,
	XCALL_ERROR_REFERENCE
} xcallerror;



/*
	调用帧只借用 Self、参数、名称、值和上下文，不接管任何资源。
	位置参数包含传入的全部参数，超出固定形参的部分即为变长参数。
*/
struct xrtcallframe {
	xvalue* Self;
	const xrtfunctionsig* Signature;
	size_t ArgumentCount;
	xvalue* const* Arguments;
	size_t KeywordCount;
	const xstrview* KeywordNames;
	xvalue* const* KeywordValues;
	ptr Context;
};



/*
	调用结果拥有其中的 Value 引用；零初始化与 XRT_CALL_RESULT_INIT 都是有效状态。
	字段用于无分配初始化，调用方不得直接修改字段。
*/
struct xrtcallresult {
	size_t Count;
	size_t OverflowCapacity;
	xvalue* Inline[XRT_CALL_RESULT_INLINE_COUNT];
	xvalue** Overflow;
};



#define XRT_CALL_RESULT_INIT { 0 }



/* 动态入口可以并发和重入调用，具体环境是否支持并发由入口实现决定。 */
typedef bool (*xrtcallproc)(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
);



/* 环境释放器在最后一个 callable 引用释放时执行一次。 */
typedef void (*xrtcalldrop)(ptr pEnvironment);

/* Explicit immutable resident environment family. Trace covers every strong
 * payload/code edge; Drop runs only after the active invocation's full return
 * tail. The producer certifies all environment transitions are coordinated.
 * A collector must authorize this exact descriptor, not an arbitrary tracer.
 * Signature/Entry and this descriptor are resident or kept alive by the
 * environment until Drop returns; none are read after retired-environment
 * Finish. A native creator that only binds Trace does NOT get this contract. */
typedef struct xrtcallableownershipv1 {
	size_t size;
	xrtownershiptrace Trace;
	xrtcalldrop Drop;
} xrtcallableownershipv1;



XRT_EXTERN_C_BEGIN

/* Quiescent physical ownership view; no reference is acquired. A nonempty
 * environment is opaque until its producer binds a complete strong-edge
 * trace before publication. The callback and signature must remain resident;
 * this metadata does not by itself acquire a code lease. */
XRT_API xrtownershipref xrtCallableOwnership(const xrtcallable* pCallable);
XRT_API bool xrtCallableOwnershipTraceBind(xrtcallable* pCallable, xrtownershiptrace pTrace);
XRT_API bool xrtCallableOwnershipBindV1(xrtcallable* pCallable, const xrtcallableownershipv1* pPolicy);
/* Immutable resident-policy identity probe on a borrowed LIVE callable.
 * Active invocation is allowed; no Count/Trace/drop/user callback is called.
 * The caller owns a reference and provides a known permanent policy. This is
 * NOT graph lifecycle/quiescence admission and grants no environment borrow.
 * False leaves the diagnostic unchanged. Equal policy copies are not identity. */
XRT_API bool xrtCallableAdmitsOwnershipPolicyV1(const xrtcallable* pCallable,
    const xrtcallableownershipv1* pExpectedPolicy);
XRT_API const xrtownershipadapterv1* xrtCallableOwnershipAdapterV1(
	xrtownershipref Reference, const xrtcallableownershipv1* pExpectedPolicy);



/* 检查调用帧结构、参数绑定、可选参数、变长参数和关键字参数是否符合有效签名。 */
XRT_API bool xrtCallFrameValidate(const xrtcallframe* pFrame);



/* 按原始位置返回借用参数，越界时报告范围错误。 */
XRT_API xvalue* xrtCallFrameArgument(
	const xrtcallframe* pFrame,
	size_t iIndex
);



/* 按完整名称返回借用关键字值；未提供时返回空而不设置错误。 */
XRT_API xvalue* xrtCallFrameKeyword(
	const xrtcallframe* pFrame,
	xstrview Name
);



/* 按有效签名的形参下标返回位置或关键字传入的借用值；可选参数缺失时返回空。 */
XRT_API xvalue* xrtCallFrameParameter(
	const xrtcallframe* pFrame,
	size_t iIndex
);



/* 初始化一个新的调用结果；已经持有资源的结果必须先 Unit。 */
XRT_API void xrtCallResultInit(xrtcallresult* pResult);



/* 释放结果值但保留溢出容量，结果可以继续复用。 */
XRT_API void xrtCallResultClear(xrtcallresult* pResult);



/* 释放结果值和溢出存储。 */
XRT_API void xrtCallResultUnit(xrtcallresult* pResult);



/* 返回调用结果数量。 */
XRT_API size_t xrtCallResultCount(const xrtcallresult* pResult);



/* 返回指定下标借用的结果值。 */
XRT_API xvalue* xrtCallResultGet(
	const xrtcallresult* pResult,
	size_t iIndex
);



/* 增加值引用后替换已有下标或追加到末尾，不允许创建稀疏结果。 */
XRT_API bool xrtCallResultSet(
	xrtcallresult* pResult,
	size_t iIndex,
	const xvalue* pValue
);



/* 移交值引用后替换已有下标或追加到末尾，成功时清空来源槽。 */
XRT_API bool xrtCallResultSetTake(
	xrtcallresult* pResult,
	size_t iIndex,
	xvalue** pValue
);



/* 增加值引用后追加一个结果。 */
XRT_API bool xrtCallResultPush(
	xrtcallresult* pResult,
	const xvalue* pValue
);



/* 移交值引用后追加一个结果。 */
XRT_API bool xrtCallResultPushTake(
	xrtcallresult* pResult,
	xvalue** pValue
);



/* 释放目标原值并把完整结果所有权移动到目标。 */
XRT_API bool xrtCallResultMove(
	xrtcallresult* pTarget,
	xrtcallresult* pSource
);



/* 创建不可变 callable；签名可以为空，非空签名及其引用必须覆盖 callable 生命周期。 */
XRT_API xrtcallable* xrtCallableCreate(
	const xrtfunctionsig* pSignature,
	xrtcallproc pEntry,
	ptr pEnvironment,
	xrtcalldrop pDropEnvironment
);



/* 增加或释放 callable 引用，最后一个引用负责释放环境。 */
XRT_API xrtcallable* xrtCallableRef(xrtcallable* pCallable);
XRT_API void xrtCallableUnref(xrtcallable* pCallable);



/* 返回拥有一个 callable 强引用的 C ABI 槽类型描述。 */
XRT_API const xrttype* xrtTypeCallable(void);



/* 返回 callable 借用的签名；动态 callable 可以返回空。 */
XRT_API const xrtfunctionsig* xrtCallableSignature(
	const xrtcallable* pCallable
);



/* 返回 callable 的稳定签名 ID；动态 callable 返回零。 */
XRT_API uint64 xrtCallableSignatureId(const xrtcallable* pCallable);



/*
	验证调用帧并通过临时结果执行入口。
	失败时清理入口的部分结果并保留调用方原结果，成功时原子替换结果。
*/
XRT_API bool xrtCallableInvoke(
	const xrtcallable* pCallable,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/runtime_type_future.h */
/* ========================================================================== */

#ifndef XRT_RUNTIME_TYPE_FUTURE_H
#define XRT_RUNTIME_TYPE_FUTURE_H




#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE) && \
	!defined(XRT_FEATURE_FUTURE)
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE requires XRT_FEATURE_FUTURE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE)

XRT_EXTERN_C_BEGIN



/* 返回拥有一个 Future 消费端引用的 C ABI 槽类型描述。 */
XRT_API const xrttype* xrtTypeFuture(void);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/runtime_value.h */
/* ========================================================================== */

#ifndef XRT_RUNTIME_VALUE_H
#define XRT_RUNTIME_VALUE_H


#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT requires XRUNTIME_FEATURE_RUNTIME_OBJECT"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) && !defined(XRT_FEATURE_VALUE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT requires XRT_FEATURE_VALUE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_CALL)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE requires XRUNTIME_FEATURE_RUNTIME_CALL"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE requires XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) && \
	!defined(XRT_FEATURE_VALUE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE requires XRT_FEATURE_VALUE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK requires XRUNTIME_FEATURE_RUNTIME_OBJECT"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) && !defined(XRT_FEATURE_VALUE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK requires XRT_FEATURE_VALUE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) && \
	!defined(XRT_FEATURE_VALUE_GRAPH)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE requires XRT_FEATURE_VALUE_GRAPH"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE requires XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE requires XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS requires XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS requires XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)

/* 运行时 Value 桥接模块稳定错误代码。 */
typedef enum xruntimevalueerror {
	XRUNTIME_VALUE_ERROR_TYPE = 1,
	XRUNTIME_VALUE_ERROR_OBJECT,
	XRUNTIME_VALUE_ERROR_CALLABLE,
	XRUNTIME_VALUE_ERROR_FUTURE,
	XRUNTIME_VALUE_ERROR_WEAK,
	XRUNTIME_VALUE_ERROR_OWNERSHIP,
	XRUNTIME_VALUE_ERROR_TRACE,
	XRUNTIME_VALUE_ERROR_ROOTS
} xruntimevalueerror;



XRT_EXTERN_C_BEGIN



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT)

/* 增加对象强引用并创建拥有该引用的 Value Handle。 */
XRT_API xvalue* xrtValueRuntimeObject(xrtobject* pObject);



/* 把对象强引用移交给 Value Handle，成功时清空来源槽。 */
XRT_API xvalue* xrtValueRuntimeObjectTake(xrtobject** pObject);



/* 判断值是否是运行时对象桥接值，不把动态字典对象误判为类对象。 */
XRT_API bool xrtValueIsRuntimeObject(const xvalue* pValue);



/* 返回 Value Handle 借用的运行时对象，值负责维持强生命周期。 */
XRT_API xrtobject* xrtValueGetRuntimeObject(const xvalue* pValue);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)

/* 同步进度桥只借用 callable Value，调用方必须覆盖整个操作生命周期。 */
typedef struct xrtprogresscall {
	xvalue* Callback;
	bool InvokeFailed;
} xrtprogresscall;

/* 增加 callable 引用并创建拥有该引用的 Value Handle。 */
XRT_API xvalue* xrtValueCallable(xrtcallable* pCallable);



/* 把 callable 引用移交给 Value Handle，成功时清空来源槽。 */
XRT_API xvalue* xrtValueCallableTake(xrtcallable** pCallable);
/* Recognizes only this resident callable Handle bridge. Its callable child
 * still requires separately authorized environment ownership. */
XRT_API const xrtownershipadapterv1* xrtValueCallableOwnershipAdapterV1(xrtownershipref Reference);



/* 判断值是否是 callable 桥接值。 */
XRT_API bool xrtValueIsCallable(const xvalue* pValue);



/* 返回 Value Handle 借用的 callable。 */
XRT_API xrtcallable* xrtValueGetCallable(const xvalue* pValue);



/* 返回 callable 借用的函数签名，动态 callable 返回空。 */
XRT_API const xrtfunctionsig* xrtValueCallableSignature(
	const xvalue* pValue
);



/* 调用一个 callable Value，调用契约与 xrtCallableInvoke 一致。 */
XRT_API bool xrtValueInvoke(
	const xvalue* pCallable,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
);



/* 初始化同步进度桥；null Value 与空指针都表示不报告进度。 */
XRT_API void xrtProgressCallInit(
	xrtprogresscall* pContext,
	xvalue* pCallback
);



/* 把进度转发为 callable(processed, total, output) 并读取 bool 结果。 */
XRT_API bool xrtProgressCallInvoke(
	const xrtprogress* pProgress,
	ptr pUserData
);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
/* 增加 Future 引用并创建拥有该引用的 Value Handle。 */
XRT_API xvalue* xrtValueFuture(xfuture* pFuture);



/* 把 Future 引用移交给 Value Handle，成功时清空来源槽。 */
XRT_API xvalue* xrtValueFutureTake(xfuture** pFuture);

/* Only this resident Value Handle family is admitted; its Future control
 * block and payload require independent resolver authorization. */
XRT_API const xrtownershipadapterv1* xrtValueFutureOwnershipAdapterV1(xrtownershipref Reference);



/* 判断值是否是 Future 桥接值。 */
XRT_API bool xrtValueIsFuture(const xvalue* pValue);



/* 返回 Value Handle 借用的 Future。 */
XRT_API xfuture* xrtValueGetFuture(const xvalue* pValue);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK)

/* 复制弱引用并创建拥有该控制块引用的 Value Handle。 */
XRT_API xvalue* xrtValueWeak(const xrtweak* pWeak);



/* 把弱引用移交给 Value Handle，成功时把来源变为空弱引用。 */
XRT_API xvalue* xrtValueWeakTake(xrtweak* pWeak);



/* 判断值是否是运行时弱引用桥接值。 */
XRT_API bool xrtValueIsWeak(const xvalue* pValue);



/* 把 Value 中的弱引用复制到已初始化目标，并替换目标原值。 */
XRT_API bool xrtValueGetWeak(
	const xvalue* pValue,
	xrtweak* pWeak
);



/* 查询 Value 中的弱引用是否为空或已经过期。 */
XRT_API bool xrtValueWeakExpired(const xvalue* pValue);



/* 成功时从 Value 中的弱引用取得一个新的对象强引用。 */
XRT_API xrtobject* xrtValueWeakLock(const xvalue* pValue);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)

/* 返回拥有一个 xvalue 引用的 C ABI 槽所使用的进程期类型描述。 */
XRT_API const xrttype* xrtTypeValue(void);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)

/*
	枚举完整 Value 所有权图实际持有的运行时对象强引用。
	共享 Value 外壳和容器 backing 只追踪一次，重复拥有槽仍按实际引用计数保留。
	访问器返回 false 时必须设置错误；未设置时函数报告运行时追踪状态错误。
	成功返回会恢复调用前错误，不保留访问器在成功路径留下的临时错误。
*/
XRT_API bool xrtValueTraceRuntimeObjects(
	const xvalue* pValue,
	xrtobjectvisitor pVisit,
	ptr pContext
);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)

/* 使用一个外部 Value 所有权图作为显式根执行对象图收集。 */
XRT_API bool xrtObjectGraphCollectValueRoot(
	xrtobjectgraph* pGraph,
	const xvalue* pRoot,
	xrtobjectgraphresult* pResult
);



/* 使用一组外部 Value 所有权图作为显式根执行对象图收集。 */
XRT_API bool xrtObjectGraphCollectValueRoots(
	xrtobjectgraph* pGraph,
	const xvalue* const* pRoots,
	size_t iRootCount,
	xrtobjectgraphresult* pResult
);

#endif



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/runtime_convert.h */
/* ========================================================================== */

#ifndef XRT_RUNTIME_CONVERT_H
#define XRT_RUNTIME_CONVERT_H


#if defined(XRUNTIME_FEATURE_VALUE_CONVERT)
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_CONVERT requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING) && \
	(!defined(XRUNTIME_FEATURE_RUNTIME_CONVERT) || \
	 !defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) || \
	 !defined(XRT_FEATURE_NUMBER_INTEGER) || \
	 !defined(XRT_FEATURE_NUMBER_FLOAT) || \
	 !defined(XRT_FEATURE_TIME_TEXT))
	#error "XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING requires RUNTIME_CONVERT, RUNTIME_TYPE_STRING, NUMBER_INTEGER, NUMBER_FLOAT and TIME_TEXT"
#endif

#if defined(XRUNTIME_FEATURE_VALUE_CONVERT) && \
	(!defined(XRUNTIME_FEATURE_RUNTIME_CONVERT) || \
	 !defined(XRT_FEATURE_VALUE))
	#error "XRUNTIME_FEATURE_VALUE_CONVERT requires RUNTIME_CONVERT and VALUE"
#endif

#if defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING) && \
	(!defined(XRUNTIME_FEATURE_VALUE_CONVERT) || \
	 !defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING))
	#error "XRUNTIME_FEATURE_VALUE_CONVERT_STRING requires VALUE_CONVERT and RUNTIME_CONVERT_STRING"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT)

/* 转换模式逐级包含：显式转换也允许无损拓宽和同类型复制。 */
typedef enum xtypeconvertmode {
	XTYPE_CONVERT_EXACT = 0,
	XTYPE_CONVERT_WIDEN,
	XTYPE_CONVERT_EXPLICIT
} xtypeconvertmode;



/* 运行时类型转换层稳定错误代码。 */
typedef enum xtypeconverterror {
	XTYPE_CONVERT_ERROR_ARGUMENT = 1,
	XTYPE_CONVERT_ERROR_MODE,
	XTYPE_CONVERT_ERROR_TYPE,
	XTYPE_CONVERT_ERROR_RANGE,
	XTYPE_CONVERT_ERROR_PARSE,
	XTYPE_CONVERT_ERROR_OPERATION
} xtypeconverterror;



XRT_EXTERN_C_BEGIN



/* 判断源类型的全部有效值是否都能被目标类型无损表示。 */
XRT_API bool xrtTypeCanWiden(
	const xrttype* pSourceType,
	const xrttype* pTargetType
);



/* 判断两个类型在指定模式下是否存在稳定的内建转换路径。 */
XRT_API bool xrtTypeCanConvert(
	const xrttype* pSourceType,
	const xrttype* pTargetType,
	xtypeconvertmode Mode
);



/*
	把借用的源值转换后写入已经初始化的目标值。
	失败时目标保持不变；不同类型的源和目标存储不得重叠。
*/
XRT_API bool xrtTypeConvert(
	const xrttype* pSourceType,
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
);



XRT_EXTERN_C_END

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)

XRT_EXTERN_C_BEGIN



/*
	把一个借用类型值同步分块格式化为 UTF-8 文本。
	内建类型不分配中间字符串；writer 接收的分块只在回调期间有效。
*/
XRT_API bool xrtTypeFormat(
	const xrttype* pType,
	const void* pValue,
	xrttypewriter pWrite,
	ptr pContext
);



/* 把借用类型值格式化为由 xrtFree 释放的零结尾 UTF-8 字符串。 */
XRT_API str xrtTypeToString(
	const xrttype* pType,
	const void* pValue
);



XRT_EXTERN_C_END

#endif



#if defined(XRUNTIME_FEATURE_VALUE_CONVERT)

XRT_EXTERN_C_BEGIN



/*
	把动态 Value 标量转换后写入已经初始化的目标值。
	失败时目标保持不变；容器、字节和句柄不属于标量转换。
*/
XRT_API bool xrtValueConvertTo(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/runtime_type_string.h */
/* ========================================================================== */

#ifndef XRT_RUNTIME_TYPE_STRING_H
#define XRT_RUNTIME_TYPE_STRING_H




#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) && \
	!defined(XRT_FEATURE_STRING)
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_STRING requires XRT_FEATURE_STRING"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) && \
	!defined(XRT_FEATURE_HASH64)
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_STRING requires XRT_FEATURE_HASH64"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_STRING requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING)

XRT_EXTERN_C_BEGIN



/* 返回拥有一个零结尾 XRT 字符串的 C ABI 槽类型描述。 */
XRT_API const xrttype* xrtTypeString(void);

/* Owned xstrview slot: exact byte length, including embedded NUL. Copy is
 * transactional; move detaches the source; drop frees Data and clears both
 * fields. This descriptor is resident and distinct from the char* ABI above. */
XRT_API const xrttype* xrtTypeStringView(void);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/typed_array.h */
/* ========================================================================== */

#ifndef XRT_TYPED_ARRAY_H
#define XRT_TYPED_ARRAY_H




#if defined(XRUNTIME_FEATURE_TYPED_ARRAY) && !defined(XRT_FEATURE_ARRAY)
	#error "XRUNTIME_FEATURE_TYPED_ARRAY requires XRT_FEATURE_ARRAY"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_ARRAY requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY)

/* 类型数组连续保存 ItemType::Size 字节值，并拥有每一个元素的生命周期。 */
typedef struct xtypedarray {
	xarray Storage;
	const xrttype* ItemType;
	uint32 Flags;
} xtypedarray;



/* 类型数组模块稳定错误代码。 */
typedef enum xtypedarrayerror {
	XTYPED_ARRAY_ERROR_ARGUMENT = 1,
	XTYPED_ARRAY_ERROR_TYPE,
	XTYPED_ARRAY_ERROR_RANGE,
	XTYPED_ARRAY_ERROR_OPERATION,
	XTYPED_ARRAY_ERROR_STATE
} xtypedarrayerror;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有元素值的空类型数组。 */
XRT_API bool xrtTypedArrayInit(
	xtypedarray* pArray,
	const xrttype* pItemType
);
XRT_API xtypedarray* xrtTypedArrayCreate(const xrttype* pItemType);
XRT_API void xrtTypedArrayUnit(xtypedarray* pArray);
XRT_API void xrtTypedArrayDestroy(xtypedarray* pArray);



/* 返回借用元素类型、元素数和当前容量。 */
XRT_API const xrttype* xrtTypedArrayItemType(const xtypedarray* pArray);
XRT_API size_t xrtTypedArrayCount(const xtypedarray* pArray);
XRT_API size_t xrtTypedArrayCapacity(const xtypedarray* pArray);



/* 返回活动元素连续区的可写或只读借用；结构修改后旧地址失效。 */
XRT_API ptr xrtTypedArrayData(xtypedarray* pArray);
XRT_API const void* xrtTypedArrayConstData(const xtypedarray* pArray);



/* 预留、调整、裁剪或清空数组；新增元素按类型初始化。
 * Resize 增长分配或初始化失败，保留原地址、容量、数量与活动元素。 */
XRT_API bool xrtTypedArrayReserve(xtypedarray* pArray, size_t iCapacity);
XRT_API bool xrtTypedArrayResize(xtypedarray* pArray, size_t iCount);
/* Per-growth defaults are distinct from empty destinations used by Copy/Move.
 * The initializer receives a zeroed inactive slot and the borrowed item type;
 * success transfers that value to the array. Failure must release its own
 * partial resources and set an XRT error. Context is borrowed only for this
 * synchronous call, never retained. NULL selects the ordinary type Init.
 * Existing values, capacity and addresses survive any growth failure; only
 * successfully initialized new slots are dropped, in reverse order. Same-
 * array callback/allocator reentry is rejected. Shrink/equal never invoke it. */
typedef bool (*xrttypedarrayinitializer)(ptr pValue, const xrttype* pType, ptr pContext);
XRT_API bool xrtTypedArrayResizeWithInitializer(
    xtypedarray* pArray, size_t iCount, xrttypedarrayinitializer Initializer, ptr pContext
);
XRT_API bool xrtTypedArrayTrim(xtypedarray* pArray);
XRT_API void xrtTypedArrayClear(xtypedarray* pArray);



/* 返回指定下标的借用元素地址，结构修改后旧地址失效。 */
XRT_API ptr xrtTypedArrayGet(xtypedarray* pArray, size_t iIndex);
XRT_API const void* xrtTypedArrayConstGet(
	const xtypedarray* pArray,
	size_t iIndex
);



/* 复制追加、插入或替换元素；失败时数组保持原值。 */
XRT_API bool xrtTypedArrayPush(xtypedarray* pArray, const void* pItem);
XRT_API bool xrtTypedArrayInsert(
	xtypedarray* pArray,
	size_t iIndex,
	const void* pItem
);
XRT_API bool xrtTypedArraySet(
	xtypedarray* pArray,
	size_t iIndex,
	const void* pItem
);



/* 删除元素，或把一个元素移动到已初始化输出值后删除。 */
XRT_API bool xrtTypedArrayRemove(
	xtypedarray* pArray,
	size_t iIndex,
	size_t iCount
);
XRT_API bool xrtTypedArrayTake(
	xtypedarray* pArray,
	size_t iIndex,
	ptr pValue
);
XRT_API bool xrtTypedArrayPop(xtypedarray* pArray, ptr pValue);



/* 交换、反转、查找或判断元素；查找未命中返回 SIZE_MAX。 */
XRT_API bool xrtTypedArraySwap(
	xtypedarray* pArray,
	size_t iLeft,
	size_t iRight
);
XRT_API bool xrtTypedArrayReverse(xtypedarray* pArray);
XRT_API size_t xrtTypedArrayFind(
	const xtypedarray* pArray,
	const void* pItem
);
XRT_API bool xrtTypedArrayContains(
	const xtypedarray* pArray,
	const void* pItem
);



/* 事务追加同类型数组，或把两个数组深复制并拼接为独立数组。 */
XRT_API bool xrtTypedArrayAppend(
	xtypedarray* pTarget,
	const xtypedarray* pSource
);
/* 精确区间复制为独立数组；反序只改变交付顺序，不改变来源。 */
XRT_API xtypedarray* xrtTypedArraySlice(
	const xtypedarray* pArray,
	size_t iIndex,
	size_t iCount,
	bool bReverse
);
/* 最多移交尾部指定数量为独立数组；分配失败时来源完全不变。 */
XRT_API xtypedarray* xrtTypedArrayTakeTail(
	xtypedarray* pArray,
	size_t iMaxCount,
	bool bReverse
);
XRT_API xtypedarray* xrtTypedArrayClone(const xtypedarray* pArray);
XRT_API xtypedarray* xrtTypedArrayConcat(
	const xtypedarray* pLeft,
	const xtypedarray* pRight
);



/* 比较两个数组的精确类型身份、元素数量和元素顺序。 */
XRT_API bool xrtTypedArrayEquals(
	const xtypedarray* pLeft,
	const xtypedarray* pRight
);



/* 验证对象数组类型描述，并返回其共享实例操作表。 */
XRT_API bool xrtTypedArrayTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedArrayInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/typed_list.h */
/* ========================================================================== */

#ifndef XRT_TYPED_LIST_H
#define XRT_TYPED_LIST_H




#if defined(XRUNTIME_FEATURE_TYPED_LIST) && !defined(XRT_FEATURE_INT_MAP)
	#error "XRUNTIME_FEATURE_TYPED_LIST requires XRT_FEATURE_INT_MAP"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_LIST) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_LIST requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST)

/* 类型列表按 int64 键排序保存稳定地址值，并拥有每一个值的生命周期。 */
typedef struct xtypedlist {
	xintmap Storage;
	const xrttype* ItemType;
	uint32 Flags;
} xtypedlist;



/* 类型列表外置迭代器按键升序或降序借用稳定值槽。 */
typedef struct xtypedlistiter {
	xintmapiter Base;
	xtypedlist* List;
} xtypedlistiter;



/* 类型列表模块稳定错误代码。 */
typedef enum xtypedlisterror {
	XTYPED_LIST_ERROR_ARGUMENT = 1,
	XTYPED_LIST_ERROR_TYPE,
	XTYPED_LIST_ERROR_KEY,
	XTYPED_LIST_ERROR_OPERATION,
	XTYPED_LIST_ERROR_STATE
} xtypedlisterror;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有类型值的空稀疏列表。 */
XRT_API bool xrtTypedListInit(
	xtypedlist* pList,
	const xrttype* pItemType
);
XRT_API xtypedlist* xrtTypedListCreate(const xrttype* pItemType);
XRT_API void xrtTypedListUnit(xtypedlist* pList);
XRT_API void xrtTypedListDestroy(xtypedlist* pList);



/* 返回借用元素类型和当前键值数量。 */
XRT_API const xrttype* xrtTypedListItemType(const xtypedlist* pList);
XRT_API size_t xrtTypedListCount(const xtypedlist* pList);



/* 清空全部值，或释放空闲节点池页。 */
XRT_API bool xrtTypedListClear(xtypedlist* pList);
XRT_API size_t xrtTypedListTrim(xtypedlist* pList, size_t iRetainEmpty);



/* 复制设置指定键，或在最大键后追加并返回实际键。 */
XRT_API bool xrtTypedListSet(
	xtypedlist* pList,
	int64 iKey,
	const void* pItem
);
XRT_API bool xrtTypedListAppend(
	xtypedlist* pList,
	const void* pItem,
	int64* pKey
);



/* 返回指定键的借用值槽；缺失键是正常结果。 */
XRT_API ptr xrtTypedListGet(xtypedlist* pList, int64 iKey);
XRT_API const void* xrtTypedListConstGet(
	const xtypedlist* pList,
	int64 iKey
);
XRT_API bool xrtTypedListHas(const xtypedlist* pList, int64 iKey);



/* 按键顺序返回指定位置的借用值槽和可选实际键。 */
XRT_API ptr xrtTypedListAt(
	xtypedlist* pList,
	size_t iIndex,
	int64* pKey
);
XRT_API const void* xrtTypedListConstAt(
	const xtypedlist* pList,
	size_t iIndex,
	int64* pKey
);



/* 删除指定键，或把值移动到外部已初始化输出后删除。 */
XRT_API bool xrtTypedListRemove(xtypedlist* pList, int64 iKey);
XRT_API bool xrtTypedListTake(
	xtypedlist* pList,
	int64 iKey,
	ptr pValue
);



/* 查找第一个相等值或判断是否存在；未找到不设置错误。 */
XRT_API bool xrtTypedListFind(
	const xtypedlist* pList,
	const void* pItem,
	int64* pKey
);
XRT_API bool xrtTypedListContains(
	const xtypedlist* pList,
	const void* pItem
);



/* 失败原子地合并同类型列表、深复制列表或比较键值内容。 */
XRT_API bool xrtTypedListMerge(
	xtypedlist* pTarget,
	const xtypedlist* pSource,
	bool bReplace
);
XRT_API xtypedlist* xrtTypedListClone(const xtypedlist* pList);
XRT_API bool xrtTypedListEquals(
	const xtypedlist* pLeft,
	const xtypedlist* pRight
);



/* 启动完整或有界的正反迭代。 */
XRT_API bool xrtTypedListIterBegin(
	xtypedlist* pList,
	xtypedlistiter* pIterator
);
XRT_API bool xrtTypedListIterRBegin(
	xtypedlist* pList,
	xtypedlistiter* pIterator
);
XRT_API bool xrtTypedListIterFrom(
	xtypedlist* pList,
	int64 iKey,
	xtypedlistiter* pIterator
);
XRT_API bool xrtTypedListIterRFrom(
	xtypedlist* pList,
	int64 iKey,
	xtypedlistiter* pIterator
);
XRT_API ptr xrtTypedListIterNext(
	xtypedlistiter* pIterator,
	int64* pKey
);
XRT_API void xrtTypedListIterEnd(xtypedlistiter* pIterator);



/* 验证对象列表类型描述，并返回其共享实例操作表。 */
XRT_API bool xrtTypedListTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedListInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/typed_set.h */
/* ========================================================================== */

#ifndef XRT_TYPED_SET_H
#define XRT_TYPED_SET_H




#if defined(XRUNTIME_FEATURE_TYPED_SET) && !defined(XRT_FEATURE_SET)
	#error "XRUNTIME_FEATURE_TYPED_SET requires XRT_FEATURE_SET"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_SET) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_SET requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET)

/* 显式键策略只要求相等和散列，不为元素声明排序能力。
 * 记录、回调代码和 UserData 都由调用方保持不可变，并必须比集合及其
 * 全部克隆/集合运算结果存活更久；相等键必须具有相同散列。
 * 回调遵循 xset 的错误边界与不可重入规则。 */
typedef struct xtypedsetkeypolicy {
	xsethash Hash;
	xsetequal Equal;
	ptr UserData;
} xtypedsetkeypolicy;



/* 类型集合拥有规范值；键策略在初始化时确定，不能通过底层存储更换。 */
typedef struct xtypedset {
	xset Storage;
	const xrttype* ItemType;
	const xtypedsetkeypolicy* KeyPolicy;
} xtypedset;



/* 类型集合外置迭代器按稳定插入顺序借用只读规范值。 */
typedef struct xtypedsetiter {
	xsetiter Base;
	xtypedset* Set;
} xtypedsetiter;



/* 类型集合模块稳定错误代码。 */
typedef enum xtypedseterror {
	XTYPED_SET_ERROR_ARGUMENT = 1,
	XTYPED_SET_ERROR_TYPE,
	XTYPED_SET_ERROR_RANGE,
	XTYPED_SET_ERROR_OPERATION,
	XTYPED_SET_ERROR_STATE
} xtypedseterror;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有类型值的空集合。 */
XRT_API bool xrtTypedSetInit(
	xtypedset* pSet,
	const xrttype* pItemType
);
XRT_API xtypedset* xrtTypedSetCreate(const xrttype* pItemType);
/* 生命周期仍由 ItemType 提供；元素只须有效、非零大小且可复制。
 * Policy 必须非空，Hash/Equal 必须成对非空。普通 Init/Create 仍使用
 * 类型 Compare/Hash；此入口不会给类型补入虚假的 Compare/Hash 操作。 */
XRT_API bool xrtTypedSetInitWithKeyPolicy(
	xtypedset* pSet,
	const xrttype* pItemType,
	const xtypedsetkeypolicy* pPolicy
);
XRT_API xtypedset* xrtTypedSetCreateWithKeyPolicy(
	const xrttype* pItemType,
	const xtypedsetkeypolicy* pPolicy
);
XRT_API void xrtTypedSetUnit(xtypedset* pSet);
XRT_API void xrtTypedSetDestroy(xtypedset* pSet);



/* 返回借用元素类型、当前元素数和再次扩容前的容量。 */
XRT_API const xrttype* xrtTypedSetItemType(const xtypedset* pSet);
XRT_API size_t xrtTypedSetCount(const xtypedset* pSet);
XRT_API size_t xrtTypedSetCapacity(const xtypedset* pSet);



/* 清空、预留或裁剪集合存储。 */
XRT_API bool xrtTypedSetClear(xtypedset* pSet);
XRT_API bool xrtTypedSetReserve(xtypedset* pSet, size_t iCapacity);
XRT_API bool xrtTypedSetTrim(xtypedset* pSet);



/* 返回已有或失败原子地复制加入的只读规范值。 */
XRT_API const void* xrtTypedSetGetOrAdd(
	xtypedset* pSet,
	const void* pItem,
	bool* pNew
);
XRT_API bool xrtTypedSetAdd(xtypedset* pSet, const void* pItem);



/* 返回或判断等价规范值；缺失是正常结果。 */
XRT_API const void* xrtTypedSetGet(
	const xtypedset* pSet,
	const void* pItem
);
XRT_API bool xrtTypedSetHas(
	const xtypedset* pSet,
	const void* pItem
);



/* 删除等价值，或把规范值移动到外部已初始化输出后删除。 */
XRT_API bool xrtTypedSetRemove(xtypedset* pSet, const void* pItem);
XRT_API bool xrtTypedSetTake(
	xtypedset* pSet,
	const void* pItem,
	ptr pValue
);



/* 按插入顺序返回指定位置的只读规范值，复杂度为 O(n)。 */
XRT_API const void* xrtTypedSetAt(
	const xtypedset* pSet,
	size_t iIndex
);



/* 启动按插入顺序或逆序的外置迭代。 */
XRT_API bool xrtTypedSetIterBegin(
	xtypedset* pSet,
	xtypedsetiter* pIterator
);
XRT_API bool xrtTypedSetIterRBegin(
	xtypedset* pSet,
	xtypedsetiter* pIterator
);
XRT_API const void* xrtTypedSetIterNext(xtypedsetiter* pIterator);
XRT_API void xrtTypedSetIterEnd(xtypedsetiter* pIterator);



/* 事务合并同类型集合，并深复制创建独立集合。 */
XRT_API bool xrtTypedSetMerge(
	xtypedset* pTarget,
	const xtypedset* pSource
);
XRT_API xtypedset* xrtTypedSetClone(const xtypedset* pSet);



/* 创建两个同类型集合的并、交、差或对称差。 */
XRT_API xtypedset* xrtTypedSetUnion(
	const xtypedset* pLeft,
	const xtypedset* pRight
);
XRT_API xtypedset* xrtTypedSetIntersection(
	const xtypedset* pLeft,
	const xtypedset* pRight
);
XRT_API xtypedset* xrtTypedSetDifference(
	const xtypedset* pLeft,
	const xtypedset* pRight
);
XRT_API xtypedset* xrtTypedSetSymmetricDifference(
	const xtypedset* pLeft,
	const xtypedset* pRight
);



/* 判断同类型集合的包含、相离或相等关系。 */
XRT_API bool xrtTypedSetIsSubset(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	bool bProper
);
XRT_API bool xrtTypedSetIsSuperset(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	bool bProper
);
XRT_API bool xrtTypedSetIsDisjoint(
	const xtypedset* pLeft,
	const xtypedset* pRight
);
XRT_API bool xrtTypedSetEquals(
	const xtypedset* pLeft,
	const xtypedset* pRight
);



/* 验证对象集合类型描述，并返回其共享实例操作表。 */
XRT_API bool xrtTypedSetTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedSetInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/typed_dict.h */
/* ========================================================================== */

#ifndef XRT_TYPED_DICT_H
#define XRT_TYPED_DICT_H




#if defined(XRUNTIME_FEATURE_TYPED_DICT) && !defined(XRT_FEATURE_MAP)
	#error "XRUNTIME_FEATURE_TYPED_DICT requires XRT_FEATURE_MAP"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_DICT) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_DICT requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT)

/* 类型字典复制保存文本键，并拥有每一个运行时类型值。 */
typedef struct xtypeddict {
	xmap Storage;
	const xrttype* ItemType;
} xtypeddict;



/* 类型字典外置迭代器按稳定插入顺序借用键和值槽。 */
typedef struct xtypeddictiter {
	xmapiter Base;
	xtypeddict* Dict;
} xtypeddictiter;



/* 类型字典模块稳定错误代码。 */
typedef enum xtypeddicterror {
	XTYPED_DICT_ERROR_ARGUMENT = 1,
	XTYPED_DICT_ERROR_TYPE,
	XTYPED_DICT_ERROR_RANGE,
	XTYPED_DICT_ERROR_OPERATION,
	XTYPED_DICT_ERROR_STATE
} xtypeddicterror;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有类型值的空字典。 */
XRT_API bool xrtTypedDictInit(
	xtypeddict* pDict,
	const xrttype* pItemType
);
XRT_API xtypeddict* xrtTypedDictCreate(const xrttype* pItemType);
XRT_API void xrtTypedDictUnit(xtypeddict* pDict);
XRT_API void xrtTypedDictDestroy(xtypeddict* pDict);



/* 返回借用元素类型、当前键数和再次扩容前的容量。 */
XRT_API const xrttype* xrtTypedDictItemType(const xtypeddict* pDict);
XRT_API size_t xrtTypedDictCount(const xtypeddict* pDict);
XRT_API size_t xrtTypedDictCapacity(const xtypeddict* pDict);



/* 清空、预留或裁剪字典存储。 */
XRT_API bool xrtTypedDictClear(xtypeddict* pDict);
XRT_API bool xrtTypedDictReserve(xtypeddict* pDict, size_t iCapacity);
XRT_API bool xrtTypedDictTrim(xtypeddict* pDict);



/* 返回已有值槽，或按元素类型默认初始化一个新值。 */
XRT_API ptr xrtTypedDictGetOrAdd(
	xtypeddict* pDict,
	xstrview Key,
	bool* pNew
);



/* 失败原子地复制插入或替换一个键值。 */
XRT_API bool xrtTypedDictSet(
	xtypeddict* pDict,
	xstrview Key,
	const void* pItem
);



/* 成功时把外部已初始化值移交给字典，并把来源恢复为类型空值。 */
XRT_API bool xrtTypedDictSetTake(
	xtypeddict* pDict,
	xstrview Key,
	ptr pItem
);



/* 返回指定键的可写或只读借用值槽；缺失是正常结果。 */
XRT_API ptr xrtTypedDictGet(xtypeddict* pDict, xstrview Key);
XRT_API const void* xrtTypedDictConstGet(
	const xtypeddict* pDict,
	xstrview Key
);
XRT_API bool xrtTypedDictHas(const xtypeddict* pDict, xstrview Key);



/* 返回与查询等价的内部规范键视图，缺失时清空输出。 */
XRT_API bool xrtTypedDictStoredKey(
	const xtypeddict* pDict,
	xstrview Key,
	xstrview* pStoredKey
);



/* 删除指定键，或把值移动到外部已初始化输出后删除。 */
XRT_API bool xrtTypedDictRemove(xtypeddict* pDict, xstrview Key);
XRT_API bool xrtTypedDictTake(
	xtypeddict* pDict,
	xstrview Key,
	ptr pValue
);



/* 按插入顺序返回指定位置的借用键和值，复杂度为 O(n)。 */
XRT_API ptr xrtTypedDictAt(
	xtypeddict* pDict,
	size_t iIndex,
	xstrview* pKey
);
XRT_API const void* xrtTypedDictConstAt(
	const xtypeddict* pDict,
	size_t iIndex,
	xstrview* pKey
);



/* 启动按插入顺序或逆序的外置迭代。 */
XRT_API bool xrtTypedDictIterBegin(
	xtypeddict* pDict,
	xtypeddictiter* pIterator
);
XRT_API bool xrtTypedDictIterRBegin(
	xtypeddict* pDict,
	xtypeddictiter* pIterator
);
XRT_API ptr xrtTypedDictIterNext(
	xtypeddictiter* pIterator,
	xstrview* pKey
);
XRT_API void xrtTypedDictIterEnd(xtypeddictiter* pIterator);



/* 事务合并同类型字典，并深复制创建独立字典。 */
XRT_API bool xrtTypedDictMerge(
	xtypeddict* pTarget,
	const xtypeddict* pSource,
	bool bReplace
);
XRT_API xtypeddict* xrtTypedDictClone(const xtypeddict* pDict);



/* 比较两个字典的精确类型身份、键集合和值内容。 */
XRT_API bool xrtTypedDictEquals(
	const xtypeddict* pLeft,
	const xtypeddict* pRight
);



/* 验证对象字典类型描述，并返回其共享实例操作表。 */
XRT_API bool xrtTypedDictTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedDictInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/typed_value.h */
/* ========================================================================== */

#ifndef XRT_TYPED_VALUE_H
#define XRT_TYPED_VALUE_H


#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)
#endif



#if defined(XRUNTIME_FEATURE_TYPED_VALUE) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_VALUE requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_VALUE) && !defined(XRT_FEATURE_VALUE)
	#error "XRUNTIME_FEATURE_TYPED_VALUE requires XRT_FEATURE_VALUE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE) && \
	(!defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) || \
	 !defined(XRUNTIME_FEATURE_TYPED_VALUE))
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE requires RUNTIME_TYPE_STRING and TYPED_VALUE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE) && \
	(!defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	 !defined(XRUNTIME_FEATURE_TYPED_ARRAY) || \
	 !defined(XRT_FEATURE_VALUE_CONTAINER))
	#error "XRUNTIME_FEATURE_TYPED_ARRAY_VALUE requires TYPED_VALUE, TYPED_ARRAY and VALUE_CONTAINER"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE) && \
	(!defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	 !defined(XRUNTIME_FEATURE_TYPED_LIST) || \
	 !defined(XRT_FEATURE_VALUE_CONTAINER))
	#error "XRUNTIME_FEATURE_TYPED_LIST_VALUE requires TYPED_VALUE, TYPED_LIST and VALUE_CONTAINER"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE) && \
	(!defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	 !defined(XRUNTIME_FEATURE_TYPED_SET) || \
	 !defined(XRT_FEATURE_VALUE_CONTAINER))
	#error "XRUNTIME_FEATURE_TYPED_SET_VALUE requires TYPED_VALUE, TYPED_SET and VALUE_CONTAINER"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE) && \
	(!defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	 !defined(XRUNTIME_FEATURE_TYPED_DICT) || \
	 !defined(XRT_FEATURE_VALUE_CONTAINER))
	#error "XRUNTIME_FEATURE_TYPED_DICT_VALUE requires TYPED_VALUE, TYPED_DICT and VALUE_CONTAINER"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_VALUE)

/* 动态值转换层稳定错误代码。 */
typedef enum xtypedvalueerror {
	XTYPED_VALUE_ERROR_ARGUMENT = 1,
	XTYPED_VALUE_ERROR_TYPE,
	XTYPED_VALUE_ERROR_RANGE,
	XTYPED_VALUE_ERROR_CONVERT,
	XTYPED_VALUE_ERROR_CONTAINER
} xtypedvalueerror;



/* 自定义解码器把动态值写入已经初始化的目标，失败时目标仍须可安全销毁。 */
typedef bool (*xvaluetotyped)(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	ptr pContext
);



/* 自定义编码器借用类型值，并返回一个由调用方拥有的动态值。 */
typedef xvalue* (*xvaluefromtyped)(
	const xrttype* pSourceType,
	const void* pSource,
	ptr pContext
);



/* 转换器只借用回调与上下文，允许按应用协议扩展记录、字符串和句柄类型。 */
typedef struct xvalueconverter {
	ptr Context;
	xvaluetotyped ToTyped;
	xvaluefromtyped FromTyped;
} xvalueconverter;



XRT_EXTERN_C_BEGIN



/* 把动态值安全转换为一个新初始化的运行时类型值。 */
XRT_API bool xrtValueToTyped(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	const xvalueconverter* pConverter
);



/* 把运行时类型值转换为一个独立动态值。 */
XRT_API xvalue* xrtValueFromTyped(
	const xrttype* pSourceType,
	const void* pSource,
	const xvalueconverter* pConverter
);



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)

/* 把一个动态值转换后追加到类型数组。 */
XRT_API bool xrtTypedArrayPushValue(
	xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把一个动态值转换后插入类型数组的指定下标。 */
XRT_API bool xrtTypedArrayInsertValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把一个动态值转换后原子替换类型数组的指定元素。 */
XRT_API bool xrtTypedArraySetValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把类型数组的指定元素转换为调用方拥有的动态值。 */
XRT_API xvalue* xrtTypedArrayGetValue(
	const xtypedarray* pArray,
	size_t iIndex,
	const xvalueconverter* pConverter
);



/* 转换并删除类型数组的指定元素；转换失败时数组保持不变。 */
XRT_API xvalue* xrtTypedArrayTakeValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalueconverter* pConverter
);



/* 转换并删除类型数组的末尾元素；转换失败时数组保持不变。 */
XRT_API xvalue* xrtTypedArrayPopValue(
	xtypedarray* pArray,
	const xvalueconverter* pConverter
);



/* 把动态值转换为元素类型并查找第一处相等元素。 */
XRT_API size_t xrtTypedArrayFindValue(
	const xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 判断类型数组是否包含与动态值等价的元素。 */
XRT_API bool xrtTypedArrayContainsValue(
	const xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把动态稠密数组深转换为独立的同构类型数组。 */
XRT_API xtypedarray* xrtTypedArrayFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
);



/* 把类型数组深转换为独立的动态稠密数组。 */
XRT_API xvalue* xrtTypedArrayToValue(
	const xtypedarray* pArray,
	const xvalueconverter* pConverter
);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)

/* 把一个动态值转换后写入类型列表的指定整数键。 */
XRT_API bool xrtTypedListSetValue(
	xtypedlist* pList,
	int64 iKey,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把一个动态值转换后追加到最大键之后。 */
XRT_API bool xrtTypedListAppendValue(
	xtypedlist* pList,
	const xvalue* pValue,
	int64* pKey,
	const xvalueconverter* pConverter
);



/* 把指定整数键的类型值转换为调用方拥有的动态值。 */
XRT_API xvalue* xrtTypedListGetValue(
	const xtypedlist* pList,
	int64 iKey,
	const xvalueconverter* pConverter
);



/* 转换并删除指定整数键的类型值；转换失败时列表保持不变。 */
XRT_API xvalue* xrtTypedListTakeValue(
	xtypedlist* pList,
	int64 iKey,
	const xvalueconverter* pConverter
);



/* 把动态值转换为元素类型并查找第一处相等值。 */
XRT_API bool xrtTypedListFindValue(
	const xtypedlist* pList,
	const xvalue* pValue,
	int64* pKey,
	const xvalueconverter* pConverter
);



/* 判断类型列表是否包含与动态值等价的元素。 */
XRT_API bool xrtTypedListContainsValue(
	const xtypedlist* pList,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把动态整数映射深转换为独立的同构稀疏类型列表。 */
XRT_API xtypedlist* xrtTypedListFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
);



/* 把稀疏类型列表深转换为独立的动态整数映射。 */
XRT_API xvalue* xrtTypedListToValue(
	const xtypedlist* pList,
	const xvalueconverter* pConverter
);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)

/* 把一个动态值转换后加入类型集合。 */
XRT_API bool xrtTypedSetAddValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 返回与动态值等价的规范元素副本。 */
XRT_API xvalue* xrtTypedSetGetValue(
	const xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 判断类型集合是否拥有与动态值等价的元素。 */
XRT_API bool xrtTypedSetHasValue(
	const xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 删除与动态值等价的元素。 */
XRT_API bool xrtTypedSetRemoveValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 转换并删除规范元素；转换失败时集合保持不变。 */
XRT_API xvalue* xrtTypedSetTakeValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把动态集合深转换为独立的同构类型集合。 */
XRT_API xtypedset* xrtTypedSetFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
);



/* 把类型集合深转换为独立的动态集合。 */
XRT_API xvalue* xrtTypedSetToValue(
	const xtypedset* pSet,
	const xvalueconverter* pConverter
);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 把一个动态值转换后写入类型字典的指定文本键。 */
XRT_API bool xrtTypedDictSetValue(
	xtypeddict* pDict,
	xstrview Key,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把指定文本键的类型值转换为调用方拥有的动态值。 */
XRT_API xvalue* xrtTypedDictGetValue(
	const xtypeddict* pDict,
	xstrview Key,
	const xvalueconverter* pConverter
);



/* 转换并删除指定文本键的类型值；转换失败时字典保持不变。 */
XRT_API xvalue* xrtTypedDictTakeValue(
	xtypeddict* pDict,
	xstrview Key,
	const xvalueconverter* pConverter
);



/* 把动态字符串键对象深转换为独立的同构类型字典。 */
XRT_API xtypeddict* xrtTypedDictFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
);



/* 把类型字典深转换为独立的动态字符串键对象。 */
XRT_API xvalue* xrtTypedDictToValue(
	const xtypeddict* pDict,
	const xvalueconverter* pConverter
);

#endif



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/runtime_field.h */
/* ========================================================================== */

#ifndef XRT_RUNTIME_FIELD_H
#define XRT_RUNTIME_FIELD_H


#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_FIELD) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_FIELD requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD) && !defined(XRUNTIME_FEATURE_RUNTIME_FIELD)
	#error "XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD requires XRUNTIME_FEATURE_RUNTIME_FIELD"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD) && !defined(XRUNTIME_FEATURE_TYPED_DICT)
	#error "XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD requires XRUNTIME_FEATURE_TYPED_DICT"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD) && !defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	#error "XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD requires XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD) && !defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)
	#error "XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD requires XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_FIELD)

/* 字段模块只描述实例布局事实，不承担语言级可见性或赋值策略。 */
#define XRT_FIELD_FLAG_READONLY UINT32_C(0x00000001)



/* 运行时字段模块稳定错误代码。 */
typedef enum xfielderror {
	XFIELD_ERROR_DESCRIPTOR = 1,
	XFIELD_ERROR_LOOKUP,
	XFIELD_ERROR_ACCESS
} xfielderror;



struct xrtfielddesc {
	xstrview Name;
	const xrttype* Type;
	size_t Offset;
	uint32 Flags;
};



struct xrtfieldtable {
	size_t Count;
	const xrtfielddesc* Fields;
};



XRT_EXTERN_C_BEGIN



/* 验证本类型及完整继承链中的字段名称、布局、类型和标志。 */
XRT_API bool xrtTypeFieldsValidate(const xrttype* pType);



/* 返回包含继承字段的总数；字段顺序始终是基类在前、派生类在后。 */
XRT_API size_t xrtTypeFieldCount(const xrttype* pType);



/* 按基类优先的稳定下标返回借用字段，越界时设置范围错误。 */
XRT_API const xrtfielddesc* xrtTypeField(
	const xrttype* pType,
	size_t iIndex
);



/* 沿当前类型和基类按精确名称查找借用字段；未找到不设置错误。 */
XRT_API const xrtfielddesc* xrtTypeFindField(
	const xrttype* pType,
	xstrview Name
);



/* 返回字段在给定类型继承链中的声明类型。 */
XRT_API const xrttype* xrtTypeFieldOwner(
	const xrttype* pType,
	const xrtfielddesc* pField
);



/* 返回实例中的借用字段地址；描述必须属于给定类型的继承链。 */
XRT_API const void* xrtFieldConstData(
	const xrttype* pType,
	const xrtfielddesc* pField,
	const void* pInstance
);
XRT_API ptr xrtFieldData(
	const xrttype* pType,
	const xrtfielddesc* pField,
	ptr pInstance
);



XRT_EXTERN_C_END

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)

/* 动态字段表是独立对象图节点，载荷拥有按插入顺序保存的 Value 字典。 */
typedef xrtobject xrtdynamicfields;



/* 动态字段迭代器在迭代期间保留字段对象，并检测后续结构修改。 */
typedef struct xrtdynamicfielditer {
	xtypeddictiter Base;
	xrtdynamicfields* Fields;
} xrtdynamicfielditer;



/* 动态字段模块稳定错误代码。 */
typedef enum xdynamicfielderror {
	XDYNAMIC_FIELD_ERROR_ARGUMENT = 1,
	XDYNAMIC_FIELD_ERROR_TYPE,
	XDYNAMIC_FIELD_ERROR_OPERATION,
	XDYNAMIC_FIELD_ERROR_STATE
} xdynamicfielderror;



XRT_EXTERN_C_BEGIN



/* 返回进程期稳定的 xrt.DynamicFields 运行时类型。 */
XRT_API const xrttype* xrtDynamicFieldsType(void);



/* 创建、保留和释放动态字段对象。 */
XRT_API xrtdynamicfields* xrtDynamicFieldsCreate(void);
XRT_API xrtdynamicfields* xrtDynamicFieldsRef(xrtdynamicfields* pFields);
XRT_API void xrtDynamicFieldsUnref(xrtdynamicfields* pFields);

/* Physical owning node: real object RC, one edge per stored Value slot.
 * Count/Trace/admission require the caller's ownership freeze or equivalent
 * complete quiescence. Public APIs participate, but raw payload/typed-dict
 * access still needs enclosing participation and caller serialization.
 * No callbacks are retained. The adapter uses core V1 phased retirement;
 * Finish/drop must run outside any inherited freeze/mutation. Weak promotion
 * is quarantined by Claim; abort restores it, Clear permanently retires it.
 * This does not itself collect cycles, unload code or restore Open objects. */
XRT_API xrtownershipref xrtDynamicFieldsOwnership(const xrtdynamicfields* pFields);
XRT_API const xrtownershipadapterv1* xrtDynamicFieldsOwnershipAdapterV1(xrtownershipref Reference);



/* 返回字段数量、容量，并管理字段表存储。 */
XRT_API size_t xrtDynamicFieldsCount(const xrtdynamicfields* pFields);
XRT_API size_t xrtDynamicFieldsCapacity(const xrtdynamicfields* pFields);
XRT_API bool xrtDynamicFieldsClear(xrtdynamicfields* pFields);
XRT_API bool xrtDynamicFieldsReserve(
	xrtdynamicfields* pFields,
	size_t iCapacity
);
XRT_API bool xrtDynamicFieldsTrim(xrtdynamicfields* pFields);



/* 查询字段；Get 返回只读借用，GetRef 保留同一 Value，Copy 深复制完整图。 */
XRT_API bool xrtDynamicFieldsHas(
	const xrtdynamicfields* pFields,
	xstrview Name
);
XRT_API const xvalue* xrtDynamicFieldsGet(
	const xrtdynamicfields* pFields,
	xstrview Name
);
XRT_API xvalue* xrtDynamicFieldsGetRef(
	const xrtdynamicfields* pFields,
	xstrview Name
);
XRT_API xvalue* xrtDynamicFieldsCopy(
	const xrtdynamicfields* pFields,
	xstrview Name
);
XRT_API bool xrtDynamicFieldsStoredName(
	const xrtdynamicfields* pFields,
	xstrview Name,
	xstrview* pStoredName
);



/* 三种写入都隔离完整 Value 图；Take 成功移交来源，New 总是消费临时值。 */
XRT_API bool xrtDynamicFieldsSet(
	xrtdynamicfields* pFields,
	xstrview Name,
	const xvalue* pValue
);
XRT_API bool xrtDynamicFieldsSetTake(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue** ppValue
);
XRT_API bool xrtDynamicFieldsSetNew(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue* pValue
);



/* Ref 写入保留同一 Value 身份，供语言对象和显式共享图使用。 */
XRT_API bool xrtDynamicFieldsSetRef(
	xrtdynamicfields* pFields,
	xstrview Name,
	const xvalue* pValue
);
XRT_API bool xrtDynamicFieldsSetRefTake(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue** ppValue
);
XRT_API bool xrtDynamicFieldsSetRefNew(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue* pValue
);



/* 删除并释放字段，或把字段值移交给调用方。 */
XRT_API bool xrtDynamicFieldsRemove(
	xrtdynamicfields* pFields,
	xstrview Name
);
XRT_API xvalue* xrtDynamicFieldsTake(
	xrtdynamicfields* pFields,
	xstrview Name
);



/* 同步受保护访问。回调期间拒绝本字段表的字段访问、修改和访问重入；
 * 名称和值只在回调内借用。访问器保留字段对象，回调/context 不会留存。
 * 回调 false 或设置错误使访问失败；false 没有错误时补充 STATE。
 * 成功恢复进入前的错误，失败保留首次错误。不是跨线程同步机制。
 * 普通迭代器只检查版本，不能代替此跨任意回调的借用保护。 */
typedef bool (*xdynamicfieldvisitv1)(xstrview Name, const xvalue* pValue, ptr UserData);
XRT_API bool xrtDynamicFieldsVisitV1(xrtdynamicfields* pFields,
	xdynamicfieldvisitv1 Visit, ptr UserData);

/* 按稳定插入顺序或逆序迭代借用名称和值；启动函数负责初始化迭代器。 */
XRT_API bool xrtDynamicFieldsIterBegin(
	xrtdynamicfields* pFields,
	xrtdynamicfielditer* pIterator
);
XRT_API bool xrtDynamicFieldsIterRBegin(
	xrtdynamicfields* pFields,
	xrtdynamicfielditer* pIterator
);
XRT_API const xvalue* xrtDynamicFieldsIterNext(
	xrtdynamicfielditer* pIterator,
	xstrview* pName
);
XRT_API void xrtDynamicFieldsIterEnd(xrtdynamicfielditer* pIterator);



/* 事务合并或深复制动态字段对象。 */
XRT_API bool xrtDynamicFieldsMerge(
	xrtdynamicfields* pTarget,
	const xrtdynamicfields* pSource,
	bool bReplace
);
/* Transactional reference merge for an already isolated owning graph.
 * Retains the actual Value identities, including repeated/cyclic edges;
 * does not clone, consume or modify source. A false return means publication
 * failed and target is unchanged. True means committed; a diagnostic from
 * releasing replaced values remains observable after that commit.
 * Same insertion/replacement order, visit guard and caller synchronization
 * requirements as Merge. This is not a deep-copy operation. */
XRT_API bool xrtDynamicFieldsMergeRefV1(
	xrtdynamicfields* pTarget,
	const xrtdynamicfields* pSource,
	bool bReplace
);
XRT_API xrtdynamicfields* xrtDynamicFieldsClone(
	const xrtdynamicfields* pFields
);



/* 构造便于语言绑定使用的名称、值、二元项数组和独立 Object。 */
XRT_API xvalue* xrtDynamicFieldsKeys(const xrtdynamicfields* pFields);
XRT_API xvalue* xrtDynamicFieldsValues(const xrtdynamicfields* pFields);
XRT_API xvalue* xrtDynamicFieldsItems(const xrtdynamicfields* pFields);
XRT_API xvalue* xrtDynamicFieldsToValue(const xrtdynamicfields* pFields);



/* 从 Value Object 深复制名称和值并创建独立动态字段对象。 */
XRT_API xrtdynamicfields* xrtDynamicFieldsFromValue(const xvalue* pValue);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/typed_queue.h */
/* ========================================================================== */

#ifndef XRT_TYPED_QUEUE_H
#define XRT_TYPED_QUEUE_H




#if defined(XRUNTIME_FEATURE_TYPED_QUEUE) && !defined(XRT_FEATURE_QUEUE)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE requires XRT_FEATURE_QUEUE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_SPSC) && !defined(XRUNTIME_FEATURE_TYPED_QUEUE)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_SPSC requires XRUNTIME_FEATURE_TYPED_QUEUE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_SPSC) && !defined(XRT_FEATURE_QUEUE_SPSC)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_SPSC requires XRT_FEATURE_QUEUE_SPSC"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC) && !defined(XRUNTIME_FEATURE_TYPED_QUEUE)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_MPSC requires XRUNTIME_FEATURE_TYPED_QUEUE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC) && !defined(XRT_FEATURE_QUEUE_MPSC)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_MPSC requires XRT_FEATURE_QUEUE_MPSC"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC) && !defined(XRT_FEATURE_QUEUE_MPMC)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_MPSC requires XRT_FEATURE_QUEUE_MPMC"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPMC) && !defined(XRUNTIME_FEATURE_TYPED_QUEUE)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_MPMC requires XRUNTIME_FEATURE_TYPED_QUEUE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPMC) && !defined(XRT_FEATURE_QUEUE_MPMC)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_MPMC requires XRT_FEATURE_QUEUE_MPMC"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE)

/* 类型队列核心拥有固定值槽；公开结构只用于栈分配，不允许直接修改字段。 */
typedef struct xtypedqueuecore {
	const xrttype* ItemType;
	ptr Allocation;
	bytes Values;
	size_t Stride;
	size_t Capacity;
	size_t ValueBytes;
	xatomic32 State;
	xatomic32 Active;
} xtypedqueuecore;



/* 对象负载中的类型队列通过元数据声明固定容量。 */
typedef struct xtypedqueuemeta {
	size_t Capacity;
} xtypedqueuemeta;



/* 三种类型队列共享同一错误域和稳定错误代码。 */
typedef enum xtypedqueueerror {
	XTYPED_QUEUE_ERROR_ARGUMENT = 1,
	XTYPED_QUEUE_ERROR_TYPE,
	XTYPED_QUEUE_ERROR_LAYOUT,
	XTYPED_QUEUE_ERROR_OPERATION,
	XTYPED_QUEUE_ERROR_STATE
} xtypedqueueerror;

#endif



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_SPSC)

/* SPSC 类型队列由固定值槽、就绪环和反向空闲环组成。 */
typedef struct xtypedspscqueue {
	xtypedqueuecore Core;
	xspscqueue Ready;
	xspscqueue Free;
	xatomicptr PushCell;
	xatomicptr PopCell;
} xtypedspscqueue;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个有界单生产者单消费者类型队列。 */
XRT_API bool xrtTypedSPSCQueueInit(
	xtypedspscqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API xtypedspscqueue* xrtTypedSPSCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API void xrtTypedSPSCQueueUnit(xtypedspscqueue* pQueue);
XRT_API void xrtTypedSPSCQueueDestroy(xtypedspscqueue* pQueue);



/* 返回借用元素类型、实际 2 次幂容量和并发近似元素数。 */
XRT_API const xrttype* xrtTypedSPSCQueueItemType(
	const xtypedspscqueue* pQueue
);
XRT_API size_t xrtTypedSPSCQueueCapacity(const xtypedspscqueue* pQueue);
XRT_API size_t xrtTypedSPSCQueueCount(const xtypedspscqueue* pQueue);



/* 复制或移动压入一个值；满、关闭和错误使用 xqueueresult 区分。 */
XRT_API xqueueresult xrtTypedSPSCQueueTryPush(
	xtypedspscqueue* pQueue,
	const void* pItem
);
XRT_API xqueueresult xrtTypedSPSCQueueTryPushTake(
	xtypedspscqueue* pQueue,
	ptr pItem
);



/* 移动弹出到已初始化输出；类型移动失败时元素仍由队列拥有。 */
XRT_API xqueueresult xrtTypedSPSCQueueTryPop(
	xtypedspscqueue* pQueue,
	ptr pValue
);



/* 批量处理连续类型值；部分成功返回 OK 和实际处理数量。 */
XRT_API xqueuebatchresult xrtTypedSPSCQueuePushBatch(
	xtypedspscqueue* pQueue,
	const void* pItems,
	size_t iCount
);
XRT_API xqueuebatchresult xrtTypedSPSCQueuePopBatch(
	xtypedspscqueue* pQueue,
	ptr pValues,
	size_t iCapacity
);



/* 关闭写端、查询终态，或在独占且排空后重新开放。 */
XRT_API void xrtTypedSPSCQueueClose(xtypedspscqueue* pQueue);
XRT_API bool xrtTypedSPSCQueueIsClosed(const xtypedspscqueue* pQueue);
XRT_API bool xrtTypedSPSCQueueIsDrained(const xtypedspscqueue* pQueue);
XRT_API bool xrtTypedSPSCQueueReset(xtypedspscqueue* pQueue);



/* 验证对象队列描述，并返回 SPSC 队列实例操作表。 */
XRT_API bool xrtTypedSPSCQueueTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedSPSCQueueInstanceOps(void);



XRT_EXTERN_C_END

#endif



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC)

/* MPSC 类型队列允许多个生产者并发复制，值槽由 MPMC 空闲环回收。 */
typedef struct xtypedmpscqueue {
	xtypedqueuecore Core;
	xmpscqueue Ready;
	xmpmcqueue Free;
	xatomicptr PopCell;
} xtypedmpscqueue;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个有界多生产者单消费者类型队列。 */
XRT_API bool xrtTypedMPSCQueueInit(
	xtypedmpscqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API xtypedmpscqueue* xrtTypedMPSCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API void xrtTypedMPSCQueueUnit(xtypedmpscqueue* pQueue);
XRT_API void xrtTypedMPSCQueueDestroy(xtypedmpscqueue* pQueue);



/* 返回借用元素类型、实际 2 次幂容量和并发近似元素数。 */
XRT_API const xrttype* xrtTypedMPSCQueueItemType(
	const xtypedmpscqueue* pQueue
);
XRT_API size_t xrtTypedMPSCQueueCapacity(const xtypedmpscqueue* pQueue);
XRT_API size_t xrtTypedMPSCQueueCount(const xtypedmpscqueue* pQueue);



/* 复制或移动压入一个值；满、关闭和错误使用 xqueueresult 区分。 */
XRT_API xqueueresult xrtTypedMPSCQueueTryPush(
	xtypedmpscqueue* pQueue,
	const void* pItem
);
XRT_API xqueueresult xrtTypedMPSCQueueTryPushTake(
	xtypedmpscqueue* pQueue,
	ptr pItem
);



/* 由唯一消费者移动弹出；类型移动失败时元素仍由队列拥有。 */
XRT_API xqueueresult xrtTypedMPSCQueueTryPop(
	xtypedmpscqueue* pQueue,
	ptr pValue
);



/* 批量处理连续类型值；部分成功返回 OK 和实际处理数量。 */
XRT_API xqueuebatchresult xrtTypedMPSCQueuePushBatch(
	xtypedmpscqueue* pQueue,
	const void* pItems,
	size_t iCount
);
XRT_API xqueuebatchresult xrtTypedMPSCQueuePopBatch(
	xtypedmpscqueue* pQueue,
	ptr pValues,
	size_t iCapacity
);



/* 关闭写端、查询终态，或在独占且排空后重新开放。 */
XRT_API void xrtTypedMPSCQueueClose(xtypedmpscqueue* pQueue);
XRT_API bool xrtTypedMPSCQueueIsClosed(const xtypedmpscqueue* pQueue);
XRT_API bool xrtTypedMPSCQueueIsDrained(const xtypedmpscqueue* pQueue);
XRT_API bool xrtTypedMPSCQueueReset(xtypedmpscqueue* pQueue);



/* 验证对象队列描述，并返回 MPSC 队列实例操作表。 */
XRT_API bool xrtTypedMPSCQueueTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedMPSCQueueInstanceOps(void);



XRT_EXTERN_C_END

#endif



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPMC)

/* MPMC 类型队列以独立就绪、空闲和失败重试环保存固定值槽。 */
typedef struct xtypedmpmcqueue {
	xtypedqueuecore Core;
	xmpmcqueue Ready;
	xmpmcqueue Free;
	xmpmcqueue Retry;
} xtypedmpmcqueue;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个有界多生产者多消费者类型队列。 */
XRT_API bool xrtTypedMPMCQueueInit(
	xtypedmpmcqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API xtypedmpmcqueue* xrtTypedMPMCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API void xrtTypedMPMCQueueUnit(xtypedmpmcqueue* pQueue);
XRT_API void xrtTypedMPMCQueueDestroy(xtypedmpmcqueue* pQueue);



/* 返回借用元素类型、实际 2 次幂容量和并发近似元素数。 */
XRT_API const xrttype* xrtTypedMPMCQueueItemType(
	const xtypedmpmcqueue* pQueue
);
XRT_API size_t xrtTypedMPMCQueueCapacity(const xtypedmpmcqueue* pQueue);
XRT_API size_t xrtTypedMPMCQueueCount(const xtypedmpmcqueue* pQueue);



/* 复制或移动压入一个值；满、关闭和错误使用 xqueueresult 区分。 */
XRT_API xqueueresult xrtTypedMPMCQueueTryPush(
	xtypedmpmcqueue* pQueue,
	const void* pItem
);
XRT_API xqueueresult xrtTypedMPMCQueueTryPushTake(
	xtypedmpmcqueue* pQueue,
	ptr pItem
);



/* 由任意消费者移动弹出；类型移动失败时元素进入内部重试环。 */
XRT_API xqueueresult xrtTypedMPMCQueueTryPop(
	xtypedmpmcqueue* pQueue,
	ptr pValue
);



/* 批量处理连续类型值；部分成功返回 OK 和实际处理数量。 */
XRT_API xqueuebatchresult xrtTypedMPMCQueuePushBatch(
	xtypedmpmcqueue* pQueue,
	const void* pItems,
	size_t iCount
);
XRT_API xqueuebatchresult xrtTypedMPMCQueuePopBatch(
	xtypedmpmcqueue* pQueue,
	ptr pValues,
	size_t iCapacity
);



/* 关闭写端、查询终态，或在独占且排空后重新开放。 */
XRT_API void xrtTypedMPMCQueueClose(xtypedmpmcqueue* pQueue);
XRT_API bool xrtTypedMPMCQueueIsClosed(const xtypedmpmcqueue* pQueue);
XRT_API bool xrtTypedMPMCQueueIsDrained(const xtypedmpmcqueue* pQueue);
XRT_API bool xrtTypedMPMCQueueReset(xtypedmpmcqueue* pQueue);



/* 验证对象队列描述，并返回 MPMC 队列实例操作表。 */
XRT_API bool xrtTypedMPMCQueueTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedMPMCQueueInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/typed_stack.h */
/* ========================================================================== */

#ifndef XRT_TYPED_STACK_H
#define XRT_TYPED_STACK_H




#if defined(XRUNTIME_FEATURE_TYPED_STACK) && !defined(XRUNTIME_FEATURE_TYPED_ARRAY)
	#error "XRUNTIME_FEATURE_TYPED_STACK requires XRUNTIME_FEATURE_TYPED_ARRAY"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_STACK)

/* 类型栈复用类型数组的连续存储与元素所有权，只公开后进先出语义。 */
typedef xtypedarray xtypedstack;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有元素值的空类型栈。 */
XRT_API bool xrtTypedStackInit(
	xtypedstack* pStack,
	const xrttype* pItemType
);
XRT_API xtypedstack* xrtTypedStackCreate(const xrttype* pItemType);
XRT_API void xrtTypedStackUnit(xtypedstack* pStack);
XRT_API void xrtTypedStackDestroy(xtypedstack* pStack);



/* 返回借用元素类型、当前深度和容量。 */
XRT_API const xrttype* xrtTypedStackItemType(const xtypedstack* pStack);
XRT_API size_t xrtTypedStackCount(const xtypedstack* pStack);
XRT_API size_t xrtTypedStackCapacity(const xtypedstack* pStack);



/* 清空、预留或裁剪栈存储。 */
XRT_API void xrtTypedStackClear(xtypedstack* pStack);
XRT_API bool xrtTypedStackReserve(xtypedstack* pStack, size_t iCapacity);
XRT_API bool xrtTypedStackTrim(xtypedstack* pStack);



/* 复制压入元素；失败时栈保持原值。 */
XRT_API bool xrtTypedStackPush(
	xtypedstack* pStack,
	const void* pItem
);



/* 弹出栈顶；输出为空时销毁元素，否则移动到已初始化输出值。 */
XRT_API bool xrtTypedStackPop(xtypedstack* pStack, ptr pValue);

/* 数组顺序压入（最后一项成为栈顶）；允许栈自身作为来源。 */
XRT_API bool xrtTypedStackPushBatch(
	xtypedstack* pStack,
	const xtypedarray* pItems
);
/* 最多弹出指定数量，结果按逐次 Pop 顺序；空栈返回空拥有数组。 */
XRT_API xtypedarray* xrtTypedStackPopBatch(
	xtypedstack* pStack,
	size_t iMaxCount
);
/* 从指定深度最多复制指定数量，结果按栈顶向栈底顺序。 */
XRT_API xtypedarray* xrtTypedStackPeekBatch(
	const xtypedstack* pStack,
	size_t iDepth,
	size_t iMaxCount
);



/* 按距栈顶深度返回借用值，深度零表示栈顶。 */
XRT_API ptr xrtTypedStackPeek(xtypedstack* pStack, size_t iDepth);
XRT_API const void* xrtTypedStackConstPeek(
	const xtypedstack* pStack,
	size_t iDepth
);
XRT_API ptr xrtTypedStackTop(xtypedstack* pStack);
XRT_API const void* xrtTypedStackConstTop(const xtypedstack* pStack);



/* 深复制类型栈，或比较精确类型、深度和元素顺序。 */
XRT_API xtypedstack* xrtTypedStackClone(const xtypedstack* pStack);
XRT_API bool xrtTypedStackEquals(
	const xtypedstack* pLeft,
	const xtypedstack* pRight
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xrt/typed_tree.h */
/* ========================================================================== */

#ifndef XRT_TYPED_TREE_H
#define XRT_TYPED_TREE_H




#if defined(XRUNTIME_FEATURE_TYPED_TREE) && !defined(XRT_FEATURE_AVL_TREE)
	#error "XRUNTIME_FEATURE_TYPED_TREE requires XRT_FEATURE_AVL_TREE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_TREE) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_TREE requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_TREE)

/* 类型树按任意可比较运行时类型键排序，并拥有每一个键和值。 */
typedef struct xtypedtree {
	xavltree Storage;
	const xrttype* KeyType;
	const xrttype* ValueType;
	size_t KeyOffset;
	size_t ValueOffset;
	size_t EntrySize;
	size_t Alignment;
	uint32 Flags;
} xtypedtree;



/* 类型树外置迭代器按键升序或降序借用稳定键值槽。 */
typedef struct xtypedtreeiter {
	xtypedtree* Tree;
	xavltreeiter Base;
} xtypedtreeiter;



/* 类型树模块稳定错误代码。 */
typedef enum xtypedtreeerror {
	XTYPED_TREE_ERROR_ARGUMENT = 1,
	XTYPED_TREE_ERROR_TYPE,
	XTYPED_TREE_ERROR_LAYOUT,
	XTYPED_TREE_ERROR_OPERATION,
	XTYPED_TREE_ERROR_STATE
} xtypedtreeerror;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有键值的空类型树。 */
XRT_API bool xrtTypedTreeInit(
	xtypedtree* pTree,
	const xrttype* pKeyType,
	const xrttype* pValueType
);
XRT_API xtypedtree* xrtTypedTreeCreate(
	const xrttype* pKeyType,
	const xrttype* pValueType
);
XRT_API void xrtTypedTreeUnit(xtypedtree* pTree);
XRT_API void xrtTypedTreeDestroy(xtypedtree* pTree);



/* 返回借用键类型、值类型和当前键值数量。 */
XRT_API const xrttype* xrtTypedTreeKeyType(const xtypedtree* pTree);
XRT_API const xrttype* xrtTypedTreeValueType(const xtypedtree* pTree);
XRT_API size_t xrtTypedTreeCount(const xtypedtree* pTree);



/* 清空全部键值，或释放多余空节点池页。 */
XRT_API bool xrtTypedTreeClear(xtypedtree* pTree);
XRT_API size_t xrtTypedTreeTrim(xtypedtree* pTree, size_t iRetainEmpty);



/* 返回已有值槽，或复制键并按值类型初始化一个新值。 */
XRT_API ptr xrtTypedTreeGetOrAdd(
	xtypedtree* pTree,
	const void* pKey,
	bool* pNew
);



/* 失败原子地复制设置，或移动外部已初始化值并清空来源。 */
XRT_API bool xrtTypedTreeSet(
	xtypedtree* pTree,
	const void* pKey,
	const void* pValue
);
XRT_API bool xrtTypedTreeSetTake(
	xtypedtree* pTree,
	const void* pKey,
	ptr pValue
);



/* 返回指定键的可写或只读借用值；缺失是正常结果。 */
XRT_API ptr xrtTypedTreeGet(xtypedtree* pTree, const void* pKey);
XRT_API const void* xrtTypedTreeConstGet(
	const xtypedtree* pTree,
	const void* pKey
);
XRT_API bool xrtTypedTreeHas(
	const xtypedtree* pTree,
	const void* pKey
);



/* 返回与查询等价的内部规范键，缺失时返回空。 */
XRT_API const void* xrtTypedTreeStoredKey(
	const xtypedtree* pTree,
	const void* pKey
);



/* 删除指定键值，或把值移动到外部已初始化输出后删除。 */
XRT_API bool xrtTypedTreeRemove(xtypedtree* pTree, const void* pKey);
XRT_API bool xrtTypedTreeTake(
	xtypedtree* pTree,
	const void* pKey,
	ptr pValue
);



/* 返回首尾或上下界值，并可返回对应的内部规范键。 */
XRT_API ptr xrtTypedTreeFirst(xtypedtree* pTree, const void** pKey);
XRT_API ptr xrtTypedTreeLast(xtypedtree* pTree, const void** pKey);
XRT_API ptr xrtTypedTreeLowerBound(
	xtypedtree* pTree,
	const void* pSearchKey,
	const void** pStoredKey
);
XRT_API ptr xrtTypedTreeUpperBound(
	xtypedtree* pTree,
	const void* pSearchKey,
	const void** pStoredKey
);



/* 启动完整或从指定边界开始的正反零分配迭代。 */
XRT_API bool xrtTypedTreeIterBegin(
	xtypedtree* pTree,
	xtypedtreeiter* pIterator
);
XRT_API bool xrtTypedTreeIterRBegin(
	xtypedtree* pTree,
	xtypedtreeiter* pIterator
);
XRT_API bool xrtTypedTreeIterFrom(
	xtypedtree* pTree,
	const void* pKey,
	xtypedtreeiter* pIterator
);
XRT_API bool xrtTypedTreeIterRFrom(
	xtypedtree* pTree,
	const void* pKey,
	xtypedtreeiter* pIterator
);
XRT_API ptr xrtTypedTreeIterNext(
	xtypedtreeiter* pIterator,
	const void** pKey
);
XRT_API void xrtTypedTreeIterEnd(xtypedtreeiter* pIterator);



/* 事务合并同类型树、深复制树或比较完整有序键值内容。 */
XRT_API bool xrtTypedTreeMerge(
	xtypedtree* pTarget,
	const xtypedtree* pSource,
	bool bReplace
);
XRT_API xtypedtree* xrtTypedTreeClone(const xtypedtree* pTree);
XRT_API bool xrtTypedTreeEquals(
	const xtypedtree* pLeft,
	const xtypedtree* pRight
);



/* 验证对象树类型描述，并返回其共享实例操作表。 */
XRT_API bool xrtTypedTreeTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedTreeInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xruntime/include/xruntime.h */
/* ========================================================================== */

#ifndef XRUNTIME_H
#define XRUNTIME_H



#endif

#endif

#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRUNTIME_IMPLEMENTATION_ONCE)
#define XRUNTIME_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xruntime/src/internal/xrt_runtime_type.h */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_CONVERT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) || \
	defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING) || \
	defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_FIELD) || \
	defined(XRUNTIME_FEATURE_TYPED_ARRAY) || \
	defined(XRUNTIME_FEATURE_TYPED_TREE) || \
	defined(XRUNTIME_FEATURE_TYPED_LIST) || \
	defined(XRUNTIME_FEATURE_TYPED_SET) || \
	defined(XRUNTIME_FEATURE_TYPED_DICT)
#ifndef XRT_INTERNAL_RUNTIME_TYPE_H
#define XRT_INTERNAL_RUNTIME_TYPE_H


#include <float.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE)

#define XRT_RUNTIME_TYPE_INHERITANCE_MAX 256u



/* 检查借用字符串视图的指针、长度和空文本约束。 */
static inline bool __xrtTypeViewValid(
	const xstrview* pText,
	bool bAllowEmpty
)
{
	if ( pText == NULL ) {
		return false;
	}
	if ( (pText->Data == NULL) && (pText->Size != 0u) ) {
		return false;
	}
	return bAllowEmpty || (pText->Size != 0u);
}



/* 按字节比较两个不要求零结尾的借用字符串视图。 */
static inline bool __xrtTypeViewEqual(
	const xstrview* pLeft,
	const xstrview* pRight
)
{
	return (pLeft->Size == pRight->Size) &&
		((pLeft->Size == 0u) ||
		 ((pLeft->Data != NULL) &&
		  (pRight->Data != NULL) &&
		  (memcmp(pLeft->Data, pRight->Data, pLeft->Size) == 0)));
}



/* 无对齐读取 C bool 或 32 位 ABI 布尔，并归一化为真假值。 */
static inline bool __xrtTypeReadBool(
	const void* pValue,
	size_t iSize,
	bool* pResult
)
{
	if ( iSize == sizeof(bool) ) {
		bool bValue;

		memcpy(&bValue, pValue, sizeof(bValue));
		*pResult = bValue;
		return true;
	}
	if ( iSize == sizeof(int32) ) {
		int32 iValue;

		memcpy(&iValue, pValue, sizeof(iValue));
		*pResult = iValue != 0;
		return true;
	}
	return false;
}



/* 把真假值规范写为 C bool 或 32 位 ABI 布尔。 */
static inline bool __xrtTypeWriteBool(
	bool bValue,
	size_t iSize,
	ptr pTarget
)
{
	if ( iSize == sizeof(bool) ) {
		memcpy(pTarget, &bValue, sizeof(bValue));
		return true;
	}
	if ( iSize == sizeof(int32) ) {
		int32 iValue = bValue ? 1 : 0;

		memcpy(pTarget, &iValue, sizeof(iValue));
		return true;
	}
	return false;
}



/* 无对齐读取受支持宽度的有符号整数。 */
static inline bool __xrtTypeReadSigned(
	const void* pValue,
	size_t iSize,
	int64* pResult
)
{
	switch ( iSize ) {
		case 1u: {
			int8 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (int64)iValue;
			return true;
		}
		case 2u: {
			int16 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (int64)iValue;
			return true;
		}
		case 4u: {
			int32 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (int64)iValue;
			return true;
		}
		case 8u: {
			int64 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = iValue;
			return true;
		}
		default:
			return false;
	}
}



/* 无对齐读取受支持宽度的无符号整数。 */
static inline bool __xrtTypeReadUnsigned(
	const void* pValue,
	size_t iSize,
	uint64* pResult
)
{
	switch ( iSize ) {
		case 1u: {
			uint8 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (uint64)iValue;
			return true;
		}
		case 2u: {
			uint16 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (uint64)iValue;
			return true;
		}
		case 4u: {
			uint32 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (uint64)iValue;
			return true;
		}
		case 8u: {
			uint64 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = iValue;
			return true;
		}
		default:
			return false;
	}
}



/* 无对齐写入受支持宽度的有符号整数，超出目标范围时不修改输出。 */
static inline bool __xrtTypeWriteSigned(
	int64 iValue,
	size_t iSize,
	ptr pTarget
)
{
	switch ( iSize ) {
		case 1u: {
			int8 iResult;

			if ( (iValue < INT8_MIN) || (iValue > INT8_MAX) ) {
				return false;
			}
			iResult = (int8)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 2u: {
			int16 iResult;

			if ( (iValue < INT16_MIN) || (iValue > INT16_MAX) ) {
				return false;
			}
			iResult = (int16)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 4u: {
			int32 iResult;

			if ( (iValue < INT32_MIN) || (iValue > INT32_MAX) ) {
				return false;
			}
			iResult = (int32)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 8u:
			memcpy(pTarget, &iValue, sizeof(iValue));
			return true;
		default:
			return false;
	}
}



/* 无对齐写入受支持宽度的无符号整数，超出目标范围时不修改输出。 */
static inline bool __xrtTypeWriteUnsigned(
	uint64 iValue,
	size_t iSize,
	ptr pTarget
)
{
	switch ( iSize ) {
		case 1u: {
			uint8 iResult;

			if ( iValue > UINT8_MAX ) {
				return false;
			}
			iResult = (uint8)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 2u: {
			uint16 iResult;

			if ( iValue > UINT16_MAX ) {
				return false;
			}
			iResult = (uint16)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 4u: {
			uint32 iResult;

			if ( iValue > UINT32_MAX ) {
				return false;
			}
			iResult = (uint32)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 8u:
			memcpy(pTarget, &iValue, sizeof(iValue));
			return true;
		default:
			return false;
	}
}



/* 无对齐读取受支持宽度的 IEEE-754 浮点值。 */
static inline bool __xrtTypeReadFloat(
	const void* pValue,
	size_t iSize,
	double* pResult
)
{
	if ( iSize == sizeof(float) ) {
		float fValue;

		memcpy(&fValue, pValue, sizeof(fValue));
		*pResult = (double)fValue;
		return true;
	}
	if ( iSize == sizeof(double) ) {
		memcpy(pResult, pValue, sizeof(*pResult));
		return true;
	}
	return false;
}



/* 写入浮点值；无损模式拒绝精度变化，显式模式只拒绝有限值溢出。 */
static inline bool __xrtTypeWriteFloat(
	double fValue,
	size_t iSize,
	bool bLossless,
	ptr pTarget
)
{
	if ( iSize == sizeof(double) ) {
		memcpy(pTarget, &fValue, sizeof(fValue));
		return true;
	}
	if ( iSize == sizeof(float) ) {
		float fResult;

		if (
			!bLossless && (fValue == fValue) &&
			((fValue > FLT_MAX) || (fValue < -FLT_MAX))
		) {
			return false;
		}
		fResult = (float)fValue;
		if (
			bLossless && (fValue == fValue) &&
			((double)fResult != fValue)
		) {
			return false;
		}
		memcpy(pTarget, &fResult, sizeof(fResult));
		return true;
	}
	return false;
}

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xruntime/src/internal/xrt_runtime_object.h */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH) || \
	defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)
#ifndef XRT_RUNTIME_OBJECT_INTERNAL_H
#define XRT_RUNTIME_OBJECT_INTERNAL_H




#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)

/* 对象图关闭时，对象控制块不包含任何收集器状态。 */
struct xrtobject {
	volatile int32 StrongCount;
	volatile int32 WeakCount;
	const xrttype* Type;
	size_t Size;
	size_t PayloadOffset;
	xrtownershiptrace OwnershipTrace;
	/* Resident phased adapters quarantine weak promotion; existing strong
	 * owners may still resurrect an object before Clear. Protected by the
	 * ownership domain, not a new object-graph registry or synthetic RC. */
	const void* OwnershipClaim;
	bool OwnershipCleared;
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	volatile int32 State;
	struct xrtobjectgraph* Graph;
	xrtobject* GraphPrevious;
	xrtobject* GraphNext;
#endif
	uint8 Storage[1];
};



/* 供内建引用对象类型描述复用的进程期值操作表。 */
extern const xrttypeops __xrtObjectValueOperations;



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)

#define XRT_OBJECT_STATE_ACTIVE 0
#define XRT_OBJECT_STATE_FINALIZING 1
#define XRT_OBJECT_STATE_FINALIZED 2



/* 对象图终结流程复用的内部生命周期操作。 */
bool __xrtObjectBeginFinalize(xrtobject* pObject);
void __xrtObjectCancelFinalize(xrtobject* pObject);
void __xrtObjectDropPayload(xrtobject* pObject);
void __xrtObjectEndFinalize(xrtobject* pObject);



/* 普通最后引用释放时，由对象图实现摘除仍被跟踪的对象。 */
void __xrtObjectGraphDetach(xrtobject* pObject);

#endif

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xruntime/src/internal/xrt_runtime_value.h */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS) || \
	defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)
#ifndef XRT_RUNTIME_VALUE_INTERNAL_H
#define XRT_RUNTIME_VALUE_INTERNAL_H


#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)

/* 内建 Value 槽描述供动态字段等复合运行时类型建立静态泛型实参。 */
extern const xrttype __xrtTypeValueDescriptor;

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xruntime/src/internal/xrt_runtime_convert.h */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING) || \
	defined(XRUNTIME_FEATURE_VALUE_CONVERT) || \
	defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING)
#ifndef XRT_INTERNAL_RUNTIME_CONVERT_H
#define XRT_INTERNAL_RUNTIME_CONVERT_H





#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT)

/* 判断转换模式枚举是否有效。 */
bool __xrtTypeConvertModeValid(xtypeconvertmode Mode);




/* 设置转换层结构化错误。 */
void __xrtTypeConvertError(
	xerrkind Kind,
	xtypeconverterror Code,
	cstr sOperation,
	cstr sMessage
);



/* 包装下层转换失败并保留原始错误链。 */
void __xrtTypeConvertWrap(
	xerrkind DefaultKind,
	xtypeconverterror Code,
	cstr sOperation,
	cstr sMessage
);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)

/* 判断可选文本扩展是否支持指定转换方向。 */
bool __xrtTypeStringCanConvert(
	const xrttype* pSourceType,
	const xrttype* pTargetType
);



/* 执行可选文本扩展转换，失败时保持已初始化目标不变。 */
bool __xrtTypeStringConvert(
	const xrttype* pSourceType,
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xruntime/src/internal/xrt_typed_container.h */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_ARRAY) || \
	defined(XRUNTIME_FEATURE_TYPED_LIST) || \
	defined(XRUNTIME_FEATURE_TYPED_SET) || \
	defined(XRUNTIME_FEATURE_TYPED_DICT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)
#ifndef XRT_INTERNAL_TYPED_CONTAINER_H
#define XRT_INTERNAL_TYPED_CONTAINER_H


#if defined(XRUNTIME_FEATURE_TYPED_ARRAY)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_LIST)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_SET)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_DICT)
#endif



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY)

/* 在跨模块用户回调期间拒绝当前类型数组的全部 API 重入。 */
void __xrtTypedArrayCallbackBegin(const xtypedarray* pArray);
void __xrtTypedArrayCallbackEnd(const xtypedarray* pArray);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST)

/* 在跨模块用户回调期间拒绝当前类型列表的全部 API 重入。 */
void __xrtTypedListCallbackBegin(const xtypedlist* pList);
void __xrtTypedListCallbackEnd(const xtypedlist* pList);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET)

/* 在跨模块用户回调期间拒绝当前类型集合的全部 API 重入。 */
bool __xrtTypedSetCallbackBegin(const xtypedset* pSet);
void __xrtTypedSetCallbackEnd(const xtypedset* pSet);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT)

/* 在跨模块用户回调期间拒绝当前类型字典的全部 API 重入。 */
bool __xrtTypedDictCallbackBegin(const xtypeddict* pDict);
void __xrtTypedDictCallbackEnd(const xtypeddict* pDict);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xruntime/src/internal/xrt_typed_queue.h */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE)
#ifndef XRT_INTERNAL_TYPED_QUEUE_H
#define XRT_INTERNAL_TYPED_QUEUE_H




#if defined(XRUNTIME_FEATURE_TYPED_QUEUE)

#define XRT_TYPED_QUEUE_STATE_EMPTY     0u
#define XRT_TYPED_QUEUE_STATE_READY     1u
#define XRT_TYPED_QUEUE_STATE_BROKEN    2u
#define XRT_TYPED_QUEUE_STATE_EXCLUSIVE 3u



/* 设置类型队列结构化错误。 */
void __xrtTypedQueueError(
	xerrkind Kind,
	xtypedqueueerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 为下层类型或基础队列错误补充类型队列上下文。 */
void __xrtTypedQueueWrap(
	xerrkind DefaultKind,
	xtypedqueueerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 初始化固定数量的对齐值槽，但暂不把核心发布为可用。 */
bool __xrtTypedQueueCoreInit(
	xtypedqueuecore* pCore,
	const xrttype* pItemType,
	size_t iCapacity,
	cstr sOperation
);



/* 发布已经完成全部基础队列初始化的类型队列核心。 */
void __xrtTypedQueueCoreActivate(xtypedqueuecore* pCore);



/* 验证类型队列核心及其公开拥有者布局。 */
bool __xrtTypedQueueCoreValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	cstr sOperation
);



/* 进入和退出一次允许并发的类型值操作。 */
bool __xrtTypedQueueCoreEnter(
	xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	cstr sOperation
);
void __xrtTypedQueueCoreLeave(xtypedqueuecore* pCore);



/* 独占核心并等待已经进入的操作退出；返回此前的稳定状态。 */
bool __xrtTypedQueueCoreExclusive(
	xtypedqueuecore* pCore,
	bool bAllowBroken,
	uint32* pPrevious,
	cstr sOperation
);



/* 结束临时独占并恢复此前的稳定状态。 */
void __xrtTypedQueueCoreShared(
	xtypedqueuecore* pCore,
	uint32 iPrevious
);



/* 标记不可恢复的内部基础队列状态错误。 */
void __xrtTypedQueueCoreBreak(
	xtypedqueuecore* pCore,
	cstr sOperation,
	cstr sMessage
);



/* 销毁每一个已初始化值槽并释放连续存储。 */
void __xrtTypedQueueCoreUnit(xtypedqueuecore* pCore);



/* 返回指定固定槽的地址。 */
ptr __xrtTypedQueueCell(xtypedqueuecore* pCore, size_t iIndex);



/* 验证单值或连续值区间不与队列对象和内部值槽重叠。 */
bool __xrtTypedQueueValueValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	const void* pValue,
	cstr sOperation
);
bool __xrtTypedQueueValuesValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	const void* pValues,
	size_t iCount,
	cstr sOperation
);



/* 在内部值槽和外部已初始化值之间复制或移动所有权。 */
bool __xrtTypedQueueCopyIn(
	const xtypedqueuecore* pCore,
	ptr pCell,
	const void* pItem,
	cstr sOperation
);
bool __xrtTypedQueueMoveIn(
	const xtypedqueuecore* pCore,
	ptr pCell,
	ptr pItem,
	cstr sOperation
);
bool __xrtTypedQueueMoveOut(
	const xtypedqueuecore* pCore,
	ptr pValue,
	ptr pCell,
	cstr sOperation
);



/* 合并两个并发近似数量并限制在固定容量内。 */
size_t __xrtTypedQueueCount(
	const xtypedqueuecore* pCore,
	size_t iReady,
	size_t iRetry
);



/* 验证对象队列类型描述中的元素、容量和实例操作。 */
bool __xrtTypedQueueTypeValidate(
	const xrttype* pType,
	size_t iInstanceSize,
	size_t iInstanceAlign,
	const xrtinstanceops* pInstanceOps,
	cstr sOperation
);



/* 在独占状态下追踪全部固定值槽。 */
bool __xrtTypedQueueTrace(
	xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	xrtobjectvisitor pVisit,
	ptr pContext,
	cstr sOperation
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xruntime/src/internal/xrt_typed_dict.h */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_DICT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)
#ifndef XRT_TYPED_DICT_INTERNAL_H
#define XRT_TYPED_DICT_INTERNAL_H




#if defined(XRUNTIME_FEATURE_TYPED_DICT)

/* 内建字典类型描述复用的进程期实例操作表。 */
extern const xrtinstanceops __xrtTypedDictInstanceOperations;

/* Exact built-in storage/destructor/key policy, not just a TypeId match.
 * Caller holds complete quiescence; no arbitrary callback is invoked. */
bool __xrtTypedDictOwnershipStorage(const xtypeddict*, const xrttype*);

#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_type.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE)



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE)

#define XRT_TYPE_FLAGS (XRT_TYPE_FLAG_TRIVIAL_COPY | \
	XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE | \
	XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL | \
	XRT_TYPE_FLAG_RELOCATABLE)
#define XRT_PARAM_FLAGS (XRT_PARAM_FLAG_OPTIONAL | XRT_PARAM_FLAG_NAMED_ONLY)
#define XRT_FUNCTION_FLAGS (XRT_FUNCTION_FLAG_VARARGS | XRT_FUNCTION_FLAG_KWARGS)
#define XRT_METHOD_FLAGS (XRT_METHOD_FLAG_STATIC | \
	XRT_METHOD_FLAG_VIRTUAL | XRT_METHOD_FLAG_FINAL)



struct xrttyperegistry {
	xrt_spinlock Lock;
	size_t Count;
	size_t Capacity;
	const xrttype** Types;
};



struct xrtprotocolregistry {
	xrt_spinlock Lock;
	size_t Count;
	size_t Capacity;
	const xrtprotocolwitness** Witnesses;
};



/* 设置运行时类型模块结构化错误。 */
static void __xrtRuntimeTypeError(xerrkind Kind, xtypeerror Code,
	cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.type";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为类型操作回调的失败补充统一上下文，并保留其原始错误作为原因。 */
static void __xrtRuntimeTypeWrap(
	xerrkind DefaultKind,
	xtypeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.type";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 规范标识使用稳定的 FNV-1a 64 位哈希，不依赖主机字节序。 */
static uint64 __xrtTypeHashBytes(uint64 iHash, const void* pData, size_t iSize)
{
	const uint8* pBytes = (const uint8*)pData;

	for ( size_t i = 0; i < iSize; i++ ) {
		iHash ^= pBytes[i];
		iHash *= UINT64_C(1099511628211);
	}
	return iHash;
}



/* 以固定小端字节顺序把一个 64 位整数加入稳定哈希。 */
static uint64 __xrtTypeHashU64(uint64 iHash, uint64 iValue)
{
	for ( uint32 i = 0; i < 8u; i++ ) {
		uint8 iByte = (uint8)(iValue >> (i * 8u));
		iHash = __xrtTypeHashBytes(iHash, &iByte, 1u);
	}
	return iHash;
}



/* ABI 对齐必须是非零的二次幂。 */
static bool __xrtTypeAlignValid(size_t iAlign)
{
	return (iAlign != 0) && ((iAlign & (iAlign - 1u)) == 0);
}



/* 检查内建标量类别能否由统一比较和散列路径安全解释。 */
static bool __xrtTypeScalarLayoutValid(const xrttype* pType)
{
	switch ( pType->Kind ) {
		case XRT_TYPE_NULL:
			return pType->Size == 0u;
		case XRT_TYPE_BOOL:
			return (pType->Size == sizeof(bool)) ||
				(pType->Size == sizeof(int32));
		case XRT_TYPE_SIGNED_INT:
		case XRT_TYPE_UNSIGNED_INT:
			return (pType->Size == 1u) || (pType->Size == 2u) ||
				(pType->Size == 4u) || (pType->Size == 8u);
		case XRT_TYPE_FLOAT:
			return (pType->Size == sizeof(float)) ||
				(pType->Size == sizeof(double));
		case XRT_TYPE_TIME:
			return pType->Size == sizeof(xtime);
		case XRT_TYPE_POINTER:
			return (pType->Size == sizeof(ptr)) &&
				(pType->Align == XRT_INTERNAL_ALIGNOF(ptr));
		case XRT_TYPE_TYPE:
			return pType->Size == sizeof(uint64);
		default:
			return true;
	}
}



/* 前置声明供签名中的类型引用执行规范身份检查。 */
static uint64 __xrtTypeComputedId(xstrview AbiName);



/* 只检查类型引用可安全读取且 ID 与规范 ABI 名一致。 */
static bool __xrtTypeIdentityValid(const xrttype* pType)
{
	return (pType != NULL) &&
		__xrtTypeViewValid(&pType->AbiName, false) &&
		(pType->Id == __xrtTypeComputedId(pType->AbiName));
}



/* 按参数、返回值和调用标志计算与函数显示名无关的签名 ID。 */
static uint64 __xrtFunctionSigComputedId(const xrtfunctionsig* pSignature)
{
	uint64 iHash = UINT64_C(14695981039346656037);

	iHash = __xrtTypeHashU64(iHash, (uint64)pSignature->ParamCount);
	for ( size_t i = 0; i < pSignature->ParamCount; i++ ) {
		const xrtparamdesc* pParam = &pSignature->Params[i];

		iHash = __xrtTypeHashU64(iHash, pParam->Type->Id);
		iHash = __xrtTypeHashU64(iHash, (uint64)pParam->Mode);
		iHash = __xrtTypeHashU64(iHash, (uint64)pParam->Flags);
		if ( pParam->Name.Size != 0u ) {
			iHash = __xrtTypeHashU64(iHash, (uint64)pParam->Name.Size);
			iHash = __xrtTypeHashBytes(
				iHash, pParam->Name.Data, pParam->Name.Size);
		}
	}
	iHash = __xrtTypeHashU64(iHash, (uint64)pSignature->ReturnCount);
	for ( size_t i = 0; i < pSignature->ReturnCount; i++ ) {
		iHash = __xrtTypeHashU64(iHash, pSignature->ReturnTypes[i]->Id);
	}
	iHash = __xrtTypeHashU64(iHash, (uint64)pSignature->Flags);
	return iHash != 0 ? iHash : UINT64_C(1);
}



/* 检查函数签名的视图、类型引用、标志和显式 ID。 */
static bool __xrtFunctionSigValidate(const xrtfunctionsig* pSignature)
{
	if ( pSignature == NULL ) {
		return false;
	}
	if (
		!__xrtTypeViewValid(&pSignature->Name, true) ||
		((pSignature->Flags & ~XRT_FUNCTION_FLAGS) != 0u) ||
		((pSignature->ParamCount != 0) && (pSignature->Params == NULL)) ||
		((pSignature->ReturnCount != 0) && (pSignature->ReturnTypes == NULL))
	) {
		return false;
	}
	for ( size_t i = 0; i < pSignature->ParamCount; i++ ) {
		const xrtparamdesc* pParam = &pSignature->Params[i];

		if (
			!__xrtTypeIdentityValid(pParam->Type) ||
			(pParam->Mode < XRT_PARAM_DEFAULT) ||
			(pParam->Mode > XRT_PARAM_BYREF) ||
			((pParam->Flags & ~XRT_PARAM_FLAGS) != 0u) ||
			!__xrtTypeViewValid(&pParam->Name,
				(pParam->Flags & XRT_PARAM_FLAG_NAMED_ONLY) == 0u)
		) {
			return false;
		}
		if ( pParam->Name.Size != 0u ) {
			for ( size_t j = 0; j < i; j++ ) {
				if ( __xrtTypeViewEqual(
					&pSignature->Params[j].Name, &pParam->Name
				) ) {
					return false;
				}
			}
		}
	}
	for ( size_t i = 0; i < pSignature->ReturnCount; i++ ) {
		if ( !__xrtTypeIdentityValid(pSignature->ReturnTypes[i]) ) {
			return false;
		}
	}
	return (pSignature->Id == 0) ||
		(pSignature->Id == __xrtFunctionSigComputedId(pSignature));
}



/* 检查方法表的名称、签名、入口、标志和重载唯一性。 */
static bool __xrtTypeMethodTableValidate(const xrtmethodtable* pMethods)
{
	if ( pMethods == NULL ) {
		return true;
	}
	if ( (pMethods->Count != 0) && (pMethods->Methods == NULL) ) {
		return false;
	}
	for ( size_t i = 0; i < pMethods->Count; i++ ) {
		const xrtmethoddesc* pMethod = &pMethods->Methods[i];

		if (
			!__xrtTypeViewValid(&pMethod->Name, false) ||
			!__xrtFunctionSigValidate(pMethod->Signature) ||
			(pMethod->Entry == NULL) ||
			((pMethod->Flags & ~XRT_METHOD_FLAGS) != 0u)
		) {
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			const xrtmethoddesc* pPrevious = &pMethods->Methods[j];

			if (
				__xrtTypeViewEqual(&pPrevious->Name, &pMethod->Name) &&
				(xrtFunctionSigId(pPrevious->Signature) ==
				 xrtFunctionSigId(pMethod->Signature))
			) {
				return false;
			}
		}
	}
	return true;
}



/* 在已经验证 ABI 名后计算非零规范类型 ID。 */
static uint64 __xrtTypeComputedId(xstrview AbiName)
{
	uint64 iHash = __xrtTypeHashBytes(
		UINT64_C(14695981039346656037), AbiName.Data, AbiName.Size);

	return iHash != 0u ? iHash : UINT64_C(1);
}



/* 按规范 ABI 名生成稳定类型 ID。 */
XRT_API uint64 xrtTypeId(xstrview AbiName)
{
	if ( !__xrtTypeViewValid(&AbiName, false) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
			"type-id", "the ABI type name is empty or invalid");
		return 0;
	}
	return __xrtTypeComputedId(AbiName);
}



/* 验证函数签名并返回显式或计算得到的稳定身份。 */
XRT_API uint64 xrtFunctionSigId(const xrtfunctionsig* pSignature)
{
	if ( !__xrtFunctionSigValidate(pSignature) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_SIGNATURE,
			"signature-id", "the function signature is invalid");
		return 0;
	}
	return pSignature->Id != 0
		? pSignature->Id
		: __xrtFunctionSigComputedId(pSignature);
}



/* 检查函数签名的完整结构和稳定身份。 */
XRT_API bool xrtFunctionSigValidate(const xrtfunctionsig* pSignature)
{
	if ( !__xrtFunctionSigValidate(pSignature) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_SIGNATURE,
			"signature-validate", "the function signature is invalid");
		return false;
	}
	return true;
}



/* 检查单个类型描述，不沿继承链递归。 */
static bool __xrtTypeShapeValidate(const xrttype* pType)
{
	const xrttypeops* pOps;

	if (
		(pType == NULL) ||
		(pType->Kind <= XRT_TYPE_INVALID) ||
		(pType->Kind > XRT_TYPE_WEAK) ||
		!__xrtTypeViewValid(&pType->Name, false) ||
		!__xrtTypeViewValid(&pType->AbiName, false) ||
		(pType->Id != __xrtTypeComputedId(pType->AbiName)) ||
		((pType->Flags & ~XRT_TYPE_FLAGS) != 0u) ||
		!__xrtTypeAlignValid(pType->Align) ||
		!__xrtTypeAlignValid(pType->InstanceAlign) ||
		!__xrtTypeScalarLayoutValid(pType) ||
		((pType->ArgumentCount != 0) && (pType->Arguments == NULL)) ||
		!__xrtTypeMethodTableValidate(pType->Methods)
	) {
		return false;
	}
	pOps = pType->Ops;
	if (
		((pType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) != 0) &&
		((pType->Flags & XRT_TYPE_FLAG_COPYABLE) == 0)
	) {
		return false;
	}
	if (
		((pType->Flags & XRT_TYPE_FLAG_COPYABLE) != 0u) &&
		((pType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) == 0u) &&
		((pOps == NULL) || (pOps->Copy == NULL))
	) {
		return false;
	}
	if (
		(pOps != NULL) &&
		((((pType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) != 0u) &&
		  (pOps->Copy != NULL)) ||
		 (((pType->Flags & XRT_TYPE_FLAG_TRIVIAL_DROP) != 0u) &&
		  (pOps->Drop != NULL)) ||
		 (((pType->Flags & XRT_TYPE_FLAG_COPYABLE) == 0u) &&
		  (pOps->Copy != NULL)))
	) {
		return false;
	}
	if (
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) != 0) &&
		((pType->Size != sizeof(ptr)) || (pType->Align != XRT_INTERNAL_ALIGNOF(ptr)))
	) {
		return false;
	}
	for ( size_t i = 0; i < pType->ArgumentCount; i++ ) {
		if ( !__xrtTypeIdentityValid(pType->Arguments[i]) ) {
			return false;
		}
	}
	return true;
}



/* 检查类型描述和完整继承链的身份、布局、终结约束与环。 */
XRT_API bool xrtTypeValidate(const xrttype* pType)
{
	const xrttype* pBase = pType;

	if ( !__xrtTypeShapeValidate(pType) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
			"validate", "the runtime type descriptor is invalid");
		return false;
	}
	pBase = pType;
	for ( uint32 i = 0; i < XRT_RUNTIME_TYPE_INHERITANCE_MAX; i++ ) {
		pBase = pBase->Base;
		if ( pBase == NULL ) {
			return true;
		}
		if ( !__xrtTypeShapeValidate(pBase) ||
			 (pType->Kind != XRT_TYPE_CLASS) ||
			 (pBase->Kind != XRT_TYPE_CLASS) ||
			 ((pBase->Flags & XRT_TYPE_FLAG_FINAL) != 0u) ||
			 (pType->InstanceSize < pBase->InstanceSize) ||
			 (pType->InstanceAlign < pBase->InstanceAlign) ) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
				"validate", "the runtime type inheritance chain is invalid");
			return false;
		}
	}
	__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
		"validate", "the runtime type inheritance chain is cyclic or too deep");
	return false;
}



/* 比较两个有效类型引用是否具有相同规范身份。 */
XRT_API bool xrtTypeSame(const xrttype* pLeft, const xrttype* pRight)
{
	if ( pLeft == pRight ) {
		return __xrtTypeIdentityValid(pLeft);
	}
	if ( !__xrtTypeIdentityValid(pLeft) ||
		 !__xrtTypeIdentityValid(pRight) ||
		 (pLeft->Id != pRight->Id) ) {
		return false;
	}
	return __xrtTypeViewEqual(&pLeft->AbiName, &pRight->AbiName);
}



/* 沿有界类继承链判断类型关系。 */
XRT_API bool xrtTypeIsA(const xrttype* pType, const xrttype* pTarget)
{
	for ( uint32 i = 0;
		  (pType != NULL) && (i < XRT_RUNTIME_TYPE_INHERITANCE_MAX);
		  i++ ) {
		if ( xrtTypeSame(pType, pTarget) ) {
			return true;
		}
		pType = pType->Base;
	}
	return false;
}



/* 判断内建类型类别是否具有无需回调的比较和散列能力。 */
static bool __xrtTypeBuiltinComparable(const xrttype* pType)
{
	return __xrtTypeScalarLayoutValid(pType) &&
		((pType->Kind == XRT_TYPE_NULL) ||
		 (pType->Kind == XRT_TYPE_BOOL) ||
		 (pType->Kind == XRT_TYPE_SIGNED_INT) ||
		 (pType->Kind == XRT_TYPE_UNSIGNED_INT) ||
		 (pType->Kind == XRT_TYPE_FLOAT) ||
		 (pType->Kind == XRT_TYPE_TIME) ||
		 (pType->Kind == XRT_TYPE_POINTER) ||
		 (pType->Kind == XRT_TYPE_TYPE));
}



/* 查询值复制能力。 */
XRT_API bool xrtTypeIsCopyable(const xrttype* pType)
{
	return (pType != NULL) &&
		((pType->Flags & XRT_TYPE_FLAG_COPYABLE) != 0u);
}



/* 查询值是否允许容器按字节搬迁到另一地址。 */
XRT_API bool xrtTypeIsRelocatable(const xrttype* pType)
{
	return (pType != NULL) &&
		((pType->Flags & XRT_TYPE_FLAG_RELOCATABLE) != 0u);
}



/* 查询值比较能力。 */
XRT_API bool xrtTypeIsComparable(const xrttype* pType)
{
	return (pType != NULL) &&
		(((pType->Ops != NULL) && (pType->Ops->Compare != NULL)) ||
		 __xrtTypeBuiltinComparable(pType));
}



/* 查询值散列能力。 */
XRT_API bool xrtTypeIsHashable(const xrttype* pType)
{
	return (pType != NULL) &&
		(((pType->Ops != NULL) && (pType->Ops->Hash != NULL)) ||
		 __xrtTypeBuiltinComparable(pType));
}



/* 返回借用的泛型实参并检查下标范围。 */
XRT_API const xrttype* xrtTypeArgument(const xrttype* pType, size_t iIndex)
{
	if ( pType == NULL ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
			"argument", "the runtime type descriptor is null");
		return NULL;
	}
	if ( iIndex >= pType->ArgumentCount ) {
		__xrtRuntimeTypeError(XERR_RANGE, XTYPE_ERROR_DESCRIPTOR,
			"argument", "the generic type argument index is out of range");
		return NULL;
	}
	return pType->Arguments[iIndex];
}



/* 沿当前类型和基类查找名称及可选签名匹配的方法。 */
XRT_API const xrtmethoddesc* xrtTypeFindMethod(
	const xrttype* pType,
	xstrview Name,
	uint64 iSignatureId
)
{
	if ( (pType == NULL) || !__xrtTypeViewValid(&Name, false) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
			"find-method", "the type or method name is invalid");
		return NULL;
	}
	for ( uint32 iBase = 0;
		  (pType != NULL) &&
			(iBase < XRT_RUNTIME_TYPE_INHERITANCE_MAX);
		  iBase++, pType = pType->Base ) {
		const xrtmethodtable* pMethods = pType->Methods;

		if ( pMethods == NULL ) {
			continue;
		}
		for ( size_t i = 0; i < pMethods->Count; i++ ) {
			const xrtmethoddesc* pMethod = &pMethods->Methods[i];

			if (
				__xrtTypeViewEqual(&pMethod->Name, &Name) &&
				((iSignatureId == 0) ||
				 (xrtFunctionSigId(pMethod->Signature) == iSignatureId))
			) {
				return pMethod;
			}
		}
	}
	return NULL;
}



/* 使用自定义初始化或零初始化建立一个有效值。 */
XRT_API bool xrtTypeInitValue(const xrttype* pType, ptr pValue)
{
	if ( (pType == NULL) || ((pValue == NULL) && (pType->Size != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"init", "the type or destination value is invalid");
		return false;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Init != NULL) ) {
		return pType->Ops->Init(pValue, pType);
	}
	if ( pType->Size != 0u ) {
		memset(pValue, 0, pType->Size);
	}
	return true;
}



/* 按类型声明复制一个值，自复制保持原值。 */
XRT_API bool xrtTypeCopyValue(
	const xrttype* pType,
	ptr pTarget,
	const void* pSource
)
{
	if (
		(pType == NULL) ||
		((pTarget == NULL) && (pType->Size != 0u)) ||
		((pSource == NULL) && (pType->Size != 0u))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"copy", "the type, source, or destination value is invalid");
		return false;
	}
	if ( (pType->Flags & XRT_TYPE_FLAG_COPYABLE) == 0 ) {
		__xrtRuntimeTypeError(XERR_UNSUPPORTED, XTYPE_ERROR_OPERATION,
			"copy", "the runtime type is not copyable");
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Copy != NULL) ) {
		return pType->Ops->Copy(pTarget, pSource, pType);
	}
	if ( (pType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) == 0 ) {
		__xrtRuntimeTypeError(XERR_UNSUPPORTED, XTYPE_ERROR_OPERATION,
			"copy", "the runtime type has no copy operation");
		return false;
	}
	if ( pType->Size != 0u ) {
		memmove(pTarget, pSource, pType->Size);
	}
	return true;
}



/* 按类型声明移动一个值并把源值置为空状态。 */
XRT_API bool xrtTypeMoveValue(
	const xrttype* pType,
	ptr pTarget,
	ptr pSource
)
{
	if (
		(pType == NULL) ||
		((pTarget == NULL) && (pType->Size != 0u)) ||
		((pSource == NULL) && (pType->Size != 0u))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"move", "the type, source, or destination value is invalid");
		return false;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Move != NULL) ) {
		if ( pTarget == pSource ) {
			return true;
		}
		return pType->Ops->Move(pTarget, pSource, pType);
	}
	if ( (pType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) == 0 ) {
		__xrtRuntimeTypeError(XERR_UNSUPPORTED, XTYPE_ERROR_OPERATION,
			"move", "the runtime type has no move operation");
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( pType->Size != 0u ) {
		memmove(pTarget, pSource, pType->Size);
		memset(pSource, 0, pType->Size);
	}
	return true;
}



/* 执行可选销毁操作，平凡值不需要额外处理。 */
XRT_API void xrtTypeDropValue(const xrttype* pType, ptr pValue)
{
	if ( (pType == NULL) || ((pValue == NULL) && (pType->Size != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"drop", "the type or value is invalid");
		return;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Drop != NULL) ) {
		pType->Ops->Drop(pValue, pType);
	}
}



/* 优先深克隆一个值，无克隆操作时使用复制契约。 */
XRT_API bool xrtTypeCloneValue(
	const xrttype* pType,
	ptr pTarget,
	const void* pSource
)
{
	if (
		(pType == NULL) ||
		((pTarget == NULL) && (pType->Size != 0u)) ||
		((pSource == NULL) && (pType->Size != 0u))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"clone", "the type, source, or destination value is invalid");
		return false;
	}
	if ( pTarget == pSource ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"clone", "clone source and destination must be distinct");
		return false;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Clone != NULL) ) {
		return pType->Ops->Clone(pTarget, pSource, pType);
	}
	return xrtTypeCopyValue(pType, pTarget, pSource);
}



/* 通过类型操作枚举值直接拥有的强对象引用。 */
XRT_API bool xrtTypeTraceValue(
	const xrttype* pType,
	const void* pValue,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	if (
		(pType == NULL) ||
		((pValue == NULL) && (pType->Size != 0u)) ||
		(pVisit == NULL)
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"trace", "the type, value, or object visitor is invalid");
		return false;
	}
	if ( (pType->Ops == NULL) || (pType->Ops->Trace == NULL) ) {
		return true;
	}
	if ( !pType->Ops->Trace(pValue, pType, pVisit, pContext) ) {
		__xrtRuntimeTypeWrap(XERR_STATE, XTYPE_ERROR_OPERATION,
			"trace", "the runtime type reference trace failed");
		return false;
	}
	return true;
}



/* 使用实例初始化器或零初始化建立引用对象负载。 */
XRT_API bool xrtTypeInitInstance(const xrttype* pType, ptr pInstance)
{
	if ( (pType == NULL) ||
		 ((pInstance == NULL) && (pType->InstanceSize != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"instance-init", "the type or instance payload is invalid");
		return false;
	}
	if ( (pType->InstanceOps != NULL) &&
		 (pType->InstanceOps->Init != NULL) ) {
		return pType->InstanceOps->Init(pInstance, pType);
	}
	if ( pType->InstanceSize != 0u ) {
		memset(pInstance, 0, pType->InstanceSize);
	}
	return true;
}



/* 执行引用对象负载的可选销毁操作。 */
XRT_API void xrtTypeDropInstance(const xrttype* pType, ptr pInstance)
{
	if ( (pType == NULL) ||
		 ((pInstance == NULL) && (pType->InstanceSize != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"instance-drop", "the type or instance payload is invalid");
		return;
	}
	if ( (pType->InstanceOps != NULL) &&
		 (pType->InstanceOps->Drop != NULL) ) {
		pType->InstanceOps->Drop(pInstance, pType);
	}
}



/* 枚举引用对象负载直接拥有的全部强对象引用。 */
XRT_API bool xrtTypeTraceInstance(
	const xrttype* pType,
	const void* pInstance,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	if (
		(pType == NULL) ||
		((pInstance == NULL) && (pType->InstanceSize != 0u)) ||
		(pVisit == NULL)
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"instance-trace", "the type, instance, or object visitor is invalid");
		return false;
	}
	if ( (pType->InstanceOps == NULL) ||
		 (pType->InstanceOps->Trace == NULL) ) {
		return true;
	}
	if ( !pType->InstanceOps->Trace(
		pInstance, pType, pVisit, pContext
	) ) {
		__xrtRuntimeTypeWrap(XERR_STATE, XTYPE_ERROR_OPERATION,
			"instance-trace", "the runtime object instance trace failed");
		return false;
	}
	return true;
}



/* 把 float 位模式映射为统一零值且可直接比较的总序键。 */
static uint64 __xrtTypeFloat32Key(const void* pValue)
{
	uint32 iBits;

	memcpy(&iBits, pValue, sizeof(iBits));
	if ( (iBits & UINT32_C(0x7FFFFFFF)) == 0u ) {
		iBits = 0u;
	}
	return (iBits & UINT32_C(0x80000000)) != 0u ?
		(uint64)(~iBits & UINT32_MAX) :
		(uint64)(iBits ^ UINT32_C(0x80000000));
}



/* 把 double 位模式映射为统一零值且可直接比较的总序键。 */
static uint64 __xrtTypeFloat64Key(const void* pValue)
{
	uint64 iBits;

	memcpy(&iBits, pValue, sizeof(iBits));
	if ( (iBits & UINT64_C(0x7FFFFFFFFFFFFFFF)) == 0u ) {
		iBits = 0u;
	}
	return (iBits & UINT64_C(0x8000000000000000)) != 0u ?
		~iBits : (iBits ^ UINT64_C(0x8000000000000000));
}



/* 比较内建标量形态，未知类型返回不支持。 */
static bool __xrtTypeBuiltinCompare(const xrttype* pType,
	const void* pLeft, const void* pRight, int* pResult)
{
	int64 iLeftSigned;
	int64 iRightSigned;
	uint64 iLeft;
	uint64 iRight;

	switch ( pType->Kind ) {
		case XRT_TYPE_NULL:
			*pResult = 0;
			return true;
		case XRT_TYPE_BOOL: {
			bool bLeft;
			bool bRight;

			if ( !__xrtTypeReadBool(pLeft, pType->Size, &bLeft) ||
				 !__xrtTypeReadBool(pRight, pType->Size, &bRight) ) {
				return false;
			}
			*pResult = (int)bLeft - (int)bRight;
			return true;
		}
		case XRT_TYPE_SIGNED_INT:
		case XRT_TYPE_TIME:
			if ( !__xrtTypeReadSigned(pLeft, pType->Size, &iLeftSigned) ||
				 !__xrtTypeReadSigned(pRight, pType->Size, &iRightSigned) ) {
				return false;
			}
			*pResult = (iLeftSigned > iRightSigned) -
				(iLeftSigned < iRightSigned);
			return true;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( !__xrtTypeReadUnsigned(pLeft, pType->Size, &iLeft) ||
				 !__xrtTypeReadUnsigned(pRight, pType->Size, &iRight) ) {
				return false;
			}
			break;
		case XRT_TYPE_FLOAT:
			if ( pType->Size == sizeof(float) ) {
				iLeft = __xrtTypeFloat32Key(pLeft);
				iRight = __xrtTypeFloat32Key(pRight);
			} else if ( pType->Size == sizeof(double) ) {
				iLeft = __xrtTypeFloat64Key(pLeft);
				iRight = __xrtTypeFloat64Key(pRight);
			} else {
				return false;
			}
			break;
		case XRT_TYPE_POINTER: {
			ptr pLeftValue;
			ptr pRightValue;

			if ( pType->Size != sizeof(ptr) ) {
				return false;
			}
			memcpy(&pLeftValue, pLeft, sizeof(pLeftValue));
			memcpy(&pRightValue, pRight, sizeof(pRightValue));
			iLeft = (uint64)(uintptr_t)pLeftValue;
			iRight = (uint64)(uintptr_t)pRightValue;
			break;
		}
		default:
			return false;
	}
	*pResult = (iLeft > iRight) - (iLeft < iRight);
	return true;
}



/* 散列内建标量形态，规则与内建比较的相等关系保持一致。 */
static bool __xrtTypeBuiltinHash(const xrttype* pType,
	const void* pValue, uint64* pHash)
{
	uint64 iValue;
	int64 iSigned;

	switch ( pType->Kind ) {
		case XRT_TYPE_NULL:
			iValue = 0u;
			break;
		case XRT_TYPE_BOOL: {
			bool bValue;

			if ( !__xrtTypeReadBool(pValue, pType->Size, &bValue) ) {
				return false;
			}
			iValue = bValue ? 1u : 0u;
			break;
		}
		case XRT_TYPE_SIGNED_INT:
		case XRT_TYPE_TIME:
			if ( !__xrtTypeReadSigned(pValue, pType->Size, &iSigned) ) {
				return false;
			}
			iValue = (uint64)iSigned;
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( !__xrtTypeReadUnsigned(pValue, pType->Size, &iValue) ) {
				return false;
			}
			break;
		case XRT_TYPE_FLOAT:
			if ( pType->Size == sizeof(float) ) {
				iValue = __xrtTypeFloat32Key(pValue);
			} else if ( pType->Size == sizeof(double) ) {
				iValue = __xrtTypeFloat64Key(pValue);
			} else {
				return false;
			}
			break;
		case XRT_TYPE_POINTER: {
			ptr pPointer;

			if ( pType->Size != sizeof(ptr) ) {
				return false;
			}
			memcpy(&pPointer, pValue, sizeof(pPointer));
			iValue = (uint64)(uintptr_t)pPointer;
			break;
		}
		default:
			return false;
	}
	*pHash = __xrtTypeHashU64(
		UINT64_C(14695981039346656037), iValue);
	return true;
}



/* 使用类型比较操作并在成功后提交比较结果。 */
XRT_API bool xrtTypeCompareValue(const xrttype* pType,
	const void* pLeft, const void* pRight, int* pResult)
{
	int iResult;

	if ( (pType == NULL) || (pResult == NULL) ||
		 ((pLeft == NULL) && (pType->Size != 0u)) ||
		 ((pRight == NULL) && (pType->Size != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"compare", "the type, values, or result output is invalid");
		return false;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Compare != NULL) ) {
		iResult = pType->Ops->Compare(pLeft, pRight, pType);
	} else if ( !__xrtTypeBuiltinCompare(
		pType, pLeft, pRight, &iResult) ) {
		__xrtRuntimeTypeError(XERR_UNSUPPORTED, XTYPE_ERROR_OPERATION,
			"compare", "the runtime type has no comparison operation");
		return false;
	}
	*pResult = iResult;
	return true;
}



/* 使用类型散列操作并在成功后提交散列值。 */
XRT_API bool xrtTypeHashValue(const xrttype* pType,
	const void* pValue, uint64* pHash)
{
	uint64 iHash;

	if ( (pType == NULL) || (pHash == NULL) ||
		 ((pValue == NULL) && (pType->Size != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"hash", "the type, value, or hash output is invalid");
		return false;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Hash != NULL) ) {
		iHash = pType->Ops->Hash(pValue, pType);
	} else if ( !__xrtTypeBuiltinHash(pType, pValue, &iHash) ) {
		__xrtRuntimeTypeError(XERR_UNSUPPORTED, XTYPE_ERROR_OPERATION,
			"hash", "the runtime type has no hash operation");
		return false;
	}
	*pHash = iHash;
	return true;
}



/* 在按类型 ID 排序的注册表中查找第一个不小于目标 ID 的位置。 */
static size_t __xrtTypeRegistryLowerBound(
	const xrttyperegistry* pRegistry, uint64 iTypeId)
{
	size_t iBegin = 0u;
	size_t iEnd = pRegistry->Count;

	while ( iBegin < iEnd ) {
		size_t iMiddle = iBegin + ((iEnd - iBegin) / 2u);

		if ( pRegistry->Types[iMiddle]->Id < iTypeId ) {
			iBegin = iMiddle + 1u;
		} else {
			iEnd = iMiddle;
		}
	}
	return iBegin;
}



/* 为类型注册表增长紧凑指针数组。 */
static bool __xrtTypeRegistryGrow(xrttyperegistry* pRegistry)
{
	const xrttype** pTypes;
	size_t iCapacity;

	if ( pRegistry->Count != pRegistry->Capacity ) {
		return true;
	}
	if ( pRegistry->Capacity > (SIZE_MAX / 2u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iCapacity = pRegistry->Capacity != 0u ?
		pRegistry->Capacity * 2u : 16u;
	if ( iCapacity > (SIZE_MAX / sizeof(const xrttype*)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pTypes = (const xrttype**)xrtRealloc(pRegistry->Types,
		iCapacity * sizeof(const xrttype*));
	if ( pTypes == NULL ) {
		return false;
	}
	pRegistry->Types = pTypes;
	pRegistry->Capacity = iCapacity;
	return true;
}



/* 创建空的线程安全类型注册表。 */
XRT_API xrttyperegistry* xrtTypeRegistryCreate(void)
{
	xrttyperegistry* pRegistry =
		(xrttyperegistry*)xrtCalloc(1u, sizeof(xrttyperegistry));

	if ( pRegistry != NULL ) {
		__xrtSpinInit(&pRegistry->Lock);
	}
	return pRegistry;
}



/* 销毁注册表自身，不销毁任何借用描述。 */
XRT_API void xrtTypeRegistryDestroy(xrttyperegistry* pRegistry)
{
	if ( pRegistry == NULL ) {
		return;
	}
	__xrtSpinUnit(&pRegistry->Lock);
	xrtFree(pRegistry->Types);
	xrtFree(pRegistry);
}



/* 注册唯一描述指针并维持数组按稳定 ID 排序。 */
XRT_API bool xrtTypeRegistryAdd(
	xrttyperegistry* pRegistry,
	const xrttype* pType
)
{
	size_t iIndex;

	if ( (pRegistry == NULL) || !xrtTypeValidate(pType) ) {
		if ( pRegistry == NULL ) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
				"registry-add", "the type registry is null");
		}
		return false;
	}
	__xrtSpinLock(&pRegistry->Lock);
	iIndex = __xrtTypeRegistryLowerBound(pRegistry, pType->Id);
	if ( (iIndex < pRegistry->Count) &&
		 (pRegistry->Types[iIndex]->Id == pType->Id) ) {
		if ( pRegistry->Types[iIndex] == pType ) {
			__xrtSpinUnlock(&pRegistry->Lock);
			return true;
		}
		__xrtSpinUnlock(&pRegistry->Lock);
		__xrtRuntimeTypeError(XERR_EXISTS, XTYPE_ERROR_REGISTRY,
			"registry-add", "the stable type identity already has a descriptor");
		return false;
	}
	if ( !__xrtTypeRegistryGrow(pRegistry) ) {
		__xrtSpinUnlock(&pRegistry->Lock);
		return false;
	}
	if ( iIndex < pRegistry->Count ) {
		memmove(&pRegistry->Types[iIndex + 1u],
			&pRegistry->Types[iIndex],
			(pRegistry->Count - iIndex) * sizeof(const xrttype*));
	}
	pRegistry->Types[iIndex] = pType;
	pRegistry->Count++;
	__xrtSpinUnlock(&pRegistry->Lock);
	return true;
}



/* 按注册时的准确描述指针移除类型；未注册是正常的 false 结果。 */
XRT_API bool xrtTypeRegistryRemove(
	xrttyperegistry* pRegistry,
	const xrttype* pType
)
{
	size_t iIndex;

	if ( (pRegistry == NULL) || (pType == NULL) || (pType->Id == 0u) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"registry-remove", "the type registry or descriptor is invalid");
		return false;
	}
	__xrtSpinLock(&pRegistry->Lock);
	iIndex = __xrtTypeRegistryLowerBound(pRegistry, pType->Id);
	if ( (iIndex >= pRegistry->Count) ||
		 (pRegistry->Types[iIndex] != pType) ) {
		__xrtSpinUnlock(&pRegistry->Lock);
		return false;
	}
	pRegistry->Count--;
	if ( iIndex < pRegistry->Count ) {
		memmove(&pRegistry->Types[iIndex],
			&pRegistry->Types[iIndex + 1u],
			(pRegistry->Count - iIndex) * sizeof(const xrttype*));
	}
	__xrtSpinUnlock(&pRegistry->Lock);
	return true;
}



/* 在线程安全快照下返回已注册类型数量。 */
XRT_API size_t xrtTypeRegistryCount(const xrttyperegistry* pRegistry)
{
	size_t iCount;

	if ( pRegistry == NULL ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"registry-count", "the type registry is null");
		return 0;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	iCount = pRegistry->Count;
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return iCount;
}



/* 在线程安全快照下按稳定类型 ID 顺序读取一个借用描述。 */
XRT_API const xrttype* xrtTypeRegistryAt(
	const xrttyperegistry* pRegistry,
	size_t iIndex
)
{
	const xrttype* pType;

	if ( pRegistry == NULL ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"registry-at", "the type registry is null");
		return NULL;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	if ( iIndex >= pRegistry->Count ) {
		__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
		__xrtRuntimeTypeError(XERR_RANGE, XTYPE_ERROR_REGISTRY,
			"registry-at", "the type registry index is out of range");
		return NULL;
	}
	pType = pRegistry->Types[iIndex];
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return pType;
}



/* 按稳定 ID 二分查询借用的类型描述。 */
XRT_API const xrttype* xrtTypeRegistryFindId(
	const xrttyperegistry* pRegistry,
	uint64 iTypeId
)
{
	const xrttype* pResult = NULL;
	size_t iIndex;

	if ( (pRegistry == NULL) || (iTypeId == 0) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"registry-find-id", "the type registry or type ID is invalid");
		return NULL;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	iIndex = __xrtTypeRegistryLowerBound(pRegistry, iTypeId);
	if ( (iIndex < pRegistry->Count) &&
		 (pRegistry->Types[iIndex]->Id == iTypeId) ) {
		pResult = pRegistry->Types[iIndex];
	}
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return pResult;
}



/* 由规范 ABI 名计算 ID 并二分查询借用描述。 */
XRT_API const xrttype* xrtTypeRegistryFindName(
	const xrttyperegistry* pRegistry,
	xstrview AbiName
)
{
	const xrttype* pResult = NULL;
	uint64 iTypeId;
	size_t iIndex;

	if ( (pRegistry == NULL) || !__xrtTypeViewValid(&AbiName, false) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"registry-find-name", "the type registry or ABI name is invalid");
		return NULL;
	}
	iTypeId = __xrtTypeComputedId(AbiName);
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	iIndex = __xrtTypeRegistryLowerBound(pRegistry, iTypeId);
	if ( (iIndex < pRegistry->Count) &&
		 (pRegistry->Types[iIndex]->Id == iTypeId) &&
		 __xrtTypeViewEqual(&pRegistry->Types[iIndex]->AbiName, &AbiName) ) {
		pResult = pRegistry->Types[iIndex];
	}
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return pResult;
}



/* 验证一个协议要求的名称和函数签名。 */
static bool __xrtProtocolRequirementValid(
	const xrtprotocolrequirement* pRequirement
)
{
	return (pRequirement != NULL) &&
		__xrtTypeViewValid(&pRequirement->Name, false) &&
		__xrtFunctionSigValidate(pRequirement->Signature);
}



/* 检查协议类型、要求数组以及每个重载身份的唯一性。 */
static bool __xrtProtocolValidate(const xrtprotocol* pProtocol)
{
	if (
		(pProtocol == NULL) ||
		!xrtTypeValidate(pProtocol->Type) ||
		(pProtocol->Type->Kind != XRT_TYPE_PROTOCOL) ||
		((pProtocol->RequirementCount != 0u) &&
		 (pProtocol->Requirements == NULL))
	) {
		return false;
	}
	for ( size_t i = 0; i < pProtocol->RequirementCount; i++ ) {
		const xrtprotocolrequirement* pRequirement =
			&pProtocol->Requirements[i];

		if ( !__xrtProtocolRequirementValid(pRequirement) ) {
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			const xrtprotocolrequirement* pPrevious =
				&pProtocol->Requirements[j];

			if (
				__xrtTypeViewEqual(&pPrevious->Name, &pRequirement->Name) &&
				(__xrtFunctionSigComputedId(pPrevious->Signature) ==
				 __xrtFunctionSigComputedId(pRequirement->Signature))
			) {
				return false;
			}
		}
	}
	return true;
}



/* 验证协议描述自身，不要求先构造具体类型见证。 */
XRT_API bool xrtProtocolValidate(const xrtprotocol* pProtocol)
{
	if ( !__xrtProtocolValidate(pProtocol) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
			"protocol-validate", "the protocol descriptor is invalid");
		return false;
	}
	return true;
}



/* 验证协议和具体类型，并逐项核对唯一见证入口。 */
XRT_API bool xrtProtocolWitnessValidate(
	const xrtprotocolwitness* pWitness
)
{
	const xrtprotocol* pProtocol;

	if (
		(pWitness == NULL) ||
		(pWitness->Protocol == NULL) ||
		(pWitness->ConcreteType == NULL) ||
		((pWitness->EntryCount != 0) && (pWitness->Entries == NULL))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
			"witness-validate", "the protocol witness structure is invalid");
		return false;
	}
	pProtocol = pWitness->Protocol;
	if (
		!__xrtProtocolValidate(pProtocol) ||
		!xrtTypeValidate(pWitness->ConcreteType) ||
		(pWitness->EntryCount != pProtocol->RequirementCount)
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
			"witness-validate", "the protocol or witness shape is invalid");
		return false;
	}
	for ( size_t i = 0; i < pWitness->EntryCount; i++ ) {
		const xrtprotocolentry* pEntry = &pWitness->Entries[i];

		if (
			!__xrtTypeViewValid(&pEntry->Name, false) ||
			!__xrtFunctionSigValidate(pEntry->Signature) ||
			(pEntry->Entry == NULL)
		) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
				"witness-validate", "a protocol witness entry is invalid");
			return false;
		}
	}
	for ( size_t i = 0; i < pProtocol->RequirementCount; i++ ) {
		const xrtprotocolrequirement* pRequirement =
			&pProtocol->Requirements[i];
		size_t iMatches = 0;

		for ( size_t j = 0; j < pWitness->EntryCount; j++ ) {
			const xrtprotocolentry* pEntry = &pWitness->Entries[j];

			if (
				__xrtTypeViewEqual(&pEntry->Name, &pRequirement->Name) &&
				(__xrtFunctionSigComputedId(pEntry->Signature) ==
				 __xrtFunctionSigComputedId(pRequirement->Signature))
			) {
				iMatches++;
			}
		}
		if ( iMatches != 1u ) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
				"witness-validate", "each protocol requirement must have one witness entry");
			return false;
		}
	}
	return true;
}



/* 按名称和可选签名查询见证入口。 */
XRT_API const xrtprotocolentry* xrtProtocolWitnessFind(
	const xrtprotocolwitness* pWitness,
	xstrview Name,
	uint64 iSignatureId
)
{
	if (
		(pWitness == NULL) ||
		!__xrtTypeViewValid(&Name, false) ||
		((pWitness->EntryCount != 0) && (pWitness->Entries == NULL))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
			"witness-find", "the protocol witness or method name is invalid");
		return NULL;
	}
	for ( size_t i = 0; i < pWitness->EntryCount; i++ ) {
		const xrtprotocolentry* pEntry = &pWitness->Entries[i];

		if (
			!__xrtTypeViewValid(&pEntry->Name, false) ||
			!__xrtFunctionSigValidate(pEntry->Signature) ||
			(pEntry->Entry == NULL)
		) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
				"witness-find", "a protocol witness entry is invalid");
			return NULL;
		}
		if (
			__xrtTypeViewEqual(&pEntry->Name, &Name) &&
			((iSignatureId == 0) ||
			 (__xrtFunctionSigComputedId(pEntry->Signature) == iSignatureId))
		) {
			return pEntry;
		}
	}
	return NULL;
}



/* 比较协议和具体类型 ID 组成的稳定见证键。 */
static int __xrtProtocolKeyCompare(const xrtprotocolwitness* pWitness,
	uint64 iProtocolTypeId, uint64 iConcreteTypeId)
{
	uint64 iProtocol = pWitness->Protocol->Type->Id;
	uint64 iConcrete = pWitness->ConcreteType->Id;

	if ( iProtocol != iProtocolTypeId ) {
		return iProtocol < iProtocolTypeId ? -1 : 1;
	}
	if ( iConcrete != iConcreteTypeId ) {
		return iConcrete < iConcreteTypeId ? -1 : 1;
	}
	return 0;
}



/* 在按稳定见证键排序的注册表中执行二分下界查询。 */
static size_t __xrtProtocolRegistryLowerBound(
	const xrtprotocolregistry* pRegistry,
	uint64 iProtocolTypeId, uint64 iConcreteTypeId)
{
	size_t iBegin = 0u;
	size_t iEnd = pRegistry->Count;

	while ( iBegin < iEnd ) {
		size_t iMiddle = iBegin + ((iEnd - iBegin) / 2u);
		int iCompare = __xrtProtocolKeyCompare(
			pRegistry->Witnesses[iMiddle], iProtocolTypeId, iConcreteTypeId);

		if ( iCompare < 0 ) {
			iBegin = iMiddle + 1u;
		} else {
			iEnd = iMiddle;
		}
	}
	return iBegin;
}



/* 为协议注册表增长紧凑见证指针数组。 */
static bool __xrtProtocolRegistryGrow(xrtprotocolregistry* pRegistry)
{
	const xrtprotocolwitness** pWitnesses;
	size_t iCapacity;

	if ( pRegistry->Count != pRegistry->Capacity ) {
		return true;
	}
	if ( pRegistry->Capacity > (SIZE_MAX / 2u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iCapacity = pRegistry->Capacity != 0u ?
		pRegistry->Capacity * 2u : 16u;
	if ( iCapacity > (SIZE_MAX / sizeof(const xrtprotocolwitness*)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pWitnesses = (const xrtprotocolwitness**)xrtRealloc(
		pRegistry->Witnesses,
		iCapacity * sizeof(const xrtprotocolwitness*));
	if ( pWitnesses == NULL ) {
		return false;
	}
	pRegistry->Witnesses = pWitnesses;
	pRegistry->Capacity = iCapacity;
	return true;
}



/* 创建空的线程安全协议见证注册表。 */
XRT_API xrtprotocolregistry* xrtProtocolRegistryCreate(void)
{
	xrtprotocolregistry* pRegistry =
		(xrtprotocolregistry*)xrtCalloc(1u, sizeof(xrtprotocolregistry));

	if ( pRegistry != NULL ) {
		__xrtSpinInit(&pRegistry->Lock);
	}
	return pRegistry;
}



/* 销毁协议注册表自身，不销毁任何借用见证。 */
XRT_API void xrtProtocolRegistryDestroy(xrtprotocolregistry* pRegistry)
{
	if ( pRegistry == NULL ) {
		return;
	}
	__xrtSpinUnit(&pRegistry->Lock);
	xrtFree(pRegistry->Witnesses);
	xrtFree(pRegistry);
}



/* 按协议和具体类型组成的键注册唯一见证指针。 */
XRT_API bool xrtProtocolRegistryAdd(
	xrtprotocolregistry* pRegistry,
	const xrtprotocolwitness* pWitness
)
{
	uint64 iProtocolTypeId;
	uint64 iConcreteTypeId;
	size_t iIndex;

	if ( (pRegistry == NULL) || !xrtProtocolWitnessValidate(pWitness) ) {
		if ( pRegistry == NULL ) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
				"protocol-add", "the protocol registry is null");
		}
		return false;
	}
	iProtocolTypeId = pWitness->Protocol->Type->Id;
	iConcreteTypeId = pWitness->ConcreteType->Id;
	__xrtSpinLock(&pRegistry->Lock);
	iIndex = __xrtProtocolRegistryLowerBound(
		pRegistry, iProtocolTypeId, iConcreteTypeId);
	if ( (iIndex < pRegistry->Count) &&
		 (__xrtProtocolKeyCompare(pRegistry->Witnesses[iIndex],
			iProtocolTypeId, iConcreteTypeId) == 0) ) {
		if ( pRegistry->Witnesses[iIndex] == pWitness ) {
			__xrtSpinUnlock(&pRegistry->Lock);
			return true;
		}
		__xrtSpinUnlock(&pRegistry->Lock);
		__xrtRuntimeTypeError(XERR_EXISTS, XTYPE_ERROR_REGISTRY,
			"protocol-add", "a witness for this protocol and type already exists");
		return false;
	}
	if ( !__xrtProtocolRegistryGrow(pRegistry) ) {
		__xrtSpinUnlock(&pRegistry->Lock);
		return false;
	}
	if ( iIndex < pRegistry->Count ) {
		memmove(&pRegistry->Witnesses[iIndex + 1u],
			&pRegistry->Witnesses[iIndex],
			(pRegistry->Count - iIndex) * sizeof(const xrtprotocolwitness*));
	}
	pRegistry->Witnesses[iIndex] = pWitness;
	pRegistry->Count++;
	__xrtSpinUnlock(&pRegistry->Lock);
	return true;
}



/* 按准确见证指针移除注册项。 */
XRT_API bool xrtProtocolRegistryRemove(
	xrtprotocolregistry* pRegistry,
	const xrtprotocolwitness* pWitness
)
{
	if ( (pRegistry == NULL) || !xrtProtocolWitnessValidate(pWitness) ) {
		if ( pRegistry == NULL ) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
				"protocol-remove", "the protocol registry is null");
		}
		return false;
	}
	__xrtSpinLock(&pRegistry->Lock);
	{
		size_t iIndex = __xrtProtocolRegistryLowerBound(pRegistry,
			pWitness->Protocol->Type->Id, pWitness->ConcreteType->Id);

		if ( (iIndex < pRegistry->Count) &&
			 (pRegistry->Witnesses[iIndex] == pWitness) ) {
			pRegistry->Count--;
			if ( iIndex < pRegistry->Count ) {
				memmove(&pRegistry->Witnesses[iIndex],
					&pRegistry->Witnesses[iIndex + 1u],
					(pRegistry->Count - iIndex) *
						sizeof(const xrtprotocolwitness*));
			}
			__xrtSpinUnlock(&pRegistry->Lock);
			return true;
		}
	}
	__xrtSpinUnlock(&pRegistry->Lock);
	return false;
}



/* 按协议和具体类型 ID 二分查询见证。 */
XRT_API const xrtprotocolwitness* xrtProtocolRegistryFind(
	const xrtprotocolregistry* pRegistry,
	uint64 iProtocolTypeId,
	uint64 iConcreteTypeId
)
{
	const xrtprotocolwitness* pResult = NULL;
	size_t iIndex;

	if (
		(pRegistry == NULL) ||
		(iProtocolTypeId == 0) ||
		(iConcreteTypeId == 0)
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"protocol-find", "the protocol registry or lookup IDs are invalid");
		return NULL;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	iIndex = __xrtProtocolRegistryLowerBound(
		pRegistry, iProtocolTypeId, iConcreteTypeId);
	if ( (iIndex < pRegistry->Count) &&
		 (__xrtProtocolKeyCompare(pRegistry->Witnesses[iIndex],
			iProtocolTypeId, iConcreteTypeId) == 0) ) {
		pResult = pRegistry->Witnesses[iIndex];
	}
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return pResult;
}



/* 在线程安全快照下返回已注册见证数量。 */
XRT_API size_t xrtProtocolRegistryCount(
	const xrtprotocolregistry* pRegistry
)
{
	size_t iCount;

	if ( pRegistry == NULL ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"protocol-count", "the protocol registry is null");
		return 0;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	iCount = pRegistry->Count;
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return iCount;
}



/* 在线程安全快照下按协议和具体类型 ID 顺序读取一个借用见证。 */
XRT_API const xrtprotocolwitness* xrtProtocolRegistryAt(
	const xrtprotocolregistry* pRegistry,
	size_t iIndex
)
{
	const xrtprotocolwitness* pWitness;

	if ( pRegistry == NULL ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"protocol-at", "the protocol registry is null");
		return NULL;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	if ( iIndex >= pRegistry->Count ) {
		__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
		__xrtRuntimeTypeError(XERR_RANGE, XTYPE_ERROR_REGISTRY,
			"protocol-at", "the protocol registry index is out of range");
		return NULL;
	}
	pWitness = pRegistry->Witnesses[iIndex];
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return pWitness;
}



/* 验证枚举类型以及所有变体名称、标签和负载身份。 */
XRT_API bool xrtEnumValidate(const xrtenum* pEnum)
{
	if (
		(pEnum == NULL) ||
		!xrtTypeValidate(pEnum->Type) ||
		(pEnum->Type->Kind != XRT_TYPE_ENUM) ||
		((pEnum->VariantCount != 0) && (pEnum->Variants == NULL))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_ENUM,
			"enum-validate", "the enum descriptor is invalid");
		return false;
	}
	for ( size_t i = 0; i < pEnum->VariantCount; i++ ) {
		const xrtenumvariant* pVariant = &pEnum->Variants[i];

		if (
			!__xrtTypeViewValid(&pVariant->Name, false) ||
			((pVariant->PayloadType != NULL) &&
			 !__xrtTypeIdentityValid(pVariant->PayloadType))
		) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_ENUM,
				"enum-validate", "an enum variant is invalid");
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			if (
				(pEnum->Variants[j].Tag == pVariant->Tag) ||
				__xrtTypeViewEqual(
					&pEnum->Variants[j].Name,
					&pVariant->Name
				)
			) {
				__xrtRuntimeTypeError(XERR_EXISTS, XTYPE_ERROR_ENUM,
					"enum-validate", "enum variant names and tags must be unique");
				return false;
			}
		}
	}
	return true;
}



/* 按标签线性查询借用的枚举变体。 */
XRT_API const xrtenumvariant* xrtEnumFindTag(
	const xrtenum* pEnum,
	int64 iTag
)
{
	if ( (pEnum == NULL) ||
		 ((pEnum->VariantCount != 0u) && (pEnum->Variants == NULL)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_ENUM,
			"enum-find-tag", "the enum descriptor is invalid");
		return NULL;
	}
	for ( size_t i = 0; i < pEnum->VariantCount; i++ ) {
		if ( pEnum->Variants[i].Tag == iTag ) {
			return &pEnum->Variants[i];
		}
	}
	return NULL;
}



/* 按名称线性查询借用的枚举变体。 */
XRT_API const xrtenumvariant* xrtEnumFindName(
	const xrtenum* pEnum,
	xstrview Name
)
{
	if ( (pEnum == NULL) || !__xrtTypeViewValid(&Name, false) ||
		 ((pEnum->VariantCount != 0u) && (pEnum->Variants == NULL)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_ENUM,
			"enum-find-name", "the enum descriptor or variant name is invalid");
		return NULL;
	}
	for ( size_t i = 0; i < pEnum->VariantCount; i++ ) {
		if ( __xrtTypeViewEqual(&pEnum->Variants[i].Name, &Name) ) {
			return &pEnum->Variants[i];
		}
	}
	return NULL;
}



#define XRT_BUILTIN_TYPE( \
	Function, IdValue, KindValue, FlagsValue, NameValue, CType \
) \
	XRT_API const xrttype* Function(void) \
	{ \
		static const xrttype Type = { \
			.Id = IdValue, \
			.Kind = KindValue, \
			.Flags = (FlagsValue) | XRT_TYPE_FLAG_RELOCATABLE, \
			.Name = XRT_STR_INIT(NameValue), \
			.AbiName = XRT_STR_INIT("xrt." NameValue), \
			.Size = sizeof(CType), \
			.Align = XRT_INTERNAL_ALIGNOF(CType), \
			.InstanceSize = sizeof(CType), \
			.InstanceAlign = XRT_INTERNAL_ALIGNOF(CType) \
		}; \
		return &Type; \
	}



/* 返回进程期稳定的 null 类型描述。 */
XRT_API const xrttype* xrtTypeNull(void)
{
	static const xrttype Type = {
		.Id = UINT64_C(0x6950D50F203E7DCC),
		.Kind = XRT_TYPE_NULL,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL |
			XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("null"),
		.AbiName = XRT_STR_INIT("xrt.null"),
		.Size = 0u,
		.Align = 1u,
		.InstanceSize = 0u,
		.InstanceAlign = 1u
	};
	return &Type;
}



XRT_BUILTIN_TYPE(
	xrtTypeBool, UINT64_C(0x0DDD5573D01F8925),
	XRT_TYPE_BOOL,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"bool", bool
)
XRT_BUILTIN_TYPE(
	xrtTypeBool32, UINT64_C(0x5200B3575DC757F0),
	XRT_TYPE_BOOL,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"bool32", int32
)
XRT_BUILTIN_TYPE(
	xrtTypeInt8, UINT64_C(0x8C0A1E1A952DE77A),
	XRT_TYPE_SIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"int8", int8
)
XRT_BUILTIN_TYPE(
	xrtTypeUInt8, UINT64_C(0x6F71A731A529D6F3),
	XRT_TYPE_UNSIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"uint8", uint8
)
XRT_BUILTIN_TYPE(
	xrtTypeInt16, UINT64_C(0x2300E52B7CEC35F9),
	XRT_TYPE_SIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"int16", int16
)
XRT_BUILTIN_TYPE(
	xrtTypeUInt16, UINT64_C(0x880DEC5BA62C9A6A),
	XRT_TYPE_UNSIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"uint16", uint16
)
XRT_BUILTIN_TYPE(
	xrtTypeInt32, UINT64_C(0x22F9F92B7CE63947),
	XRT_TYPE_SIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"int32", int32
)
XRT_BUILTIN_TYPE(
	xrtTypeUInt32, UINT64_C(0x8814705BA631E664),
	XRT_TYPE_UNSIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"uint32", uint32
)
XRT_BUILTIN_TYPE(
	xrtTypeInt64, UINT64_C(0x22E8E12B7CD79D4C),
	XRT_TYPE_SIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"int64", int64
)
XRT_BUILTIN_TYPE(
	xrtTypeUInt64, UINT64_C(0x88256C5BA64052CB),
	XRT_TYPE_UNSIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"uint64", uint64
)
XRT_BUILTIN_TYPE(
	xrtTypeFloat32, UINT64_C(0x4686ECDDD67F49D8),
	XRT_TYPE_FLOAT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"float32", float
)
XRT_BUILTIN_TYPE(
	xrtTypeFloat64, UINT64_C(0x467CE0DDD676E0EF),
	XRT_TYPE_FLOAT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"float64", double
)
XRT_BUILTIN_TYPE(
	xrtTypeTime, UINT64_C(0x347415BB369EAF3C),
	XRT_TYPE_TIME,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"time", xtime
)
XRT_BUILTIN_TYPE(
	xrtTypePointer, UINT64_C(0x55A641CF4B82EC82),
	XRT_TYPE_POINTER,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_NULLABLE |
		XRT_TYPE_FLAG_FINAL,
	"pointer", ptr
)
XRT_BUILTIN_TYPE(
	xrtTypeType, UINT64_C(0xC150A9BB870BECF5),
	XRT_TYPE_TYPE,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"type", uint64
)



#undef XRT_BUILTIN_TYPE

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_object.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)

/* 设置运行时对象模块结构化错误。 */
static void __xrtRuntimeObjectError(
	xerrkind Kind,
	xobjecterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.object";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型操作错误补充对象操作上下文。 */
static void __xrtRuntimeObjectWrap(
	xerrkind DefaultKind,
	xobjecterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.object";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 返回对象内已经按类型要求对齐的可写负载。 */
static ptr __xrtObjectPayload(xrtobject* pObject)
{
	return ((uint8*)pObject) + pObject->PayloadOffset;
}



/* 返回对象内已经按类型要求对齐的只读负载。 */
static const void* __xrtObjectConstPayload(const xrtobject* pObject)
{
	return ((const uint8*)pObject) + pObject->PayloadOffset;
}



/* 把对象强引用值初始化为空。 */
static bool __xrtObjectValueInit(ptr pValue, const xrttype* pType)
{
	xrtobject* pObject = NULL;
	(void)pType;

	memcpy(pValue, &pObject, sizeof(pObject));
	return true;
}



/* 先保留新对象，再失败原子地替换目标强引用值。 */
static bool __xrtObjectValueCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	xrtobject* pObject;
	xrtobject* pOldObject;
	(void)pType;

	memcpy(&pObject, pSource, sizeof(pObject));
	if ( (pObject != NULL) && (xrtObjectRef(pObject) == NULL) ) {
		return false;
	}
	memcpy(&pOldObject, pTarget, sizeof(pOldObject));
	memcpy(pTarget, &pObject, sizeof(pObject));
	xrtObjectUnref(pOldObject);
	return true;
}



/* 把源强引用移入目标，并释放目标原来拥有的对象。 */
static bool __xrtObjectValueMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	xrtobject* pObject;
	xrtobject* pOldObject;
	xrtobject* pEmpty = NULL;
	(void)pType;

	memcpy(&pObject, pSource, sizeof(pObject));
	memcpy(&pOldObject, pTarget, sizeof(pOldObject));
	memcpy(pTarget, &pObject, sizeof(pObject));
	memcpy(pSource, &pEmpty, sizeof(pEmpty));
	xrtObjectUnref(pOldObject);
	return true;
}



/* 释放强引用值并先把槽位恢复为空。 */
static void __xrtObjectValueDrop(ptr pValue, const xrttype* pType)
{
	xrtobject* pObject;
	xrtobject* pEmpty = NULL;
	(void)pType;

	memcpy(&pObject, pValue, sizeof(pObject));
	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	xrtObjectUnref(pObject);
}



/* 按进程内对象地址比较两个强引用值。 */
static int __xrtObjectValueCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	xrtobject* pLeftObject;
	xrtobject* pRightObject;
	uintptr_t iLeft;
	uintptr_t iRight;
	(void)pType;

	memcpy(&pLeftObject, pLeft, sizeof(pLeftObject));
	memcpy(&pRightObject, pRight, sizeof(pRightObject));
	iLeft = (uintptr_t)pLeftObject;
	iRight = (uintptr_t)pRightObject;
	return iLeft == iRight ? 0 : (iLeft < iRight ? -1 : 1);
}



/* 按进程内对象地址散列强引用值。 */
static uint64 __xrtObjectValueHash(
	const void* pValue,
	const xrttype* pType
)
{
	xrtobject* pObject;
	(void)pType;

	memcpy(&pObject, pValue, sizeof(pObject));
	return (uint64)(uintptr_t)pObject;
}



/* 枚举强引用槽位当前拥有的非空对象。 */
static bool __xrtObjectValueTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xrtobject* pObject;
	(void)pType;

	memcpy(&pObject, pValue, sizeof(pObject));
	return (pObject == NULL) || pVisit(pObject, pContext);
}



/* 对象强引用槽统一使用同一份不可变生命周期和追踪策略。 */
const xrttypeops __xrtObjectValueOperations = {
	.Init = __xrtObjectValueInit,
	.Copy = __xrtObjectValueCopy,
	.Move = __xrtObjectValueMove,
	.Drop = __xrtObjectValueDrop,
	.Clone = __xrtObjectValueCopy,
	.Compare = __xrtObjectValueCompare,
	.Hash = __xrtObjectValueHash,
	.Trace = __xrtObjectValueTrace
};



/* 返回对象强引用槽使用的统一值操作表。 */
XRT_API const xrttypeops* xrtObjectValueOps(void)
{
	return &__xrtObjectValueOperations;
}



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)

/* 原子取得对象图可见的对象状态。 */
static int32 __xrtObjectState(const xrtobject* pObject)
{
	return __xrtAtomicRefLoad(&pObject->State);
}



/* 从活动状态独占取得一次负载终结权。 */
bool __xrtObjectBeginFinalize(xrtobject* pObject)
{
	return __xrtAtomicRefCompareExchange(
		&pObject->State,
		XRT_OBJECT_STATE_FINALIZING,
		XRT_OBJECT_STATE_ACTIVE
	) == XRT_OBJECT_STATE_ACTIVE;
}



/* 在尚未销毁负载前撤销对象图批量终结。 */
void __xrtObjectCancelFinalize(xrtobject* pObject)
{
	if ( __xrtAtomicRefCompareExchange(
			&pObject->State,
			XRT_OBJECT_STATE_ACTIVE,
			XRT_OBJECT_STATE_FINALIZING
		) != XRT_OBJECT_STATE_FINALIZING ) {
		__xrtErrorSetInvalidState();
	}
}



/* 执行一次类型负载销毁；调用方必须已经独占终结权。 */
void __xrtObjectDropPayload(xrtobject* pObject)
{
	xrtTypeDropInstance(pObject->Type, __xrtObjectPayload(pObject));
}



/* 在负载销毁后把控制块发布为已终结状态。 */
void __xrtObjectEndFinalize(xrtobject* pObject)
{
	if ( __xrtAtomicRefCompareExchange(
			&pObject->State,
			XRT_OBJECT_STATE_FINALIZED,
			XRT_OBJECT_STATE_FINALIZING
		) != XRT_OBJECT_STATE_FINALIZING ) {
		__xrtErrorSetInvalidState();
	}
}

#endif



/* 释放一个控制块弱引用，最后一个弱引用负责回收整块内存。 */
static void __xrtObjectWeakRelease(xrtobject* pObject)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	int32 iReferences = xrtRefRelease(&pObject->WeakCount);

	if ( iReferences < 0 ) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		__xrtErrorSetInvalidState();
		return;
	}
	if ( iReferences == 0 ) {
		xrtFree(pObject);
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
}



/* 按类型声明的负载大小创建堆对象。 */
XRT_API xrtobject* xrtObjectCreate(const xrttype* pType)
{
	return xrtObjectCreateSized(
		pType,
		pType != NULL ? pType->InstanceSize : 0
	);
}



/* 创建带有可变尾随负载且满足类型对齐要求的对象。 */
XRT_API xrtobject* xrtObjectCreateSized(
	const xrttype* pType,
	size_t iSize
)
{
	xrtobject* pObject;
	size_t iPrefix = offsetof(xrtobject, Storage);
	size_t iPayload = iSize != 0 ? iSize : 1u;
	size_t iPadding;
	size_t iAllocation;
	uintptr_t iStorage;
	uintptr_t iMask;

	if ( !xrtTypeValidate(pType) ) {
		__xrtRuntimeObjectWrap(XERR_ARGUMENT, XOBJECT_ERROR_TYPE,
			"create", "the runtime object type is invalid");
		return NULL;
	}
	if ( (pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0 ) {
		__xrtRuntimeObjectError(XERR_TYPE, XOBJECT_ERROR_TYPE,
			"create", "the runtime object type is not a reference type");
		return NULL;
	}
	if ( iSize < pType->InstanceSize ) {
		__xrtRuntimeObjectError(XERR_RANGE, XOBJECT_ERROR_SIZE,
			"create", "the runtime object payload is smaller than its type");
		return NULL;
	}
	if ( (pType->InstanceAlign - 1u) > (SIZE_MAX - iPrefix) ) {
		__xrtRuntimeObjectError(XERR_RANGE, XOBJECT_ERROR_SIZE,
			"create", "the runtime object alignment overflows its allocation");
		return NULL;
	}
	iAllocation = iPrefix + (pType->InstanceAlign - 1u);
	if ( iPayload > (SIZE_MAX - iAllocation) ) {
		__xrtRuntimeObjectError(XERR_RANGE, XOBJECT_ERROR_SIZE,
			"create", "the runtime object payload overflows its allocation");
		return NULL;
	}
	iAllocation += iPayload;
	pObject = (xrtobject*)xrtCalloc(1u, iAllocation);
	if ( pObject == NULL ) {
		return NULL;
	}
	iStorage = (uintptr_t)(((uint8*)pObject) + iPrefix);
	iMask = (uintptr_t)(pType->InstanceAlign - 1u);
	iPadding = (iStorage & iMask) == 0 ?
		0u : pType->InstanceAlign - (size_t)(iStorage & iMask);
	pObject->StrongCount = 1;
	pObject->WeakCount = 1;
	pObject->Type = pType;
	pObject->Size = iSize;
	pObject->PayloadOffset = iPrefix + iPadding;
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	pObject->State = XRT_OBJECT_STATE_ACTIVE;
#endif
	if ( !xrtTypeInitInstance(pType, __xrtObjectPayload(pObject)) ) {
		xrtFree(pObject);
		__xrtRuntimeObjectWrap(XERR_STATE, XOBJECT_ERROR_INITIALIZE,
			"create", "the runtime object initializer failed");
		return NULL;
	}
	return pObject;
}



/* 增加一个已经存活对象的强引用。 */
static xrtobject* __xrtObjectRefBody(xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"ref", "the runtime object is null");
		return NULL;
	}
	if (pObject->OwnershipCleared) {
		__xrtErrorSetInvalidState(); return NULL;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	if ( __xrtObjectState(pObject) != XRT_OBJECT_STATE_ACTIVE ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_REFERENCE,
			"ref", "the runtime object is being finalized");
		return NULL;
	}
#endif
	if ( xrtRefRetain(&pObject->StrongCount) < 0 ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_REFERENCE,
			"ref", "the runtime object reference cannot be retained");
		return NULL;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	if ( __xrtObjectState(pObject) != XRT_OBJECT_STATE_ACTIVE ) {
		(void)xrtRefRelease(&pObject->StrongCount);
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_REFERENCE,
			"ref", "the runtime object began finalizing during retain");
		return NULL;
	}
#endif
	return pObject;
}

XRT_API xrtobject* xrtObjectRef(xrtobject* pObject)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	xrtobject* pResult = __xrtObjectRefBody(pObject);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return pResult;
}



/* 释放一个对象强引用，并在最后一次释放时销毁负载。 */
XRT_API void xrtObjectUnref(xrtobject* pObject)
{
	int32 iReferences;

	if ( pObject == NULL ) {
		return;
	}
	/* Only the counter transition participates here. Instance destruction is
	 * outside this scope; participating instance adapters detach their own
	 * fields under mutation before invoking arbitrary payload destructors. */
	iReferences = xrtOwnershipRefRelease(&pObject->StrongCount);
	if ( iReferences < 0 ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_REFERENCE,
			"unref", "the runtime object reference cannot be released");
		return;
	}
	if ( iReferences != 0 ) {
		return;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	if ( __xrtObjectState(pObject) == XRT_OBJECT_STATE_FINALIZED ) {
		__xrtObjectWeakRelease(pObject);
		return;
	}
	if ( !__xrtObjectBeginFinalize(pObject) ) {
		return;
	}
	__xrtObjectGraphDetach(pObject);
	__xrtObjectDropPayload(pObject);
	__xrtObjectEndFinalize(pObject);
#else
	xrtTypeDropInstance(pObject->Type, __xrtObjectPayload(pObject));
#endif
	__xrtObjectWeakRelease(pObject);
}



/* 返回对象借用的运行时类型描述。 */
XRT_API const xrttype* xrtObjectType(const xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"type", "the runtime object is null");
		return NULL;
	}
	return pObject->Type;
}



/* 返回对象借用的可写负载。 */
XRT_API ptr xrtObjectData(xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"data", "the runtime object is null");
		return NULL;
	}
	return __xrtObjectPayload(pObject);
}



/* 返回对象借用的只读负载。 */
XRT_API const void* xrtObjectConstData(const xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"const-data", "the runtime object is null");
		return NULL;
	}
	return __xrtObjectConstPayload(pObject);
}



/* 返回对象创建时声明的真实负载长度。 */
XRT_API size_t xrtObjectSize(const xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"size", "the runtime object is null");
		return 0;
	}
	return pObject->Size;
}



/* 原子读取对象当前的瞬时强引用数量。 */
XRT_API size_t xrtObjectRefCount(const xrtobject* pObject)
{
	int32 iReferences;

	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"ref-count", "the runtime object is null");
		return 0u;
	}
	iReferences = __xrtAtomicRefLoad(&pObject->StrongCount);
	return iReferences > 0 ? (size_t)iReferences : 0u;
}



static bool __xrtObjectOwnershipCount(const void* pData, size_t* pCount)
{
	if (((const xrtobject*)pData)->OwnershipCleared) return false;
	#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
		if (__xrtAtomicRefLoad(&((const xrtobject*)pData)->State) != XRT_OBJECT_STATE_ACTIVE) return false;
	#endif
	*pCount = xrtObjectRefCount((const xrtobject*)pData);
	return *pCount != 0;
}

static bool __xrtObjectOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xrtobject* pObject = (const xrtobject*)pData;
	if (pObject->OwnershipCleared) return false;
	#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
		if (__xrtAtomicRefLoad(&pObject->State) != XRT_OBJECT_STATE_ACTIVE) return false;
	#endif
	const void* pPayload = xrtObjectConstData(pObject);
	if (pPayload == NULL) return false;
	if (pObject->OwnershipTrace == NULL) { __xrtErrorSetUnsupported(); return false; }
	return pObject->OwnershipTrace(pPayload, pVisit, pContext);
}

static const xrtownershipops __xrtObjectOwnershipOps = {
	__xrtObjectOwnershipCount, __xrtObjectOwnershipTrace
};

XRT_API xrtownershipref xrtObjectOwnership(const xrtobject* pObject)
{
	return (xrtownershipref){pObject, &__xrtObjectOwnershipOps};
}

XRT_API bool xrtObjectOwnershipTraceBind(xrtobject* pObject, xrtownershiptrace pTrace)
{
	#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
		if (pObject != NULL && __xrtAtomicRefLoad(&pObject->State) != XRT_OBJECT_STATE_ACTIVE) {
			__xrtErrorSetInvalidState(); return false;
		}
	#endif
	if (pObject == NULL || pTrace == NULL || pObject->OwnershipTrace != NULL ||
		xrtObjectRefCount(pObject) != 1 || xrtObjectConstData(pObject) == NULL) {
		__xrtErrorSetInvalidState(); return false;
	}
	pObject->OwnershipTrace = pTrace;
	return true;
}

/* 判断调用方持有的对象是否只有一个瞬时强引用。 */
XRT_API bool xrtObjectUnique(const xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"unique", "the runtime object is null");
		return false;
	}
	return __xrtAtomicRefLoad(&pObject->StrongCount) == 1;
}



/* 从可选的存活对象初始化一个空弱引用。 */
XRT_API bool xrtWeakInit(xrtweak* pWeak, xrtobject* pObject)
{
	if ( (pWeak == NULL) || (pWeak->Control != NULL) ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-init", "the destination weak reference is invalid");
		return false;
	}
	if ( pObject == NULL ) {
		return true;
	}
	if ( xrtRefRetain(&pObject->WeakCount) < 0 ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_WEAK,
			"weak-init", "the object weak reference cannot be retained");
		return false;
	}
	pWeak->Control = pObject;
	return true;
}



/* 复制弱引用并替换目标，先保留新控制块以保证失败原子性。 */
XRT_API bool xrtWeakCopy(xrtweak* pTarget, const xrtweak* pSource)
{
	xrtobject* pObject;
	xrtobject* pOldObject;

	if ( (pTarget == NULL) || (pSource == NULL) ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-copy", "the source or destination weak reference is null");
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	pObject = (xrtobject*)pSource->Control;
	if ( (pObject != NULL) && (xrtRefRetain(&pObject->WeakCount) < 0) ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_WEAK,
			"weak-copy", "the object weak reference cannot be retained");
		return false;
	}
	pOldObject = (xrtobject*)pTarget->Control;
	pTarget->Control = pObject;
	if ( pOldObject != NULL ) {
		__xrtObjectWeakRelease(pOldObject);
	}
	return true;
}



/* 移动弱引用并替换目标，不增加源控制块引用。 */
XRT_API bool xrtWeakMove(xrtweak* pTarget, xrtweak* pSource)
{
	xrtobject* pOldObject;

	if ( (pTarget == NULL) || (pSource == NULL) ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-move", "the source or destination weak reference is null");
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	pOldObject = (xrtobject*)pTarget->Control;
	pTarget->Control = pSource->Control;
	pSource->Control = NULL;
	if ( pOldObject != NULL ) {
		__xrtObjectWeakRelease(pOldObject);
	}
	return true;
}



/* 用可选的存活对象替换弱引用，先保留新控制块再释放旧控制块。 */
XRT_API bool xrtWeakSet(xrtweak* pWeak, xrtobject* pObject)
{
	xrtobject* pOldObject;

	if ( pWeak == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-set", "the destination weak reference is null");
		return false;
	}
	pOldObject = (xrtobject*)pWeak->Control;
	if ( pOldObject == pObject ) {
		return true;
	}
	if ( (pObject != NULL) && (xrtRefRetain(&pObject->WeakCount) < 0) ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_WEAK,
			"weak-set", "the object weak reference cannot be retained");
		return false;
	}
	pWeak->Control = pObject;
	if ( pOldObject != NULL ) {
		__xrtObjectWeakRelease(pOldObject);
	}
	return true;
}



/* 销毁弱引用值并使其恢复为空。 */
XRT_API void xrtWeakUnit(xrtweak* pWeak)
{
	xrtobject* pObject;

	if ( pWeak == NULL ) {
		return;
	}
	pObject = (xrtobject*)pWeak->Control;
	pWeak->Control = NULL;
	if ( pObject != NULL ) {
		__xrtObjectWeakRelease(pObject);
	}
}



/* 以原子方式读取强引用状态并返回瞬时过期结果。 */
XRT_API bool xrtWeakExpired(const xrtweak* pWeak)
{
	xrtobject* pObject;

	if ( pWeak == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-expired", "the weak reference is null");
		return true;
	}
	pObject = (xrtobject*)pWeak->Control;
	return (pObject == NULL) ||
		(__xrtAtomicRefLoad(&pObject->StrongCount) <= 0)
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
		|| (__xrtObjectState(pObject) != XRT_OBJECT_STATE_ACTIVE)
#endif
	;
}



/* 尝试把弱引用提升为新的强引用。 */
static xrtobject* __xrtWeakLockBody(const xrtweak* pWeak)
{
	xrtobject* pObject;

	if ( pWeak == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-lock", "the weak reference is null");
		return NULL;
	}
	pObject = (xrtobject*)pWeak->Control;
	if ( pObject == NULL ) {
		return NULL;
	}
	if (pObject->OwnershipClaim != NULL || pObject->OwnershipCleared) return NULL;
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	if ( __xrtObjectState(pObject) != XRT_OBJECT_STATE_ACTIVE ) {
		return NULL;
	}
#endif
	if ( xrtRefRetain(&pObject->StrongCount) < 0 ) {
		return NULL;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	if ( __xrtObjectState(pObject) != XRT_OBJECT_STATE_ACTIVE ) {
		(void)xrtRefRelease(&pObject->StrongCount);
		return NULL;
	}
#endif
	return pObject;
}

XRT_API xrtobject* xrtWeakLock(const xrtweak* pWeak)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	xrtobject* pResult = __xrtWeakLockBody(pWeak);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return pResult;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_value.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)

/* 设置运行时 Value 桥接模块结构化错误。 */
static void __xrtRuntimeValueError(
	xerrkind Kind,
	xruntimevalueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.runtime-value";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层 Value、对象、弱引用或 callable 错误补充桥接上下文。 */
static void __xrtRuntimeValueWrap(
	xerrkind DefaultKind,
	xruntimevalueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.runtime-value";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 运行时身份句柄直接使用进程内稳定地址作为 Handle 哈希。 */
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK)

static uint64 __xrtRuntimeValueIdentityHash(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	return (uint64)(uintptr_t)pHandle;
}



/* 同一策略域内的运行时句柄按对象身份比较。 */
static bool __xrtRuntimeValueIdentityEqual(
	ptr pLeft,
	ptr pRight,
	ptr pUserData
)
{
	(void)pUserData;
	return pLeft == pRight;
}



/* 判断值是否属于指定的私有 Handle 策略。 */
static bool __xrtRuntimeValueIs(
	const xvalue* pValue,
	const xvaluehandleops* pExpected
)
{
	ptr pHandle;
	ptr pUserData;
	const xvaluehandleops* pOps;

	if ( xrtValueType(pValue) != XVALUE_HANDLE ) {
		return false;
	}
	return xrtValueGetHandle(
		pValue, &pHandle, &pOps, &pUserData) &&
		(pOps == pExpected) &&
		(pUserData == NULL);
}



/* 从指定私有 Handle 策略读取借用句柄并报告明确类型错误。 */
static bool __xrtRuntimeValueGet(
	const xvalue* pValue,
	const xvaluehandleops* pExpected,
	bool bNullable,
	xruntimevalueerror Code,
	cstr sOperation,
	ptr* pHandle
)
{
	ptr pValueHandle;
	ptr pUserData;
	const xvaluehandleops* pOps;

	if ( pHandle == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, Code,
			sOperation, "the runtime Value output is null");
		return false;
	}
	if ( xrtValueType(pValue) != XVALUE_HANDLE ) {
		__xrtRuntimeValueError(XERR_TYPE, XRUNTIME_VALUE_ERROR_TYPE,
			sOperation, "the Value is not a runtime bridge handle");
		return false;
	}
	if ( !xrtValueGetHandle(
		pValue, &pValueHandle, &pOps, &pUserData) ) {
		__xrtRuntimeValueWrap(XERR_TYPE, XRUNTIME_VALUE_ERROR_TYPE,
			sOperation, "the runtime bridge handle cannot be read");
		return false;
	}
	if (
		(pOps != pExpected) ||
		(pUserData != NULL) ||
		(!bNullable && (pValueHandle == NULL))
	) {
		__xrtRuntimeValueError(XERR_TYPE, XRUNTIME_VALUE_ERROR_TYPE,
			sOperation, "the Value has another runtime handle type");
		return false;
	}
	*pHandle = pValueHandle;
	return true;
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT)

/* 克隆对象 Handle 时只增加对象引用，保留可变对象的稳定身份。 */
static bool __xrtRuntimeValueObjectClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	xrtobject* pReference;

	(void)pUserData;
	if ( pClone == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pReference = xrtObjectRef((xrtobject*)pHandle);
	if ( pReference == NULL ) {
		return false;
	}
	*pClone = pReference;
	return true;
}



/* 释放 Value Handle 持有的运行时对象强引用。 */
static void __xrtRuntimeValueObjectDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	xrtObjectUnref((xrtobject*)pHandle);
}



/* 对象具有可变身份，故意不提供伪深拷贝 Clone。 */
static const xvaluehandleops __xrtRuntimeValueObjectOps = {
	__xrtRuntimeValueObjectClone,
	__xrtRuntimeValueObjectDrop,
	__xrtRuntimeValueIdentityHash,
	__xrtRuntimeValueIdentityEqual
};



static bool __xrtRuntimeValueObjectOwnership(const xvalue* pValue,
	xrtownershipvisitor pVisit, ptr pContext)
{
	xrtobject* pObject = xrtValueGetRuntimeObject(pValue);
	return pObject != NULL && pVisit(xrtObjectOwnership(pObject), pContext);
}

/* 增加对象引用并包装成 Value Handle。 */
XRT_API xvalue* xrtValueRuntimeObject(xrtobject* pObject)
{
	xrtobject* pReference;
	ptr pHandle;
	xvalue* pValue;

	if ( pObject == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_OBJECT,
			"object", "the runtime object is null");
		return NULL;
	}
	pReference = xrtObjectRef(pObject);
	if ( pReference == NULL ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_OBJECT,
			"object", "the runtime object cannot be retained");
		return NULL;
	}
	pHandle = pReference;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueObjectOps, NULL);
	if ( pValue == NULL ) {
		xrtObjectUnref(pReference);
	}
	if (pValue != NULL && !xrtValueHandleOwnershipBind(pValue, __xrtRuntimeValueObjectOwnership)) {
		xrtValueRelease(pValue); return NULL;
	}
	return pValue;
}



/* 把对象强引用移交给 Value Handle。 */
XRT_API xvalue* xrtValueRuntimeObjectTake(xrtobject** pObject)
{
	ptr pHandle;
	xvalue* pValue;

	if ( (pObject == NULL) || (*pObject == NULL) ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			"object-take", "the runtime object source is empty or invalid");
		return NULL;
	}
	pHandle = *pObject;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueObjectOps, NULL);
	if ( pValue != NULL ) {
		*pObject = NULL;
		if (!xrtValueHandleOwnershipBind(pValue, __xrtRuntimeValueObjectOwnership)) {
			xrtValueRelease(pValue); return NULL;
		}
	}
	return pValue;
}



/* 判断值是否由运行时对象桥接策略创建。 */
XRT_API bool xrtValueIsRuntimeObject(const xvalue* pValue)
{
	return __xrtRuntimeValueIs(pValue, &__xrtRuntimeValueObjectOps);
}



/* 返回 Value Handle 借用的运行时对象。 */
XRT_API xrtobject* xrtValueGetRuntimeObject(const xvalue* pValue)
{
	ptr pObject;

	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueObjectOps,
		false,
		XRUNTIME_VALUE_ERROR_OBJECT,
		"object-get",
		&pObject
	) ) {
		return NULL;
	}
	return (xrtobject*)pObject;
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)

/* 深克隆 callable Handle 时共享不可变 callable 身份。 */
static bool __xrtRuntimeValueCallableClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	xrtcallable* pReference;

	(void)pUserData;
	if ( pClone == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pReference = xrtCallableRef((xrtcallable*)pHandle);
	if ( pReference == NULL ) {
		return false;
	}
	*pClone = pReference;
	return true;
}



/* 释放 Value Handle 持有的 callable 引用。 */
static void __xrtRuntimeValueCallableDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	xrtCallableUnref((xrtcallable*)pHandle);
}



static const xvaluehandleops __xrtRuntimeValueCallableOps = {
	__xrtRuntimeValueCallableClone,
	__xrtRuntimeValueCallableDrop,
	__xrtRuntimeValueIdentityHash,
	__xrtRuntimeValueIdentityEqual
};

static bool __xrtRuntimeValueCallableOwnership(const xvalue* pValue,
	xrtownershipvisitor pVisit, ptr pContext)
{
	ptr pCallable; const xvaluehandleops* pOps; ptr pUserData;
	if (!xrtValueGetHandle(pValue, &pCallable, &pOps, &pUserData) ||
		pOps != &__xrtRuntimeValueCallableOps || pUserData != NULL) return false;
	return pCallable == NULL || pVisit(xrtCallableOwnership((const xrtcallable*)pCallable), pContext);
}
XRT_API const xrtownershipadapterv1* xrtValueCallableOwnershipAdapterV1(xrtownershipref Reference)
{
	return xrtValueHandleOwnershipAdapterV1(Reference, &__xrtRuntimeValueCallableOps, __xrtRuntimeValueCallableOwnership);
}



/* 增加 callable 引用并包装成 Value Handle。 */
XRT_API xvalue* xrtValueCallable(xrtcallable* pCallable)
{
	xrtcallable* pReference;
	ptr pHandle;
	xvalue* pValue;

	if ( pCallable == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_CALLABLE,
			"callable", "the callable is null");
		return NULL;
	}
	pReference = xrtCallableRef(pCallable);
	if ( pReference == NULL ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_CALLABLE,
			"callable", "the callable cannot be retained");
		return NULL;
	}
	pHandle = pReference;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueCallableOps, NULL);
	if ( pValue == NULL ) {
		xrtCallableUnref(pReference);
	}
	if (pValue != NULL && !xrtValueHandleOwnershipBindPhased(pValue, __xrtRuntimeValueCallableOwnership)) {
		xrtValueRelease(pValue); return NULL;
	}
	return pValue;
}



/* 把 callable 引用移交给 Value Handle。 */
XRT_API xvalue* xrtValueCallableTake(xrtcallable** pCallable)
{
	ptr pHandle;
	xvalue* pValue;

	if ( (pCallable == NULL) || (*pCallable == NULL) ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			"callable-take", "the callable source is empty or invalid");
		return NULL;
	}
	pHandle = *pCallable;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueCallableOps, NULL);
	if ( pValue != NULL ) {
		*pCallable = NULL;
		if (!xrtValueHandleOwnershipBindPhased(pValue, __xrtRuntimeValueCallableOwnership)) {
			xrtValueRelease(pValue); return NULL;
		}
	}
	return pValue;
}



/* 判断值是否由 callable 桥接策略创建。 */
XRT_API bool xrtValueIsCallable(const xvalue* pValue)
{
	return __xrtRuntimeValueIs(pValue, &__xrtRuntimeValueCallableOps);
}



/* 返回 Value Handle 借用的 callable。 */
XRT_API xrtcallable* xrtValueGetCallable(const xvalue* pValue)
{
	ptr pCallable;

	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueCallableOps,
		false,
		XRUNTIME_VALUE_ERROR_CALLABLE,
		"callable-get",
		&pCallable
	) ) {
		return NULL;
	}
	return (xrtcallable*)pCallable;
}



/* 返回 callable Value 借用的函数签名。 */
XRT_API const xrtfunctionsig* xrtValueCallableSignature(
	const xvalue* pValue
)
{
	xrtcallable* pCallable = xrtValueGetCallable(pValue);

	return pCallable != NULL ? xrtCallableSignature(pCallable) : NULL;
}



/* 调用 callable Value。 */
XRT_API bool xrtValueInvoke(
	const xvalue* pCallable,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	xrtcallable* pTarget = xrtValueGetCallable(pCallable);

	return (pTarget != NULL) &&
		xrtCallableInvoke(pTarget, pFrame, pResult);
}



/* 初始化借用 callable 的同步进度桥。 */
XRT_API void xrtProgressCallInit(
	xrtprogresscall* pContext,
	xvalue* pCallback
)
{
	if ( pContext == NULL ) {
		return;
	}
	pContext->Callback =
		(pCallback != NULL) &&
		(xrtValueType(pCallback) != XVALUE_NULL) ?
			pCallback : NULL;
	pContext->InvokeFailed = false;
}



/* 构造三个整数参数并同步调用进度 callable。 */
XRT_API bool xrtProgressCallInvoke(
	const xrtprogress* pProgress,
	ptr pUserData
)
{
	xrtprogresscall* pContext = (xrtprogresscall*)pUserData;
	xrtcallframe Frame;
	xrtcallresult Result;
	xvalue* arrArguments[3] = { NULL, NULL, NULL };
	xvalue* pReturn;
	bool bContinue = false;

	if ( (pContext == NULL) || (pContext->Callback == NULL) ) {
		return true;
	}
	if ( pProgress == NULL ) {
		pContext->InvokeFailed = true;
		return false;
	}
	arrArguments[0] = xrtValueUInt(pProgress->iInputBytes);
	arrArguments[1] = xrtValueUInt(pProgress->iTotalInputBytes);
	arrArguments[2] = xrtValueUInt(pProgress->iOutputBytes);
	if ( (arrArguments[0] == NULL) ||
		 (arrArguments[1] == NULL) ||
		 (arrArguments[2] == NULL) ) {
		pContext->InvokeFailed = true;
		goto cleanup;
	}
	memset(&Frame, 0, sizeof(Frame));
	Frame.ArgumentCount = 3u;
	Frame.Arguments = arrArguments;
	xrtCallResultInit(&Result);
	if ( !xrtValueInvoke(pContext->Callback, &Frame, &Result) ) {
		pContext->InvokeFailed = true;
	} else if ( xrtCallResultCount(&Result) == 0u ) {
		pContext->InvokeFailed = true;
	} else {
		pReturn = xrtCallResultGet(&Result, 0u);
		if ( !xrtValueGetBool(pReturn, &bContinue) ) {
			pContext->InvokeFailed = true;
			bContinue = false;
		}
	}
	xrtCallResultUnit(&Result);

cleanup:
	xrtValueRelease(arrArguments[0]);
	xrtValueRelease(arrArguments[1]);
	xrtValueRelease(arrArguments[2]);
	return bContinue;
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)

/* 深克隆 Future Handle 时增加消费端引用。 */
static bool __xrtRuntimeValueFutureClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	xfuture* pReference;
	(void)pUserData;

	if ( pClone == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pReference = xrtFutureRef((xfuture*)pHandle);
	if ( pReference == NULL ) {
		return false;
	}
	*pClone = pReference;
	return true;
}



/* 释放 Value Handle 持有的 Future 引用。 */
static void __xrtRuntimeValueFutureDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	xrtFutureDestroy((xfuture*)pHandle);
}



static const xvaluehandleops __xrtRuntimeValueFutureOps = {
	__xrtRuntimeValueFutureClone,
	__xrtRuntimeValueFutureDrop,
	__xrtRuntimeValueIdentityHash,
	__xrtRuntimeValueIdentityEqual
};

static bool __xrtRuntimeValueFutureOwnership(const xvalue* pValue,
	xrtownershipvisitor pVisit, ptr pContext)
{
	ptr pFuture; const xvaluehandleops* pOps; ptr pUserData;
	if (!xrtValueGetHandle(pValue, &pFuture, &pOps, &pUserData) ||
		pOps != &__xrtRuntimeValueFutureOps || pUserData != NULL) return false;
	return pFuture == NULL || pVisit(xrtFutureOwnership((const xfuture*)pFuture), pContext);
}

XRT_API const xrtownershipadapterv1* xrtValueFutureOwnershipAdapterV1(xrtownershipref Reference)
{
	return xrtValueHandleOwnershipAdapterV1(Reference, &__xrtRuntimeValueFutureOps, __xrtRuntimeValueFutureOwnership);
}



/* 增加 Future 引用并包装成 Value Handle。 */
XRT_API xvalue* xrtValueFuture(xfuture* pFuture)
{
	xfuture* pReference;
	ptr pHandle;
	xvalue* pValue;

	if ( pFuture == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_FUTURE,
			"future", "the Future is null");
		return NULL;
	}
	pReference = xrtFutureRef(pFuture);
	if ( pReference == NULL ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_FUTURE,
			"future", "the Future cannot be retained");
		return NULL;
	}
	pHandle = pReference;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueFutureOps, NULL);
	if ( pValue == NULL ) {
		xrtFutureDestroy(pReference);
	}
	if (pValue != NULL && !xrtValueHandleOwnershipBindPhased(pValue, __xrtRuntimeValueFutureOwnership)) {
		xrtValueRelease(pValue); return NULL;
	}
	return pValue;
}



/* 把 Future 引用移交给 Value Handle。 */
XRT_API xvalue* xrtValueFutureTake(xfuture** pFuture)
{
	ptr pHandle;
	xvalue* pValue;

	if ( (pFuture == NULL) || (*pFuture == NULL) ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			"future-take", "the Future source is empty or invalid");
		return NULL;
	}
	pHandle = *pFuture;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueFutureOps, NULL);
	if ( pValue != NULL ) {
		*pFuture = NULL;
		if (!xrtValueHandleOwnershipBindPhased(pValue, __xrtRuntimeValueFutureOwnership)) {
			xrtValueRelease(pValue); return NULL;
		}
	}
	return pValue;
}



/* 判断值是否由 Future 桥接策略创建。 */
XRT_API bool xrtValueIsFuture(const xvalue* pValue)
{
	return __xrtRuntimeValueIs(pValue, &__xrtRuntimeValueFutureOps);
}



/* 返回 Value Handle 借用的 Future。 */
XRT_API xfuture* xrtValueGetFuture(const xvalue* pValue)
{
	ptr pFuture;

	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueFutureOps,
		false,
		XRUNTIME_VALUE_ERROR_FUTURE,
		"future-get",
		&pFuture
	) ) {
		return NULL;
	}
	return (xfuture*)pFuture;
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK)

/* The weak control block keeps an address, never a strong payload owner. */
static bool __xrtRuntimeValueWeakOwnership(const xvalue* pValue,
	xrtownershipvisitor pVisit, ptr pContext)
{
	(void)pValue; (void)pVisit; (void)pContext;
	return true;
}

/* 深克隆弱引用 Handle 时复制控制块引用，不增加对象强引用。 */
static bool __xrtRuntimeValueWeakClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	xrtweak Source = { pHandle };
	xrtweak Target = { 0 };

	(void)pUserData;
	if ( pClone == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtWeakCopy(&Target, &Source) ) {
		return false;
	}
	*pClone = Target.Control;
	Target.Control = NULL;
	return true;
}



/* 释放 Value Handle 持有的弱控制块引用。 */
static void __xrtRuntimeValueWeakDrop(ptr pHandle, ptr pUserData)
{
	xrtweak Weak = { pHandle };

	(void)pUserData;
	xrtWeakUnit(&Weak);
}



static const xvaluehandleops __xrtRuntimeValueWeakOps = {
	__xrtRuntimeValueWeakClone,
	__xrtRuntimeValueWeakDrop,
	__xrtRuntimeValueIdentityHash,
	__xrtRuntimeValueIdentityEqual
};



/* 复制弱引用并包装成 Value Handle。 */
XRT_API xvalue* xrtValueWeak(const xrtweak* pWeak)
{
	xrtweak Copy = { 0 };
	ptr pHandle;
	xvalue* pValue;

	if ( pWeak == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_WEAK,
			"weak", "the weak reference is null");
		return NULL;
	}
	if ( !xrtWeakCopy(&Copy, pWeak) ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_WEAK,
			"weak", "the weak reference cannot be copied");
		return NULL;
	}
	pHandle = Copy.Control;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueWeakOps, NULL);
	if ( pValue != NULL ) {
		Copy.Control = NULL;
		if (!xrtValueHandleOwnershipBind(pValue, __xrtRuntimeValueWeakOwnership)) {
			xrtValueRelease(pValue); pValue = NULL;
		}
	}
	xrtWeakUnit(&Copy);
	return pValue;
}



/* 把弱引用移交给 Value Handle。 */
XRT_API xvalue* xrtValueWeakTake(xrtweak* pWeak)
{
	ptr pHandle;
	xvalue* pValue;

	if ( pWeak == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			"weak-take", "the weak reference source is null");
		return NULL;
	}
	pHandle = pWeak->Control;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueWeakOps, NULL);
	if ( pValue != NULL ) {
		pWeak->Control = NULL;
		if (!xrtValueHandleOwnershipBind(pValue, __xrtRuntimeValueWeakOwnership)) {
			xrtValueRelease(pValue); pValue = NULL;
		}
	}
	return pValue;
}



/* 判断值是否由弱引用桥接策略创建。 */
XRT_API bool xrtValueIsWeak(const xvalue* pValue)
{
	return __xrtRuntimeValueIs(pValue, &__xrtRuntimeValueWeakOps);
}



/* 把 Value 中的弱引用复制到目标。 */
XRT_API bool xrtValueGetWeak(
	const xvalue* pValue,
	xrtweak* pWeak
)
{
	ptr pControl;
	xrtweak Source;

	if ( pWeak == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_WEAK,
			"weak-get", "the weak reference output is null");
		return false;
	}
	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueWeakOps,
		true,
		XRUNTIME_VALUE_ERROR_WEAK,
		"weak-get",
		&pControl
	) ) {
		return false;
	}
	Source.Control = pControl;
	if ( !xrtWeakCopy(pWeak, &Source) ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_WEAK,
			"weak-get", "the weak reference cannot be copied");
		return false;
	}
	return true;
}



/* 查询 Value 中的弱引用是否已经过期。 */
XRT_API bool xrtValueWeakExpired(const xvalue* pValue)
{
	ptr pControl;
	xrtweak Weak;

	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueWeakOps,
		true,
		XRUNTIME_VALUE_ERROR_WEAK,
		"weak-expired",
		&pControl
	) ) {
		return true;
	}
	Weak.Control = pControl;
	return xrtWeakExpired(&Weak);
}



/* 从 Value 中的弱引用取得一个新的对象强引用。 */
XRT_API xrtobject* xrtValueWeakLock(const xvalue* pValue)
{
	ptr pControl;
	xrtweak Weak;

	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueWeakOps,
		true,
		XRUNTIME_VALUE_ERROR_WEAK,
		"weak-lock",
		&pControl
	) ) {
		return NULL;
	}
	Weak.Control = pControl;
	return xrtWeakLock(&Weak);
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)

#define XRT_RUNTIME_VALUE_TRACE_INLINE 32u



/* 一次 Value 所有权图追踪使用栈内身份表，并只在大图时创建集合。 */
typedef struct xruntimevaluetrace {
	const void* Inline[XRT_RUNTIME_VALUE_TRACE_INLINE];
	size_t InlineCount;
	xset Overflow;
	bool OverflowReady;
	xrtobjectvisitor Visit;
	ptr Context;
} xruntimevaluetrace;



/* 记录一个 Value 外壳或容器 backing，重复身份不再次追踪。 */
static bool __xrtRuntimeValueTraceSeen(
	xruntimevaluetrace* pTrace,
	const void* pIdentity,
	bool* pSeen
)
{
	*pSeen = false;
	if ( pTrace->OverflowReady ) {
		if ( xrtSetHas(&pTrace->Overflow, &pIdentity) ) {
			*pSeen = true;
			return true;
		}
		return xrtSetAdd(&pTrace->Overflow, &pIdentity);
	}
	for ( size_t i = 0u; i < pTrace->InlineCount; i++ ) {
		if ( pTrace->Inline[i] == pIdentity ) {
			*pSeen = true;
			return true;
		}
	}
	if ( pTrace->InlineCount < XRT_RUNTIME_VALUE_TRACE_INLINE ) {
		pTrace->Inline[pTrace->InlineCount++] = pIdentity;
		return true;
	}
	if ( !xrtSetInit(&pTrace->Overflow, sizeof(pIdentity)) ) {
		return false;
	}
	if ( !xrtSetReserve(
			&pTrace->Overflow,
			XRT_RUNTIME_VALUE_TRACE_INLINE * 2u
		) ) {
		xrtSetUnit(&pTrace->Overflow);
		return false;
	}
	for ( size_t i = 0u; i < pTrace->InlineCount; i++ ) {
		const void* pInline = pTrace->Inline[i];

		if ( !xrtSetAdd(&pTrace->Overflow, &pInline) ) {
			xrtSetUnit(&pTrace->Overflow);
			return false;
		}
	}
	if ( !xrtSetAdd(&pTrace->Overflow, &pIdentity) ) {
		xrtSetUnit(&pTrace->Overflow);
		return false;
	}
	pTrace->OverflowReady = true;
	return true;
}



/* 递归枚举一个 Value 所有权图实际持有的对象强引用。 */
static bool __xrtRuntimeValueTraceGraph(
	const xvalue* pValue,
	xruntimevaluetrace* pTrace,
	uint32 iDepth
)
{
	xvalueiter Iterator;
	xvaluetype Type;
	const void* pIdentity;
	xvalue* pItem;
	bool bSeen;

	if ( iDepth >= XRT_VALUE_DEPTH_MAX ) {
		__xrtRuntimeValueError(XERR_RANGE, XRUNTIME_VALUE_ERROR_TRACE,
			"trace", "the Value graph exceeds the trace depth limit");
		return false;
	}
	Type = xrtValueType(pValue);
	if ( Type == XVALUE_INVALID ) {
		return false;
	}
	if ( xrtValueIsRuntimeObject(pValue) ) {
		if ( !__xrtRuntimeValueTraceSeen(
			pTrace, pValue, &bSeen
		) ) {
			return false;
		}
		if ( bSeen ) {
			return true;
		}
		return pTrace->Visit(
			xrtValueGetRuntimeObject(pValue), pTrace->Context);
	}
	if ( !__xrtValueContainerType(Type) ) {
		return true;
	}
	pIdentity = pValue->Data.Backing;
	if ( !__xrtRuntimeValueTraceSeen(
		pTrace, pIdentity, &bSeen
	) ) {
		return false;
	}
	if ( bSeen ) {
		return true;
	}
	if ( !xrtValueIterBegin(pValue, &Iterator) ) {
		return false;
	}
	while ( (pItem = xrtValueIterNext(&Iterator, NULL)) != NULL ) {
		if ( !__xrtRuntimeValueTraceGraph(
			pItem, pTrace, iDepth + 1u
		) ) {
			xrtValueIterEnd(&Iterator);
			return false;
		}
	}
	xrtValueIterEnd(&Iterator);
	return true;
}



/* 枚举 Value 图拥有的对象边，并在失败后释放大型图身份表。 */
XRT_API bool xrtValueTraceRuntimeObjects(
	const xvalue* pValue,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xruntimevaluetrace Trace;
	xerror* pPrevious;
	xerror* pDiscard;
	bool bResult;

	if ( (pValue == NULL) || (pVisit == NULL) ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_TRACE,
			"trace", "the Value or object visitor is null");
		return false;
	}
	memset(&Trace, 0, sizeof(Trace));
	Trace.Visit = pVisit;
	Trace.Context = pContext;

	/* 隔离调用前错误，避免静态错误对象复用地址时误判访问器状态。 */
	pPrevious = __xrtErrorSwapOwned(NULL);
	bResult = __xrtRuntimeValueTraceGraph(pValue, &Trace, 0u);
	if ( Trace.OverflowReady ) {
		xrtSetUnit(&Trace.Overflow);
	}
	if ( bResult ) {
		pDiscard = __xrtErrorSwapOwned(pPrevious);
		xrtErrorFree(pDiscard);
		return true;
	}

	/* 失败只保留本次遍历产生的错误，访问器未设置错误时补充统一错误。 */
	xrtErrorFree(pPrevious);
	if ( xrtGetError() == NULL ) {
		__xrtRuntimeValueError(XERR_STATE, XRUNTIME_VALUE_ERROR_TRACE,
			"trace", "the Value object graph visitor rejected an edge");
	}
	return false;
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)

/* 一次对象图收集借用的外部 Value 根数组。 */
typedef struct xruntimevalueroots {
	const xvalue* const* Values;
	size_t Count;
} xruntimevalueroots;



/* 把全部外部 Value 所有权图中的对象引用报告给对象图收集器。 */
static bool __xrtRuntimeValueVisitRoots(
	xrtobjectvisitor pVisit,
	ptr pVisitContext,
	ptr pContext
)
{
	const xruntimevalueroots* pRoots =
		(const xruntimevalueroots*)pContext;

	for ( size_t i = 0u; i < pRoots->Count; i++ ) {
		if ( !xrtValueTraceRuntimeObjects(
			pRoots->Values[i], pVisit, pVisitContext
		) ) {
			__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_ROOTS,
				"collect-roots", "a Value root could not be traced");
			return false;
		}
	}
	return true;
}



/* 验证批量 Value 根视图，避免对象图快照完成后才报告参数错误。 */
static bool __xrtRuntimeValueRootsValid(
	const xvalue* const* pRoots,
	size_t iRootCount
)
{
	if ( (pRoots == NULL) && (iRootCount != 0u) ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_ROOTS,
			"collect-roots", "the Value root array is null");
		return false;
	}
	for ( size_t i = 0u; i < iRootCount; i++ ) {
		if ( pRoots[i] == NULL ) {
			__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_ROOTS,
				"collect-roots", "the Value root array contains a null root");
			return false;
		}
	}
	return true;
}



/* 使用一个外部 Value 所有权图作为显式根执行对象图收集。 */
XRT_API bool xrtObjectGraphCollectValueRoot(
	xrtobjectgraph* pGraph,
	const xvalue* pRoot,
	xrtobjectgraphresult* pResult
)
{
	const xvalue* pRoots[1];

	if ( pRoot == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_ROOTS,
			"collect-root", "the Value root is null");
		return false;
	}
	pRoots[0] = pRoot;
	return xrtObjectGraphCollectValueRoots(
		pGraph, pRoots, 1u, pResult);
}



/* 使用一组外部 Value 所有权图作为显式根执行对象图收集。 */
XRT_API bool xrtObjectGraphCollectValueRoots(
	xrtobjectgraph* pGraph,
	const xvalue* const* pRoots,
	size_t iRootCount,
	xrtobjectgraphresult* pResult
)
{
	xruntimevalueroots Roots;

	if ( !__xrtRuntimeValueRootsValid(pRoots, iRootCount) ) {
		return false;
	}
	if ( iRootCount == 0u ) {
		return xrtObjectGraphCollect(pGraph, pResult);
	}
	Roots.Values = pRoots;
	Roots.Count = iRootCount;
	return xrtObjectGraphCollectRoots(
		pGraph, __xrtRuntimeValueVisitRoots, &Roots, pResult);
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)

/* 初始化一个拥有 Value 引用的槽位为空值。 */
static bool __xrtTypeValueInit(ptr pValue, const xrttype* pType)
{
	xvalue* pNull = xrtValueNull();
	(void)pType;

	memcpy(pValue, &pNull, sizeof(pNull));
	return true;
}



/* 使用指定复制策略准备新引用，成功后再替换目标槽。 */
static bool __xrtTypeValueReplace(
	ptr pTarget,
	const void* pSource,
	xvalue* (*pDuplicate)(const xvalue*),
	cstr sOperation,
	cstr sFailure
)
{
	xvalue* pSourceValue;
	xvalue* pTargetValue;
	xvalue* pCopy;

	memcpy(&pSourceValue, pSource, sizeof(pSourceValue));
	if ( pSourceValue == NULL ) {
		__xrtRuntimeValueError(XERR_STATE, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			sOperation, "the source Value slot is empty");
		return false;
	}
	pCopy = pDuplicate(pSourceValue);
	if ( pCopy == NULL ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			sOperation, sFailure);
		return false;
	}
	memcpy(&pTargetValue, pTarget, sizeof(pTargetValue));
	memcpy(pTarget, &pCopy, sizeof(pCopy));
	xrtValueRelease(pTargetValue);
	return true;
}



/* 复制 Value 所有权，容器只创建共享 backing 的独立 COW 外壳。 */
static bool __xrtTypeValueCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	(void)pType;
	return __xrtTypeValueReplace(
		pTarget,
		pSource,
		xrtValueClone,
		"type-copy",
		"the source Value could not be copied"
	);
}



/* 深克隆完整 Value 图，并保留图中的共享拓扑。 */
static bool __xrtTypeValueClone(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	(void)pType;
	return __xrtTypeValueReplace(
		pTarget,
		pSource,
		xrtValueDeepClone,
		"type-clone",
		"the source Value graph could not be cloned"
	);
}



/* 移交 Value 图所有权，并把源槽恢复为有效空值。 */
static bool __xrtTypeValueMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	xvalue* pSourceValue;
	xvalue* pTargetValue;
	xvalue* pNull = xrtValueNull();
	(void)pType;

	memcpy(&pSourceValue, pSource, sizeof(pSourceValue));
	if ( pSourceValue == NULL ) {
		pSourceValue = pNull;
	}
	memcpy(&pTargetValue, pTarget, sizeof(pTargetValue));
	memcpy(pTarget, &pSourceValue, sizeof(pSourceValue));
	memcpy(pSource, &pNull, sizeof(pNull));
	xrtValueRelease(pTargetValue);
	return true;
}



/* 释放槽位拥有的 Value 图引用。 */
static void __xrtTypeValueDrop(ptr pValue, const xrttype* pType)
{
	xvalue* pOwned;
	xvalue* pEmpty = NULL;
	(void)pType;

	memcpy(&pOwned, pValue, sizeof(pOwned));
	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	xrtValueRelease(pOwned);
}



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)

/* 从一个 Value 槽追踪其完整所有权图中的对象强引用。 */
static bool __xrtTypeValueTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xvalue* pOwned;
	(void)pType;

	memcpy(&pOwned, pValue, sizeof(pOwned));
	if ( pOwned == NULL ) {
		return true;
	}
	return xrtValueTraceRuntimeObjects(pOwned, pVisit, pContext);
}

#endif



/* Value 槽支持 COW 复制、深克隆、移动和确定释放。 */
static const xrttypeops __xrtTypeValueOps = {
	.Init = __xrtTypeValueInit,
	.Copy = __xrtTypeValueCopy,
	.Move = __xrtTypeValueMove,
	.Drop = __xrtTypeValueDrop,
	.Clone = __xrtTypeValueClone,
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)
	.Trace = __xrtTypeValueTrace
#endif
};



/* 进程期稳定描述同时供复合类型的静态泛型实参引用。 */
const xrttype __xrtTypeValueDescriptor = {
	.Id = UINT64_C(0xD382E6686762D482),
	.Kind = XRT_TYPE_HANDLE,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_REFERENCE |
		XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL |
		XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("Value"),
	.AbiName = XRT_STR_INIT("xrt.Value"),
	.Size = sizeof(xvalue*),
	.Align = XRT_INTERNAL_ALIGNOF(xvalue*),
	.InstanceSize = 0u,
	.InstanceAlign = 1u,
	.Ops = &__xrtTypeValueOps
};



/* 返回 Value 所有权槽的稳定运行时类型。 */
XRT_API const xrttype* xrtTypeValue(void)
{
	return &__xrtTypeValueDescriptor;
}

#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_call.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_CALL)



#if defined(XRUNTIME_FEATURE_RUNTIME_CALL)

struct xrtcallable {
	volatile int32 RefCount;
	const xrtfunctionsig* Signature;
	xrtcallproc Entry;
	ptr Environment;
	xrtcalldrop DropEnvironment;
	xrtownershiptrace TraceEnvironment;
	const xrtcallableownershipv1* OwnershipPolicy;
	const void* OwnershipClaim;
	ptr RetiredEnvironment;
	volatile int32 ActiveReferences; /* 1 plus active calls; not an owning edge. */
	bool OwnershipCleared;
};

static bool __xrtCallableOwnershipCount(const void* pData, size_t* pCount)
{
	const xrtcallable* pCallable = (const xrtcallable*)pData;
	int32 iCount = __xrtAtomicRefLoad(&pCallable->RefCount);
	if (iCount <= 0 || pCallable->OwnershipCleared || __xrtAtomicRefLoad(&pCallable->ActiveReferences) != 1) return false;
	*pCount = (size_t)iCount; return true;
}

static bool __xrtCallableOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xrtcallable* pCallable = (const xrtcallable*)pData;
	size_t iCount;
	if (!__xrtCallableOwnershipCount(pData, &iCount)) return false;
	if (pCallable->Environment == NULL) return true;
	if (pCallable->TraceEnvironment == NULL) { __xrtErrorSetUnsupported(); return false; }
	return pCallable->TraceEnvironment(pCallable->Environment, pVisit, pContext);
}

static const xrtownershipops __xrtCallableOwnershipOps = {
	__xrtCallableOwnershipCount, __xrtCallableOwnershipTrace
};

XRT_API xrtownershipref xrtCallableOwnership(const xrtcallable* pCallable)
{
	return (xrtownershipref){pCallable, &__xrtCallableOwnershipOps};
}

static bool __xrtOwnershipBody_CallableOwnershipTraceBind(xrtcallable* pCallable, xrtownershiptrace pTrace)
{
	if (pCallable == NULL || pTrace == NULL || pCallable->TraceEnvironment != NULL ||
		__xrtAtomicRefLoad(&pCallable->RefCount) != 1 || pCallable->OwnershipCleared ||
		pCallable->OwnershipPolicy != NULL || __xrtAtomicRefLoad(&pCallable->ActiveReferences) != 1) {
		__xrtErrorSetInvalidState(); return false;
	}
	pCallable->TraceEnvironment = pTrace;
	return true;
}
XRT_API bool xrtCallableOwnershipTraceBind(xrtcallable* pCallable, xrtownershiptrace pTrace)
{ XRT_OWNERSHIP_MUTATION_RETURN(bool, false, __xrtOwnershipBody_CallableOwnershipTraceBind(pCallable, pTrace)); }
static bool __xrtOwnershipBody_CallableOwnershipBindV1(xrtcallable* pCallable, const xrtcallableownershipv1* pPolicy)
{
	if (pCallable == NULL || pPolicy == NULL || pPolicy->size != sizeof(*pPolicy) ||
		pPolicy->Trace == NULL || pPolicy->Drop == NULL || pCallable->DropEnvironment != pPolicy->Drop ||
		pCallable->TraceEnvironment != NULL || pCallable->OwnershipPolicy != NULL ||
		pCallable->OwnershipCleared || pCallable->OwnershipClaim != NULL ||
		__xrtAtomicRefLoad(&pCallable->RefCount) != 1 || __xrtAtomicRefLoad(&pCallable->ActiveReferences) != 1) {
		__xrtErrorSetInvalidState(); return false;
	}
	pCallable->TraceEnvironment = pPolicy->Trace; pCallable->OwnershipPolicy = pPolicy;
	return true;
}
XRT_API bool xrtCallableOwnershipBindV1(xrtcallable* pCallable, const xrtcallableownershipv1* pPolicy)
{ XRT_OWNERSHIP_MUTATION_RETURN(bool, false, __xrtOwnershipBody_CallableOwnershipBindV1(pCallable, pPolicy)); }
static bool __xrtCallableAdapterHold(const void* pData)
{ return xrtCallableRef((xrtcallable*)pData) != NULL; }
static void __xrtCallableAdapterDrop(const void* pData)
{ xrtCallableUnref((xrtcallable*)pData); }
static bool __xrtCallableAdapterClaim(const void* pData, const void* pToken)
{
	xrtcallable* pCallable = (xrtcallable*)pData;
	if (pToken == NULL || pCallable->OwnershipCleared ||
		(pCallable->OwnershipClaim != NULL && pCallable->OwnershipClaim != pToken)) return false;
	pCallable->OwnershipClaim = pToken; return true;
}
static void __xrtCallableAdapterRestore(const void* pData, const void* pToken)
{
	xrtcallable* pCallable = (xrtcallable*)pData;
	if (pCallable->OwnershipClaim != pToken || pCallable->OwnershipCleared) abort();
	pCallable->OwnershipClaim = NULL;
}
static void __xrtCallableAdapterClear(const void* pData, const void* pToken)
{
	xrtcallable* pCallable = (xrtcallable*)pData;
	if (pCallable->OwnershipClaim != pToken || pCallable->OwnershipCleared ||
		__xrtAtomicRefLoad(&pCallable->ActiveReferences) != 1) abort();
	pCallable->RetiredEnvironment = pCallable->Environment; pCallable->Environment = NULL;
	pCallable->Entry = NULL; pCallable->Signature = NULL; pCallable->OwnershipCleared = true;
}
static bool __xrtCallableAdapterFinish(const void* pData, const void* pToken)
{
	xrtcallable* pCallable = (xrtcallable*)pData;
	xrtownershipscope Mutation = {0}; ptr pEnvironment; xrtcalldrop pDrop;
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (pCallable->OwnershipClaim != pToken || !pCallable->OwnershipCleared) abort();
	pEnvironment = pCallable->RetiredEnvironment; pDrop = pCallable->DropEnvironment;
	pCallable->RetiredEnvironment = NULL; pCallable->DropEnvironment = NULL;
	pCallable->TraceEnvironment = NULL; pCallable->OwnershipPolicy = NULL;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (pDrop != NULL) pDrop(pEnvironment);
	return true;
}
XRT_API bool xrtCallableAdmitsOwnershipPolicyV1(const xrtcallable* pCallable,
    const xrtcallableownershipv1* pExpectedPolicy)
{
    return pCallable && pExpectedPolicy && pCallable->OwnershipPolicy == pExpectedPolicy &&
        pExpectedPolicy->size == sizeof(*pExpectedPolicy) && !pCallable->OwnershipCleared &&
        pCallable->TraceEnvironment == pExpectedPolicy->Trace &&
        pCallable->DropEnvironment == pExpectedPolicy->Drop &&
        __xrtAtomicRefLoad(&pCallable->RefCount) > 0;
}
XRT_API const xrtownershipadapterv1* xrtCallableOwnershipAdapterV1(
	xrtownershipref Reference, const xrtcallableownershipv1* pExpectedPolicy)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter),
		__xrtCallableAdapterHold, __xrtCallableAdapterDrop, __xrtCallableAdapterClaim,
		__xrtCallableAdapterRestore, NULL, __xrtCallableAdapterClear, __xrtCallableAdapterFinish};
	const xrtcallable* pCallable;
	if (Reference.Data == NULL || Reference.Ops != &__xrtCallableOwnershipOps ||
		pExpectedPolicy == NULL || pExpectedPolicy->size != sizeof(*pExpectedPolicy)) return NULL;
	pCallable = (const xrtcallable*)Reference.Data;
	if (!xrtCallableAdmitsOwnershipPolicyV1(pCallable, pExpectedPolicy) ||
		__xrtAtomicRefLoad(&pCallable->ActiveReferences) != 1) return NULL;
	return &Adapter;
}



/* 设置动态调用模块结构化错误。 */
static void __xrtCallError(
	xerrkind Kind,
	xcallerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.call";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层签名、值或入口错误补充动态调用上下文。 */
static void __xrtCallWrap(
	xerrkind DefaultKind,
	xcallerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.call";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 使用已经取出的入口错误建立调用错误并释放原因。 */
static void __xrtCallEntryError(xerror* pCause)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_STATE;
	Desc.Domain = "xrt.call";
	Desc.Code = XCALL_ERROR_ENTRY;
	Desc.Operation = "invoke";
	Desc.Message = "the callable entry failed";
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 恢复调用前由当前执行上下文持有的错误。 */
static void __xrtCallRestoreError(xerror* pError)
{
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 判断两个名称视图是否按完整字节相等。 */
static bool __xrtCallNameEqual(
	const xstrview* pLeft,
	const xstrview* pRight
)
{
	return (pLeft->Size == pRight->Size) &&
		((pLeft->Size == 0u) ||
		 (memcmp(pLeft->Data, pRight->Data, pLeft->Size) == 0));
}



/* 检查调用帧借用数组、值和关键字名称的基础结构。 */
static bool __xrtCallFrameShape(const xrtcallframe* pFrame)
{
	if ( pFrame == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-validate", "the call frame is null");
		return false;
	}
	if ( (pFrame->ArgumentCount != 0u) && (pFrame->Arguments == NULL) ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-validate", "the positional argument array is missing");
		return false;
	}
	if (
		(pFrame->KeywordCount != 0u) &&
		((pFrame->KeywordNames == NULL) || (pFrame->KeywordValues == NULL))
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-validate", "the keyword argument arrays are missing");
		return false;
	}
	for ( size_t i = 0; i < pFrame->ArgumentCount; i++ ) {
		if ( pFrame->Arguments[i] == NULL ) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
				"frame-validate", "a positional argument is null");
			return false;
		}
	}
	for ( size_t i = 0; i < pFrame->KeywordCount; i++ ) {
		const xstrview* pName = &pFrame->KeywordNames[i];

		if (
			(pName->Data == NULL) ||
			(pName->Size == 0u) ||
			(pFrame->KeywordValues[i] == NULL)
		) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
				"frame-validate", "a keyword argument is invalid");
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			if ( __xrtCallNameEqual(pName, &pFrame->KeywordNames[j]) ) {
				__xrtCallError(XERR_EXISTS, XCALL_ERROR_FRAME,
					"frame-validate", "a keyword argument is duplicated");
				return false;
			}
		}
	}
	return true;
}



/* 返回关键字名称在调用帧中的位置，缺失时返回 SIZE_MAX。 */
static size_t __xrtCallFrameKeywordIndex(
	const xrtcallframe* pFrame,
	const xstrview* pName
)
{
	for ( size_t i = 0; i < pFrame->KeywordCount; i++ ) {
		if ( __xrtCallNameEqual(pName, &pFrame->KeywordNames[i]) ) {
			return i;
		}
	}
	return SIZE_MAX;
}



/* 检查调用帧结构和签名参数绑定。 */
XRT_API bool xrtCallFrameValidate(const xrtcallframe* pFrame)
{
	const xrtfunctionsig* pSignature;
	size_t iPositional = 0u;

	if ( !__xrtCallFrameShape(pFrame) ) {
		return false;
	}
	pSignature = pFrame->Signature;
	if ( pSignature == NULL ) {
		return true;
	}
	if ( !xrtFunctionSigValidate(pSignature) ) {
		__xrtCallWrap(XERR_ARGUMENT, XCALL_ERROR_SIGNATURE,
			"frame-validate", "the callable signature is invalid");
		return false;
	}
	for ( size_t i = 0; i < pSignature->ParamCount; i++ ) {
		const xrtparamdesc* pParam = &pSignature->Params[i];
		bool bPositional = false;
		bool bKeyword = false;

		if (
			((pParam->Flags & XRT_PARAM_FLAG_NAMED_ONLY) == 0u) &&
			(iPositional < pFrame->ArgumentCount)
		) {
			bPositional = true;
			iPositional++;
		}
		if ( pParam->Name.Size != 0u ) {
			bKeyword = __xrtCallFrameKeywordIndex(
				pFrame, &pParam->Name) != SIZE_MAX;
		}
		if ( bPositional && bKeyword ) {
			__xrtCallError(XERR_EXISTS, XCALL_ERROR_FRAME,
				"frame-validate", "a parameter was passed more than once");
			return false;
		}
		if (
			!bPositional &&
			!bKeyword &&
			((pParam->Flags & XRT_PARAM_FLAG_OPTIONAL) == 0u)
		) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
				"frame-validate", "a required parameter is missing");
			return false;
		}
	}
	if (
		(iPositional < pFrame->ArgumentCount) &&
		((pSignature->Flags & XRT_FUNCTION_FLAG_VARARGS) == 0u)
	) {
		__xrtCallError(XERR_RANGE, XCALL_ERROR_FRAME,
			"frame-validate", "too many positional arguments were passed");
		return false;
	}
	for ( size_t i = 0; i < pFrame->KeywordCount; i++ ) {
		bool bKnown = false;

		for ( size_t j = 0; j < pSignature->ParamCount; j++ ) {
			const xrtparamdesc* pParam = &pSignature->Params[j];

			if (
				(pParam->Name.Size != 0u) &&
				__xrtCallNameEqual(
					&pFrame->KeywordNames[i], &pParam->Name)
			) {
				bKnown = true;
				break;
			}
		}
		if (
			!bKnown &&
			((pSignature->Flags & XRT_FUNCTION_FLAG_KWARGS) == 0u)
		) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
				"frame-validate", "an unknown keyword argument was passed");
			return false;
		}
	}
	return true;
}



/* 返回调用帧中的原始位置参数。 */
XRT_API xvalue* xrtCallFrameArgument(
	const xrtcallframe* pFrame,
	size_t iIndex
)
{
	if (
		(pFrame == NULL) ||
		((pFrame->ArgumentCount != 0u) && (pFrame->Arguments == NULL))
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-argument", "the positional argument array is invalid");
		return NULL;
	}
	if ( iIndex >= pFrame->ArgumentCount ) {
		__xrtCallError(XERR_RANGE, XCALL_ERROR_FRAME,
			"frame-argument", "the positional argument index is out of range");
		return NULL;
	}
	if ( pFrame->Arguments[iIndex] == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-argument", "the positional argument is null");
		return NULL;
	}
	return pFrame->Arguments[iIndex];
}



/* 返回调用帧中按完整名称匹配的关键字参数。 */
XRT_API xvalue* xrtCallFrameKeyword(
	const xrtcallframe* pFrame,
	xstrview Name
)
{
	if (
		(pFrame == NULL) ||
		((pFrame->KeywordCount != 0u) &&
		 ((pFrame->KeywordNames == NULL) || (pFrame->KeywordValues == NULL)))
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-keyword", "the keyword argument arrays are invalid");
		return NULL;
	}
	if ( (Name.Data == NULL) || (Name.Size == 0u) ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-keyword", "the keyword name is empty or invalid");
		return NULL;
	}
	for ( size_t i = 0; i < pFrame->KeywordCount; i++ ) {
		if (
			(pFrame->KeywordNames[i].Data == NULL) ||
			(pFrame->KeywordNames[i].Size == 0u) ||
			(pFrame->KeywordValues[i] == NULL)
		) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
				"frame-keyword", "a keyword argument is invalid");
			return NULL;
		}
		if ( __xrtCallNameEqual(&Name, &pFrame->KeywordNames[i]) ) {
			return pFrame->KeywordValues[i];
		}
	}
	return NULL;
}



/* 按有效签名读取一个形参，不要求入口区分位置和关键字传递。 */
XRT_API xvalue* xrtCallFrameParameter(
	const xrtcallframe* pFrame,
	size_t iIndex
)
{
	const xrtfunctionsig* pSignature;
	const xrtparamdesc* pParam;
	xvalue* pPositional = NULL;
	xvalue* pKeyword = NULL;
	size_t iPosition = 0u;

	if ( pFrame == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-parameter", "the call frame is null");
		return NULL;
	}
	pSignature = pFrame->Signature;
	if (
		(pSignature == NULL) ||
		((pSignature->ParamCount != 0u) && (pSignature->Params == NULL))
	) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_SIGNATURE,
			"frame-parameter", "the effective call signature is invalid");
		return NULL;
	}
	if ( iIndex >= pSignature->ParamCount ) {
		__xrtCallError(XERR_RANGE, XCALL_ERROR_FRAME,
			"frame-parameter", "the effective parameter index is out of range");
		return NULL;
	}
	pParam = &pSignature->Params[iIndex];
	for ( size_t i = 0; i <= iIndex; i++ ) {
		if (
			(pSignature->Params[i].Flags &
			 XRT_PARAM_FLAG_NAMED_ONLY) != 0u
		) {
			continue;
		}
		if ( (i == iIndex) && (iPosition < pFrame->ArgumentCount) ) {
			pPositional = xrtCallFrameArgument(pFrame, iPosition);
			if ( pPositional == NULL ) {
				return NULL;
			}
		}
		iPosition++;
	}
	if ( pParam->Name.Size != 0u ) {
		pKeyword = xrtCallFrameKeyword(pFrame, pParam->Name);
	}
	if ( (pPositional != NULL) && (pKeyword != NULL) ) {
		__xrtCallError(XERR_EXISTS, XCALL_ERROR_FRAME,
			"frame-parameter", "the parameter was passed more than once");
		return NULL;
	}
	if ( pPositional != NULL ) {
		return pPositional;
	}
	if ( pKeyword != NULL ) {
		return pKeyword;
	}
	if ( (pParam->Flags & XRT_PARAM_FLAG_OPTIONAL) != 0u ) {
		return NULL;
	}
	__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
		"frame-parameter", "the required parameter is missing");
	return NULL;
}



/* 返回结果下标对应的内部值槽。 */
static xvalue** __xrtCallResultSlot(xrtcallresult* pResult, size_t iIndex)
{
	return iIndex < XRT_CALL_RESULT_INLINE_COUNT
		? &pResult->Inline[iIndex]
		: &pResult->Overflow[iIndex - XRT_CALL_RESULT_INLINE_COUNT];
}



/* 返回结果下标对应的只读内部值槽。 */
static xvalue* const* __xrtCallResultConstSlot(
	const xrtcallresult* pResult,
	size_t iIndex
)
{
	return iIndex < XRT_CALL_RESULT_INLINE_COUNT
		? &pResult->Inline[iIndex]
		: &pResult->Overflow[iIndex - XRT_CALL_RESULT_INLINE_COUNT];
}



/* 检查结果计数和溢出存储是否自洽，不扫描已经持有的值。 */
static bool __xrtCallResultStorage(const xrtcallresult* pResult)
{
	size_t iCapacity;

	if ( pResult == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-validate", "the call result is null");
		return false;
	}
	if (
		((pResult->OverflowCapacity == 0u) != (pResult->Overflow == NULL)) ||
		(pResult->OverflowCapacity > (SIZE_MAX - XRT_CALL_RESULT_INLINE_COUNT)) ||
		(pResult->OverflowCapacity > (SIZE_MAX / sizeof(xvalue*)))
	) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_RESULT,
			"result-validate", "the call result storage is invalid");
		return false;
	}
	iCapacity = XRT_CALL_RESULT_INLINE_COUNT + pResult->OverflowCapacity;
	if ( pResult->Count > iCapacity ) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_RESULT,
			"result-validate", "the call result count exceeds its storage");
		return false;
	}
	return true;
}



/* 在调用提交边界检查结果全部已持有值是否自洽。 */
static bool __xrtCallResultShape(const xrtcallresult* pResult)
{
	if ( !__xrtCallResultStorage(pResult) ) {
		return false;
	}
	for ( size_t i = 0; i < pResult->Count; i++ ) {
		if ( *__xrtCallResultConstSlot(pResult, i) == NULL ) {
			__xrtCallError(XERR_STATE, XCALL_ERROR_RESULT,
				"result-validate", "the call result contains an empty value slot");
			return false;
		}
	}
	return true;
}



/* 保证结果能够容纳指定总项数，增长失败时保持原状态。 */
static bool __xrtCallResultReserve(
	xrtcallresult* pResult,
	size_t iCount
)
{
	size_t iRequired;
	size_t iCapacity;
	xvalue** pOverflow;

	if ( iCount <= XRT_CALL_RESULT_INLINE_COUNT ) {
		return true;
	}
	iRequired = iCount - XRT_CALL_RESULT_INLINE_COUNT;
	if ( iRequired <= pResult->OverflowCapacity ) {
		return true;
	}
	iCapacity = pResult->OverflowCapacity != 0u
		? pResult->OverflowCapacity
		: XRT_CALL_RESULT_INLINE_COUNT;
	while ( iCapacity < iRequired ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity *= 2u;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(xvalue*)) ) {
		__xrtCallError(XERR_RANGE, XCALL_ERROR_RESULT,
			"result-reserve", "the call result capacity overflows memory size");
		return false;
	}
	pOverflow = (xvalue**)xrtRealloc(
		pResult->Overflow,
		iCapacity * sizeof(xvalue*)
	);
	if ( pOverflow == NULL ) {
		return false;
	}
	memset(
		pOverflow + pResult->OverflowCapacity,
		0,
		(iCapacity - pResult->OverflowCapacity) * sizeof(xvalue*)
	);
	pResult->Overflow = pOverflow;
	pResult->OverflowCapacity = iCapacity;
	return true;
}



/* 检查 Take 来源槽不会被结果结构或溢出存储覆盖。 */
static bool __xrtCallResultTakeSlotValid(
	const xrtcallresult* pResult,
	xvalue** pValue
)
{
	if ( pValue == NULL ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pValue, sizeof(*pValue), pResult, sizeof(*pResult))
	) {
		return false;
	}
	if (
		(pResult->Overflow != NULL) &&
		__xrtRangesOverlap(
			pValue,
			sizeof(*pValue),
			pResult->Overflow,
			pResult->OverflowCapacity * sizeof(xvalue*)
		)
	) {
		return false;
	}
	return true;
}



/* 初始化一个新的空调用结果。 */
XRT_API void xrtCallResultInit(xrtcallresult* pResult)
{
	if ( pResult != NULL ) {
		memset(pResult, 0, sizeof(*pResult));
	}
}



/* 释放结果持有的值并保留容量。 */
XRT_API void xrtCallResultClear(xrtcallresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	for ( size_t i = 0; i < pResult->Count; i++ ) {
		xvalue** pSlot = __xrtCallResultSlot(pResult, i);

		xrtValueRelease(*pSlot);
		*pSlot = NULL;
	}
	pResult->Count = 0u;
}



/* 释放结果持有的值和溢出存储。 */
XRT_API void xrtCallResultUnit(xrtcallresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xrtCallResultClear(pResult);
	xrtFree(pResult->Overflow);
	memset(pResult, 0, sizeof(*pResult));
}



/* 返回调用结果数量。 */
XRT_API size_t xrtCallResultCount(const xrtcallresult* pResult)
{
	if ( !__xrtCallResultStorage(pResult) ) {
		return 0u;
	}
	return pResult->Count;
}



/* 返回指定下标借用的调用结果。 */
XRT_API xvalue* xrtCallResultGet(
	const xrtcallresult* pResult,
	size_t iIndex
)
{
	if ( !__xrtCallResultStorage(pResult) ) {
		return NULL;
	}
	if ( iIndex >= pResult->Count ) {
		__xrtCallError(XERR_RANGE, XCALL_ERROR_RESULT,
			"result-get", "the call result index is out of range");
		return NULL;
	}
	if ( *__xrtCallResultConstSlot(pResult, iIndex) == NULL ) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_RESULT,
			"result-get", "the call result value slot is empty");
		return NULL;
	}
	return *__xrtCallResultConstSlot(pResult, iIndex);
}



/* 增加引用后替换或追加一个连续结果。 */
XRT_API bool xrtCallResultSet(
	xrtcallresult* pResult,
	size_t iIndex,
	const xvalue* pValue
)
{
	xvalue* pRetained;
	xvalue** pSlot;
	xvalue* pPrevious;

	if ( !__xrtCallResultStorage(pResult) ) {
		return false;
	}
	if (
		(pValue == NULL) ||
		(iIndex > pResult->Count) ||
		(iIndex == SIZE_MAX)
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-set", "the result value or index is invalid");
		return false;
	}
	pRetained = xrtValueRetain(pValue);
	if ( pRetained == NULL ) {
		__xrtCallWrap(XERR_STATE, XCALL_ERROR_RESULT,
			"result-set", "the result value cannot be retained");
		return false;
	}
	if ( !__xrtCallResultReserve(pResult, iIndex + 1u) ) {
		xrtValueRelease(pRetained);
		return false;
	}
	pSlot = __xrtCallResultSlot(pResult, iIndex);
	pPrevious = *pSlot;
	*pSlot = pRetained;
	if ( iIndex == pResult->Count ) {
		pResult->Count++;
	}
	xrtValueRelease(pPrevious);
	return true;
}



/* 移交引用后替换或追加一个连续结果。 */
XRT_API bool xrtCallResultSetTake(
	xrtcallresult* pResult,
	size_t iIndex,
	xvalue** pValue
)
{
	xvalue** pSlot;
	xvalue* pPrevious;

	if ( !__xrtCallResultStorage(pResult) ) {
		return false;
	}
	if (
		!__xrtCallResultTakeSlotValid(pResult, pValue) ||
		(*pValue == NULL) ||
		(iIndex > pResult->Count) ||
		(iIndex == SIZE_MAX)
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-set-take", "the result source or index is invalid");
		return false;
	}
	if ( !__xrtCallResultReserve(pResult, iIndex + 1u) ) {
		return false;
	}
	pSlot = __xrtCallResultSlot(pResult, iIndex);
	pPrevious = *pSlot;
	*pSlot = *pValue;
	*pValue = NULL;
	if ( iIndex == pResult->Count ) {
		pResult->Count++;
	}
	xrtValueRelease(pPrevious);
	return true;
}



/* 增加引用后追加一个结果。 */
XRT_API bool xrtCallResultPush(
	xrtcallresult* pResult,
	const xvalue* pValue
)
{
	if ( pResult == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-push", "the call result is null");
		return false;
	}
	return xrtCallResultSet(pResult, pResult->Count, pValue);
}



/* 移交引用后追加一个结果。 */
XRT_API bool xrtCallResultPushTake(
	xrtcallresult* pResult,
	xvalue** pValue
)
{
	if ( pResult == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-push-take", "the call result is null");
		return false;
	}
	return xrtCallResultSetTake(pResult, pResult->Count, pValue);
}



/* 把完整调用结果移动到已经初始化的目标。 */
XRT_API bool xrtCallResultMove(
	xrtcallresult* pTarget,
	xrtcallresult* pSource
)
{
	if ( pTarget == pSource ) {
		if ( pTarget == NULL ) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
				"result-move", "the call result is null");
			return false;
		}
		return __xrtCallResultShape(pTarget);
	}
	if (
		(pTarget == NULL) ||
		(pSource == NULL)
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-move", "the source or target call result is null");
		return false;
	}
	if ( __xrtRangesOverlap(
		pTarget, sizeof(*pTarget), pSource, sizeof(*pSource))
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-move", "the source and target call results overlap");
		return false;
	}
	if (
		!__xrtCallResultShape(pTarget) ||
		!__xrtCallResultShape(pSource)
	) {
		return false;
	}
	xrtCallResultUnit(pTarget);
	*pTarget = *pSource;
	memset(pSource, 0, sizeof(*pSource));
	return true;
}



/* 创建持有入口环境的不可变 callable。 */
XRT_API xrtcallable* xrtCallableCreate(
	const xrtfunctionsig* pSignature,
	xrtcallproc pEntry,
	ptr pEnvironment,
	xrtcalldrop pDropEnvironment
)
{
	xrtcallable* pCallable;

	if ( pEntry == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_CALLABLE,
			"create", "the callable entry is null");
		return NULL;
	}
	if (
		(pSignature != NULL) &&
		!xrtFunctionSigValidate(pSignature)
	) {
		__xrtCallWrap(XERR_ARGUMENT, XCALL_ERROR_SIGNATURE,
			"create", "the callable signature is invalid");
		return NULL;
	}
	pCallable = (xrtcallable*)xrtCalloc(1, sizeof(xrtcallable));
	if ( pCallable == NULL ) {
		return NULL;
	}
	pCallable->RefCount = 1;
	pCallable->ActiveReferences = 1;
	pCallable->Signature = pSignature;
	pCallable->Entry = pEntry;
	pCallable->Environment = pEnvironment;
	pCallable->DropEnvironment = pDropEnvironment;
	pCallable->TraceEnvironment = NULL;
	return pCallable;
}



/* 增加一个已经存活 callable 的引用。 */
static xrtcallable* __xrtOwnershipBody_CallableRef(xrtcallable* pCallable)
{
	if ( pCallable == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_REFERENCE,
			"ref", "the callable is null");
		return NULL;
	}
	if ( pCallable->OwnershipCleared || xrtRefRetain(&pCallable->RefCount) < 0 ) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_REFERENCE,
			"ref", "the callable reference cannot be retained");
		return NULL;
	}
	return pCallable;
}
XRT_API xrtcallable* xrtCallableRef(xrtcallable* pCallable)
{ XRT_OWNERSHIP_MUTATION_RETURN(xrtcallable*, NULL, __xrtOwnershipBody_CallableRef(pCallable)); }



/* 释放 callable 引用，最后一个引用负责释放环境。 */
XRT_API void xrtCallableUnref(xrtcallable* pCallable)
{
	int32 iReferences;
	xrtownershipscope Mutation = {0}; ptr pEnvironment; xrtcalldrop pDrop;

	if ( pCallable == NULL ) {
		return;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	iReferences = xrtRefRelease(&pCallable->RefCount);
	if ( iReferences < 0 ) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		__xrtCallError(XERR_STATE, XCALL_ERROR_REFERENCE,
			"unref", "the callable reference cannot be released");
		return;
	}
	if ( iReferences != 0 ) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		return;
	}
	if (__xrtAtomicRefLoad(&pCallable->ActiveReferences) != 1) abort();
	pDrop = pCallable->DropEnvironment;
	pEnvironment = pCallable->OwnershipCleared ? pCallable->RetiredEnvironment : pCallable->Environment;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	/* No descriptor reads after the environment's actual code owner returns. */
	if (pDrop != NULL) pDrop(pEnvironment);
	xrtFree(pCallable);
}



/* 初始化一个拥有 callable 强引用的槽为空。 */
static bool __xrtTypeCallableInit(
	ptr pValue,
	const xrttype* pType
)
{
	xrtcallable* pEmpty = NULL;
	(void)pType;

	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	return true;
}



/* 增加来源 callable 引用，成功后再替换目标槽。 */
static bool __xrtTypeCallableCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	xrtcallable* pSourceCallable;
	xrtcallable* pTargetCallable;
	xrtcallable* pReference = NULL;
	(void)pType;

	memcpy(&pSourceCallable, pSource, sizeof(pSourceCallable));
	if ( pSourceCallable != NULL ) {
		pReference = xrtCallableRef(pSourceCallable);
		if ( pReference == NULL ) {
			return false;
		}
	}
	memcpy(&pTargetCallable, pTarget, sizeof(pTargetCallable));
	memcpy(pTarget, &pReference, sizeof(pReference));
	xrtCallableUnref(pTargetCallable);
	return true;
}



/* 转移 callable 引用，清空来源并释放目标原引用。 */
static bool __xrtTypeCallableMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	xrtcallable* pSourceCallable;
	xrtcallable* pTargetCallable;
	xrtcallable* pEmpty = NULL;
	(void)pType;

	memcpy(&pSourceCallable, pSource, sizeof(pSourceCallable));
	memcpy(&pTargetCallable, pTarget, sizeof(pTargetCallable));
	memcpy(pTarget, &pSourceCallable, sizeof(pSourceCallable));
	memcpy(pSource, &pEmpty, sizeof(pEmpty));
	xrtCallableUnref(pTargetCallable);
	return true;
}



/* 释放槽拥有的 callable 引用并恢复为空。 */
static void __xrtTypeCallableDrop(
	ptr pValue,
	const xrttype* pType
)
{
	xrtcallable* pCallable;
	xrtcallable* pEmpty = NULL;
	(void)pType;

	memcpy(&pCallable, pValue, sizeof(pCallable));
	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	xrtCallableUnref(pCallable);
}



/* callable 槽按进程内对象身份比较。 */
static int __xrtTypeCallableCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	xrtcallable* pLeftCallable;
	xrtcallable* pRightCallable;
	uintptr_t iLeft;
	uintptr_t iRight;
	(void)pType;

	memcpy(&pLeftCallable, pLeft, sizeof(pLeftCallable));
	memcpy(&pRightCallable, pRight, sizeof(pRightCallable));
	iLeft = (uintptr_t)pLeftCallable;
	iRight = (uintptr_t)pRightCallable;
	return (iLeft > iRight) - (iLeft < iRight);
}



/* callable 槽按进程内对象身份散列。 */
static uint64 __xrtTypeCallableHash(
	const void* pValue,
	const xrttype* pType
)
{
	xrtcallable* pCallable;
	(void)pType;

	memcpy(&pCallable, pValue, sizeof(pCallable));
	return (uint64)(uintptr_t)pCallable;
}



static const xrttypeops __xrtTypeCallableOps = {
	.Init = __xrtTypeCallableInit,
	.Copy = __xrtTypeCallableCopy,
	.Move = __xrtTypeCallableMove,
	.Drop = __xrtTypeCallableDrop,
	.Clone = __xrtTypeCallableCopy,
	.Compare = __xrtTypeCallableCompare,
	.Hash = __xrtTypeCallableHash
};



static const xrttype __xrtTypeCallableDescriptor = {
	.Id = UINT64_C(0x2F78E864B00430C1),
	.Kind = XRT_TYPE_CALLABLE,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_REFERENCE |
		XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL |
		XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("callable"),
	.AbiName = XRT_STR_INIT("xrt.callable"),
	.Size = sizeof(xrtcallable*),
	.Align = XRT_INTERNAL_ALIGNOF(xrtcallable*),
	.InstanceSize = 0u,
	.InstanceAlign = 1u,
	.Ops = &__xrtTypeCallableOps
};



/* 返回拥有型 callable 引用槽的稳定运行时类型。 */
XRT_API const xrttype* xrtTypeCallable(void)
{
	return &__xrtTypeCallableDescriptor;
}



/* 返回 callable 借用的签名。 */
XRT_API const xrtfunctionsig* xrtCallableSignature(
	const xrtcallable* pCallable
)
{
	if ( pCallable == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_CALLABLE,
			"signature", "the callable is null");
		return NULL;
	}
	return pCallable->Signature;
}



/* 返回 callable 的稳定签名 ID。 */
XRT_API uint64 xrtCallableSignatureId(const xrtcallable* pCallable)
{
	const xrtfunctionsig* pSignature = xrtCallableSignature(pCallable);

	return pSignature != NULL ? xrtFunctionSigId(pSignature) : 0u;
}



/* 验证调用并以失败原子方式提交入口结果。 */
static bool __xrtCallableInvokeBody(
	const xrtcallable* pCallable,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	xrtcallframe EffectiveFrame;
	xrtcallresult Temporary = XRT_CALL_RESULT_INIT;
	const xrtfunctionsig* pEffectiveSignature;
	xerror* pPreviousError;
	xerror* pEntryError;
	bool bSuccess;

	if ( pCallable == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_CALLABLE,
			"invoke", "the callable is null");
		return false;
	}
	if ( !__xrtCallResultShape(pResult) ) {
		return false;
	}
	if ( pFrame != NULL ) {
		EffectiveFrame = *pFrame;
	} else {
		memset(&EffectiveFrame, 0, sizeof(EffectiveFrame));
	}
	if (
		(pCallable->Signature != NULL) &&
		(EffectiveFrame.Signature != NULL) &&
		(EffectiveFrame.Signature != pCallable->Signature)
	) {
		uint64 iFrameSignature;

		if ( !xrtFunctionSigValidate(EffectiveFrame.Signature) ) {
			__xrtCallWrap(XERR_ARGUMENT, XCALL_ERROR_SIGNATURE,
				"invoke", "the call frame signature is invalid");
			return false;
		}
		iFrameSignature = xrtFunctionSigId(EffectiveFrame.Signature);
		if ( iFrameSignature != xrtFunctionSigId(pCallable->Signature) ) {
			__xrtCallError(XERR_TYPE, XCALL_ERROR_SIGNATURE,
				"invoke", "the call frame signature does not match the callable");
			return false;
		}
	}
	if ( pCallable->Signature != NULL ) {
		EffectiveFrame.Signature = pCallable->Signature;
	}
	pEffectiveSignature = EffectiveFrame.Signature;
	if ( !xrtCallFrameValidate(&EffectiveFrame) ) {
		return false;
	}
	pPreviousError = xrtTakeError();
	bSuccess = pCallable->Entry(
		pCallable->Environment,
		&EffectiveFrame,
		&Temporary
	);
	pEntryError = xrtTakeError();
	if ( !bSuccess ) {
		xrtCallResultUnit(&Temporary);
		xrtErrorFree(pPreviousError);
		__xrtCallEntryError(pEntryError);
		return false;
	}
	if ( !__xrtCallResultShape(&Temporary) ) {
		xrtCallResultUnit(&Temporary);
		xrtErrorFree(pPreviousError);
		xrtErrorFree(pEntryError);
		return false;
	}
	if (
		(pEffectiveSignature != NULL) &&
		(Temporary.Count != pEffectiveSignature->ReturnCount)
	) {
		xrtCallResultUnit(&Temporary);
		xrtErrorFree(pPreviousError);
		xrtErrorFree(pEntryError);
		__xrtCallError(XERR_TYPE, XCALL_ERROR_RESULT,
			"invoke", "the callable returned an unexpected number of values");
		return false;
	}
	xrtErrorFree(pEntryError);
	if ( !xrtCallResultMove(pResult, &Temporary) ) {
		xrtCallResultUnit(&Temporary);
		xrtErrorFree(pPreviousError);
		return false;
	}
	__xrtCallRestoreError(pPreviousError);
	return true;
}

XRT_API bool xrtCallableInvoke(const xrtcallable* pCallable, const xrtcallframe* pFrame, xrtcallresult* pResult)
{
	xrtownershipscope Mutation = {0}; xrtcallable* pHeld; bool bResult; xerror* pOutcome;
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	pHeld = xrtCallableRef((xrtcallable*)pCallable);
	if (pHeld == NULL) { if (!xrtOwnershipScopeEnd(&Mutation)) abort(); return false; }
	if (xrtRefRetain(&pHeld->ActiveReferences) < 0) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		xrtCallableUnref(pHeld); return false;
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	/* Real strong invocation owner protects signature, result validation and
	 * rollback after Entry has returned. User code does not inherit Mutation. */
	bResult = __xrtCallableInvokeBody(pHeld, pFrame, pResult); pOutcome = xrtTakeError();
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (xrtRefRelease(&pHeld->ActiveReferences) < 1) abort();
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	xrtCallableUnref(pHeld); xrtClearError(); xrtSetErrorTake(pOutcome);
	return bResult;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_object_graph.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)

typedef struct xrtobjectgraphnode {
	xrtobject* Object;
	size_t StrongCount;
	size_t IncomingCount;
	bool Reachable;
	bool Claimed;
} xrtobjectgraphnode;



typedef struct xrtobjectgraphtrace {
	xrtobjectgraphnode* Nodes;
	size_t NodeCount;
	size_t* Slots;
	size_t SlotCount;
	size_t* Work;
	size_t WorkCount;
	size_t EdgeCount;
	bool CountEdges;
} xrtobjectgraphtrace;



struct xrtobjectgraph {
	xrt_spinlock Lock;
	xrtobject* Head;
	xrtobject* Tail;
	size_t Count;
};



/* 设置对象图模块结构化错误。 */
static void __xrtObjectGraphError(
	xerrkind Kind,
	xobjectgrapherror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.object-graph";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为类型追踪或根枚举失败补充对象图上下文。 */
static void __xrtObjectGraphWrap(
	xerrkind DefaultKind,
	xobjectgrapherror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.object-graph";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 在已经持有图锁时摘除对象，不改变对象强引用。 */
static void __xrtObjectGraphRemoveLocked(
	xrtobjectgraph* pGraph,
	xrtobject* pObject
)
{
	if ( pObject->GraphPrevious != NULL ) {
		pObject->GraphPrevious->GraphNext = pObject->GraphNext;
	} else {
		pGraph->Head = pObject->GraphNext;
	}
	if ( pObject->GraphNext != NULL ) {
		pObject->GraphNext->GraphPrevious = pObject->GraphPrevious;
	} else {
		pGraph->Tail = pObject->GraphPrevious;
	}
	pObject->Graph = NULL;
	pObject->GraphPrevious = NULL;
	pObject->GraphNext = NULL;
	if ( pGraph->Count != 0u ) {
		pGraph->Count--;
	}
}



/* 普通最后引用释放通过对象记录的所属图自动摘除对象。 */
void __xrtObjectGraphDetach(xrtobject* pObject)
{
	xrtobjectgraph* pGraph = pObject->Graph;

	if ( pGraph == NULL ) {
		return;
	}
	__xrtSpinLock(&pGraph->Lock);
	if ( pObject->Graph == pGraph ) {
		__xrtObjectGraphRemoveLocked(pGraph, pObject);
	}
	__xrtSpinUnlock(&pGraph->Lock);
}



/* 混合对象地址的有效位，供开放寻址哈希表使用。 */
static size_t __xrtObjectGraphHash(const xrtobject* pObject)
{
	uintptr_t iValue = (uintptr_t)pObject;

	iValue >>= 3u;
	iValue ^= iValue >> 17u;
	iValue *= (uintptr_t)UINT32_C(0xed5ad4bb);
	iValue ^= iValue >> 11u;
#if UINTPTR_MAX > UINT32_MAX
	iValue *= (uintptr_t)UINT64_C(0x9e3779b97f4a7c15);
	iValue ^= iValue >> 29u;
#endif
	return (size_t)iValue;
}



/* 计算至少容纳两倍节点数量的二次幂哈希容量。 */
static bool __xrtObjectGraphSlotCount(size_t iCount, size_t* pSlotCount)
{
	size_t iRequired;
	size_t iSlots = 8u;

	if ( iCount > (SIZE_MAX / 2u) ) {
		__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_STATE,
			"collect", "the object graph is too large to index");
		return false;
	}
	iRequired = iCount * 2u;
	while ( iSlots < iRequired ) {
		if ( iSlots > (SIZE_MAX / 2u) ) {
			__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph hash capacity overflows");
			return false;
		}
		iSlots *= 2u;
	}
	*pSlotCount = iSlots;
	return true;
}



/* 在对象地址哈希表中查找节点，不存在时返回 SIZE_MAX。 */
static size_t __xrtObjectGraphFind(
	const xrtobjectgraphtrace* pTrace,
	const xrtobject* pObject
)
{
	size_t iMask = pTrace->SlotCount - 1u;
	size_t iSlot = __xrtObjectGraphHash(pObject) & iMask;

	for ( size_t i = 0; i < pTrace->SlotCount; i++ ) {
		size_t iNode = pTrace->Slots[iSlot];

		if ( iNode == SIZE_MAX ) {
			return SIZE_MAX;
		}
		if ( pTrace->Nodes[iNode].Object == pObject ) {
			return iNode;
		}
		iSlot = (iSlot + 1u) & iMask;
	}
	return SIZE_MAX;
}



/* 把全部快照节点插入无重复地址的哈希索引。 */
static void __xrtObjectGraphIndex(xrtobjectgraphtrace* pTrace)
{
	size_t iMask = pTrace->SlotCount - 1u;

	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		size_t iSlot = __xrtObjectGraphHash(pTrace->Nodes[i].Object) & iMask;

		while ( pTrace->Slots[iSlot] != SIZE_MAX ) {
			iSlot = (iSlot + 1u) & iMask;
		}
		pTrace->Slots[iSlot] = i;
	}
}



/* 释放快照为每一个节点临时持有的强引用。 */
static void __xrtObjectGraphReleaseSnapshot(
	xrtobjectgraphnode* pNodes,
	size_t iCount
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtObjectUnref(pNodes[i].Object);
	}
}



/* 在安全点建立稳定对象快照并为每个对象持有一个临时强引用。 */
static bool __xrtObjectGraphSnapshot(
	xrtobjectgraph* pGraph,
	xrtobjectgraphnode* pNodes,
	size_t iCount
)
{
	xrtobject* pObject;
	size_t i = 0u;

	__xrtSpinLock(&pGraph->Lock);
	if ( pGraph->Count != iCount ) {
		__xrtSpinUnlock(&pGraph->Lock);
		__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
			"collect", "the object graph changed while collection began");
		return false;
	}
	for ( pObject = pGraph->Head;
		  (pObject != NULL) && (i < iCount);
		  pObject = pObject->GraphNext ) {
		int32 iReferences;

		if ( (__xrtAtomicRefLoad(&pObject->State) !=
			 XRT_OBJECT_STATE_ACTIVE) ||
			 (xrtObjectRef(pObject) == NULL) ) {
			break;
		}
		iReferences = __xrtAtomicRefLoad(&pObject->StrongCount);
		pNodes[i].Object = pObject;
		pNodes[i].StrongCount = iReferences > 0 ?
			(size_t)(iReferences - 1) : 0u;
		i++;
	}
	__xrtSpinUnlock(&pGraph->Lock);
	if ( (i != iCount) || (pObject != NULL) ) {
		__xrtObjectGraphReleaseSnapshot(pNodes, i);
		if ( xrtGetError() != NULL ) {
			__xrtObjectGraphWrap(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph snapshot could not retain an object");
		} else {
			__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph list is inconsistent");
		}
		return false;
	}
	return true;
}



/* 第一遍统计图内入边，第二遍把新发现的可达对象压入工作栈。 */
static bool __xrtObjectGraphVisit(xrtobject* pObject, ptr pContext)
{
	xrtobjectgraphtrace* pTrace = (xrtobjectgraphtrace*)pContext;
	size_t iNode;

	if ( pObject == NULL ) {
		__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_TRACE,
			"trace", "a type trace visited a null strong reference");
		return false;
	}
	iNode = __xrtObjectGraphFind(pTrace, pObject);
	if ( iNode == SIZE_MAX ) {
		return true;
	}
	if ( pTrace->CountEdges ) {
		if ( (pTrace->Nodes[iNode].IncomingCount == SIZE_MAX) ||
			 (pTrace->EdgeCount == SIZE_MAX) ) {
			__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_TRACE,
				"trace", "the object graph edge count overflows");
			return false;
		}
		pTrace->Nodes[iNode].IncomingCount++;
		pTrace->EdgeCount++;
		return true;
	}
	if ( !pTrace->Nodes[iNode].Reachable ) {
		pTrace->Nodes[iNode].Reachable = true;
		pTrace->Work[pTrace->WorkCount++] = iNode;
	}
	return true;
}



/* 调用类型描述追踪一个快照节点，并把失败包装到对象图域。 */
static bool __xrtObjectGraphTraceNode(
	xrtobjectgraphtrace* pTrace,
	size_t iNode
)
{
	xrtobject* pObject = pTrace->Nodes[iNode].Object;
	const void* pPayload = ((const uint8*)pObject) + pObject->PayloadOffset;

	if ( !xrtTypeTraceInstance(
			pObject->Type, pPayload, __xrtObjectGraphVisit, pTrace
		) ) {
		__xrtObjectGraphWrap(XERR_STATE, XOBJECT_GRAPH_ERROR_TRACE,
			"collect", "a runtime object reference trace failed");
		return false;
	}
	return true;
}



/* 标记一个显式根以及从它传播到的所有图内对象。 */
static bool __xrtObjectGraphVisitRoot(xrtobject* pObject, ptr pContext)
{
	xrtobjectgraphtrace* pTrace = (xrtobjectgraphtrace*)pContext;
	size_t iNode;

	if ( pObject == NULL ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ROOTS,
			"roots", "the root enumerator visited a null object");
		return false;
	}
	iNode = __xrtObjectGraphFind(pTrace, pObject);
	if ( (iNode != SIZE_MAX) && !pTrace->Nodes[iNode].Reachable ) {
		pTrace->Nodes[iNode].Reachable = true;
		pTrace->Work[pTrace->WorkCount++] = iNode;
	}
	return true;
}



/* 收集前确认快照引用计数和对象状态没有在安全点内变化。 */
static bool __xrtObjectGraphValidateCandidates(
	const xrtobjectgraph* pGraph,
	const xrtobjectgraphtrace* pTrace
)
{
	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		const xrtobjectgraphnode* pNode = &pTrace->Nodes[i];
		int32 iReferences;

		iReferences = __xrtAtomicRefLoad(&pNode->Object->StrongCount);
		if ( (pNode->StrongCount == SIZE_MAX) ||
			 (iReferences <= 0) ||
			 ((size_t)iReferences != (pNode->StrongCount + 1u)) ||
			 (__xrtAtomicRefLoad(&pNode->Object->State) !=
				XRT_OBJECT_STATE_ACTIVE) ||
			 (pNode->Object->Graph != pGraph) ) {
			__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph changed outside its collection safe point");
			return false;
		}
	}
	return true;
}



/* 在任何负载销毁前原子取得全部候选对象的终结权。 */
static bool __xrtObjectGraphClaimCandidates(xrtobjectgraphtrace* pTrace)
{
	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		xrtobjectgraphnode* pNode = &pTrace->Nodes[i];

		if ( pNode->Reachable ) {
			continue;
		}
		if ( !__xrtObjectBeginFinalize(pNode->Object) ) {
			for ( size_t j = 0; j < i; j++ ) {
				if ( pTrace->Nodes[j].Claimed ) {
					__xrtObjectCancelFinalize(pTrace->Nodes[j].Object);
					pTrace->Nodes[j].Claimed = false;
				}
			}
			__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "an object could not enter graph finalization");
			return false;
		}
		pNode->Claimed = true;
	}
	return true;
}



/* 一次摘除全部候选对象，随后统一销毁负载并发布终结状态。 */
static size_t __xrtObjectGraphFinalizeCandidates(
	xrtobjectgraph* pGraph,
	xrtobjectgraphtrace* pTrace
)
{
	size_t iCollected = 0u;

	__xrtSpinLock(&pGraph->Lock);
	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		xrtobjectgraphnode* pNode = &pTrace->Nodes[i];

		if ( pNode->Claimed && (pNode->Object->Graph == pGraph) ) {
			__xrtObjectGraphRemoveLocked(pGraph, pNode->Object);
		}
	}
	__xrtSpinUnlock(&pGraph->Lock);

	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		if ( pTrace->Nodes[i].Claimed ) {
			__xrtObjectDropPayload(pTrace->Nodes[i].Object);
			iCollected++;
		}
	}
	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		if ( pTrace->Nodes[i].Claimed ) {
			__xrtObjectEndFinalize(pTrace->Nodes[i].Object);
		}
	}
	return iCollected;
}



/* 创建一个空对象图。 */
XRT_API xrtobjectgraph* xrtObjectGraphCreate(void)
{
	xrtobjectgraph* pGraph = (xrtobjectgraph*)xrtCalloc(1u, sizeof(*pGraph));

	if ( pGraph != NULL ) {
		__xrtSpinInit(&pGraph->Lock);
	}
	return pGraph;
}



/* 摘除全部借用对象并销毁空图，不影响对象强生命周期。 */
XRT_API void xrtObjectGraphDestroy(xrtobjectgraph* pGraph)
{
	if ( pGraph == NULL ) {
		return;
	}
	__xrtSpinLock(&pGraph->Lock);
	while ( pGraph->Head != NULL ) {
		__xrtObjectGraphRemoveLocked(pGraph, pGraph->Head);
	}
	__xrtSpinUnlock(&pGraph->Lock);
	__xrtSpinUnit(&pGraph->Lock);
	xrtFree(pGraph);
}



/* 把活动对象幂等加入图尾。 */
XRT_API bool xrtObjectGraphTrack(
	xrtobjectgraph* pGraph,
	xrtobject* pObject
)
{
	if ( (pGraph == NULL) || (pObject == NULL) ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"track", "the object graph or runtime object is null");
		return false;
	}
	__xrtSpinLock(&pGraph->Lock);
	if ( pObject->Graph == pGraph ) {
		__xrtSpinUnlock(&pGraph->Lock);
		return true;
	}
	if ( pObject->Graph != NULL ) {
		__xrtSpinUnlock(&pGraph->Lock);
		__xrtObjectGraphError(XERR_EXISTS, XOBJECT_GRAPH_ERROR_TRACK,
			"track", "the runtime object already belongs to another graph");
		return false;
	}
	if ( (__xrtAtomicRefLoad(&pObject->State) !=
		 XRT_OBJECT_STATE_ACTIVE) ||
		 (__xrtAtomicRefLoad(&pObject->StrongCount) <= 0) ) {
		__xrtSpinUnlock(&pGraph->Lock);
		__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_TRACK,
			"track", "only a live runtime object can be tracked");
		return false;
	}
	if ( pGraph->Count == SIZE_MAX ) {
		__xrtSpinUnlock(&pGraph->Lock);
		__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_STATE,
			"track", "the object graph member count overflows");
		return false;
	}
	pObject->Graph = pGraph;
	pObject->GraphPrevious = pGraph->Tail;
	pObject->GraphNext = NULL;
	if ( pGraph->Tail != NULL ) {
		pGraph->Tail->GraphNext = pObject;
	} else {
		pGraph->Head = pObject;
	}
	pGraph->Tail = pObject;
	pGraph->Count++;
	__xrtSpinUnlock(&pGraph->Lock);
	return true;
}



/* 从指定对象图摘除对象。 */
XRT_API bool xrtObjectGraphUntrack(
	xrtobjectgraph* pGraph,
	xrtobject* pObject
)
{
	if ( (pGraph == NULL) || (pObject == NULL) ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"untrack", "the object graph or runtime object is null");
		return false;
	}
	__xrtSpinLock(&pGraph->Lock);
	if ( pObject->Graph != pGraph ) {
		__xrtSpinUnlock(&pGraph->Lock);
		return false;
	}
	__xrtObjectGraphRemoveLocked(pGraph, pObject);
	__xrtSpinUnlock(&pGraph->Lock);
	return true;
}



/* 查询对象当前是否由指定对象图跟踪。 */
XRT_API bool xrtObjectGraphContains(
	const xrtobjectgraph* pGraph,
	const xrtobject* pObject
)
{
	xrtobjectgraph* pMutable = (xrtobjectgraph*)pGraph;
	bool bContains;

	if ( (pGraph == NULL) || (pObject == NULL) ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"contains", "the object graph or runtime object is null");
		return false;
	}
	__xrtSpinLock(&pMutable->Lock);
	bContains = pObject->Graph == pGraph;
	__xrtSpinUnlock(&pMutable->Lock);
	return bContains;
}



/* 返回对象图当前跟踪的借用对象数量。 */
XRT_API size_t xrtObjectGraphCount(const xrtobjectgraph* pGraph)
{
	xrtobjectgraph* pMutable = (xrtobjectgraph*)pGraph;
	size_t iCount;

	if ( pGraph == NULL ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"count", "the object graph is null");
		return 0u;
	}
	__xrtSpinLock(&pMutable->Lock);
	iCount = pGraph->Count;
	__xrtSpinUnlock(&pMutable->Lock);
	return iCount;
}



/* 使用可选宿主根执行失败原子的强引用环收集。 */
XRT_API bool xrtObjectGraphCollectRoots(
	xrtobjectgraph* pGraph,
	xrtobjectrootproc pRoots,
	ptr pContext,
	xrtobjectgraphresult* pResult
)
{
	xrtobjectgraphresult Result = { 0u, 0u, 0u, 0u };
	xrtobjectgraphtrace Trace;
	xrtobjectgraphnode* pNodes = NULL;
	size_t* pSlots = NULL;
	size_t* pWork = NULL;
	size_t iSlotCount;
	size_t iCount;
	bool bSnapshot = false;
	bool bSuccess = false;
	xerror* pPrevious;
	xerror* pDiscard;

	memset(&Trace, 0, sizeof(Trace));
	if ( pGraph == NULL ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"collect", "the object graph is null");
		return false;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	iCount = xrtObjectGraphCount(pGraph);
	Result.TrackedCount = iCount;
	if ( iCount == 0u ) {
		if ( pResult != NULL ) {
			*pResult = Result;
		}
		pDiscard = __xrtErrorSwapOwned(pPrevious);
		xrtErrorFree(pDiscard);
		return true;
	}
	if ( (iCount > (SIZE_MAX / sizeof(*pNodes))) ||
		 (iCount > (SIZE_MAX / sizeof(*pWork))) ||
		 !__xrtObjectGraphSlotCount(iCount, &iSlotCount) ||
		 (iSlotCount > (SIZE_MAX / sizeof(*pSlots))) ) {
		if ( xrtGetError() == NULL ) {
			__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph snapshot size overflows");
		}
		xrtErrorFree(pPrevious);
		return false;
	}
	pNodes = (xrtobjectgraphnode*)xrtCalloc(iCount, sizeof(*pNodes));
	pSlots = (size_t*)xrtMalloc(iSlotCount * sizeof(*pSlots));
	pWork = (size_t*)xrtMalloc(iCount * sizeof(*pWork));
	if ( (pNodes == NULL) || (pSlots == NULL) || (pWork == NULL) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < iSlotCount; i++ ) {
		pSlots[i] = SIZE_MAX;
	}
	if ( !__xrtObjectGraphSnapshot(pGraph, pNodes, iCount) ) {
		goto cleanup;
	}
	bSnapshot = true;
	Trace.Nodes = pNodes;
	Trace.NodeCount = iCount;
	Trace.Slots = pSlots;
	Trace.SlotCount = iSlotCount;
	Trace.Work = pWork;
	Trace.CountEdges = true;
	__xrtObjectGraphIndex(&Trace);

	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtObjectGraphTraceNode(&Trace, i) ) {
			goto cleanup;
		}
	}
	Result.EdgeCount = Trace.EdgeCount;
	Trace.CountEdges = false;
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pNodes[i].IncomingCount > pNodes[i].StrongCount ) {
			__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_TRACE,
				"collect", "a type trace reported more strong references than exist");
			goto cleanup;
		}
		if ( pNodes[i].StrongCount > pNodes[i].IncomingCount ) {
			pNodes[i].Reachable = true;
			pWork[Trace.WorkCount++] = i;
			Result.RootCount++;
		}
	}
	if ( (pRoots != NULL) &&
		 !pRoots(__xrtObjectGraphVisitRoot, &Trace, pContext) ) {
		__xrtObjectGraphWrap(XERR_STATE, XOBJECT_GRAPH_ERROR_ROOTS,
			"collect", "the object graph root enumeration failed");
		goto cleanup;
	}
	Result.RootCount = Trace.WorkCount;
	while ( Trace.WorkCount != 0u ) {
		size_t iNode = pWork[--Trace.WorkCount];

		if ( !__xrtObjectGraphTraceNode(&Trace, iNode) ) {
			goto cleanup;
		}
	}
	if ( !__xrtObjectGraphValidateCandidates(pGraph, &Trace) ||
		 !__xrtObjectGraphClaimCandidates(&Trace) ) {
		goto cleanup;
	}
	Result.CollectedCount = __xrtObjectGraphFinalizeCandidates(pGraph, &Trace);
	bSuccess = true;

cleanup:
	if ( bSnapshot ) {
		__xrtObjectGraphReleaseSnapshot(pNodes, iCount);
	}
	xrtFree(pWork);
	xrtFree(pSlots);
	xrtFree(pNodes);
	if ( bSuccess ) {
		if ( pResult != NULL ) {
			*pResult = Result;
		}
		pDiscard = __xrtErrorSwapOwned(pPrevious);
		xrtErrorFree(pDiscard);
	} else {
		xrtErrorFree(pPrevious);
		if ( xrtGetError() == NULL ) {
			__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph collection failed");
		}
	}
	return bSuccess;
}



/* Complete ownership uses the same validated/atomic candidate claim and
 * destruction pipeline, with physical reachability replacing flattened
 * object edge subtraction. Legacy collectors retain their explicit ABI. */
static bool __xrtObjectGraphOwnershipBoundary(xrtownershipref Reference, ptr pContext)
{
	/* Objects outside this collection domain are not candidates. Keep them
	 * as roots so a weak lock in another domain cannot expose a half-finalized
	 * cycle. Canonical Ops identify native objects, never payload guessing. */
	return Reference.Ops == xrtObjectOwnership(NULL).Ops &&
		((const xrtobject*)Reference.Data)->Graph != (xrtobjectgraph*)pContext;
}

XRT_API bool xrtObjectGraphCollectOwned(
	xrtobjectgraph* pGraph, xrtobjectgraphownedresult* pResult)
{
	xrtobjectgraphownedresult Result = {0};
	xrtobjectgraphtrace Trace = {0};
	xrtownershipref* pAnchors = NULL;
	bool* pReachable = NULL;
	bool bSnapshot = false, bSuccess = false;
	xerror* pPrevious;
	size_t iCount;
	if (pGraph == NULL) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"collect-owned", "the object graph is null"); return false;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	iCount = xrtObjectGraphCount(pGraph);
	Result.TrackedCount = iCount;
	if (iCount == 0) { bSuccess = true; goto cleanup; }
	if (iCount > SIZE_MAX / sizeof(*Trace.Nodes) ||
		iCount > SIZE_MAX / sizeof(*pAnchors) || iCount > SIZE_MAX / sizeof(*pReachable)) {
		__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_STATE,
			"collect-owned", "the physical graph snapshot size overflows"); goto cleanup;
	}
	Trace.Nodes = (xrtobjectgraphnode*)xrtCalloc(iCount, sizeof(*Trace.Nodes));
	pAnchors = (xrtownershipref*)xrtMalloc(iCount * sizeof(*pAnchors));
	pReachable = (bool*)xrtMalloc(iCount * sizeof(*pReachable));
	if (Trace.Nodes == NULL || pAnchors == NULL || pReachable == NULL) goto cleanup;
	if (!__xrtObjectGraphSnapshot(pGraph, Trace.Nodes, iCount)) goto cleanup;
	bSnapshot = true; Trace.NodeCount = iCount;
	for (size_t i = 0; i < iCount; ++i) pAnchors[i] = xrtObjectOwnership(Trace.Nodes[i].Object);
	/* The snapshot REALLY retains one strong reference per tracked object.
	 * Discount only those pins, not the caller's native references or aliases. */
	if (!xrtOwnershipInspectReachable(pAnchors, iCount, pAnchors, iCount,
		pReachable, &Result.Ownership, __xrtObjectGraphOwnershipBoundary, pGraph)) goto cleanup;
	for (size_t i = 0; i < iCount; ++i) Trace.Nodes[i].Reachable = pReachable[i];
	if (!__xrtObjectGraphValidateCandidates(pGraph, &Trace) ||
		!__xrtObjectGraphClaimCandidates(&Trace)) goto cleanup;
	Result.CollectedCount = __xrtObjectGraphFinalizeCandidates(pGraph, &Trace);
	bSuccess = true;
cleanup:
	if (bSnapshot) __xrtObjectGraphReleaseSnapshot(Trace.Nodes, iCount);
	xrtFree(pReachable); xrtFree(pAnchors); xrtFree(Trace.Nodes);
	if (bSuccess) {
		if (pResult != NULL) *pResult = Result;
		xrtErrorFree(__xrtErrorSwapOwned(pPrevious));
	} else {
		xrtErrorFree(pPrevious);
		if (xrtGetError() == NULL) __xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
			"collect-owned", "the physical ownership collection failed");
	}
	return bSuccess;
}

/* 使用引用计数自动根识别执行常规收集。 */
XRT_API bool xrtObjectGraphCollect(
	xrtobjectgraph* pGraph,
	xrtobjectgraphresult* pResult
)
{
	return xrtObjectGraphCollectRoots(pGraph, NULL, NULL, pResult);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_convert.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT)



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT)

/* 归一化保存所有不分配内存的内建标量来源。 */
typedef struct __xrttypeconvertvalue {
	xrttypekind Kind;
	bool Bool;
	int64 Signed;
	uint64 Unsigned;
	double Float;
	xtime Time;
	ptr Pointer;
} __xrttypeconvertvalue;



/* 设置转换层结构化错误。 */
void __xrtTypeConvertError(
	xerrkind Kind,
	xtypeconverterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.type-convert";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 包装下层转换失败并保留原始错误链。 */
void __xrtTypeConvertWrap(
	xerrkind DefaultKind,
	xtypeconverterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.type-convert";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 判断转换模式枚举是否有效。 */
bool __xrtTypeConvertModeValid(xtypeconvertmode Mode)
{
	return (Mode == XTYPE_CONVERT_EXACT) ||
		(Mode == XTYPE_CONVERT_WIDEN) ||
		(Mode == XTYPE_CONVERT_EXPLICIT);
}



/* 判断类型是否为可执行算术转换的定宽数值。 */
static bool __xrtTypeConvertNumeric(const xrttype* pType)
{
	return (pType->Kind == XRT_TYPE_BOOL) ||
		(pType->Kind == XRT_TYPE_SIGNED_INT) ||
		(pType->Kind == XRT_TYPE_UNSIGNED_INT) ||
		(pType->Kind == XRT_TYPE_FLOAT) ||
		(pType->Kind == XRT_TYPE_TYPE);
}



/* 返回浮点格式能够连续精确表示的整数有效位数。 */
static size_t __xrtTypeConvertFloatBits(const xrttype* pType)
{
	if ( (pType->Kind != XRT_TYPE_FLOAT) ||
		 ((pType->Size != sizeof(float)) &&
		  (pType->Size != sizeof(double))) ) {
		return 0u;
	}
	return pType->Size == sizeof(float) ? 24u : 53u;
}



/* 判断两个已验证类型是否存在覆盖全部来源值的无损方向。 */
static bool __xrtTypeCanWidenBuiltin(
	const xrttype* pSourceType,
	const xrttype* pTargetType
)
{
	size_t iFloatBits;
	size_t iIntegerBits;

	if ( xrtTypeSame(pSourceType, pTargetType) ) {
		return xrtTypeIsCopyable(pSourceType);
	}
	if ( (pSourceType->Kind == XRT_TYPE_NULL) &&
		 (pTargetType->Kind == XRT_TYPE_POINTER) ) {
		return true;
	}
	if ( pSourceType->Kind == XRT_TYPE_BOOL ) {
		return (pTargetType->Kind == XRT_TYPE_BOOL) ||
			(pTargetType->Kind == XRT_TYPE_SIGNED_INT) ||
			(pTargetType->Kind == XRT_TYPE_UNSIGNED_INT) ||
			(pTargetType->Kind == XRT_TYPE_FLOAT);
	}
	if ( (pSourceType->Kind == XRT_TYPE_SIGNED_INT) &&
		 (pTargetType->Kind == XRT_TYPE_SIGNED_INT) ) {
		return pTargetType->Size >= pSourceType->Size;
	}
	if ( (pSourceType->Kind == XRT_TYPE_UNSIGNED_INT) &&
		 (pTargetType->Kind == XRT_TYPE_UNSIGNED_INT) ) {
		return pTargetType->Size >= pSourceType->Size;
	}
	if ( (pSourceType->Kind == XRT_TYPE_UNSIGNED_INT) &&
		 (pTargetType->Kind == XRT_TYPE_SIGNED_INT) ) {
		return pTargetType->Size > pSourceType->Size;
	}
	if (
		((pSourceType->Kind == XRT_TYPE_SIGNED_INT) ||
		 (pSourceType->Kind == XRT_TYPE_UNSIGNED_INT)) &&
		(pTargetType->Kind == XRT_TYPE_FLOAT)
	) {
		iFloatBits = __xrtTypeConvertFloatBits(pTargetType);
		iIntegerBits = pSourceType->Size * CHAR_BIT;
		if ( pSourceType->Kind == XRT_TYPE_SIGNED_INT ) {
			iIntegerBits--;
		}
		return (iFloatBits != 0u) && (iIntegerBits <= iFloatBits);
	}
	return (pSourceType->Kind == XRT_TYPE_FLOAT) &&
		(pTargetType->Kind == XRT_TYPE_FLOAT) &&
		(pSourceType->Size == sizeof(float)) &&
		(pTargetType->Size == sizeof(double));
}



/* 判断显式模式是否允许指定已验证类型组合。 */
static bool __xrtTypeCanExplicitBuiltin(
	const xrttype* pSourceType,
	const xrttype* pTargetType
)
{
	if ( __xrtTypeCanWidenBuiltin(pSourceType, pTargetType) ) {
		return true;
	}
	if ( __xrtTypeConvertNumeric(pSourceType) &&
		 __xrtTypeConvertNumeric(pTargetType) ) {
		return true;
	}
	if ( pSourceType->Kind == XRT_TYPE_NULL ) {
		if ( __xrtTypeConvertNumeric(pTargetType) ||
			(pTargetType->Kind == XRT_TYPE_TIME) ||
			(pTargetType->Kind == XRT_TYPE_POINTER) ) {
			return true;
		}
	}
	if ( pSourceType->Kind == XRT_TYPE_TIME ) {
		if ( __xrtTypeConvertNumeric(pTargetType) ) {
			return true;
		}
	}
	if ( pTargetType->Kind == XRT_TYPE_TIME ) {
		if ( (pSourceType->Kind == XRT_TYPE_SIGNED_INT) ||
			 (pSourceType->Kind == XRT_TYPE_UNSIGNED_INT) ) {
			return true;
		}
	}
	if ( (pSourceType->Kind == XRT_TYPE_POINTER) &&
		 (pTargetType->Kind == XRT_TYPE_BOOL) ) {
		return true;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)
	return __xrtTypeStringCanConvert(pSourceType, pTargetType);
#else
	return false;
#endif
}



/* 在类型和模式已经验证后判断转换关系，不读取或修改线程错误。 */
static bool __xrtTypeCanConvertValidated(
	const xrttype* pSourceType,
	const xrttype* pTargetType,
	xtypeconvertmode Mode
)
{
	if ( Mode == XTYPE_CONVERT_EXACT ) {
		return xrtTypeSame(pSourceType, pTargetType) &&
			xrtTypeIsCopyable(pSourceType);
	}
	if ( Mode == XTYPE_CONVERT_WIDEN ) {
		return __xrtTypeCanWidenBuiltin(pSourceType, pTargetType);
	}
	return __xrtTypeCanExplicitBuiltin(pSourceType, pTargetType);
}



/* 把来源内建标量安全读取到对齐的归一化槽。 */
static bool __xrtTypeConvertRead(
	const xrttype* pSourceType,
	const void* pSource,
	__xrttypeconvertvalue* pValue
)
{
	memset(pValue, 0, sizeof(*pValue));
	pValue->Kind = pSourceType->Kind;
	switch ( pSourceType->Kind ) {
		case XRT_TYPE_NULL:
			return true;
		case XRT_TYPE_BOOL:
			return __xrtTypeReadBool(
				pSource, pSourceType->Size, &pValue->Bool
			);
		case XRT_TYPE_SIGNED_INT:
			return __xrtTypeReadSigned(
				pSource, pSourceType->Size, &pValue->Signed
			);
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			return __xrtTypeReadUnsigned(
				pSource, pSourceType->Size, &pValue->Unsigned
			);
		case XRT_TYPE_FLOAT:
			return __xrtTypeReadFloat(
				pSource, pSourceType->Size, &pValue->Float
			);
		case XRT_TYPE_TIME:
			if ( pSourceType->Size == sizeof(pValue->Time) ) {
				memcpy(&pValue->Time, pSource, sizeof(pValue->Time));
				return true;
			}
			break;
		case XRT_TYPE_POINTER:
			if ( pSourceType->Size == sizeof(pValue->Pointer) ) {
				memcpy(&pValue->Pointer, pSource, sizeof(pValue->Pointer));
				return true;
			}
			break;
		default:
			break;
	}
	__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
		"convert", "the source type has no supported scalar representation");
	return false;
}



/* 把来源标量按脚本语言真值规则转换为布尔值。 */
static bool __xrtTypeConvertBool(
	const __xrttypeconvertvalue* pValue,
	bool* pResult
)
{
	switch ( pValue->Kind ) {
		case XRT_TYPE_NULL:
			*pResult = false;
			return true;
		case XRT_TYPE_BOOL:
			*pResult = pValue->Bool;
			return true;
		case XRT_TYPE_SIGNED_INT:
			*pResult = pValue->Signed != 0;
			return true;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			*pResult = pValue->Unsigned != 0u;
			return true;
		case XRT_TYPE_FLOAT:
			*pResult = pValue->Float != 0.0;
			return true;
		case XRT_TYPE_TIME:
			*pResult = pValue->Time != 0;
			return true;
		case XRT_TYPE_POINTER:
			*pResult = pValue->Pointer != NULL;
			return true;
		default:
			return false;
	}
}



/* 范围检查后把浮点值截断为目标宽度的有符号整数。 */
static bool __xrtTypeFloatToSigned(
	double fValue,
	size_t iSize,
	int64* pResult
)
{
	long double fMinimum;
	long double fMaximum;
	long double fWide;

	if ( fValue != fValue ) {
		return false;
	}
	switch ( iSize ) {
		case 1u:
			fMinimum = -128.0L;
			fMaximum = 128.0L;
			break;
		case 2u:
			fMinimum = -32768.0L;
			fMaximum = 32768.0L;
			break;
		case 4u:
			fMinimum = -2147483648.0L;
			fMaximum = 2147483648.0L;
			break;
		case 8u:
			fMinimum = -9223372036854775808.0L;
			fMaximum = 9223372036854775808.0L;
			break;
		default:
			return false;
	}
	fWide = (long double)fValue;
	if ( (fWide < fMinimum) || (fWide >= fMaximum) ) {
		return false;
	}
	*pResult = (int64)fValue;
	return true;
}



/* 范围检查后把浮点值截断为目标宽度的无符号整数。 */
static bool __xrtTypeFloatToUnsigned(
	double fValue,
	size_t iSize,
	uint64* pResult
)
{
	long double fMaximum;
	long double fWide;

	if ( fValue != fValue ) {
		return false;
	}
	switch ( iSize ) {
		case 1u:
			fMaximum = 256.0L;
			break;
		case 2u:
			fMaximum = 65536.0L;
			break;
		case 4u:
			fMaximum = 4294967296.0L;
			break;
		case 8u:
			fMaximum = 18446744073709551616.0L;
			break;
		default:
			return false;
	}
	fWide = (long double)fValue;
	if ( (fWide < 0.0L) || (fWide >= fMaximum) ) {
		return false;
	}
	*pResult = (uint64)fValue;
	return true;
}



/* 把归一化来源写成目标有符号整数。 */
static bool __xrtTypeConvertSigned(
	const __xrttypeconvertvalue* pValue,
	size_t iSize,
	ptr pTarget
)
{
	int64 iSigned;

	switch ( pValue->Kind ) {
		case XRT_TYPE_NULL:
			iSigned = 0;
			break;
		case XRT_TYPE_BOOL:
			iSigned = pValue->Bool ? 1 : 0;
			break;
		case XRT_TYPE_SIGNED_INT:
			iSigned = pValue->Signed;
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( pValue->Unsigned > INT64_MAX ) {
				return false;
			}
			iSigned = (int64)pValue->Unsigned;
			break;
		case XRT_TYPE_FLOAT:
			if ( !__xrtTypeFloatToSigned(
				pValue->Float, iSize, &iSigned
			) ) {
				return false;
			}
			break;
		case XRT_TYPE_TIME:
			iSigned = (int64)pValue->Time;
			break;
		default:
			return false;
	}
	return __xrtTypeWriteSigned(iSigned, iSize, pTarget);
}



/* 把归一化来源写成目标无符号整数或类型标识。 */
static bool __xrtTypeConvertUnsigned(
	const __xrttypeconvertvalue* pValue,
	size_t iSize,
	ptr pTarget
)
{
	uint64 iUnsigned;

	switch ( pValue->Kind ) {
		case XRT_TYPE_NULL:
			iUnsigned = 0u;
			break;
		case XRT_TYPE_BOOL:
			iUnsigned = pValue->Bool ? 1u : 0u;
			break;
		case XRT_TYPE_SIGNED_INT:
			if ( pValue->Signed < 0 ) {
				return false;
			}
			iUnsigned = (uint64)pValue->Signed;
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			iUnsigned = pValue->Unsigned;
			break;
		case XRT_TYPE_FLOAT:
			if ( !__xrtTypeFloatToUnsigned(
				pValue->Float, iSize, &iUnsigned
			) ) {
				return false;
			}
			break;
		case XRT_TYPE_TIME:
			if ( pValue->Time < 0 ) {
				return false;
			}
			iUnsigned = (uint64)pValue->Time;
			break;
		default:
			return false;
	}
	return __xrtTypeWriteUnsigned(iUnsigned, iSize, pTarget);
}



/* 把归一化来源写成目标浮点值。 */
static bool __xrtTypeConvertFloat(
	const __xrttypeconvertvalue* pValue,
	size_t iSize,
	bool bLossless,
	ptr pTarget
)
{
	double fValue;

	switch ( pValue->Kind ) {
		case XRT_TYPE_NULL:
			fValue = 0.0;
			break;
		case XRT_TYPE_BOOL:
			fValue = pValue->Bool ? 1.0 : 0.0;
			break;
		case XRT_TYPE_SIGNED_INT:
			fValue = (double)pValue->Signed;
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			fValue = (double)pValue->Unsigned;
			break;
		case XRT_TYPE_FLOAT:
			fValue = pValue->Float;
			break;
		case XRT_TYPE_TIME:
			fValue = (double)pValue->Time;
			break;
		default:
			return false;
	}
	return __xrtTypeWriteFloat(fValue, iSize, bLossless, pTarget);
}



/* 执行不依赖文本模块的内建标量转换。 */
static bool __xrtTypeConvertBuiltin(
	const xrttype* pSourceType,
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
)
{
	__xrttypeconvertvalue Value;
	bool bValue;
	xtime Time;
	ptr pPointer = NULL;
	bool bSuccess = false;

	if ( !__xrtTypeConvertRead(pSourceType, pSource, &Value) ) {
		return false;
	}
	switch ( pTargetType->Kind ) {
		case XRT_TYPE_BOOL:
			bSuccess = __xrtTypeConvertBool(&Value, &bValue);
			if ( bSuccess ) {
				bSuccess = __xrtTypeWriteBool(
					bValue, pTargetType->Size, pTarget
				);
			}
			break;
		case XRT_TYPE_SIGNED_INT:
			bSuccess = __xrtTypeConvertSigned(
				&Value, pTargetType->Size, pTarget
			);
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			bSuccess = __xrtTypeConvertUnsigned(
				&Value, pTargetType->Size, pTarget
			);
			break;
		case XRT_TYPE_FLOAT:
			bSuccess = __xrtTypeConvertFloat(
				&Value, pTargetType->Size,
				Mode != XTYPE_CONVERT_EXPLICIT, pTarget
			);
			break;
		case XRT_TYPE_TIME:
			if ( Value.Kind == XRT_TYPE_NULL ) {
				Time = 0;
				bSuccess = true;
			} else if ( Value.Kind == XRT_TYPE_SIGNED_INT ) {
				Time = (xtime)Value.Signed;
				bSuccess = true;
			} else if (
				(Value.Kind == XRT_TYPE_UNSIGNED_INT) &&
				(Value.Unsigned <= INT64_MAX)
			) {
				Time = (xtime)Value.Unsigned;
				bSuccess = true;
			}
			if ( bSuccess ) {
				memcpy(pTarget, &Time, sizeof(Time));
			}
			break;
		case XRT_TYPE_POINTER:
			if ( Value.Kind == XRT_TYPE_NULL ) {
				memcpy(pTarget, &pPointer, sizeof(pPointer));
				bSuccess = true;
			}
			break;
		default:
			break;
	}
	if ( !bSuccess ) {
		__xrtTypeConvertError(XERR_RANGE, XTYPE_CONVERT_ERROR_RANGE,
			"convert", "the source value is outside the target representation");
	}
	return bSuccess;
}



/* 判断源类型的全部有效值是否都能被目标类型无损表示。 */
XRT_API bool xrtTypeCanWiden(
	const xrttype* pSourceType,
	const xrttype* pTargetType
)
{
	if ( (pSourceType == NULL) || (pTargetType == NULL) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"can-widen", "the source and target types are required");
		return false;
	}
	if ( !xrtTypeValidate(pSourceType) || !xrtTypeValidate(pTargetType) ) {
		__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"can-widen", "a runtime type descriptor is invalid");
		return false;
	}
	return __xrtTypeCanWidenBuiltin(pSourceType, pTargetType);
}



/* 判断两个类型在指定模式下是否存在稳定的内建转换路径。 */
XRT_API bool xrtTypeCanConvert(
	const xrttype* pSourceType,
	const xrttype* pTargetType,
	xtypeconvertmode Mode
)
{
	if ( (pSourceType == NULL) || (pTargetType == NULL) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"can-convert", "the source and target types are required");
		return false;
	}
	if ( !__xrtTypeConvertModeValid(Mode) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_MODE,
			"can-convert", "the conversion mode is invalid");
		return false;
	}
	if ( !xrtTypeValidate(pSourceType) || !xrtTypeValidate(pTargetType) ) {
		__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"can-convert", "a runtime type descriptor is invalid");
		return false;
	}
	return __xrtTypeCanConvertValidated(pSourceType, pTargetType, Mode);
}



/* 把借用的源值转换后写入已经初始化的目标值。 */
XRT_API bool xrtTypeConvert(
	const xrttype* pSourceType,
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
)
{
	if (
		(pSourceType == NULL) || (pTargetType == NULL) ||
		((pSource == NULL) && (pSourceType->Size != 0u)) ||
		((pTarget == NULL) && (pTargetType->Size != 0u))
	) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"convert", "the source, target, or runtime type is invalid");
		return false;
	}
	if ( !__xrtTypeConvertModeValid(Mode) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_MODE,
			"convert", "the conversion mode is invalid");
		return false;
	}
	if ( !xrtTypeValidate(pSourceType) || !xrtTypeValidate(pTargetType) ) {
		__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"convert", "a runtime type descriptor is invalid");
		return false;
	}
	if ( !__xrtTypeCanConvertValidated(pSourceType, pTargetType, Mode) ) {
		__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"convert", "the runtime types do not support this conversion mode");
		return false;
	}
	if ( xrtTypeSame(pSourceType, pTargetType) ) {
		if ( xrtTypeCopyValue(pSourceType, pTarget, pSource) ) {
			return true;
		}
		__xrtTypeConvertWrap(XERR_STATE, XTYPE_CONVERT_ERROR_OPERATION,
			"convert", "the exact type copy operation failed");
		return false;
	}
	if ( __xrtRangesOverlap(
		pSource, pSourceType->Size, pTarget, pTargetType->Size
	) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"convert", "different-type source and target ranges overlap");
		return false;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)
	if ( __xrtTypeStringCanConvert(pSourceType, pTargetType) ) {
		return __xrtTypeStringConvert(
			pSourceType, pSource, pTargetType, pTarget
		);
	}
#endif
	return __xrtTypeConvertBuiltin(
		pSourceType, pSource, pTargetType, pTarget, Mode
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_type_string.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING)




#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING)

/* 初始化一个拥有型字符串槽为空字符串。 */
static bool __xrtRuntimeTypeStringInit(
	ptr pValue,
	const xrttype* pType
)
{
	str sEmpty = NULL;
	(void)pType;

	memcpy(pValue, &sEmpty, sizeof(sEmpty));
	return true;
}



/* 复制来源字符串，成功后替换目标拥有的旧字符串。 */
static bool __xrtRuntimeTypeStringCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	str sSource;
	str sTarget;
	str sCopy = NULL;
	(void)pType;

	memcpy(&sSource, pSource, sizeof(sSource));
	if ( sSource != NULL ) {
		sCopy = xrtStrDup(sSource);
		if ( sCopy == NULL ) {
			return false;
		}
	}
	memcpy(&sTarget, pTarget, sizeof(sTarget));
	memcpy(pTarget, &sCopy, sizeof(sCopy));
	xrtFree(sTarget);
	return true;
}



/* 移交字符串所有权，清空来源并释放目标旧字符串。 */
static bool __xrtRuntimeTypeStringMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	str sSource;
	str sTarget;
	str sEmpty = NULL;
	(void)pType;

	memcpy(&sSource, pSource, sizeof(sSource));
	memcpy(&sTarget, pTarget, sizeof(sTarget));
	memcpy(pTarget, &sSource, sizeof(sSource));
	memcpy(pSource, &sEmpty, sizeof(sEmpty));
	xrtFree(sTarget);
	return true;
}



/* 释放字符串槽并恢复为空字符串。 */
static void __xrtRuntimeTypeStringDrop(
	ptr pValue,
	const xrttype* pType
)
{
	str sValue;
	str sEmpty = NULL;
	(void)pType;

	memcpy(&sValue, pValue, sizeof(sValue));
	memcpy(pValue, &sEmpty, sizeof(sEmpty));
	xrtFree(sValue);
}



/* 按无符号字节词典序比较两个字符串槽。 */
static int __xrtRuntimeTypeStringCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	str sLeft;
	str sRight;
	(void)pType;

	memcpy(&sLeft, pLeft, sizeof(sLeft));
	memcpy(&sRight, pRight, sizeof(sRight));
	return xrtStrCompare(xrtStrView(sLeft), xrtStrView(sRight));
}



/* 按字符串内容计算确定性的 64 位散列。 */
static uint64 __xrtRuntimeTypeStringHash(
	const void* pValue,
	const xrttype* pType
)
{
	str sValue;
	xstrview Text;
	(void)pType;

	memcpy(&sValue, pValue, sizeof(sValue));
	Text = xrtStrView(sValue);
	return xrtHash64(Text.Data, Text.Size);
}



static const xrttypeops __xrtRuntimeTypeStringOps = {
	.Init = __xrtRuntimeTypeStringInit,
	.Copy = __xrtRuntimeTypeStringCopy,
	.Move = __xrtRuntimeTypeStringMove,
	.Drop = __xrtRuntimeTypeStringDrop,
	.Clone = __xrtRuntimeTypeStringCopy,
	.Compare = __xrtRuntimeTypeStringCompare,
	.Hash = __xrtRuntimeTypeStringHash
};



static const xrttype __xrtRuntimeTypeString = {
	.Id = UINT64_C(0xFD3B54B7F64A3170),
	.Kind = XRT_TYPE_STRING,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_REFERENCE |
		XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL |
		XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("string"),
	.AbiName = XRT_STR_INIT("xrt.string"),
	.Size = sizeof(str),
	.Align = XRT_INTERNAL_ALIGNOF(str),
	.InstanceSize = 0u,
	.InstanceAlign = 1u,
	.Ops = &__xrtRuntimeTypeStringOps
};



/* 返回拥有型 XRT 字符串槽的稳定运行时类型。 */
XRT_API const xrttype* xrtTypeString(void)
{
	return &__xrtRuntimeTypeString;
}

static bool __xrtRuntimeTypeStringViewInit(ptr pValue, const xrttype* pType)
{
	xstrview Empty = {0};
	(void)pType;
	memcpy(pValue, &Empty, sizeof(Empty));
	return true;
}

static bool __xrtRuntimeTypeStringViewCopy(ptr pTarget, const void* pSource, const xrttype* pType)
{
	xstrview Source, Target, Copy = {0};
	(void)pType;
	memcpy(&Source, pSource, sizeof(Source));
	if ( Source.Data == NULL && Source.Size != 0u ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( Source.Data != NULL ) {
		Copy.Data = xrtStrDupView(Source);
		if ( Copy.Data == NULL ) return false;
		Copy.Size = Source.Size;
	}
	memcpy(&Target, pTarget, sizeof(Target));
	memcpy(pTarget, &Copy, sizeof(Copy));
	xrtFree((ptr)Target.Data);
	return true;
}

static bool __xrtRuntimeTypeStringViewMove(ptr pTarget, ptr pSource, const xrttype* pType)
{
	xstrview Source, Target, Empty = {0};
	(void)pType;
	if ( pTarget == pSource ) return true;
	memcpy(&Source, pSource, sizeof(Source));
	memcpy(&Target, pTarget, sizeof(Target));
	memcpy(pSource, &Empty, sizeof(Empty));
	memcpy(pTarget, &Source, sizeof(Source));
	xrtFree((ptr)Target.Data);
	return true;
}

static void __xrtRuntimeTypeStringViewDrop(ptr pValue, const xrttype* pType)
{
	xstrview Value, Empty = {0};
	(void)pType;
	memcpy(&Value, pValue, sizeof(Value));
	memcpy(pValue, &Empty, sizeof(Empty));
	xrtFree((ptr)Value.Data);
}

static int __xrtRuntimeTypeStringViewCompare(const void* pLeft, const void* pRight, const xrttype* pType)
{
	xstrview Left, Right;
	(void)pType;
	memcpy(&Left, pLeft, sizeof(Left));
	memcpy(&Right, pRight, sizeof(Right));
	return xrtStrCompare(Left, Right);
}

static uint64 __xrtRuntimeTypeStringViewHash(const void* pValue, const xrttype* pType)
{
	xstrview Value;
	(void)pType;
	memcpy(&Value, pValue, sizeof(Value));
	return xrtHash64(Value.Data, Value.Size);
}

static bool __xrtRuntimeTypeStringViewFormat(const void* pValue, const xrttype* pType,
	xrttypewriter pWrite, ptr pContext)
{
	xstrview Value;
	(void)pType;
	memcpy(&Value, pValue, sizeof(Value));
	return pWrite(Value, pContext);
}

static const xrttypeops __xrtRuntimeTypeStringViewOps = {
	.Init = __xrtRuntimeTypeStringViewInit,
	.Copy = __xrtRuntimeTypeStringViewCopy,
	.Move = __xrtRuntimeTypeStringViewMove,
	.Drop = __xrtRuntimeTypeStringViewDrop,
	.Clone = __xrtRuntimeTypeStringViewCopy,
	.Compare = __xrtRuntimeTypeStringViewCompare,
	.Hash = __xrtRuntimeTypeStringViewHash,
	.Format = __xrtRuntimeTypeStringViewFormat
};

static const xrttype __xrtRuntimeTypeStringView = {
	.Id = UINT64_C(0x71847431A2BA8956),
	.Kind = XRT_TYPE_STRING,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL | XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("string-view"),
	.AbiName = XRT_STR_INIT("xrt.owned-string-view"),
	.Size = sizeof(xstrview),
	.Align = XRT_INTERNAL_ALIGNOF(xstrview),
	.InstanceAlign = 1u,
	.Ops = &__xrtRuntimeTypeStringViewOps
};

XRT_API const xrttype* xrtTypeStringView(void)
{
	return &__xrtRuntimeTypeStringView;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_convert_string.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)




#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)

/* 判断类型是否为转换层唯一支持的拥有型 XRT 字符串槽。 */
static bool __xrtTypeConvertCanonicalString(const xrttype* pType)
{
	return xrtTypeSame(pType, xrtTypeString());
}



/* 判断可选文本扩展是否支持指定转换方向。 */
bool __xrtTypeStringCanConvert(
	const xrttype* pSourceType,
	const xrttype* pTargetType
)
{
	if ( __xrtTypeConvertCanonicalString(pSourceType) ) {
		return (pTargetType->Kind == XRT_TYPE_BOOL) ||
			(pTargetType->Kind == XRT_TYPE_SIGNED_INT) ||
			(pTargetType->Kind == XRT_TYPE_UNSIGNED_INT) ||
			(pTargetType->Kind == XRT_TYPE_FLOAT) ||
			(pTargetType->Kind == XRT_TYPE_TIME) ||
			(pTargetType->Kind == XRT_TYPE_TYPE);
	}
	if ( !__xrtTypeConvertCanonicalString(pTargetType) ) {
		return false;
	}
	if ( (pSourceType->Ops != NULL) &&
		 (pSourceType->Ops->Format != NULL) ) {
		return true;
	}
	return (pSourceType->Kind == XRT_TYPE_NULL) ||
		(pSourceType->Kind == XRT_TYPE_BOOL) ||
		(pSourceType->Kind == XRT_TYPE_SIGNED_INT) ||
		(pSourceType->Kind == XRT_TYPE_UNSIGNED_INT) ||
		(pSourceType->Kind == XRT_TYPE_FLOAT) ||
		(pSourceType->Kind == XRT_TYPE_TIME) ||
		(pSourceType->Kind == XRT_TYPE_POINTER) ||
		(pSourceType->Kind == XRT_TYPE_TYPE);
}



/* 严格解析布尔文本，只接受 true、false、1 和 0。 */
static bool __xrtTypeStringParseBool(xstrview Text, bool* pValue)
{
	if ( xrtStrCaseEqual(Text, XRT_STR_LITERAL("true")) ||
		 xrtStrEqual(Text, XRT_STR_LITERAL("1")) ) {
		*pValue = true;
		return true;
	}
	if ( xrtStrCaseEqual(Text, XRT_STR_LITERAL("false")) ||
		 xrtStrEqual(Text, XRT_STR_LITERAL("0")) ) {
		*pValue = false;
		return true;
	}
	__xrtTypeConvertError(XERR_VALUE, XTYPE_CONVERT_ERROR_PARSE,
		"parse-string", "the string is not a strict boolean value");
	return false;
}



/* 包装文本解析器错误，保留具体的格式或范围原因。 */
static bool __xrtTypeStringParseFailed(cstr sMessage)
{
	__xrtTypeConvertWrap(XERR_VALUE, XTYPE_CONVERT_ERROR_PARSE,
		"parse-string", sMessage);
	return false;
}



/* 把字符串严格解析为目标标量，解析失败和范围失败均保持目标不变。 */
static bool __xrtTypeStringParse(
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget
)
{
	str sSource;
	xstrview Text;
	bool bValue;
	int64 iSigned;
	uint64 iUnsigned;
	double fValue;
	xtime Time;

	memcpy(&sSource, pSource, sizeof(sSource));
	Text = xrtStrView(sSource);
	switch ( pTargetType->Kind ) {
		case XRT_TYPE_BOOL:
			if ( !__xrtTypeStringParseBool(Text, &bValue) ) {
				return false;
			}
			return xrtTypeConvert(xrtTypeBool(), &bValue,
				pTargetType, pTarget, XTYPE_CONVERT_EXPLICIT);
		case XRT_TYPE_SIGNED_INT:
			if ( !xrtIntParse(Text, 10u, 0u, &iSigned) ) {
				return __xrtTypeStringParseFailed(
					"the string is not a strict signed integer"
				);
			}
			return xrtTypeConvert(xrtTypeInt64(), &iSigned,
				pTargetType, pTarget, XTYPE_CONVERT_EXPLICIT);
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( !xrtUIntParse(Text, 10u, 0u, &iUnsigned) ) {
				return __xrtTypeStringParseFailed(
					"the string is not a strict unsigned integer"
				);
			}
			return xrtTypeConvert(xrtTypeUInt64(), &iUnsigned,
				pTargetType, pTarget, XTYPE_CONVERT_EXPLICIT);
		case XRT_TYPE_FLOAT:
			if ( !xrtNumParse(Text,
				(uint32)XNUMBER_PARSE_SPECIAL, &fValue) ) {
				return __xrtTypeStringParseFailed(
					"the string is not a strict floating-point value"
				);
			}
			return xrtTypeConvert(xrtTypeFloat64(), &fValue,
				pTargetType, pTarget, XTYPE_CONVERT_EXPLICIT);
		case XRT_TYPE_TIME:
			if ( !xrtTimeParseAny(Text, &Time) ) {
				return __xrtTypeStringParseFailed(
					"the string is not a supported time value"
				);
			}
			return xrtTypeConvert(xrtTypeTime(), &Time,
				pTargetType, pTarget, XTYPE_CONVERT_EXPLICIT);
		default:
			__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
				"parse-string", "the target type cannot be parsed from a string");
			return false;
	}
}



/*
	把内建标量格式化到调用方栈缓冲。
	pSupported 区分不支持的类型与已经产生下层错误的格式化失败。
*/
static bool __xrtTypeStringFormatBuiltin(
	const xrttype* pSourceType,
	const void* pSource,
	char* sBuffer,
	size_t iCapacity,
	xstrview* pText,
	bool* pSupported
)
{
	bool bValue;
	int64 iSigned;
	uint64 iUnsigned;
	double fValue;
	xtime Time;
	ptr pPointer;
	size_t iSize;

	*pSupported = true;
	switch ( pSourceType->Kind ) {
		case XRT_TYPE_NULL:
			*pText = XRT_STR_LITERAL("null");
			return true;
		case XRT_TYPE_BOOL:
			if ( !__xrtTypeReadBool(
				pSource, pSourceType->Size, &bValue
			) ) {
				return false;
			}
			*pText = bValue ? XRT_STR_LITERAL("true") :
				XRT_STR_LITERAL("false");
			return true;
		case XRT_TYPE_SIGNED_INT:
			if ( !__xrtTypeReadSigned(
				pSource, pSourceType->Size, &iSigned
			) || !xrtIntWrite(
				iSigned, 10u, sBuffer, iCapacity, &iSize, 0u
			) ) {
				return false;
			}
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( !__xrtTypeReadUnsigned(
				pSource, pSourceType->Size, &iUnsigned
			) || !xrtUIntWrite(
				iUnsigned, 10u, sBuffer, iCapacity, &iSize, 0u
			) ) {
				return false;
			}
			break;
		case XRT_TYPE_FLOAT:
			if ( !__xrtTypeReadFloat(
				pSource, pSourceType->Size, &fValue
			) || !xrtNumWrite(
				fValue, sBuffer, iCapacity, &iSize, 0u
			) ) {
				return false;
			}
			break;
		case XRT_TYPE_TIME:
			memcpy(&Time, pSource, sizeof(Time));
			iSize = xrtTimeWriteRFC3339(
				sBuffer, iCapacity, Time, 0
			);
			if ( iSize == XRT_NPOS ) {
				return false;
			}
			break;
		case XRT_TYPE_POINTER:
			memcpy(&pPointer, pSource, sizeof(pPointer));
			if ( pPointer == NULL ) {
				*pText = XRT_STR_LITERAL("null");
				return true;
			}
			if ( !xrtUIntWrite(
				(uint64)(uintptr_t)pPointer, 16u,
				sBuffer, iCapacity, &iSize, (uint32)XNUMBER_PREFIX
			) ) {
				return false;
			}
			break;
		default:
			*pSupported = false;
			return false;
	}
	pText->Data = sBuffer;
	pText->Size = iSize;
	return true;
}



/* 把格式化分块追加到临时字符串构建器。 */
static bool __xrtTypeStringBufferWrite(xstrview Text, ptr pContext)
{
	return xrtStrBufAppend((xstrbuf*)pContext, Text);
}



/* 把一个借用类型值分块格式化为 UTF-8 文本。 */
XRT_API bool xrtTypeFormat(
	const xrttype* pType,
	const void* pValue,
	xrttypewriter pWrite,
	ptr pContext
)
{
	const xerror* pPrevious;
	char sBuffer[128];
	xstrview Text;
	bool bSupported;
	bool bSuccess;

	if ( (pType == NULL) || (pWrite == NULL) ||
		 ((pValue == NULL) && (pType->Size != 0u)) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"format", "the runtime type, value, or writer is invalid");
		return false;
	}
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"format", "the runtime type descriptor is invalid");
		return false;
	}
	pPrevious = xrtGetError();
	if ( (pType->Ops != NULL) && (pType->Ops->Format != NULL) ) {
		bSuccess = pType->Ops->Format(
			pValue, pType, pWrite, pContext
		);
		if ( !bSuccess ) {
			if ( xrtGetError() == pPrevious ) {
				__xrtTypeConvertError(XERR_STATE,
					XTYPE_CONVERT_ERROR_OPERATION, "format",
					"the custom type formatter failed without an error");
			} else {
				__xrtTypeConvertWrap(XERR_STATE,
					XTYPE_CONVERT_ERROR_OPERATION, "format",
					"the custom type formatter failed");
			}
		}
		return bSuccess;
	}
	bSupported = false;
	if ( !__xrtTypeStringFormatBuiltin(
		pType, pValue, sBuffer, sizeof(sBuffer), &Text, &bSupported
	) ) {
		if ( !bSupported ) {
			__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
				"format", "the runtime type has no text representation");
		} else if ( xrtGetError() == pPrevious ) {
			__xrtTypeConvertError(XERR_STATE,
				XTYPE_CONVERT_ERROR_OPERATION, "format",
				"the built-in type formatter failed without an error");
		} else {
			__xrtTypeConvertWrap(XERR_STATE,
				XTYPE_CONVERT_ERROR_OPERATION, "format",
				"the source value could not be formatted");
		}
		return false;
	}
	pPrevious = xrtGetError();
	bSuccess = pWrite(Text, pContext);
	if ( !bSuccess ) {
		if ( xrtGetError() == pPrevious ) {
			__xrtTypeConvertError(XERR_STATE,
				XTYPE_CONVERT_ERROR_OPERATION, "format",
				"the type format writer failed without an error");
		} else {
			__xrtTypeConvertWrap(XERR_STATE,
				XTYPE_CONVERT_ERROR_OPERATION, "format",
				"the type format writer failed");
		}
	}
	return bSuccess;
}



/* 把一个借用类型值格式化为新分配的零结尾 UTF-8 字符串。 */
XRT_API str xrtTypeToString(
	const xrttype* pType,
	const void* pValue
)
{
	xstrbuf Buffer;
	str sResult;

	xrtStrBufInit(&Buffer);
	if ( !xrtTypeFormat(
		pType, pValue, __xrtTypeStringBufferWrite, &Buffer
	) ) {
		xrtStrBufFree(&Buffer);
		return NULL;
	}
	sResult = xrtStrBufTake(&Buffer);
	if ( sResult == NULL ) {
		__xrtTypeConvertWrap(XERR_MEMORY, XTYPE_CONVERT_ERROR_OPERATION,
			"to-string", "the formatted string could not be allocated");
	}
	xrtStrBufFree(&Buffer);
	return sResult;
}



/* 格式化成功后原子替换目标拥有的旧字符串。 */
static bool __xrtTypeStringFormatReplace(
	const xrttype* pSourceType,
	const void* pSource,
	ptr pTarget
)
{
	str sResult;
	str sPrevious;

	sResult = xrtTypeToString(pSourceType, pSource);
	if ( sResult == NULL ) {
		__xrtTypeConvertWrap(XERR_STATE, XTYPE_CONVERT_ERROR_OPERATION,
			"format-string", "the source value could not be formatted");
		return false;
	}
	memcpy(&sPrevious, pTarget, sizeof(sPrevious));
	memcpy(pTarget, &sResult, sizeof(sResult));
	xrtFree(sPrevious);
	return true;
}



/* 执行可选文本扩展转换，失败时保持已初始化目标不变。 */
bool __xrtTypeStringConvert(
	const xrttype* pSourceType,
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget
)
{
	if ( __xrtTypeConvertCanonicalString(pSourceType) ) {
		return __xrtTypeStringParse(pSource, pTargetType, pTarget);
	}
	if ( __xrtTypeConvertCanonicalString(pTargetType) ) {
		return __xrtTypeStringFormatReplace(
			pSourceType, pSource, pTarget
		);
	}
	__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
		"convert-string", "the conversion does not use the canonical string type");
	return false;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/value_convert.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_VALUE_CONVERT) || \
	defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING)


#if defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING)
#endif



#if defined(XRUNTIME_FEATURE_VALUE_CONVERT)

/* 归一化保存动态 Value 可以直接借用的标量。 */
typedef union __xrt_value_convert_scalar {
	bool Bool;
	int64 Integer;
	uint64 Unsigned;
	double Float;
	xtime Time;
	ptr Pointer;
} __xrt_value_convert_scalar;



/* 包装动态值读取失败，保留 Value 层给出的具体原因。 */
static bool __xrtValueConvertReadFailed(void)
{
	__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
		"value-convert", "the dynamic scalar could not be read");
	return false;
}



#if defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING)

/*
	把动态字符串作为规范拥有型字符串的借用来源参与转换。
	Value 保证末尾零，内嵌零会使 str 语义丢失长度，因此明确拒绝。
*/
static bool __xrtValueConvertString(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
)
{
	xstrview Text;
	str sBorrowed;

	if ( !xrtValueGetString(pSource, &Text) ) {
		return __xrtValueConvertReadFailed();
	}
	if ( pTargetType == xrtTypeStringView() ) {
		return xrtTypeCopyValue(pTargetType, pTarget, &Text);
	}
	if ( (Text.Size != 0u) &&
		 (memchr(Text.Data, 0, Text.Size) != NULL) ) {
		__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"value-convert", "a runtime string cannot represent embedded zero bytes");
		return false;
	}
	sBorrowed = (str)Text.Data;
	return xrtTypeConvert(
		xrtTypeString(), &sBorrowed, pTargetType, pTarget, Mode
	);
}

#endif



/* 把动态 Value 标量转换后写入已经初始化的目标值。 */
XRT_API bool xrtValueConvertTo(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
)
{
	__xrt_value_convert_scalar Scalar;
	const xrttype* pSourceType;
	const void* pValue;
	xvaluetype Type;

	if (
		(pSource == NULL) || (pTargetType == NULL) ||
		((pTarget == NULL) && (pTargetType->Size != 0u))
	) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"value-convert", "the source, target, or runtime type is invalid");
		return false;
	}
	if ( !__xrtTypeConvertModeValid(Mode) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_MODE,
			"value-convert", "the conversion mode is invalid");
		return false;
	}
	if ( !xrtTypeValidate(pTargetType) ) {
		__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"value-convert", "the target runtime type is invalid");
		return false;
	}
	Type = xrtValueType(pSource);
	if ( Type == XVALUE_INVALID ) {
		return __xrtValueConvertReadFailed();
	}
	memset(&Scalar, 0, sizeof(Scalar));
	pSourceType = NULL;
	pValue = NULL;
	switch ( Type ) {
		case XVALUE_NULL:
			pSourceType = xrtTypeNull();
			break;
		case XVALUE_BOOL:
			if ( !xrtValueGetBool(pSource, &Scalar.Bool) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypeBool();
			pValue = &Scalar.Bool;
			break;
		case XVALUE_INT:
			if ( !xrtValueGetInt(pSource, &Scalar.Integer) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypeInt64();
			pValue = &Scalar.Integer;
			break;
		case XVALUE_UINT:
			if ( !xrtValueGetUInt(pSource, &Scalar.Unsigned) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypeUInt64();
			pValue = &Scalar.Unsigned;
			break;
		case XVALUE_FLOAT:
			if ( !xrtValueGetFloat(pSource, &Scalar.Float) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypeFloat64();
			pValue = &Scalar.Float;
			break;
#if defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING)
		case XVALUE_STRING:
			return __xrtValueConvertString(
				pSource, pTargetType, pTarget, Mode
			);
#endif
		case XVALUE_TIME:
			if ( !xrtValueGetTime(pSource, &Scalar.Time) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypeTime();
			pValue = &Scalar.Time;
			break;
		case XVALUE_POINTER:
			if ( !xrtValueGetPointer(pSource, &Scalar.Pointer) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypePointer();
			pValue = &Scalar.Pointer;
			break;
		default:
			__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
				"value-convert", "the dynamic value is not a supported scalar");
			return false;
	}
	return xrtTypeConvert(
		pSourceType, pValue, pTargetType, pTarget, Mode
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_value.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_SET_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE)
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)
#endif

#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)
#endif

#include <float.h>



#if defined(XRUNTIME_FEATURE_TYPED_VALUE)

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_SET_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

#define XRT_TYPED_VALUE_SCRATCH_INLINE_SIZE 64u



typedef union __xrt_typed_value_inline {
	long double Float;
	ptr Pointer;
	uint64 Integer;
	void (*Function)(void);
	uint8 Data[XRT_TYPED_VALUE_SCRATCH_INLINE_SIZE];
} __xrt_typed_value_inline;



typedef struct __xrt_typed_value_scratch {
	ptr Allocation;
	ptr Value;
	__xrt_typed_value_inline Inline;
} __xrt_typed_value_scratch;

#endif



/* 设置动态值转换层结构化错误。 */
static void __xrtTypedValueError(
	xerrkind Kind,
	xtypedvalueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-value";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为类型、Value 或用户转换器错误补充转换上下文。 */
static void __xrtTypedValueWrap(
	xerrkind DefaultKind,
	xtypedvalueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-value";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 检查转换参数和运行时类型描述。 */
static bool __xrtTypedValueArguments(
	const xrttype* pType,
	const void* pValue,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_TYPE,
			sOperation, "the runtime type is invalid");
		return false;
	}
	if ( (pValue == NULL) && (pType->Size != 0u) ) {
		__xrtTypedValueError(XERR_ARGUMENT, XTYPED_VALUE_ERROR_ARGUMENT,
			sOperation, "the typed value storage is null");
		return false;
	}
	return true;
}



/* 在清理资源后恢复进入清理阶段时持有的根错误。 */
static void __xrtTypedValueRestoreError(xerror* pError)
{
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_SET_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 按类型对齐申请一个可重复使用的临时值槽。 */
static bool __xrtTypedValueScratchCreate(
	const xrttype* pType,
	__xrt_typed_value_scratch* pScratch
)
{
	size_t iSize;
	uintptr_t iAddress;

	memset(pScratch, 0, sizeof(*pScratch));
	if ( (pType->Size <= XRT_TYPED_VALUE_SCRATCH_INLINE_SIZE) &&
		 (pType->Align <= XRT_INTERNAL_ALIGNOF(__xrt_typed_value_inline)) ) {
		pScratch->Value = pScratch->Inline.Data;
		return true;
	}
	if ( pType->Size > (SIZE_MAX - (pType->Align - 1u)) ) {
		__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
			"scratch", "the aligned typed value size overflows");
		return false;
	}
	iSize = pType->Size + (pType->Align - 1u);
	pScratch->Allocation = xrtMalloc(iSize != 0u ? iSize : 1u);
	if ( pScratch->Allocation == NULL ) {
		return false;
	}
	iAddress = (uintptr_t)pScratch->Allocation;
	pScratch->Value = (ptr)((iAddress + (pType->Align - 1u)) &
		~((uintptr_t)pType->Align - 1u));
	return true;
}



/* 释放临时值槽，不改变当前错误。 */
static void __xrtTypedValueScratchDestroy(
	__xrt_typed_value_scratch* pScratch
)
{
	xerror* pError = xrtTakeError();

	if ( pScratch->Allocation != NULL ) {
		xrtFree(pScratch->Allocation);
	}
	memset(pScratch, 0, sizeof(*pScratch));
	__xrtTypedValueRestoreError(pError);
}



/* 结束动态容器快照迭代，同时保留根错误。 */
static void __xrtTypedValueIteratorEnd(xvalueiter* pIterator)
{
	xerror* pError = xrtTakeError();

	xrtValueIterEnd(pIterator);
	__xrtTypedValueRestoreError(pError);
}



/* 释放动态值结果，同时保留根错误。 */
static void __xrtTypedValueRelease(xvalue* pValue)
{
	xerror* pError = xrtTakeError();

	xrtValueRelease(pValue);
	__xrtTypedValueRestoreError(pError);
}

#endif



/* 销毁一个已初始化临时值，并保留进入函数时的错误。 */
static void __xrtTypedValueDropPreserveError(
	const xrttype* pType,
	ptr pValue
)
{
	xerror* pError = xrtTakeError();

	xrtTypeDropValue(pType, pValue);
	__xrtTypedValueRestoreError(pError);
}



/* 使用 XRT 内建安全标量规则解码动态值。 */
static bool __xrtTypedValueDecodeBuiltin(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	bool* pHandled
)
{
	int64 iInteger;
	uint64 iUnsigned;
	double fNumber;

	*pHandled = true;
	switch ( pTargetType->Kind ) {
		case XRT_TYPE_NULL:
			if ( xrtValueType(pSource) != XVALUE_NULL ) {
				break;
			}
			return true;
		case XRT_TYPE_BOOL: {
			bool bValue;

			if ( xrtValueGetBool(pSource, &bValue) &&
				 __xrtTypeWriteBool(
					bValue, pTargetType->Size, pTarget
				 ) ) {
				return true;
			}
			break;
		}
		case XRT_TYPE_SIGNED_INT:
			if ( xrtValueType(pSource) == XVALUE_UINT ) {
				if ( !xrtValueGetUInt(pSource, &iUnsigned) ) {
					break;
				}
				if ( iUnsigned > (uint64)INT64_MAX ) {
					__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
						"to-typed", "the integer exceeds the target signed range");
					return false;
				}
				iInteger = (int64)iUnsigned;
			} else if ( !xrtValueGetInt(pSource, &iInteger) ) {
				break;
			}
			if ( !__xrtTypeWriteSigned(
				iInteger, pTargetType->Size, pTarget
			) ) {
				__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
					"to-typed", "the integer exceeds the target signed range");
				return false;
			}
			return true;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( xrtValueType(pSource) == XVALUE_UINT ) {
				if ( !xrtValueGetUInt(pSource, &iUnsigned) ) {
					break;
				}
			} else {
				if ( !xrtValueGetInt(pSource, &iInteger) ) {
					break;
				}
				if ( iInteger < 0 ) {
					__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
						"to-typed", "the integer exceeds the target unsigned range");
					return false;
				}
				iUnsigned = (uint64)iInteger;
			}
			if ( !__xrtTypeWriteUnsigned(
				iUnsigned, pTargetType->Size, pTarget
			) ) {
				__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
					"to-typed", "the integer exceeds the target unsigned range");
				return false;
			}
			return true;
		case XRT_TYPE_FLOAT:
			if ( !xrtValueGetFloat(pSource, &fNumber) ) {
				break;
			}
			if ( !__xrtTypeWriteFloat(
				fNumber, pTargetType->Size, true, pTarget
			) ) {
				__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
					"to-typed", "the float is not losslessly representable");
				return false;
			}
			return true;
		case XRT_TYPE_TIME: {
			xtime Time;

			if ( (pTargetType->Size == sizeof(Time)) &&
				 xrtValueGetTime(pSource, &Time) ) {
				memcpy(pTarget, &Time, sizeof(Time));
				return true;
			}
			break;
		}
		case XRT_TYPE_POINTER: {
			ptr pPointer;

			if ( (pTargetType->Size == sizeof(pPointer)) &&
				 xrtValueGetPointer(pSource, &pPointer) ) {
				memcpy(pTarget, &pPointer, sizeof(pPointer));
				return true;
			}
			break;
		}
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)
		case XRT_TYPE_CALLABLE: {
			xrtcallable* pCallable;

			if ( pTargetType->Size != sizeof(pCallable) ) {
				break;
			}
			if ( xrtValueType(pSource) == XVALUE_NULL ) {
				return true;
			}
			if ( !xrtValueIsCallable(pSource) ) {
				break;
			}
			pCallable = xrtValueGetCallable(pSource);
			return (pCallable != NULL) && xrtTypeCopyValue(
				pTargetType, pTarget, &pCallable
			);
		}
#endif
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
		case XRT_TYPE_FUTURE: {
			xfuture* pFuture;

			if ( pTargetType->Size != sizeof(pFuture) ) {
				break;
			}
			if ( xrtValueType(pSource) == XVALUE_NULL ) {
				return true;
			}
			if ( !xrtValueIsFuture(pSource) ) {
				break;
			}
			pFuture = xrtValueGetFuture(pSource);
			return (pFuture != NULL) && xrtTypeCopyValue(
				pTargetType, pTarget, &pFuture
			);
		}
#endif
		default:
			*pHandled = false;
			return false;
	}
	__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_TYPE,
		"to-typed", "the dynamic value cannot be represented by the target type");
	return false;
}



/* 使用 XRT 内建安全标量规则编码类型值。 */
static xvalue* __xrtTypedValueEncodeBuiltin(
	const xrttype* pSourceType,
	const void* pSource,
	bool* pHandled
)
{
	int64 iInteger;
	uint64 iUnsigned;
	double fNumber;

	*pHandled = true;
	switch ( pSourceType->Kind ) {
		case XRT_TYPE_NULL:
			return xrtValueNull();
		case XRT_TYPE_BOOL: {
			bool bValue;

			if ( __xrtTypeReadBool(
				pSource, pSourceType->Size, &bValue
			) ) {
				return xrtValueBool(bValue);
			}
			break;
		}
		case XRT_TYPE_SIGNED_INT:
			if ( __xrtTypeReadSigned(
				pSource, pSourceType->Size, &iInteger
			) ) {
				return xrtValueInt(iInteger);
			}
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( __xrtTypeReadUnsigned(
				pSource, pSourceType->Size, &iUnsigned
			) ) {
				return xrtValueUInt(iUnsigned);
			}
			break;
		case XRT_TYPE_FLOAT:
			if ( __xrtTypeReadFloat(
				pSource, pSourceType->Size, &fNumber
			) ) {
				return xrtValueFloat(fNumber);
			}
			break;
		case XRT_TYPE_TIME: {
			xtime Time;

			if ( pSourceType->Size == sizeof(Time) ) {
				memcpy(&Time, pSource, sizeof(Time));
				return xrtValueTime(Time);
			}
			break;
		}
		case XRT_TYPE_POINTER: {
			ptr pPointer;

			if ( pSourceType->Size == sizeof(pPointer) ) {
				memcpy(&pPointer, pSource, sizeof(pPointer));
				return xrtValuePointer(pPointer);
			}
			break;
		}
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)
		case XRT_TYPE_CALLABLE: {
			xrtcallable* pCallable;

			if ( pSourceType->Size != sizeof(pCallable) ) {
				break;
			}
			memcpy(&pCallable, pSource, sizeof(pCallable));
			return pCallable != NULL ?
				xrtValueCallable(pCallable) : xrtValueNull();
		}
#endif
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
		case XRT_TYPE_FUTURE: {
			xfuture* pFuture;

			if ( pSourceType->Size != sizeof(pFuture) ) {
				break;
			}
			memcpy(&pFuture, pSource, sizeof(pFuture));
			return pFuture != NULL ?
				xrtValueFuture(pFuture) : xrtValueNull();
		}
#endif
		default:
			*pHandled = false;
			return NULL;
	}
	__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
		"from-typed", "the typed value cannot be represented by a dynamic value");
	return NULL;
}



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE)

/* 把动态字符串复制到规范拥有型 str 槽。 */
static bool __xrtTypedValueDecodeString(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	bool* pHandled
)
{
	xstrview Text;
	str sResult;

	*pHandled = xrtTypeSame(pTargetType, xrtTypeString()) ||
		xrtTypeSame(pTargetType, xrtTypeStringView());
	if ( !*pHandled ) {
		return false;
	}
	if ( !xrtValueGetString(pSource, &Text) ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_TYPE,
			"to-typed", "the dynamic value is not a string");
		return false;
	}
	if ( xrtTypeSame(pTargetType, xrtTypeStringView()) ) {
		xstrview Result = {0};
		if ( !xrtTypeCopyValue(pTargetType, &Result, &Text) ) return false;
		memcpy(pTarget, &Result, sizeof(Result));
		return true;
	}
	if ( (Text.Size != 0u) &&
		 (memchr(Text.Data, 0, Text.Size) != NULL) ) {
		__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			"to-typed", "an owned str cannot represent embedded zero bytes");
		return false;
	}
	sResult = xrtStrDupView(Text);
	if ( sResult == NULL ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONVERT,
			"to-typed", "the dynamic string could not be copied");
		return false;
	}
	memcpy(pTarget, &sResult, sizeof(sResult));
	return true;
}



/* 把规范拥有型 str 槽复制为独立动态字符串。 */
static xvalue* __xrtTypedValueEncodeString(
	const xrttype* pSourceType,
	const void* pSource,
	bool* pHandled
)
{
	str sSource;

	*pHandled = xrtTypeSame(pSourceType, xrtTypeString()) ||
		xrtTypeSame(pSourceType, xrtTypeStringView());
	if ( !*pHandled ) {
		return NULL;
	}
	if ( xrtTypeSame(pSourceType, xrtTypeStringView()) ) {
		xstrview Text;
		memcpy(&Text, pSource, sizeof(Text));
		return xrtValueString(Text);
	}
	memcpy(&sSource, pSource, sizeof(sSource));
	return xrtValueString(xrtStrView(sSource));
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)

/* 深复制动态来源到运行时 Value 所有权槽。 */
static bool __xrtTypedValueDecodeRuntimeValue(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	bool* pHandled
)
{
	xvalue* pBorrowed = (xvalue*)pSource;

	*pHandled = xrtTypeSame(pTargetType, xrtTypeValue());
	if ( !*pHandled ) {
		return false;
	}
	if ( !xrtTypeCloneValue(pTargetType, pTarget, &pBorrowed) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONVERT,
			"to-typed", "the dynamic Value graph could not be copied");
		return false;
	}
	return true;
}



/* 从运行时 Value 所有权槽深复制独立动态值。 */
static xvalue* __xrtTypedValueEncodeRuntimeValue(
	const xrttype* pSourceType,
	const void* pSource
)
{
	xvalue* pOwned;
	xvalue* pResult;

	if ( !xrtTypeSame(pSourceType, xrtTypeValue()) ) {
		return NULL;
	}
	memcpy(&pOwned, pSource, sizeof(pOwned));
	if ( pOwned == NULL ) {
		__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONVERT,
			"from-typed", "the runtime Value ownership slot is empty");
		return NULL;
	}
	pResult = xrtValueDeepClone(pOwned);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONVERT,
			"from-typed", "the runtime Value graph could not be copied");
	}
	return pResult;
}

#endif



/* 把动态值转换为一个新初始化的运行时类型值。 */
XRT_API bool xrtValueToTyped(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	const xvalueconverter* pConverter
)
{
	bool bHandled;
	bool bResult;

	if ( pSource == NULL ) {
		__xrtTypedValueError(XERR_ARGUMENT, XTYPED_VALUE_ERROR_ARGUMENT,
			"to-typed", "the source dynamic value is null");
		return false;
	}
	if ( !__xrtTypedValueArguments(pTargetType, pTarget, "to-typed") ) {
		return false;
	}
	if ( !xrtTypeInitValue(pTargetType, pTarget) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONVERT,
			"to-typed", "the target typed value could not be initialized");
		return false;
	}
	bHandled = false;
	bResult = false;
#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE)
	bResult = __xrtTypedValueDecodeString(
		pSource, pTargetType, pTarget, &bHandled
	);
#endif
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)
	if ( !bHandled ) {
		bResult = __xrtTypedValueDecodeRuntimeValue(
			pSource, pTargetType, pTarget, &bHandled
		);
	}
#endif
	if ( !bHandled ) {
		bResult = __xrtTypedValueDecodeBuiltin(
			pSource, pTargetType, pTarget, &bHandled
		);
	}
	if ( !bHandled && (pConverter != NULL) &&
		 (pConverter->ToTyped != NULL) ) {
		bResult = pConverter->ToTyped(
			pSource, pTargetType, pTarget, pConverter->Context
		);
		if ( !bResult ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"to-typed", "the custom dynamic value decoder failed");
		}
	} else if ( !bHandled ) {
		__xrtTypedValueError(XERR_UNSUPPORTED, XTYPED_VALUE_ERROR_TYPE,
			"to-typed", "the target type requires a custom Value converter");
	}
	if ( !bResult ) {
		__xrtTypedValueDropPreserveError(pTargetType, pTarget);
		return false;
	}
	return true;
}



/* 把运行时类型值转换为一个独立动态值。 */
XRT_API xvalue* xrtValueFromTyped(
	const xrttype* pSourceType,
	const void* pSource,
	const xvalueconverter* pConverter
)
{
	xvalue* pResult;
	bool bHandled;

	if ( !__xrtTypedValueArguments(pSourceType, pSource, "from-typed") ) {
		return NULL;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE)
	pResult = __xrtTypedValueEncodeString(
		pSourceType, pSource, &bHandled
	);
	if ( bHandled ) {
		return pResult;
	}
#endif
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)
	if ( xrtTypeSame(pSourceType, xrtTypeValue()) ) {
		return __xrtTypedValueEncodeRuntimeValue(pSourceType, pSource);
	}
#endif
	pResult = __xrtTypedValueEncodeBuiltin(
		pSourceType, pSource, &bHandled
	);
	if ( bHandled ) {
		return pResult;
	}
	if ( (pConverter == NULL) || (pConverter->FromTyped == NULL) ) {
		__xrtTypedValueError(XERR_UNSUPPORTED, XTYPED_VALUE_ERROR_TYPE,
			"from-typed", "the source type requires a custom Value converter");
		return NULL;
	}
	pResult = pConverter->FromTyped(
		pSourceType, pSource, pConverter->Context
	);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			"from-typed", "the custom dynamic value encoder failed");
	}
	return pResult;
}



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_SET_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 把一个动态值解码到新初始化的临时值槽。 */
static bool __xrtTypedValueScratchDecode(
	const xrttype* pType,
	const xvalue* pValue,
	const xvalueconverter* pConverter,
	__xrt_typed_value_scratch* pScratch,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedValueError(XERR_ARGUMENT, XTYPED_VALUE_ERROR_ARGUMENT,
			sOperation, "the dynamic item is null");
		return false;
	}
	if ( !__xrtTypedValueScratchCreate(pType, pScratch) ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the temporary typed item could not be allocated");
		return false;
	}
	if ( !xrtValueToTyped(pValue, pType, pScratch->Value, pConverter) ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the dynamic item could not be converted");
		__xrtTypedValueScratchDestroy(pScratch);
		return false;
	}
	return true;
}



/* 销毁临时类型值和对齐存储，同时保留进入清理阶段时的错误。 */
static void __xrtTypedValueScratchDrop(
	const xrttype* pType,
	__xrt_typed_value_scratch* pScratch
)
{
	__xrtTypedValueDropPreserveError(pType, pScratch->Value);
	__xrtTypedValueScratchDestroy(pScratch);
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)

/* 在数组回调门内把动态值解码为临时元素。 */
static bool __xrtTypedValueArrayDecode(
	const xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter,
	__xrt_typed_value_scratch* pScratch,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedArrayItemType(pArray);
	bool bResult;

	if ( pType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed array is invalid");
		return false;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	bResult = __xrtTypedValueScratchDecode(
		pType, pValue, pConverter, pScratch, sOperation
	);
	__xrtTypedArrayCallbackEnd(pArray);
	return bResult;
}



/* 在数组回调门内把借用元素编码为独立动态值。 */
static xvalue* __xrtTypedValueArrayEncode(
	const xtypedarray* pArray,
	const void* pItem,
	const xvalueconverter* pConverter,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedArrayItemType(pArray);
	xvalue* pResult;

	__xrtTypedArrayCallbackBegin(pArray);
	pResult = xrtValueFromTyped(pType, pItem, pConverter);
	__xrtTypedArrayCallbackEnd(pArray);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the typed array item could not be converted");
	}
	return pResult;
}



/* 把一个动态值转换后追加到类型数组。 */
XRT_API bool xrtTypedArrayPushValue(
	xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueArrayDecode(
		pArray, pValue, pConverter, &Scratch, "array-push-value"
	) ) {
		return false;
	}
	pType = xrtTypedArrayItemType(pArray);
	bResult = xrtTypedArrayPush(pArray, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-push-value", "the converted array item could not be appended");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把一个动态值转换后插入类型数组的指定下标。 */
XRT_API bool xrtTypedArrayInsertValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueArrayDecode(
		pArray, pValue, pConverter, &Scratch, "array-insert-value"
	) ) {
		return false;
	}
	pType = xrtTypedArrayItemType(pArray);
	bResult = xrtTypedArrayInsert(pArray, iIndex, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-insert-value", "the converted array item could not be inserted");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把一个动态值转换后原子替换类型数组的指定元素。 */
XRT_API bool xrtTypedArraySetValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueArrayDecode(
		pArray, pValue, pConverter, &Scratch, "array-set-value"
	) ) {
		return false;
	}
	pType = xrtTypedArrayItemType(pArray);
	bResult = xrtTypedArraySet(pArray, iIndex, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-set-value", "the converted array item could not replace the target");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把类型数组的指定元素转换为独立动态值。 */
XRT_API xvalue* xrtTypedArrayGetValue(
	const xtypedarray* pArray,
	size_t iIndex,
	const xvalueconverter* pConverter
)
{
	const void* pItem = xrtTypedArrayConstGet(pArray, iIndex);

	return pItem != NULL ? __xrtTypedValueArrayEncode(
		pArray, pItem, pConverter, "array-get-value"
	) : NULL;
}



/* 转换并删除类型数组的指定元素。 */
XRT_API xvalue* xrtTypedArrayTakeValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalueconverter* pConverter
)
{
	xvalue* pResult = xrtTypedArrayGetValue(pArray, iIndex, pConverter);

	if ( pResult == NULL ) {
		return NULL;
	}
	if ( !xrtTypedArrayRemove(pArray, iIndex, 1u) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-take-value", "the converted array item could not be removed");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	return pResult;
}



/* 转换并删除类型数组的末尾元素。 */
XRT_API xvalue* xrtTypedArrayPopValue(
	xtypedarray* pArray,
	const xvalueconverter* pConverter
)
{
	size_t iCount = xrtTypedArrayCount(pArray);

	return xrtTypedArrayTakeValue(
		pArray, iCount != 0u ? iCount - 1u : SIZE_MAX, pConverter
	);
}



/* 把动态值转换为元素类型并查找第一处相等元素。 */
XRT_API size_t xrtTypedArrayFindValue(
	const xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	size_t iIndex;

	if ( !__xrtTypedValueArrayDecode(
		pArray, pValue, pConverter, &Scratch, "array-find-value"
	) ) {
		return SIZE_MAX;
	}
	pType = xrtTypedArrayItemType(pArray);
	iIndex = xrtTypedArrayFind(pArray, Scratch.Value);
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return iIndex;
}



/* 判断类型数组是否包含与动态值等价的元素。 */
XRT_API bool xrtTypedArrayContainsValue(
	const xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	return xrtTypedArrayFindValue(pArray, pValue, pConverter) != SIZE_MAX;
}

/* 清理失败的临时类型数组并恢复根错误。 */
static void __xrtTypedValueArrayDestroy(xtypedarray* pArray)
{
	xerror* pError = xrtTakeError();

	xrtTypedArrayDestroy(pArray);
	__xrtTypedValueRestoreError(pError);
}



/* 从动态稠密数组构造同构类型数组。 */
XRT_API xtypedarray* xrtTypedArrayFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	xvalueiter Iterator;
	xvaluekey Key;
	xtypedarray* pResult;
	xvalue* pItem;
	xvalueiterresult IterResult;
	size_t iCount;
	size_t iIndex = 0u;

	if ( (pSource == NULL) || (xrtValueType(pSource) != XVALUE_ARRAY) ) {
		__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-from-value", "the source Value is not an array");
		return NULL;
	}
	pResult = xrtTypedArrayCreate(pItemType);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-from-value", "the typed array could not be created");
		return NULL;
	}
	iCount = xrtValueCount(pSource);
	if ( !xrtTypedArrayReserve(pResult, iCount) ||
		 !__xrtTypedValueScratchCreate(pItemType, &Scratch) ) {
		__xrtTypedValueArrayDestroy(pResult);
		return NULL;
	}
	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pSource, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-from-value", "the dynamic array snapshot could not be started");
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueArrayDestroy(pResult);
		return NULL;
	}
	while ( (IterResult = xrtValueIterAdvance(
		&Iterator, &Key, &pItem
	)) == XVALUE_ITER_ITEM ) {
		if ( (Key.Type != XVALUE_KEY_INDEX) || (Key.Index != iIndex) ) {
			__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"array-from-value", "the dynamic array snapshot returned an invalid index");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueArrayDestroy(pResult);
			return NULL;
		}
		if ( !xrtValueToTyped(
			pItem, pItemType, Scratch.Value, pConverter
		) ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"array-from-value", "an array item could not be converted");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueArrayDestroy(pResult);
			return NULL;
		}
		if ( !xrtTypedArrayPush(pResult, Scratch.Value) ) {
			__xrtTypedValueDropPreserveError(pItemType, Scratch.Value);
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"array-from-value", "a converted item could not be appended");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueArrayDestroy(pResult);
			return NULL;
		}
		xrtTypeDropValue(pItemType, Scratch.Value);
		iIndex++;
	}
	if ( IterResult == XVALUE_ITER_ERROR ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-from-value", "the dynamic array snapshot iteration failed");
		__xrtTypedValueIteratorEnd(&Iterator);
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueArrayDestroy(pResult);
		return NULL;
	}
	__xrtTypedValueIteratorEnd(&Iterator);
	__xrtTypedValueScratchDestroy(&Scratch);
	return pResult;
}



/* 把类型数组编码为动态稠密数组。 */
XRT_API xvalue* xrtTypedArrayToValue(
	const xtypedarray* pArray,
	const xvalueconverter* pConverter
)
{
	xvalue* pResult;
	const xrttype* pItemType = xrtTypedArrayItemType(pArray);
	size_t iCount;

	if ( pItemType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			"array-to-value", "the typed array is invalid");
		return NULL;
	}
	pResult = xrtValueArray();
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONTAINER,
			"array-to-value", "the dynamic array could not be created");
		return NULL;
	}
	iCount = xrtTypedArrayCount(pArray);
	if ( !xrtValueReserve(pResult, iCount) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-to-value", "the dynamic array capacity could not be reserved");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const void* pTyped = xrtTypedArrayConstGet(pArray, i);
		xvalue* pItem;

		if ( pTyped == NULL ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"array-to-value", "a typed array item could not be borrowed");
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		__xrtTypedArrayCallbackBegin(pArray);
		pItem = xrtValueFromTyped(pItemType, pTyped, pConverter);
		__xrtTypedArrayCallbackEnd(pArray);
		if ( pItem == NULL ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"array-to-value", "a typed array item could not be converted");
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		if ( !xrtValueArrayAppendNew(pResult, pItem) ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"array-to-value", "a dynamic array item could not be appended");
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
	}
	return pResult;
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)

/* 在列表回调门内把动态值解码为临时元素。 */
static bool __xrtTypedValueListDecode(
	const xtypedlist* pList,
	const xvalue* pValue,
	const xvalueconverter* pConverter,
	__xrt_typed_value_scratch* pScratch,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedListItemType(pList);
	bool bResult;

	if ( pType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed list is invalid");
		return false;
	}
	__xrtTypedListCallbackBegin(pList);
	bResult = __xrtTypedValueScratchDecode(
		pType, pValue, pConverter, pScratch, sOperation
	);
	__xrtTypedListCallbackEnd(pList);
	return bResult;
}



/* 在列表回调门内把借用元素编码为独立动态值。 */
static xvalue* __xrtTypedValueListEncode(
	const xtypedlist* pList,
	const void* pItem,
	const xvalueconverter* pConverter,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedListItemType(pList);
	xvalue* pResult;

	__xrtTypedListCallbackBegin(pList);
	pResult = xrtValueFromTyped(pType, pItem, pConverter);
	__xrtTypedListCallbackEnd(pList);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the typed list item could not be converted");
	}
	return pResult;
}



/* 把一个动态值转换后写入类型列表的指定整数键。 */
XRT_API bool xrtTypedListSetValue(
	xtypedlist* pList,
	int64 iKey,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueListDecode(
		pList, pValue, pConverter, &Scratch, "list-set-value"
	) ) {
		return false;
	}
	pType = xrtTypedListItemType(pList);
	bResult = xrtTypedListSet(pList, iKey, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-set-value", "the converted list item could not be stored");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把一个动态值转换后追加到最大键之后。 */
XRT_API bool xrtTypedListAppendValue(
	xtypedlist* pList,
	const xvalue* pValue,
	int64* pKey,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtTypedValueListDecode(
		pList, pValue, pConverter, &Scratch, "list-append-value"
	) ) {
		return false;
	}
	pType = xrtTypedListItemType(pList);
	bResult = xrtTypedListAppend(pList, Scratch.Value, pKey);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-append-value", "the converted list item could not be appended");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把指定整数键的类型值转换为独立动态值。 */
XRT_API xvalue* xrtTypedListGetValue(
	const xtypedlist* pList,
	int64 iKey,
	const xvalueconverter* pConverter
)
{
	const void* pItem = xrtTypedListConstGet(pList, iKey);

	return pItem != NULL ? __xrtTypedValueListEncode(
		pList, pItem, pConverter, "list-get-value"
	) : NULL;
}



/* 转换并删除指定整数键的类型值。 */
XRT_API xvalue* xrtTypedListTakeValue(
	xtypedlist* pList,
	int64 iKey,
	const xvalueconverter* pConverter
)
{
	xvalue* pResult = xrtTypedListGetValue(pList, iKey, pConverter);

	if ( pResult == NULL ) {
		return NULL;
	}
	if ( !xrtTypedListRemove(pList, iKey) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-take-value", "the converted list item could not be removed");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	return pResult;
}



/* 把动态值转换为元素类型并查找第一处相等值。 */
XRT_API bool xrtTypedListFindValue(
	const xtypedlist* pList,
	const xvalue* pValue,
	int64* pKey,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtTypedValueListDecode(
		pList, pValue, pConverter, &Scratch, "list-find-value"
	) ) {
		return false;
	}
	pType = xrtTypedListItemType(pList);
	bResult = xrtTypedListFind(pList, Scratch.Value, pKey);
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 判断类型列表是否包含与动态值等价的元素。 */
XRT_API bool xrtTypedListContainsValue(
	const xtypedlist* pList,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	return xrtTypedListFindValue(pList, pValue, NULL, pConverter);
}



/* 清理失败的临时类型列表并恢复根错误。 */
static void __xrtTypedValueListDestroy(xtypedlist* pList)
{
	xerror* pError = xrtTakeError();

	xrtTypedListDestroy(pList);
	__xrtTypedValueRestoreError(pError);
}



/* 从动态整数映射构造同构稀疏类型列表。 */
XRT_API xtypedlist* xrtTypedListFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	xvalueiter Iterator;
	xvaluekey Key;
	xtypedlist* pResult;
	xvalue* pItem;
	xvalueiterresult IterResult;

	if ( (pSource == NULL) || (xrtValueType(pSource) != XVALUE_INT_MAP) ) {
		__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-from-value", "the source Value is not an integer map");
		return NULL;
	}
	pResult = xrtTypedListCreate(pItemType);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-from-value", "the typed list could not be created");
		return NULL;
	}
	if ( !__xrtTypedValueScratchCreate(pItemType, &Scratch) ) {
		__xrtTypedValueListDestroy(pResult);
		return NULL;
	}
	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pSource, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-from-value", "the integer map snapshot could not be started");
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueListDestroy(pResult);
		return NULL;
	}
	while ( (IterResult = xrtValueIterAdvance(
		&Iterator, &Key, &pItem
	)) == XVALUE_ITER_ITEM ) {
		if ( Key.Type != XVALUE_KEY_INT ) {
			__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"list-from-value", "the integer map iterator returned a non-integer key");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueListDestroy(pResult);
			return NULL;
		}
		if ( !xrtValueToTyped(
			pItem, pItemType, Scratch.Value, pConverter
		) ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"list-from-value", "a list item could not be converted");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueListDestroy(pResult);
			return NULL;
		}
		if ( !xrtTypedListSet(
			pResult, Key.Integer, Scratch.Value
		) ) {
			__xrtTypedValueDropPreserveError(pItemType, Scratch.Value);
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"list-from-value", "a converted list item could not be stored");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueListDestroy(pResult);
			return NULL;
		}
		xrtTypeDropValue(pItemType, Scratch.Value);
	}
	if ( IterResult == XVALUE_ITER_ERROR ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-from-value", "the integer map snapshot iteration failed");
		__xrtTypedValueIteratorEnd(&Iterator);
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueListDestroy(pResult);
		return NULL;
	}
	__xrtTypedValueIteratorEnd(&Iterator);
	__xrtTypedValueScratchDestroy(&Scratch);
	return pResult;
}



/* 把稀疏类型列表编码为动态整数映射。 */
XRT_API xvalue* xrtTypedListToValue(
	const xtypedlist* pList,
	const xvalueconverter* pConverter
)
{
	xtypedlistiter Iterator;
	const xrttype* pItemType = xrtTypedListItemType(pList);
	xvalue* pResult;
	ptr pItem;
	int64 iKey;

	if ( pItemType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			"list-to-value", "the typed list is invalid");
		return NULL;
	}
	pResult = xrtValueIntMap();
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONTAINER,
			"list-to-value", "the dynamic integer map could not be created");
		return NULL;
	}
	if ( !xrtTypedListIterBegin((xtypedlist*)pList, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-to-value", "the typed list iterator could not be started");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	while ( (pItem = xrtTypedListIterNext(&Iterator, &iKey)) != NULL ) {
		xvalue* pValue;

		__xrtTypedListCallbackBegin(pList);
		pValue = xrtValueFromTyped(pItemType, pItem, pConverter);
		__xrtTypedListCallbackEnd(pList);
		if ( pValue == NULL ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"list-to-value", "a typed list item could not be converted");
			xrtTypedListIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		if ( !xrtValueIntMapSetNew(pResult, iKey, pValue) ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"list-to-value", "a dynamic integer map item could not be stored");
			xrtTypedListIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
	}
	xrtTypedListIterEnd(&Iterator);
	return pResult;
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)

/* 在集合回调门内把动态值解码为临时元素。 */
static bool __xrtTypedValueSetDecode(
	const xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter,
	__xrt_typed_value_scratch* pScratch,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedSetItemType(pSet);
	bool bResult;

	if ( pType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed set is invalid");
		return false;
	}
	if ( !__xrtTypedSetCallbackBegin(pSet) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed set callback gate could not be entered");
		return false;
	}
	bResult = __xrtTypedValueScratchDecode(
		pType, pValue, pConverter, pScratch, sOperation
	);
	__xrtTypedSetCallbackEnd(pSet);
	return bResult;
}



/* 在集合回调门内把规范元素编码为独立动态值。 */
static xvalue* __xrtTypedValueSetEncode(
	const xtypedset* pSet,
	const void* pItem,
	const xvalueconverter* pConverter,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedSetItemType(pSet);
	xvalue* pResult;

	if ( !__xrtTypedSetCallbackBegin(pSet) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed set callback gate could not be entered");
		return NULL;
	}
	pResult = xrtValueFromTyped(pType, pItem, pConverter);
	__xrtTypedSetCallbackEnd(pSet);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the typed set item could not be converted");
	}
	return pResult;
}



/* 把一个动态值转换后加入类型集合。 */
XRT_API bool xrtTypedSetAddValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueSetDecode(
		pSet, pValue, pConverter, &Scratch, "set-add-value"
	) ) {
		return false;
	}
	pType = xrtTypedSetItemType(pSet);
	bResult = xrtTypedSetAdd(pSet, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-add-value", "the converted set item could not be added");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 返回与动态值等价的规范元素副本。 */
XRT_API xvalue* xrtTypedSetGetValue(
	const xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	const void* pStored;
	xvalue* pResult = NULL;

	if ( !__xrtTypedValueSetDecode(
		pSet, pValue, pConverter, &Scratch, "set-get-value"
	) ) {
		return NULL;
	}
	pType = xrtTypedSetItemType(pSet);
	pStored = xrtTypedSetGet(pSet, Scratch.Value);
	if ( pStored != NULL ) {
		pResult = __xrtTypedValueSetEncode(
			pSet, pStored, pConverter, "set-get-value"
		);
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return pResult;
}



/* 判断类型集合是否拥有与动态值等价的元素。 */
XRT_API bool xrtTypedSetHasValue(
	const xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueSetDecode(
		pSet, pValue, pConverter, &Scratch, "set-has-value"
	) ) {
		return false;
	}
	pType = xrtTypedSetItemType(pSet);
	bResult = xrtTypedSetHas(pSet, Scratch.Value);
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 删除与动态值等价的元素。 */
XRT_API bool xrtTypedSetRemoveValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueSetDecode(
		pSet, pValue, pConverter, &Scratch, "set-remove-value"
	) ) {
		return false;
	}
	pType = xrtTypedSetItemType(pSet);
	bResult = xrtTypedSetRemove(pSet, Scratch.Value);
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 转换并删除规范元素。 */
XRT_API xvalue* xrtTypedSetTakeValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	const void* pStored;
	xvalue* pResult = NULL;

	if ( !__xrtTypedValueSetDecode(
		pSet, pValue, pConverter, &Scratch, "set-take-value"
	) ) {
		return NULL;
	}
	pType = xrtTypedSetItemType(pSet);
	pStored = xrtTypedSetGet(pSet, Scratch.Value);
	if ( pStored != NULL ) {
		pResult = __xrtTypedValueSetEncode(
			pSet, pStored, pConverter, "set-take-value"
		);
		if ( (pResult != NULL) &&
			 !xrtTypedSetRemove(pSet, Scratch.Value) ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"set-take-value", "the converted set item could not be removed");
			__xrtTypedValueRelease(pResult);
			pResult = NULL;
		}
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return pResult;
}



/* 清理失败的临时类型集合并恢复根错误。 */
static void __xrtTypedValueSetDestroy(xtypedset* pSet)
{
	xerror* pError = xrtTakeError();

	xrtTypedSetDestroy(pSet);
	__xrtTypedValueRestoreError(pError);
}



/* 从动态集合构造同构类型集合。 */
XRT_API xtypedset* xrtTypedSetFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	xvalueiter Iterator;
	xtypedset* pResult;
	xvalue* pItem;
	xvalueiterresult IterResult;

	if ( (pSource == NULL) || (xrtValueType(pSource) != XVALUE_SET) ) {
		__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-from-value", "the source Value is not a set");
		return NULL;
	}
	pResult = xrtTypedSetCreate(pItemType);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-from-value", "the typed set could not be created");
		return NULL;
	}
	if ( !xrtTypedSetReserve(pResult, xrtValueCount(pSource)) ||
		 !__xrtTypedValueScratchCreate(pItemType, &Scratch) ) {
		__xrtTypedValueSetDestroy(pResult);
		return NULL;
	}
	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pSource, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-from-value", "the dynamic set snapshot could not be started");
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueSetDestroy(pResult);
		return NULL;
	}
	while ( (IterResult = xrtValueIterAdvance(
		&Iterator, NULL, &pItem
	)) == XVALUE_ITER_ITEM ) {
		if ( !xrtValueToTyped(
			pItem, pItemType, Scratch.Value, pConverter
		) ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"set-from-value", "a set item could not be converted");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueSetDestroy(pResult);
			return NULL;
		}
		if ( !xrtTypedSetAdd(pResult, Scratch.Value) ) {
			__xrtTypedValueDropPreserveError(pItemType, Scratch.Value);
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"set-from-value", "a converted set item could not be stored");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueSetDestroy(pResult);
			return NULL;
		}
		xrtTypeDropValue(pItemType, Scratch.Value);
	}
	if ( IterResult == XVALUE_ITER_ERROR ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-from-value", "the dynamic set snapshot iteration failed");
		__xrtTypedValueIteratorEnd(&Iterator);
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueSetDestroy(pResult);
		return NULL;
	}
	__xrtTypedValueIteratorEnd(&Iterator);
	__xrtTypedValueScratchDestroy(&Scratch);
	return pResult;
}



/* 把类型集合编码为动态集合。 */
XRT_API xvalue* xrtTypedSetToValue(
	const xtypedset* pSet,
	const xvalueconverter* pConverter
)
{
	xtypedsetiter Iterator;
	const xrttype* pItemType = xrtTypedSetItemType(pSet);
	xvalue* pResult;
	const void* pItem;

	if ( pItemType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			"set-to-value", "the typed set is invalid");
		return NULL;
	}
	pResult = xrtValueSet();
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONTAINER,
			"set-to-value", "the dynamic set could not be created");
		return NULL;
	}
	if ( !xrtValueReserve(pResult, xrtTypedSetCount(pSet)) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-to-value", "the dynamic set capacity could not be reserved");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	if ( !xrtTypedSetIterBegin((xtypedset*)pSet, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-to-value", "the typed set iterator could not be started");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	while ( (pItem = xrtTypedSetIterNext(&Iterator)) != NULL ) {
		xvalue* pValue;

		if ( !__xrtTypedSetCallbackBegin(pSet) ) {
			__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"set-to-value", "the typed set callback gate could not be entered");
			xrtTypedSetIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		pValue = xrtValueFromTyped(pItemType, pItem, pConverter);
		__xrtTypedSetCallbackEnd(pSet);
		if ( pValue == NULL ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"set-to-value", "a typed set item could not be converted");
			xrtTypedSetIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		if ( !xrtValueSetAddNew(pResult, pValue) ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"set-to-value", "a dynamic set item could not be stored");
			xrtTypedSetIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
	}
	xrtTypedSetIterEnd(&Iterator);
	return pResult;
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 在字典回调门内把动态值解码为临时元素。 */
static bool __xrtTypedValueDictDecode(
	const xtypeddict* pDict,
	const xvalue* pValue,
	const xvalueconverter* pConverter,
	__xrt_typed_value_scratch* pScratch,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedDictItemType(pDict);
	bool bResult;

	if ( pType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed dictionary is invalid");
		return false;
	}
	if ( !__xrtTypedDictCallbackBegin(pDict) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed dictionary callback gate could not be entered");
		return false;
	}
	bResult = __xrtTypedValueScratchDecode(
		pType, pValue, pConverter, pScratch, sOperation
	);
	__xrtTypedDictCallbackEnd(pDict);
	return bResult;
}



/* 在字典回调门内把借用值编码为独立动态值。 */
static xvalue* __xrtTypedValueDictEncode(
	const xtypeddict* pDict,
	const void* pItem,
	const xvalueconverter* pConverter,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedDictItemType(pDict);
	xvalue* pResult;

	if ( !__xrtTypedDictCallbackBegin(pDict) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed dictionary callback gate could not be entered");
		return NULL;
	}
	pResult = xrtValueFromTyped(pType, pItem, pConverter);
	__xrtTypedDictCallbackEnd(pDict);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the typed dictionary item could not be converted");
	}
	return pResult;
}



/* 把一个动态值转换后写入类型字典的指定文本键。 */
XRT_API bool xrtTypedDictSetValue(
	xtypeddict* pDict,
	xstrview Key,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueDictDecode(
		pDict, pValue, pConverter, &Scratch, "dict-set-value"
	) ) {
		return false;
	}
	pType = xrtTypedDictItemType(pDict);
	bResult = xrtTypedDictSet(pDict, Key, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-set-value", "the converted dictionary item could not be stored");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把指定文本键的类型值转换为独立动态值。 */
XRT_API xvalue* xrtTypedDictGetValue(
	const xtypeddict* pDict,
	xstrview Key,
	const xvalueconverter* pConverter
)
{
	const void* pItem = xrtTypedDictConstGet(pDict, Key);

	return pItem != NULL ? __xrtTypedValueDictEncode(
		pDict, pItem, pConverter, "dict-get-value"
	) : NULL;
}



/* 转换并删除指定文本键的类型值。 */
XRT_API xvalue* xrtTypedDictTakeValue(
	xtypeddict* pDict,
	xstrview Key,
	const xvalueconverter* pConverter
)
{
	xvalue* pResult = xrtTypedDictGetValue(pDict, Key, pConverter);

	if ( pResult == NULL ) {
		return NULL;
	}
	if ( !xrtTypedDictRemove(pDict, Key) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-take-value", "the converted dictionary item could not be removed");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	return pResult;
}



/* 清理失败的临时类型字典并恢复根错误。 */
static void __xrtTypedValueDictDestroy(xtypeddict* pDict)
{
	xerror* pError = xrtTakeError();

	xrtTypedDictDestroy(pDict);
	__xrtTypedValueRestoreError(pError);
}



/* 从动态字符串键对象构造同构类型字典。 */
XRT_API xtypeddict* xrtTypedDictFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	xvalueiter Iterator;
	xvaluekey Key;
	xtypeddict* pResult;
	xvalue* pItem;
	xvalueiterresult IterResult;

	if ( (pSource == NULL) || (xrtValueType(pSource) != XVALUE_OBJECT) ) {
		__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-from-value", "the source Value is not an object");
		return NULL;
	}
	pResult = xrtTypedDictCreate(pItemType);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-from-value", "the typed dictionary could not be created");
		return NULL;
	}
	if ( !xrtTypedDictReserve(pResult, xrtValueCount(pSource)) ||
		 !__xrtTypedValueScratchCreate(pItemType, &Scratch) ) {
		__xrtTypedValueDictDestroy(pResult);
		return NULL;
	}
	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pSource, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-from-value", "the dynamic object snapshot could not be started");
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueDictDestroy(pResult);
		return NULL;
	}
	while ( (IterResult = xrtValueIterAdvance(
		&Iterator, &Key, &pItem
	)) == XVALUE_ITER_ITEM ) {
		if ( Key.Type != XVALUE_KEY_STRING ) {
			__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"dict-from-value", "the object iterator returned a non-string key");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueDictDestroy(pResult);
			return NULL;
		}
		if ( !xrtValueToTyped(
			pItem, pItemType, Scratch.Value, pConverter
		) ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"dict-from-value", "a dictionary item could not be converted");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueDictDestroy(pResult);
			return NULL;
		}
		if ( !xrtTypedDictSet(
			pResult, Key.String, Scratch.Value
		) ) {
			__xrtTypedValueDropPreserveError(pItemType, Scratch.Value);
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"dict-from-value", "a converted dictionary item could not be stored");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueDictDestroy(pResult);
			return NULL;
		}
		xrtTypeDropValue(pItemType, Scratch.Value);
	}
	if ( IterResult == XVALUE_ITER_ERROR ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-from-value", "the dynamic object snapshot iteration failed");
		__xrtTypedValueIteratorEnd(&Iterator);
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueDictDestroy(pResult);
		return NULL;
	}
	__xrtTypedValueIteratorEnd(&Iterator);
	__xrtTypedValueScratchDestroy(&Scratch);
	return pResult;
}



/* 把类型字典编码为动态字符串键对象。 */
XRT_API xvalue* xrtTypedDictToValue(
	const xtypeddict* pDict,
	const xvalueconverter* pConverter
)
{
	xtypeddictiter Iterator;
	const xrttype* pItemType = xrtTypedDictItemType(pDict);
	xvalue* pResult;
	xstrview Key;
	ptr pItem;

	if ( pItemType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-to-value", "the typed dictionary is invalid");
		return NULL;
	}
	pResult = xrtValueObject();
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-to-value", "the dynamic object could not be created");
		return NULL;
	}
	if ( !xrtValueReserve(pResult, xrtTypedDictCount(pDict)) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-to-value", "the dynamic object capacity could not be reserved");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	if ( !xrtTypedDictIterBegin((xtypeddict*)pDict, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-to-value", "the typed dictionary iterator could not be started");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	while ( (pItem = xrtTypedDictIterNext(&Iterator, &Key)) != NULL ) {
		xvalue* pValue;

		if ( !__xrtTypedDictCallbackBegin(pDict) ) {
			__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"dict-to-value", "the typed dictionary callback gate could not be entered");
			xrtTypedDictIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		pValue = xrtValueFromTyped(pItemType, pItem, pConverter);
		__xrtTypedDictCallbackEnd(pDict);
		if ( pValue == NULL ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"dict-to-value", "a typed dictionary item could not be converted");
			xrtTypedDictIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		if ( !xrtValueObjectSetNew(pResult, Key, pValue) ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"dict-to-value", "a dynamic object item could not be stored");
			xrtTypedDictIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
	}
	xrtTypedDictIterEnd(&Iterator);
	return pResult;
}

#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_type_future.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE)



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE)

/* 初始化一个拥有 Future 引用的槽位为空。 */
static bool __xrtRuntimeTypeFutureInit(
	ptr pValue,
	const xrttype* pType
)
{
	xfuture* pEmpty = NULL;
	(void)pType;

	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	return true;
}



/* 增加源 Future 引用，成功后替换目标槽。 */
static bool __xrtRuntimeTypeFutureCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	xfuture* pSourceFuture;
	xfuture* pTargetFuture;
	xfuture* pReference = NULL;
	(void)pType;

	memcpy(&pSourceFuture, pSource, sizeof(pSourceFuture));
	if ( pSourceFuture != NULL ) {
		pReference = xrtFutureRef(pSourceFuture);
		if ( pReference == NULL ) {
			return false;
		}
	}
	memcpy(&pTargetFuture, pTarget, sizeof(pTargetFuture));
	memcpy(pTarget, &pReference, sizeof(pReference));
	xrtFutureDestroy(pTargetFuture);
	return true;
}



/* 移交 Future 引用，清空源槽并释放目标旧引用。 */
static bool __xrtRuntimeTypeFutureMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	xfuture* pSourceFuture;
	xfuture* pTargetFuture;
	xfuture* pEmpty = NULL;
	(void)pType;

	memcpy(&pSourceFuture, pSource, sizeof(pSourceFuture));
	memcpy(&pTargetFuture, pTarget, sizeof(pTargetFuture));
	memcpy(pTarget, &pSourceFuture, sizeof(pSourceFuture));
	memcpy(pSource, &pEmpty, sizeof(pEmpty));
	xrtFutureDestroy(pTargetFuture);
	return true;
}



/* 释放槽位拥有的 Future 引用并恢复为空。 */
static void __xrtRuntimeTypeFutureDrop(
	ptr pValue,
	const xrttype* pType
)
{
	xfuture* pFuture;
	xfuture* pEmpty = NULL;
	(void)pType;

	memcpy(&pFuture, pValue, sizeof(pFuture));
	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	xrtFutureDestroy(pFuture);
}



/* Future 槽按进程内稳定身份比较。 */
static int __xrtRuntimeTypeFutureCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	xfuture* pLeftFuture;
	xfuture* pRightFuture;
	uintptr_t iLeft;
	uintptr_t iRight;
	(void)pType;

	memcpy(&pLeftFuture, pLeft, sizeof(pLeftFuture));
	memcpy(&pRightFuture, pRight, sizeof(pRightFuture));
	iLeft = (uintptr_t)pLeftFuture;
	iRight = (uintptr_t)pRightFuture;
	return (iLeft > iRight) - (iLeft < iRight);
}



/* Future 槽按进程内稳定身份散列。 */
static uint64 __xrtRuntimeTypeFutureHash(
	const void* pValue,
	const xrttype* pType
)
{
	xfuture* pFuture;
	(void)pType;

	memcpy(&pFuture, pValue, sizeof(pFuture));
	return (uint64)(uintptr_t)pFuture;
}



static const xrttypeops __xrtRuntimeTypeFutureOps = {
	.Init = __xrtRuntimeTypeFutureInit,
	.Copy = __xrtRuntimeTypeFutureCopy,
	.Move = __xrtRuntimeTypeFutureMove,
	.Drop = __xrtRuntimeTypeFutureDrop,
	.Clone = __xrtRuntimeTypeFutureCopy,
	.Compare = __xrtRuntimeTypeFutureCompare,
	.Hash = __xrtRuntimeTypeFutureHash
};



static const xrttype __xrtRuntimeTypeFuture = {
	.Id = UINT64_C(0x144E843036511A0E),
	.Kind = XRT_TYPE_FUTURE,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_REFERENCE |
		XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL |
		XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("future"),
	.AbiName = XRT_STR_INIT("xrt.future"),
	.Size = sizeof(xfuture*),
	.Align = XRT_INTERNAL_ALIGNOF(xfuture*),
	.InstanceSize = 0u,
	.InstanceAlign = 1u,
	.Ops = &__xrtRuntimeTypeFutureOps
};



/* 返回 Future 消费端引用槽的稳定运行时类型。 */
XRT_API const xrttype* xrtTypeFuture(void)
{
	return &__xrtRuntimeTypeFuture;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_field.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_FIELD)



#if defined(XRUNTIME_FEATURE_RUNTIME_FIELD)

#define XRT_FIELD_FLAGS XRT_FIELD_FLAG_READONLY



/* 设置运行时字段模块结构化错误。 */
static void __xrtFieldError(
	xerrkind Kind,
	xfielderror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.field";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型描述错误补充字段上下文并保留原始原因。 */
static void __xrtFieldWrap(
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_ARGUMENT;
	Desc.Domain = "xrt.field";
	Desc.Code = XFIELD_ERROR_DESCRIPTOR;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 检查字段表的借用数组形态。 */
static bool __xrtFieldTableShapeValid(const xrtfieldtable* pFields)
{
	return (pFields == NULL) ||
		(pFields->Count == 0u) ||
		(pFields->Fields != NULL);
}



/* 检查字段在声明类型中的边界、对齐和基础属性。 */
static bool __xrtFieldShapeValid(
	const xrttype* pOwner,
	const xrtfielddesc* pField
)
{
	size_t iMinimumOffset = pOwner->Base != NULL ?
		pOwner->Base->InstanceSize : 0u;

	if (
		!__xrtTypeViewValid(&pField->Name, false) ||
		(pField->Type == NULL) ||
		((pField->Flags & ~XRT_FIELD_FLAGS) != 0u) ||
		(pField->Offset < iMinimumOffset) ||
		(pField->Offset > pOwner->InstanceSize) ||
		(pField->Type->Align == 0u) ||
		((pField->Type->Align & (pField->Type->Align - 1u)) != 0u) ||
		(pOwner->InstanceAlign < pField->Type->Align) ||
		((pField->Offset % pField->Type->Align) != 0u)
	) {
		return false;
	}
	return pField->Type->Size <=
		(pOwner->InstanceSize - pField->Offset);
}



/* 判断两个非空存储区间是否重叠。 */
static bool __xrtFieldOverlaps(
	const xrtfielddesc* pLeft,
	const xrtfielddesc* pRight
)
{
	if ( (pLeft->Type->Size == 0u) || (pRight->Type->Size == 0u) ) {
		return false;
	}
	return (pLeft->Offset < (pRight->Offset + pRight->Type->Size)) &&
		(pRight->Offset < (pLeft->Offset + pLeft->Type->Size));
}



/* 验证一个声明类型的局部字段，不检查继承重名。 */
static bool __xrtFieldOwnerValidate(const xrttype* pOwner)
{
	const xrtfieldtable* pTable = pOwner->Fields;

	if ( !__xrtFieldTableShapeValid(pTable) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
			"validate", "a field table has no descriptor array");
		return false;
	}
	if ( pTable == NULL ) {
		return true;
	}
	for ( size_t i = 0; i < pTable->Count; i++ ) {
		const xrtfielddesc* pField = &pTable->Fields[i];

		if ( !__xrtFieldShapeValid(pOwner, pField) ) {
			__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
				"validate", "a field descriptor has an invalid name, layout, or flag");
			return false;
		}
		if ( !xrtTypeValidate(pField->Type) ) {
			__xrtFieldWrap("validate", "a field refers to an invalid runtime type");
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			const xrtfielddesc* pPrevious = &pTable->Fields[j];

			if ( __xrtTypeViewEqual(&pPrevious->Name, &pField->Name) ) {
				__xrtFieldError(XERR_EXISTS, XFIELD_ERROR_DESCRIPTOR,
					"validate", "field names must be unique within a type");
				return false;
			}
			if ( __xrtFieldOverlaps(pPrevious, pField) ) {
				__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
					"validate", "field storage ranges must not overlap");
				return false;
			}
		}
	}
	return true;
}



/* 在已经验证的基类链中查询字段名称。 */
static bool __xrtFieldBaseHasName(
	const xrttype* pBase,
	const xstrview* pName
)
{
	while ( pBase != NULL ) {
		const xrtfieldtable* pTable = pBase->Fields;

		if ( pTable != NULL ) {
			for ( size_t i = 0; i < pTable->Count; i++ ) {
				if ( __xrtTypeViewEqual(
					&pTable->Fields[i].Name, pName
				) ) {
					return true;
				}
			}
		}
		pBase = pBase->Base;
	}
	return false;
}



/* 返回继承链字段总数，并拒绝损坏的表或计数溢出。 */
static bool __xrtFieldCount(
	const xrttype* pType,
	size_t* pCount,
	cstr sOperation
)
{
	size_t iCount = 0u;
	uint32 iDepth = 0u;

	if ( (pType == NULL) || (pCount == NULL) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_LOOKUP,
			sOperation, "the runtime type descriptor is null");
		return false;
	}
	while ( (pType != NULL) &&
			(iDepth < XRT_RUNTIME_TYPE_INHERITANCE_MAX) ) {
		const xrtfieldtable* pTable = pType->Fields;

		if ( !__xrtFieldTableShapeValid(pTable) ) {
			__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
				sOperation, "a field table has no descriptor array");
			return false;
		}
		if ( (pTable != NULL) &&
			 (pTable->Count > (SIZE_MAX - iCount)) ) {
			__xrtFieldError(XERR_RANGE, XFIELD_ERROR_DESCRIPTOR,
				sOperation, "the inherited field count overflows");
			return false;
		}
		if ( pTable != NULL ) {
			iCount += pTable->Count;
		}
		pType = pType->Base;
		iDepth++;
	}
	if ( pType != NULL ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
			sOperation, "the field inheritance chain is cyclic or too deep");
		return false;
	}
	*pCount = iCount;
	return true;
}



/* 按准确描述符地址查找声明类型，不读取不受信任的字段内容。 */
static const xrttype* __xrtFieldOwner(
	const xrttype* pType,
	const xrtfielddesc* pField,
	cstr sOperation
)
{
	uint32 iDepth = 0u;

	if ( (pType == NULL) || (pField == NULL) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_ACCESS,
			sOperation, "the runtime type or field descriptor is null");
		return NULL;
	}
	while ( (pType != NULL) &&
			(iDepth < XRT_RUNTIME_TYPE_INHERITANCE_MAX) ) {
		const xrtfieldtable* pTable = pType->Fields;

		if ( !__xrtFieldTableShapeValid(pTable) ) {
			__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
				sOperation, "a field table has no descriptor array");
			return NULL;
		}
		if ( pTable != NULL ) {
			for ( size_t i = 0; i < pTable->Count; i++ ) {
				if ( &pTable->Fields[i] == pField ) {
					return pType;
				}
			}
		}
		pType = pType->Base;
		iDepth++;
	}
	__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_ACCESS,
		sOperation, pType != NULL ?
		"the field inheritance chain is cyclic or too deep" :
		"the field descriptor does not belong to the runtime type");
	return NULL;
}



/* 验证完整字段继承链，并禁止字段隐藏与基类负载重叠。 */
XRT_API bool xrtTypeFieldsValidate(const xrttype* pType)
{
	const xrttype* arrTypes[XRT_RUNTIME_TYPE_INHERITANCE_MAX];
	size_t iDepth = 0u;

	if ( !xrtTypeValidate(pType) ) {
		__xrtFieldWrap("validate", "the field owner type is invalid");
		return false;
	}
	if ( (pType->Kind != XRT_TYPE_CLASS) &&
		 (pType->Kind != XRT_TYPE_RECORD) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
			"validate", "only class and record types can declare fields");
		return false;
	}
	while ( pType != NULL ) {
		if ( iDepth >= XRT_RUNTIME_TYPE_INHERITANCE_MAX ) {
			__xrtFieldError(XERR_RANGE, XFIELD_ERROR_DESCRIPTOR,
				"validate", "the field inheritance chain exceeds the local depth limit");
			return false;
		}
		arrTypes[iDepth++] = pType;
		pType = pType->Base;
	}
	for ( size_t i = iDepth; i != 0u; i-- ) {
		const xrttype* pOwner = arrTypes[i - 1u];
		const xrtfieldtable* pTable = pOwner->Fields;

		if ( !__xrtFieldOwnerValidate(pOwner) ) {
			return false;
		}
		if ( (pTable == NULL) || (pOwner->Base == NULL) ) {
			continue;
		}
		for ( size_t j = 0; j < pTable->Count; j++ ) {
			if ( __xrtFieldBaseHasName(
				pOwner->Base, &pTable->Fields[j].Name
			) ) {
				__xrtFieldError(XERR_EXISTS, XFIELD_ERROR_DESCRIPTOR,
					"validate", "derived fields must not hide inherited fields");
				return false;
			}
		}
	}
	return true;
}



/* 返回继承链中的字段总数。 */
XRT_API size_t xrtTypeFieldCount(const xrttype* pType)
{
	size_t iCount;

	return __xrtFieldCount(pType, &iCount, "count") ? iCount : 0u;
}



/* 按基类优先顺序返回指定下标的字段。 */
XRT_API const xrtfielddesc* xrtTypeField(
	const xrttype* pType,
	size_t iIndex
)
{
	const xrttype* arrTypes[XRT_RUNTIME_TYPE_INHERITANCE_MAX];
	const xrttype* pCursor = pType;
	size_t iCount;
	size_t iDepth = 0u;

	if ( !__xrtFieldCount(pType, &iCount, "field") ) {
		return NULL;
	}
	if ( iIndex >= iCount ) {
		__xrtFieldError(XERR_RANGE, XFIELD_ERROR_LOOKUP,
			"field", "the field index is out of range");
		return NULL;
	}
	while ( pCursor != NULL ) {
		if ( iDepth >= XRT_RUNTIME_TYPE_INHERITANCE_MAX ) {
			__xrtFieldError(XERR_RANGE, XFIELD_ERROR_LOOKUP,
				"field", "the field inheritance chain exceeds the local depth limit");
			return NULL;
		}
		arrTypes[iDepth++] = pCursor;
		pCursor = pCursor->Base;
	}
	for ( size_t i = iDepth; i != 0u; i-- ) {
		const xrtfieldtable* pTable = arrTypes[i - 1u]->Fields;

		if ( pTable == NULL ) {
			continue;
		}
		if ( iIndex < pTable->Count ) {
			return &pTable->Fields[iIndex];
		}
		iIndex -= pTable->Count;
	}
	return NULL;
}



/* 从最具体类型开始按名称查询字段。 */
XRT_API const xrtfielddesc* xrtTypeFindField(
	const xrttype* pType,
	xstrview Name
)
{
	uint32 iDepth = 0u;

	if ( (pType == NULL) || !__xrtTypeViewValid(&Name, false) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_LOOKUP,
			"find", "the runtime type or field name is invalid");
		return NULL;
	}
	while ( (pType != NULL) &&
			(iDepth < XRT_RUNTIME_TYPE_INHERITANCE_MAX) ) {
		const xrtfieldtable* pTable = pType->Fields;

		if ( !__xrtFieldTableShapeValid(pTable) ) {
			__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
				"find", "a field table has no descriptor array");
			return NULL;
		}
		if ( pTable != NULL ) {
			for ( size_t i = 0; i < pTable->Count; i++ ) {
				if ( __xrtTypeViewEqual(&pTable->Fields[i].Name, &Name) ) {
					return &pTable->Fields[i];
				}
			}
		}
		pType = pType->Base;
		iDepth++;
	}
	if ( pType != NULL ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
			"find", "the field inheritance chain is cyclic or too deep");
	}
	return NULL;
}



/* 返回字段在继承链中的声明类型。 */
XRT_API const xrttype* xrtTypeFieldOwner(
	const xrttype* pType,
	const xrtfielddesc* pField
)
{
	return __xrtFieldOwner(pType, pField, "owner");
}



/* 检查字段归属和布局后返回实例内地址。 */
static const void* __xrtFieldData(
	const xrttype* pType,
	const xrtfielddesc* pField,
	const void* pInstance,
	cstr sOperation
)
{
	const xrttype* pOwner = __xrtFieldOwner(pType, pField, sOperation);

	if ( pOwner == NULL ) {
		return NULL;
	}
	if ( pInstance == NULL ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_ACCESS,
			sOperation, "the instance payload is null");
		return NULL;
	}
	if ( !__xrtFieldShapeValid(pOwner, pField) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
			sOperation, "the field layout is invalid");
		return NULL;
	}
	return (const uint8*)pInstance + pField->Offset;
}



/* 返回只读实例中的字段地址。 */
XRT_API const void* xrtFieldConstData(
	const xrttype* pType,
	const xrtfielddesc* pField,
	const void* pInstance
)
{
	return __xrtFieldData(pType, pField, pInstance, "const-data");
}



/* 返回可写实例中的字段地址；只读标志仍由上层策略解释。 */
XRT_API ptr xrtFieldData(
	const xrttype* pType,
	const xrtfielddesc* pField,
	ptr pInstance
)
{
	return (ptr)__xrtFieldData(pType, pField, pInstance, "data");
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_array.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY)



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY)

#define XRT_TYPED_ARRAY_FLAG_READY 0x0001u
#define XRT_TYPED_ARRAY_FLAG_BUSY  0x0002u
#define XRT_TYPED_ARRAY_FLAGS      0x0003u



/* 设置类型数组模块结构化错误。 */
static void __xrtTypedArrayError(
	xerrkind Kind,
	xtypedarrayerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-array";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型或存储错误补充类型数组上下文。 */
static void __xrtTypedArrayWrap(
	xerrkind DefaultKind,
	xtypedarrayerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-array";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 验证元素类型可被连续拥有式数组安全使用。 */
static bool __xrtTypedArrayItemTypeValidate(
	const xrttype* pItemType,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pItemType) ) {
		__xrtTypedArrayWrap(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_TYPE,
			sOperation, "the array item type is invalid");
		return false;
	}
	if ( pItemType->Size == 0u ) {
		__xrtTypedArrayError(XERR_TYPE, XTYPED_ARRAY_ERROR_TYPE,
			sOperation, "a typed array item must occupy storage");
		return false;
	}
	if ( !xrtTypeIsCopyable(pItemType) ) {
		__xrtTypedArrayError(XERR_UNSUPPORTED, XTYPED_ARRAY_ERROR_TYPE,
			sOperation, "the array item type is not copyable");
		return false;
	}
	if ( !xrtTypeIsRelocatable(pItemType) ) {
		__xrtTypedArrayError(XERR_UNSUPPORTED, XTYPED_ARRAY_ERROR_TYPE,
			sOperation, "the array item type is not relocatable");
		return false;
	}
	return true;
}



/* 检查公开类型数组状态与元素布局是否一致。 */
static bool __xrtTypedArrayValid(
	const xtypedarray* pArray,
	cstr sOperation
)
{
	if ( (pArray == NULL) || (pArray->ItemType == NULL) ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the typed array is null or uninitialized");
		return false;
	}
	if (
		((pArray->Flags & XRT_TYPED_ARRAY_FLAG_READY) == 0u) ||
		((pArray->Flags & XRT_TYPED_ARRAY_FLAG_BUSY) != 0u) ||
		((pArray->Flags & ~XRT_TYPED_ARRAY_FLAGS) != 0u)
	) {
		__xrtTypedArrayError(XERR_STATE, XTYPED_ARRAY_ERROR_STATE,
			sOperation, "the typed array is not available for API access");
		return false;
	}
	if ( !__xrtArrayValid(&pArray->Storage) ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_STATE,
			sOperation, "the typed array storage is invalid");
		return false;
	}
	if ( (pArray->Storage.ItemSize != pArray->ItemType->Size) ||
		 (pArray->Storage.Alignment < pArray->ItemType->Align) ) {
		__xrtTypedArrayError(XERR_STATE, XTYPED_ARRAY_ERROR_STATE,
			sOperation, "the typed array item layout does not match its type");
		return false;
	}
	return true;
}



/* 在用户类型回调期间拒绝当前数组的全部 API 重入。 */
void __xrtTypedArrayCallbackBegin(const xtypedarray* pArray)
{
	((xtypedarray*)pArray)->Flags |= XRT_TYPED_ARRAY_FLAG_BUSY;
}



/* 结束当前数组的用户类型回调门禁。 */
void __xrtTypedArrayCallbackEnd(const xtypedarray* pArray)
{
	((xtypedarray*)pArray)->Flags &= ~XRT_TYPED_ARRAY_FLAG_BUSY;
}



/* 判断字节区间是否触及类型数组结构或完整底层分配。 */
static bool __xrtTypedArrayOwnsRange(
	const xtypedarray* pArray,
	const void* pMemory,
	size_t iSize
)
{
	size_t iAllocationSize;

	if ( __xrtRangesOverlap(
		pMemory, iSize, pArray, sizeof(*pArray)
	) ) {
		return true;
	}
	if ( pArray->Storage.Capacity == 0u ) {
		return false;
	}
	iAllocationSize = pArray->Storage.Capacity *
		pArray->Storage.ItemSize;
	if ( pArray->Storage.Alignment > XRT_ARRAY_ALIGNMENT_DEFAULT ) {
		iAllocationSize += pArray->Storage.Alignment - 1u;
	}
	return __xrtRangesOverlap(
		pMemory,
		iSize,
		pArray->Storage.Allocation,
		iAllocationSize
	);
}



/* 判断来源是否是外部值或数组内部的准确活动元素。 */
static bool __xrtTypedArraySource(
	const xtypedarray* pArray,
	const void* pItem,
	size_t* pIndex,
	bool* pInternal,
	cstr sOperation
)
{
	uintptr_t iBegin;
	uintptr_t iItem;
	size_t iOffset;

	if ( pItem == NULL ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the source item is null");
		return false;
	}
	*pInternal = false;
	if ( !__xrtTypedArrayOwnsRange(
		pArray, pItem, pArray->ItemType->Size
	) ) {
		return true;
	}
	iBegin = (uintptr_t)pArray->Storage.Data;
	iItem = (uintptr_t)pItem;
	if ( iItem >= iBegin ) {
		iOffset = (size_t)(iItem - iBegin);
		if ( ((iOffset % pArray->Storage.ItemSize) == 0u) &&
			 ((iOffset / pArray->Storage.ItemSize) < pArray->Storage.Count) ) {
			*pIndex = iOffset / pArray->Storage.ItemSize;
			*pInternal = true;
			return true;
		}
	}
	if ( pArray->Storage.Count == 0u ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the source item aliases empty array storage");
		return false;
	}
	__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
		sOperation, "an internal source must be an active element boundary");
	return false;
}



/* 判断输出是否完全位于类型数组拥有的内存之外。 */
static bool __xrtTypedArrayOutputExternal(
	const xtypedarray* pArray,
	const void* pValue,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the output value is null");
		return false;
	}
	if ( !__xrtRangeValid(pValue, pArray->Storage.ItemSize) ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the output value range overflows");
		return false;
	}
	if ( __xrtTypedArrayOwnsRange(
		pArray, pValue, pArray->Storage.ItemSize
	) ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the output value must not alias typed array storage");
		return false;
	}
	return true;
}



/* 以指定公开操作名移动并删除一个元素。 */
static bool __xrtTypedArrayTake(
	xtypedarray* pArray,
	size_t iIndex,
	ptr pValue,
	cstr sOperation
)
{
	ptr pItem;

	if ( !__xrtTypedArrayValid(pArray, sOperation) ||
		 !__xrtTypedArrayOutputExternal(pArray, pValue, sOperation) ) {
		return false;
	}
	if ( iIndex >= pArray->Storage.Count ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			sOperation, "the typed array index is out of range");
		return false;
	}
	pItem = xrtArrayGet(&pArray->Storage, iIndex);
	__xrtTypedArrayCallbackBegin(pArray);
	if ( !xrtTypeMoveValue(pArray->ItemType, pValue, pItem) ) {
		__xrtTypedArrayCallbackEnd(pArray);
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			sOperation, "the typed array item could not be moved");
		return false;
	}
	if ( !xrtArrayRemove(&pArray->Storage, iIndex, 1u) ) {
		__xrtTypedArrayCallbackEnd(pArray);
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_STATE,
			sOperation, "the moved typed array item could not be removed");
		return false;
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 释放指定活动元素区间，不改变数组结构。 */
static void __xrtTypedArrayDropRange(
	xtypedarray* pArray,
	size_t iIndex,
	size_t iCount
)
{
	/* Trivial slots cannot run a callback or publish a cleanup error. */
	if ( (pArray->ItemType->Ops == NULL) ||
		 (pArray->ItemType->Ops->Drop == NULL) ) {
		return;
	}
	/* Finish every Drop, but never let a later cleanup replace the first
	 * failure (or an error already being unwound). Each callback starts with
	 * an empty error context so its own checked work can proceed normally. */
	xerror* pPrimary = xrtTakeError();
	while ( iCount != 0u ) {
		iCount--;
		xrtTypeDropValue(
			pArray->ItemType,
			xrtArrayGet(&pArray->Storage, iIndex + iCount)
		);
		if ( pPrimary == NULL ) {
			pPrimary = xrtTakeError();
		} else {
			xrtClearError();
		}
	}
	xrtSetErrorTake(pPrimary);
}



/* 回滚追加区间，并在清理后恢复原始错误。 */
static void __xrtTypedArrayRollback(
	xtypedarray* pArray,
	size_t iOriginalCount
)
{
	xerror* pError = xrtTakeError();
	size_t iAdded = pArray->Storage.Count - iOriginalCount;

	__xrtTypedArrayCallbackBegin(pArray);
	__xrtTypedArrayDropRange(pArray, iOriginalCount, iAdded);
	(void)xrtArrayRemove(&pArray->Storage, iOriginalCount, iAdded);
	__xrtTypedArrayCallbackEnd(pArray);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 销毁临时数组，并在清理后恢复原始错误。 */
static void __xrtTypedArrayDestroyPreserveError(xtypedarray* pArray)
{
	xerror* pError = xrtTakeError();

	xrtTypedArrayDestroy(pArray);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 初始化一个拥有类型值的空连续数组。 */
XRT_API bool xrtTypedArrayInit(
	xtypedarray* pArray,
	const xrttype* pItemType
)
{
	bool bSuccess;

	if ( pArray == NULL ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			"init", "the typed array is null");
		return false;
	}
	memset(pArray, 0, sizeof(*pArray));
	if ( !__xrtTypedArrayItemTypeValidate(pItemType, "init") ) {
		return false;
	}
	bSuccess = pItemType->Align > XRT_ARRAY_ALIGNMENT_DEFAULT ?
		xrtArrayInitAligned(
			&pArray->Storage, pItemType->Size, pItemType->Align
		) :
		xrtArrayInit(&pArray->Storage, pItemType->Size);
	if ( !bSuccess ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			"init", "the typed array storage could not be initialized");
		return false;
	}
	pArray->ItemType = pItemType;
	pArray->Flags = XRT_TYPED_ARRAY_FLAG_READY;
	return true;
}



/* 创建一个堆分配的空类型数组。 */
XRT_API xtypedarray* xrtTypedArrayCreate(const xrttype* pItemType)
{
	xtypedarray* pArray = (xtypedarray*)xrtMalloc(sizeof(*pArray));

	if ( pArray == NULL ) {
		return NULL;
	}
	if ( !xrtTypedArrayInit(pArray, pItemType) ) {
		xrtFree(pArray);
		return NULL;
	}
	return pArray;
}



/* 释放全部元素和存储，但不释放数组结构。 */
XRT_API void xrtTypedArrayUnit(xtypedarray* pArray)
{
	if ( pArray == NULL ) {
		return;
	}
	if ( (pArray->ItemType == NULL) && (pArray->Flags == 0u) ) {
		return;
	}
	if ( !__xrtTypedArrayValid(pArray, "unit") ) {
		return;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	__xrtTypedArrayDropRange(pArray, 0u, pArray->Storage.Count);
	xrtArrayUnit(&pArray->Storage);
	memset(pArray, 0, sizeof(*pArray));
}



/* 释放类型数组持有的全部资源和结构。 */
XRT_API void xrtTypedArrayDestroy(xtypedarray* pArray)
{
	if ( pArray == NULL ) {
		return;
	}
	if ( !__xrtTypedArrayValid(pArray, "destroy") ) {
		return;
	}
	xrtTypedArrayUnit(pArray);
	xrtFree(pArray);
}



/* 返回数组借用的元素类型。 */
XRT_API const xrttype* xrtTypedArrayItemType(const xtypedarray* pArray)
{
	return __xrtTypedArrayValid(pArray, "item-type") ?
		pArray->ItemType : NULL;
}



/* 返回当前元素数。 */
XRT_API size_t xrtTypedArrayCount(const xtypedarray* pArray)
{
	return __xrtTypedArrayValid(pArray, "count") ?
		pArray->Storage.Count : 0u;
}



/* 返回当前元素容量。 */
XRT_API size_t xrtTypedArrayCapacity(const xtypedarray* pArray)
{
	return __xrtTypedArrayValid(pArray, "capacity") ?
		pArray->Storage.Capacity : 0u;
}



/* 返回活动元素连续区的可写借用。 */
XRT_API ptr xrtTypedArrayData(xtypedarray* pArray)
{
	return __xrtTypedArrayValid(pArray, "data") ?
		pArray->Storage.Data : NULL;
}



/* 返回活动元素连续区的只读借用。 */
XRT_API const void* xrtTypedArrayConstData(const xtypedarray* pArray)
{
	return __xrtTypedArrayValid(pArray, "const-data") ?
		pArray->Storage.Data : NULL;
}



/* 保证数组至少具有指定元素容量。 */
XRT_API bool xrtTypedArrayReserve(xtypedarray* pArray, size_t iCapacity)
{
	if ( !__xrtTypedArrayValid(pArray, "reserve") ) {
		return false;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	bool bReserved = xrtArrayReserve(&pArray->Storage, iCapacity);
	__xrtTypedArrayCallbackEnd(pArray);
	if ( !bReserved ) {
		__xrtTypedArrayWrap(XERR_MEMORY, XTYPED_ARRAY_ERROR_OPERATION,
			"reserve", "the typed array capacity could not be reserved");
		return false;
	}
	return true;
}



/* 增长先完成所有可失败工作再发布；失败保留地址、容量、数量和旧值。
 * 无 Init 回调的零初始化走原始数组快路径；有回调时只初始化新增值，
 * 不复制或销毁旧元素。需要扩容时使用独立存储，提交只重定位字节。 */
XRT_API bool xrtTypedArrayResizeWithInitializer(
    xtypedarray* pArray, size_t iCount, xrttypedarrayinitializer Initializer, ptr pContext
)
{
	size_t iOriginalCount;
	size_t iAdded;
	xarray Prepared;
	bytes pNewItems;
	bool bDetached;

	if ( !__xrtTypedArrayValid(pArray, "resize") ) {
		return false;
	}
	iOriginalCount = pArray->Storage.Count;
	if ( iCount < iOriginalCount ) {
		bool bRemoved;

		__xrtTypedArrayCallbackBegin(pArray);
		__xrtTypedArrayDropRange(
			pArray, iCount, iOriginalCount - iCount
		);
		bRemoved = xrtArrayRemove(
			&pArray->Storage, iCount, iOriginalCount - iCount
		);
		__xrtTypedArrayCallbackEnd(pArray);
		return bRemoved;
	}
	if ( iCount == iOriginalCount ) {
		return true;
	}
	iAdded = iCount - iOriginalCount;
	__xrtTypedArrayCallbackBegin(pArray);
	if ( (Initializer == NULL) && ((pArray->ItemType->Ops == NULL) ||
		 (pArray->ItemType->Ops->Init == NULL)) ) {
		bool bResized = xrtArrayResize(&pArray->Storage, iCount);

		if ( !bResized ) {
			__xrtTypedArrayWrap(XERR_MEMORY, XTYPED_ARRAY_ERROR_OPERATION,
				"resize", "the typed array growth failed");
		}
		__xrtTypedArrayCallbackEnd(pArray);
		return bResized;
	}
	bDetached = iCount > pArray->Storage.Capacity;
	if ( bDetached ) {
		/* Keep the validated layout, not its allocation or ownership count.
		 * The raw allocator retains its own alignment/growth/overflow policy. */
		Prepared = pArray->Storage;
		Prepared.Data = NULL;
		Prepared.Allocation = NULL;
		Prepared.Count = 0u;
		Prepared.Capacity = 0u;
		if ( !xrtArrayReserve(&Prepared, iCount) ) {
			__xrtTypedArrayWrap(XERR_MEMORY, XTYPED_ARRAY_ERROR_OPERATION,
				"resize", "the typed array growth failed");
			__xrtTypedArrayCallbackEnd(pArray);
			return false;
		}
		pNewItems = Prepared.Data + iOriginalCount * pArray->ItemType->Size;
	} else {
		/* Inactive tail slots are not published while Init can still fail. */
		pNewItems = pArray->Storage.Data + iOriginalCount * pArray->ItemType->Size;
	}
	for ( size_t i = 0; i < iAdded; i++ ) {
		ptr pItem = pNewItems + i * pArray->ItemType->Size;

		bool bInitialized;
		if ( Initializer != NULL ) {
			memset(pItem, 0, pArray->ItemType->Size);
			bInitialized = Initializer(pItem, pArray->ItemType, pContext);
		} else {
			bInitialized = xrtTypeInitValue(pArray->ItemType, pItem);
		}
		if ( !bInitialized ) {
			xerror* pError = xrtTakeError();

			/* A failed Init cleans its own partial value. Drop only completed
			 * defaults, in reverse order, and preserve the primary failure even
			 * if Drop or the allocator writes a secondary error. */
			for ( size_t j = i; j != 0u; j-- ) {
				xrtTypeDropValue(pArray->ItemType,
					pNewItems + (j - 1u) * pArray->ItemType->Size);
			}
			if ( bDetached ) {
				xrtArrayUnit(&Prepared);
			}
			xrtSetErrorTake(pError);
			__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
				"resize", "a new typed array item could not be initialized");
			__xrtTypedArrayCallbackEnd(pArray);
			return false;
		}
	}
	if ( bDetached ) {
		xarray Previous = pArray->Storage;

		/* Relocatable is an admission requirement. After all defaults exist,
		 * no Copy/Move/Drop callback or allocation may precede publication. */
		if ( iOriginalCount != 0u ) {
			memcpy(Prepared.Data, Previous.Data,
				iOriginalCount * pArray->ItemType->Size);
		}
		Prepared.Count = iCount;
		pArray->Storage = Prepared;
		/* Old bytes no longer own values. Keep the receiver BUSY through
		 * allocator cleanup so a Free callback cannot retire the new array. */
		xrtArrayUnit(&Previous);
	} else {
		pArray->Storage.Count = iCount;
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



XRT_API bool xrtTypedArrayResize(xtypedarray* pArray, size_t iCount)
{
	return xrtTypedArrayResizeWithInitializer(pArray, iCount, NULL, NULL);
}



/* 把容量裁剪到当前元素数量。 */
XRT_API bool xrtTypedArrayTrim(xtypedarray* pArray)
{
	if ( !__xrtTypedArrayValid(pArray, "trim") ) {
		return false;
	}
	if ( !xrtArrayTrim(&pArray->Storage) ) {
		__xrtTypedArrayWrap(XERR_MEMORY, XTYPED_ARRAY_ERROR_OPERATION,
			"trim", "the typed array capacity could not be trimmed");
		return false;
	}
	return true;
}



/* 清空全部元素并保留存储容量。 */
XRT_API void xrtTypedArrayClear(xtypedarray* pArray)
{
	if ( !__xrtTypedArrayValid(pArray, "clear") ) {
		return;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	__xrtTypedArrayDropRange(pArray, 0u, pArray->Storage.Count);
	xrtArrayClear(&pArray->Storage);
	__xrtTypedArrayCallbackEnd(pArray);
}



/* 返回指定下标的可写借用元素。 */
XRT_API ptr xrtTypedArrayGet(xtypedarray* pArray, size_t iIndex)
{
	if ( !__xrtTypedArrayValid(pArray, "get") ) {
		return NULL;
	}
	if ( iIndex >= pArray->Storage.Count ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"get", "the typed array index is out of range");
		return NULL;
	}
	return xrtArrayGet(&pArray->Storage, iIndex);
}



/* 返回指定下标的只读借用元素。 */
XRT_API const void* xrtTypedArrayConstGet(
	const xtypedarray* pArray,
	size_t iIndex
)
{
	if ( !__xrtTypedArrayValid(pArray, "const-get") ) {
		return NULL;
	}
	if ( iIndex >= pArray->Storage.Count ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"const-get", "the typed array index is out of range");
		return NULL;
	}
	return xrtArrayConstGet(&pArray->Storage, iIndex);
}



/* 在指定位置复制插入一个值，并处理数组内部来源。 */
static bool __xrtTypedArrayInsert(
	xtypedarray* pArray,
	size_t iIndex,
	const void* pItem,
	cstr sOperation
)
{
	size_t iSourceIndex = 0u;
	bool bInternal;
	ptr pSlot;

	if ( !__xrtTypedArrayValid(pArray, sOperation) ||
		 !__xrtTypedArraySource(
			pArray, pItem, &iSourceIndex, &bInternal, sOperation
		) ) {
		return false;
	}
	if ( iIndex > pArray->Storage.Count ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			sOperation, "the typed array insertion index is out of range");
		return false;
	}
	if ( pArray->Storage.Count == SIZE_MAX ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			sOperation, "the typed array element count overflows");
		return false;
	}
	if ( !xrtArrayReserve(&pArray->Storage, pArray->Storage.Count + 1u) ) {
		__xrtTypedArrayWrap(XERR_MEMORY, XTYPED_ARRAY_ERROR_OPERATION,
			sOperation, "the typed array insertion could not reserve storage");
		return false;
	}
	if ( bInternal ) {
		pItem = xrtArrayConstGet(&pArray->Storage, iSourceIndex);
	}
	pSlot = xrtArrayInsertSpace(&pArray->Storage, iIndex, 1u);
	if ( pSlot == NULL ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			sOperation, "the typed array insertion failed");
		return false;
	}
	if ( bInternal ) {
		size_t iMovedIndex = iSourceIndex >= iIndex ?
			iSourceIndex + 1u : iSourceIndex;

		pItem = xrtArrayConstGet(&pArray->Storage, iMovedIndex);
	}
	__xrtTypedArrayCallbackBegin(pArray);
	if ( !xrtTypeInitValue(pArray->ItemType, pSlot) ) {
		xerror* pError = xrtTakeError();

		(void)xrtArrayRemove(&pArray->Storage, iIndex, 1u);
		__xrtTypedArrayCallbackEnd(pArray);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			sOperation, "the inserted array item could not be initialized");
		return false;
	}
	if ( !xrtTypeCopyValue(pArray->ItemType, pSlot, pItem) ) {
		xerror* pError = xrtTakeError();

		xrtTypeDropValue(pArray->ItemType, pSlot);
		(void)xrtArrayRemove(&pArray->Storage, iIndex, 1u);
		__xrtTypedArrayCallbackEnd(pArray);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			sOperation, "the inserted array item could not be copied");
		return false;
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 复制追加一个值。 */
XRT_API bool xrtTypedArrayPush(xtypedarray* pArray, const void* pItem)
{
	return __xrtTypedArrayInsert(
		pArray,
		pArray != NULL ? pArray->Storage.Count : 0u,
		pItem,
		"push"
	);
}



/* 在指定下标复制插入一个值。 */
XRT_API bool xrtTypedArrayInsert(
	xtypedarray* pArray,
	size_t iIndex,
	const void* pItem
)
{
	return __xrtTypedArrayInsert(pArray, iIndex, pItem, "insert");
}



/* 失败原子地替换指定元素值。 */
XRT_API bool xrtTypedArraySet(
	xtypedarray* pArray,
	size_t iIndex,
	const void* pItem
)
{
	ptr pTarget;
	size_t iSourceIndex;
	bool bInternal;

	if ( !__xrtTypedArrayValid(pArray, "set") ||
		 !__xrtTypedArraySource(
			pArray, pItem, &iSourceIndex, &bInternal, "set"
		) ) {
		return false;
	}
	if ( iIndex >= pArray->Storage.Count ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"set", "the typed array index is out of range");
		return false;
	}
	pTarget = xrtArrayGet(&pArray->Storage, iIndex);
	__xrtTypedArrayCallbackBegin(pArray);
	if ( !xrtTypeCopyValue(pArray->ItemType, pTarget, pItem) ) {
		__xrtTypedArrayCallbackEnd(pArray);
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			"set", "the typed array item could not be replaced");
		return false;
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 销毁并删除精确元素区间。 */
XRT_API bool xrtTypedArrayRemove(
	xtypedarray* pArray,
	size_t iIndex,
	size_t iCount
)
{
	if ( !__xrtTypedArrayValid(pArray, "remove") ) {
		return false;
	}
	if ( (iIndex > pArray->Storage.Count) ||
		 (iCount > (pArray->Storage.Count - iIndex)) ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"remove", "the typed array removal range is invalid");
		return false;
	}
	if ( iCount == 0u ) {
		return true;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	__xrtTypedArrayDropRange(pArray, iIndex, iCount);
	if ( !xrtArrayRemove(&pArray->Storage, iIndex, iCount) ) {
		__xrtTypedArrayCallbackEnd(pArray);
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_STATE,
			"remove", "the dropped typed array range could not be removed");
		return false;
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 把指定元素移动到外部已初始化值后删除。 */
XRT_API bool xrtTypedArrayTake(
	xtypedarray* pArray,
	size_t iIndex,
	ptr pValue
)
{
	return __xrtTypedArrayTake(pArray, iIndex, pValue, "take");
}



/* 把末尾元素移动到外部已初始化值后删除。 */
XRT_API bool xrtTypedArrayPop(xtypedarray* pArray, ptr pValue)
{
	if ( !__xrtTypedArrayValid(pArray, "pop") ) {
		return false;
	}
	if ( pArray->Storage.Count == 0u ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"pop", "the typed array is empty");
		return false;
	}
	return __xrtTypedArrayTake(
		pArray, pArray->Storage.Count - 1u, pValue, "pop"
	);
}



/* 交换两个元素的字节位置。 */
XRT_API bool xrtTypedArraySwap(
	xtypedarray* pArray,
	size_t iLeft,
	size_t iRight
)
{
	if ( !__xrtTypedArrayValid(pArray, "swap") ) {
		return false;
	}
	if ( !xrtArraySwap(&pArray->Storage, iLeft, iRight) ) {
		__xrtTypedArrayWrap(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"swap", "the typed array swap indices are invalid");
		return false;
	}
	return true;
}



/* 原地反转元素顺序。 */
XRT_API bool xrtTypedArrayReverse(xtypedarray* pArray)
{
	if ( !__xrtTypedArrayValid(pArray, "reverse") ) {
		return false;
	}
	if ( !xrtArrayReverse(&pArray->Storage) ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			"reverse", "the typed array could not be reversed");
		return false;
	}
	return true;
}



/* 使用类型比较操作查找第一个相等元素。 */
XRT_API size_t xrtTypedArrayFind(
	const xtypedarray* pArray,
	const void* pItem
)
{
	size_t iSourceIndex;
	bool bInternal;

	if ( !__xrtTypedArrayValid(pArray, "find") ||
		 !__xrtTypedArraySource(
			pArray, pItem, &iSourceIndex, &bInternal, "find"
		) ) {
		return SIZE_MAX;
	}
	if ( !xrtTypeIsComparable(pArray->ItemType) ) {
		__xrtTypedArrayError(XERR_UNSUPPORTED, XTYPED_ARRAY_ERROR_TYPE,
			"find", "the array item type is not comparable");
		return SIZE_MAX;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	for ( size_t i = 0; i < pArray->Storage.Count; i++ ) {
		int iCompare;

		if ( !xrtTypeCompareValue(
			pArray->ItemType,
			xrtArrayConstGet(&pArray->Storage, i),
			pItem,
			&iCompare
		) ) {
			__xrtTypedArrayCallbackEnd(pArray);
			__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
				"find", "typed array item comparison failed");
			return SIZE_MAX;
		}
		if ( iCompare == 0 ) {
			__xrtTypedArrayCallbackEnd(pArray);
			return i;
		}
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return SIZE_MAX;
}



/* 判断数组是否包含相等元素。 */
XRT_API bool xrtTypedArrayContains(
	const xtypedarray* pArray,
	const void* pItem
)
{
	return xrtTypedArrayFind(pArray, pItem) != SIZE_MAX;
}



/* 事务追加另一个同类型数组，允许自追加。 */
XRT_API bool xrtTypedArrayAppend(
	xtypedarray* pTarget,
	const xtypedarray* pSource
)
{
	size_t iOriginalCount;
	size_t iSourceCount;

	if ( !__xrtTypedArrayValid(pTarget, "append") ||
		 !__xrtTypedArrayValid(pSource, "append") ) {
		return false;
	}
	if ( !xrtTypeSame(pTarget->ItemType, pSource->ItemType) ) {
		__xrtTypedArrayError(XERR_TYPE, XTYPED_ARRAY_ERROR_TYPE,
			"append", "typed arrays have different item types");
		return false;
	}
	iOriginalCount = pTarget->Storage.Count;
	iSourceCount = pSource->Storage.Count;
	if ( iSourceCount > (SIZE_MAX - iOriginalCount) ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"append", "the typed array element count overflows");
		return false;
	}
	if ( pTarget != pSource ) {
		__xrtTypedArrayCallbackBegin(pSource);
	}
	if ( !xrtTypedArrayReserve(pTarget, iOriginalCount + iSourceCount) ) {
		if ( pTarget != pSource ) {
			__xrtTypedArrayCallbackEnd(pSource);
		}
		return false;
	}
	for ( size_t i = 0; i < iSourceCount; i++ ) {
		const void* pItem = xrtArrayConstGet(&pSource->Storage, i);

		if ( !xrtTypedArrayPush(pTarget, pItem) ) {
			__xrtTypedArrayRollback(pTarget, iOriginalCount);
			if ( pTarget != pSource ) {
				__xrtTypedArrayCallbackEnd(pSource);
			}
			__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
				"append", "a typed array item could not be appended");
			return false;
		}
	}
	if ( pTarget != pSource ) {
		__xrtTypedArrayCallbackEnd(pSource);
	}
	return true;
}



/* 区间结果复用同一类型数组布局；复制和移交共享分配/门禁边界。
 * Relocatable 是类型数组准入条件：尾部移交不执行可失败的 Move/Copy。
 * 在任何来源修改前完成结果分配，提交区间只有字节移交和计数更新。 */
static xtypedarray* __xrtTypedArrayExtract(
	const xtypedarray* pArray,
	size_t iIndex,
	size_t iCount,
	bool bReverse,
	bool bTake,
	cstr sOperation
)
{
	xtypedarray* pResult;

	__xrtTypedArrayCallbackBegin(pArray);
	pResult = xrtTypedArrayCreate(pArray->ItemType);
	if ( pResult == NULL ) {
		__xrtTypedArrayCallbackEnd(pArray);
		return NULL;
	}
	if ( !xrtTypedArrayReserve(pResult, iCount) ) {
		__xrtTypedArrayDestroyPreserveError(pResult);
		__xrtTypedArrayCallbackEnd(pArray);
		return NULL;
	}
	if ( bTake ) {
		/* Result storage is reserved and empty. No allocations, callbacks or
		 * fallible operations are allowed until both ownership counts publish. */
		for ( size_t i = 0u; i < iCount; i++ ) {
			size_t iSource = iIndex + (bReverse ? iCount - i - 1u : i);
			memcpy(pResult->Storage.Data + i * pResult->ItemType->Size,
				pArray->Storage.Data + iSource * pArray->ItemType->Size,
				pArray->ItemType->Size);
		}
		pResult->Storage.Count = iCount;
		((xtypedarray*)pArray)->Storage.Count -= iCount;
	} else {
		for ( size_t i = 0u; i < iCount; i++ ) {
			size_t iSource = iIndex + (bReverse ? iCount - i - 1u : i);
			if ( !xrtTypedArrayPush(pResult,
					xrtArrayConstGet(&pArray->Storage, iSource)) ) {
				__xrtTypedArrayDestroyPreserveError(pResult);
				__xrtTypedArrayCallbackEnd(pArray);
				__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
					sOperation, "a typed array range item could not be copied");
				return NULL;
			}
		}
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return pResult;
}



/* 复制精确有界区间；不把错误区间静默截断为合法区间。 */
XRT_API xtypedarray* xrtTypedArraySlice(
	const xtypedarray* pArray,
	size_t iIndex,
	size_t iCount,
	bool bReverse
)
{
	if ( !__xrtTypedArrayValid(pArray, "slice") ) {
		return NULL;
	}
	if ( iIndex > pArray->Storage.Count ||
		 iCount > pArray->Storage.Count - iIndex ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"slice", "the typed array range is out of bounds");
		return NULL;
	}
	return __xrtTypedArrayExtract(pArray, iIndex, iCount, bReverse, false, "slice");
}



/* 移交实际存在的尾部元素，空请求仍返回独立的空拥有数组。 */
XRT_API xtypedarray* xrtTypedArrayTakeTail(
	xtypedarray* pArray,
	size_t iMaxCount,
	bool bReverse
)
{
	size_t iCount;

	if ( !__xrtTypedArrayValid(pArray, "take-tail") ) {
		return NULL;
	}
	iCount = iMaxCount < pArray->Storage.Count ? iMaxCount : pArray->Storage.Count;
	return __xrtTypedArrayExtract(pArray, pArray->Storage.Count - iCount,
		iCount, bReverse, true, "take-tail");
}



/* 深复制一个独立类型数组。 */
XRT_API xtypedarray* xrtTypedArrayClone(const xtypedarray* pArray)
{
	xtypedarray* pClone;

	if ( !__xrtTypedArrayValid(pArray, "clone") ) {
		return NULL;
	}
	pClone = xrtTypedArrayCreate(pArray->ItemType);
	if ( pClone == NULL ) {
		return NULL;
	}
	if ( !xrtTypedArrayAppend(pClone, pArray) ) {
		__xrtTypedArrayDestroyPreserveError(pClone);
		return NULL;
	}
	return pClone;
}



/* 深复制并拼接两个同类型数组。 */
XRT_API xtypedarray* xrtTypedArrayConcat(
	const xtypedarray* pLeft,
	const xtypedarray* pRight
)
{
	xtypedarray* pResult;

	if ( !__xrtTypedArrayValid(pLeft, "concat") ||
		 !__xrtTypedArrayValid(pRight, "concat") ) {
		return NULL;
	}
	if ( !xrtTypeSame(pLeft->ItemType, pRight->ItemType) ) {
		__xrtTypedArrayError(XERR_TYPE, XTYPED_ARRAY_ERROR_TYPE,
			"concat", "typed arrays have different item types");
		return NULL;
	}
	pResult = xrtTypedArrayClone(pLeft);
	if ( pResult == NULL ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			"concat", "the left typed array could not be cloned");
		return NULL;
	}
	if ( !xrtTypedArrayAppend(pResult, pRight) ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			"concat", "the right typed array could not be appended");
		__xrtTypedArrayDestroyPreserveError(pResult);
		return NULL;
	}
	return pResult;
}



/* 比较两个数组的类型、数量和有序元素内容。 */
XRT_API bool xrtTypedArrayEquals(
	const xtypedarray* pLeft,
	const xtypedarray* pRight
)
{
	int iCompare;

	if ( !__xrtTypedArrayValid(pLeft, "equals") ||
		 !__xrtTypedArrayValid(pRight, "equals") ) {
		return false;
	}
	if ( pLeft == pRight ) {
		return true;
	}
	if ( !xrtTypeSame(pLeft->ItemType, pRight->ItemType) ||
		 (pLeft->Storage.Count != pRight->Storage.Count) ) {
		return false;
	}
	if ( !xrtTypeIsComparable(pLeft->ItemType) ) {
		__xrtTypedArrayError(XERR_UNSUPPORTED, XTYPED_ARRAY_ERROR_TYPE,
			"equals", "the array item type is not comparable");
		return false;
	}
	__xrtTypedArrayCallbackBegin(pLeft);
	__xrtTypedArrayCallbackBegin(pRight);
	for ( size_t i = 0; i < pLeft->Storage.Count; i++ ) {
		if ( !xrtTypeCompareValue(
			pLeft->ItemType,
			xrtArrayConstGet(&pLeft->Storage, i),
			xrtArrayConstGet(&pRight->Storage, i),
			&iCompare
		) ) {
			__xrtTypedArrayCallbackEnd(pRight);
			__xrtTypedArrayCallbackEnd(pLeft);
			__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
				"equals", "typed array item comparison failed");
			return false;
		}
		if ( iCompare != 0 ) {
			__xrtTypedArrayCallbackEnd(pRight);
			__xrtTypedArrayCallbackEnd(pLeft);
			return false;
		}
	}
	__xrtTypedArrayCallbackEnd(pRight);
	__xrtTypedArrayCallbackEnd(pLeft);
	return true;
}



/* 按数组类型实参初始化对象负载。 */
static bool __xrtTypedArrayInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	if ( !xrtTypedArrayTypeValidate(pType) ) {
		return false;
	}
	return xrtTypedArrayInit(
		(xtypedarray*)pInstance, pType->Arguments[0]
	);
}



/* 销毁对象负载中的类型数组。 */
static void __xrtTypedArrayInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedArrayUnit((xtypedarray*)pInstance);
}



/* 枚举类型数组所有元素值直接拥有的强对象引用。 */
static bool __xrtTypedArrayInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const xtypedarray* pArray = (const xtypedarray*)pInstance;
	(void)pType;

	if ( !__xrtTypedArrayValid(pArray, "instance-trace") ) {
		return false;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	for ( size_t i = 0; i < pArray->Storage.Count; i++ ) {
		if ( !xrtTypeTraceValue(
			pArray->ItemType,
			xrtArrayConstGet(&pArray->Storage, i),
			pVisit,
			pContext
		) ) {
			__xrtTypedArrayCallbackEnd(pArray);
			return false;
		}
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 返回对象数组负载共享的实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedArrayInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedArrayInstanceInit,
		.Drop = __xrtTypedArrayInstanceDrop,
		.Trace = __xrtTypedArrayInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的泛型数组类型描述。 */
XRT_API bool xrtTypedArrayTypeValidate(const xrttype* pType)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedArrayWrap(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_TYPE,
			"type-validate", "the typed array object type is invalid");
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_ARRAY) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 1u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != sizeof(xtypedarray)) ||
		(pType->InstanceAlign <
		 XRT_INTERNAL_OBJECT_ALIGNOF(xtypedarray)) ||
		(pType->InstanceOps != xrtTypedArrayInstanceOps())
	) {
		__xrtTypedArrayError(XERR_TYPE, XTYPED_ARRAY_ERROR_TYPE,
			"type-validate", "the typed array object type contract is invalid");
		return false;
	}
	return __xrtTypedArrayItemTypeValidate(
		pType->Arguments[0], "type-validate"
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_queue.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE)



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE)

/* 设置类型队列模块结构化错误。 */
void __xrtTypedQueueError(
	xerrkind Kind,
	xtypedqueueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-queue";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型或基础队列错误补充类型队列上下文。 */
void __xrtTypedQueueWrap(
	xerrkind DefaultKind,
	xtypedqueueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-queue";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 验证元素类型具有固定值槽需要的复制和移动能力。 */
static bool __xrtTypedQueueItemTypeValidate(
	const xrttype* pItemType,
	cstr sOperation
)
{
	bool bMovable;

	if ( !xrtTypeValidate(pItemType) ) {
		__xrtTypedQueueWrap(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"the queue item type is invalid"
		);
		return false;
	}
	if ( pItemType->Size == 0u ) {
		__xrtTypedQueueError(
			XERR_TYPE, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"a typed queue item must occupy storage"
		);
		return false;
	}
	if ( !xrtTypeIsCopyable(pItemType) ) {
		__xrtTypedQueueError(
			XERR_UNSUPPORTED, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"the queue item type is not copyable"
		);
		return false;
	}
	bMovable = ((pItemType->Ops != NULL) &&
		(pItemType->Ops->Move != NULL)) ||
		((pItemType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) != 0u);
	if ( !bMovable ) {
		__xrtTypedQueueError(
			XERR_UNSUPPORTED, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"the queue item type has no move operation"
		);
		return false;
	}
	return true;
}



/* 按元素对齐向上计算一个固定值槽跨度。 */
static bool __xrtTypedQueueStride(
	const xrttype* pItemType,
	size_t* pStride
)
{
	size_t iMask = pItemType->Align - 1u;

	if ( pItemType->Size > (SIZE_MAX - iMask) ) {
		__xrtTypedQueueError(
			XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, "init",
			"the queue item stride overflows size_t"
		);
		return false;
	}
	*pStride = (pItemType->Size + iMask) & ~iMask;
	return true;
}



/* 初始化固定数量的对齐值槽，但暂不发布核心状态。 */
bool __xrtTypedQueueCoreInit(
	xtypedqueuecore* pCore,
	const xrttype* pItemType,
	size_t iCapacity,
	cstr sOperation
)
{
	uintptr_t iRaw;
	uintptr_t iAligned;
	size_t iAllocationSize;
	size_t iInitialized = 0u;

	if ( pCore == NULL ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, sOperation,
			"the queue core is null"
		);
		return false;
	}
	memset(pCore, 0, sizeof(*pCore));
	if ( !__xrtTypedQueueItemTypeValidate(pItemType, sOperation) ) {
		return false;
	}
	if ( (iCapacity == 0u) || (iCapacity > XRT_QUEUE_MAX_CAPACITY) ) {
		__xrtTypedQueueError(
			XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, sOperation,
			"the queue capacity is outside the supported range"
		);
		return false;
	}
	if ( !__xrtTypedQueueStride(pItemType, &pCore->Stride) ||
		 (iCapacity > (SIZE_MAX / pCore->Stride)) ) {
		if ( xrtGetError() == NULL ) {
			__xrtTypedQueueError(
				XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, sOperation,
				"the queue value storage size overflows size_t"
			);
		}
		memset(pCore, 0, sizeof(*pCore));
		return false;
	}
	pCore->ValueBytes = iCapacity * pCore->Stride;
	if ( pCore->ValueBytes > (SIZE_MAX - (pItemType->Align - 1u)) ) {
		__xrtTypedQueueError(
			XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, sOperation,
			"the aligned queue allocation size overflows size_t"
		);
		memset(pCore, 0, sizeof(*pCore));
		return false;
	}
	iAllocationSize = pCore->ValueBytes + pItemType->Align - 1u;
	pCore->Allocation = xrtMalloc(iAllocationSize);
	if ( pCore->Allocation == NULL ) {
		memset(pCore, 0, sizeof(*pCore));
		return false;
	}
	iRaw = (uintptr_t)pCore->Allocation;
	iAligned = (iRaw + pItemType->Align - 1u) &
		~(uintptr_t)(pItemType->Align - 1u);
	pCore->Values = (bytes)iAligned;
	pCore->ItemType = pItemType;
	pCore->Capacity = iCapacity;
	xrtAtomic32Init(&pCore->State, XRT_TYPED_QUEUE_STATE_EMPTY);
	xrtAtomic32Init(&pCore->Active, 0u);

	while ( iInitialized < iCapacity ) {
		ptr pCell = pCore->Values + (iInitialized * pCore->Stride);

		if ( !xrtTypeInitValue(pItemType, pCell) ) {
			xerror* pError = xrtTakeError();

			while ( iInitialized != 0u ) {
				iInitialized--;
				xrtTypeDropValue(
					pItemType,
					pCore->Values + (iInitialized * pCore->Stride)
				);
			}
			xrtFree(pCore->Allocation);
			memset(pCore, 0, sizeof(*pCore));
			if ( pError != NULL ) {
				xrtSetError(pError);
				xrtErrorFree(pError);
			}
			__xrtTypedQueueWrap(
				XERR_STATE, XTYPED_QUEUE_ERROR_OPERATION, sOperation,
				"a queue value slot could not be initialized"
			);
			return false;
		}
		iInitialized++;
	}
	return true;
}



/* 发布已经完成全部基础队列初始化的类型队列核心。 */
void __xrtTypedQueueCoreActivate(xtypedqueuecore* pCore)
{
	xrtAtomic32Store(
		&pCore->State, XRT_TYPED_QUEUE_STATE_READY, XMEMORY_RELEASE
	);
}



/* 检查类型队列核心公开状态、布局和拥有者边界。 */
bool __xrtTypedQueueCoreValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	cstr sOperation
)
{
	uint32 iState;
	uintptr_t iAllocation;
	uintptr_t iValues;
	size_t iGap;

	if ( (pCore == NULL) || (pOwner == NULL) || (pCore->ItemType == NULL) ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, sOperation,
			"the typed queue is null or uninitialized"
		);
		return false;
	}
	iState = xrtAtomic32Load(&pCore->State, XMEMORY_ACQUIRE);
	if ( iState != XRT_TYPED_QUEUE_STATE_READY ) {
		__xrtTypedQueueError(
			XERR_STATE, XTYPED_QUEUE_ERROR_STATE, sOperation,
			iState == XRT_TYPED_QUEUE_STATE_BROKEN ?
				"the typed queue is broken" :
				"the typed queue is not available for API access"
		);
		return false;
	}
	iAllocation = (uintptr_t)pCore->Allocation;
	iValues = (uintptr_t)pCore->Values;
	iGap = iValues >= iAllocation ? (size_t)(iValues - iAllocation) : SIZE_MAX;
	if (
		(pCore->Allocation == NULL) ||
		(pCore->Values == NULL) ||
		(pCore->Stride < pCore->ItemType->Size) ||
		((pCore->Stride & (pCore->ItemType->Align - 1u)) != 0u) ||
		(pCore->Capacity == 0u) ||
		(pCore->Capacity > (SIZE_MAX / pCore->Stride)) ||
		(pCore->ValueBytes != (pCore->Stride * pCore->Capacity)) ||
		(iGap > (pCore->ItemType->Align - 1u)) ||
		(iAllocation > (UINTPTR_MAX - iGap)) ||
		(iValues > (UINTPTR_MAX - pCore->ValueBytes)) ||
		(((uintptr_t)pCore->Values & (pCore->ItemType->Align - 1u)) != 0u) ||
		!__xrtRangesOverlap(pCore, sizeof(*pCore), pOwner, iOwnerSize)
	) {
		__xrtTypedQueueError(
			XERR_STATE, XTYPED_QUEUE_ERROR_STATE, sOperation,
			"the typed queue value storage layout is invalid"
		);
		return false;
	}
	return true;
}



/* 进入一次允许并发的类型值操作，并与独占生命周期操作握手。 */
bool __xrtTypedQueueCoreEnter(
	xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	cstr sOperation
)
{
	if ( !__xrtTypedQueueCoreValid(
		pCore, pOwner, iOwnerSize, sOperation
	) ) {
		return false;
	}
	xrtAtomic32FetchAdd(&pCore->Active, 1u, XMEMORY_ACQ_REL);
	{
		uint32 iState = xrtAtomic32Load(
			&pCore->State, XMEMORY_ACQUIRE
		);

		if ( iState == XRT_TYPED_QUEUE_STATE_READY ) {
			return true;
		}
		xrtAtomic32FetchSub(&pCore->Active, 1u, XMEMORY_RELEASE);
		__xrtTypedQueueError(
			XERR_STATE, XTYPED_QUEUE_ERROR_STATE, sOperation,
			iState == XRT_TYPED_QUEUE_STATE_BROKEN ?
				"the typed queue became broken while entering an operation" :
				"the typed queue entered an exclusive lifecycle operation"
		);
		return false;
	}
}



/* 退出一次允许并发的类型值操作。 */
void __xrtTypedQueueCoreLeave(xtypedqueuecore* pCore)
{
	(void)xrtAtomic32FetchSub(&pCore->Active, 1u, XMEMORY_RELEASE);
}



/* 独占核心状态并等待已进入操作离开。 */
bool __xrtTypedQueueCoreExclusive(
	xtypedqueuecore* pCore,
	bool bAllowBroken,
	uint32* pPrevious,
	cstr sOperation
)
{
	uint32 iExpected;

	if ( (pCore == NULL) || (pPrevious == NULL) ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, sOperation,
			"the typed queue or exclusive-state output is null"
		);
		return false;
	}
	iExpected = xrtAtomic32Load(&pCore->State, XMEMORY_ACQUIRE);
	for ( ;; ) {
		if (
			(iExpected != XRT_TYPED_QUEUE_STATE_READY) &&
			(!bAllowBroken || (iExpected != XRT_TYPED_QUEUE_STATE_BROKEN))
		) {
			__xrtTypedQueueError(
				XERR_STATE, XTYPED_QUEUE_ERROR_STATE, sOperation,
				"the typed queue cannot enter an exclusive operation"
			);
			return false;
		}
		*pPrevious = iExpected;
		if ( xrtAtomic32CompareExchange(
			&pCore->State,
			&iExpected,
			XRT_TYPED_QUEUE_STATE_EXCLUSIVE,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
	}
	while ( xrtAtomic32Load(&pCore->Active, XMEMORY_ACQUIRE) != 0u ) {
		xrtAtomicPause();
	}
	return true;
}



/* 结束临时独占并恢复此前的稳定状态。 */
void __xrtTypedQueueCoreShared(
	xtypedqueuecore* pCore,
	uint32 iPrevious
)
{
	xrtAtomic32Store(&pCore->State, iPrevious, XMEMORY_RELEASE);
}



/* 标记不可恢复的内部基础队列状态错误。 */
void __xrtTypedQueueCoreBreak(
	xtypedqueuecore* pCore,
	cstr sOperation,
	cstr sMessage
)
{
	xrtAtomic32Store(
		&pCore->State, XRT_TYPED_QUEUE_STATE_BROKEN, XMEMORY_RELEASE
	);
	__xrtTypedQueueWrap(
		XERR_STATE, XTYPED_QUEUE_ERROR_STATE, sOperation, sMessage
	);
}



/* 销毁每一个已初始化值槽并释放连续存储。 */
void __xrtTypedQueueCoreUnit(xtypedqueuecore* pCore)
{
	const xrttype* pItemType;

	if ( (pCore == NULL) || (pCore->ItemType == NULL) ) {
		return;
	}
	pItemType = pCore->ItemType;
	for ( size_t i = pCore->Capacity; i != 0u; i-- ) {
		xrtTypeDropValue(
			pItemType,
			pCore->Values + ((i - 1u) * pCore->Stride)
		);
	}
	xrtFree(pCore->Allocation);
	memset(pCore, 0, sizeof(*pCore));
}



/* 返回指定固定槽的地址。 */
ptr __xrtTypedQueueCell(xtypedqueuecore* pCore, size_t iIndex)
{
	return pCore->Values + (iIndex * pCore->Stride);
}



/* 验证一段值内存可被本队列安全访问。 */
static bool __xrtTypedQueueRangeValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	const void* pValues,
	size_t iBytes,
	cstr sOperation
)
{
	uintptr_t iValue = (uintptr_t)pValues;

	if (
		(pValues == NULL) ||
		((iValue & (pCore->ItemType->Align - 1u)) != 0u) ||
		(iValue > (UINTPTR_MAX - iBytes)) ||
		__xrtRangesOverlap(pValues, iBytes, pOwner, iOwnerSize) ||
		__xrtRangesOverlap(
			pValues, iBytes, pCore->Values, pCore->ValueBytes
		)
	) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, sOperation,
			"the value range is null, unaligned, overflowing, or internal"
		);
		return false;
	}
	return true;
}



/* 验证单值不与队列对象和内部值槽重叠。 */
bool __xrtTypedQueueValueValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	const void* pValue,
	cstr sOperation
)
{
	return __xrtTypedQueueRangeValid(
		pCore, pOwner, iOwnerSize, pValue, pCore->ItemType->Size,
		sOperation
	);
}



/* 验证连续值区间的尺寸、地址和外部所有权。 */
bool __xrtTypedQueueValuesValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	const void* pValues,
	size_t iCount,
	cstr sOperation
)
{
	if ( iCount == 0u ) {
		return true;
	}
	if ( iCount > (SIZE_MAX / pCore->ItemType->Size) ) {
		__xrtTypedQueueError(
			XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, sOperation,
			"the contiguous value range size overflows size_t"
		);
		return false;
	}
	return __xrtTypedQueueRangeValid(
		pCore, pOwner, iOwnerSize, pValues,
		iCount * pCore->ItemType->Size, sOperation
	);
}



/* 失败原子地复制一个外部值到空内部槽。 */
bool __xrtTypedQueueCopyIn(
	const xtypedqueuecore* pCore,
	ptr pCell,
	const void* pItem,
	cstr sOperation
)
{
	if ( !xrtTypeCopyValue(pCore->ItemType, pCell, pItem) ) {
		__xrtTypedQueueWrap(
			XERR_STATE, XTYPED_QUEUE_ERROR_OPERATION, sOperation,
			"the queue item could not be copied"
		);
		return false;
	}
	return true;
}



/* 失败原子地移动一个外部值到空内部槽。 */
bool __xrtTypedQueueMoveIn(
	const xtypedqueuecore* pCore,
	ptr pCell,
	ptr pItem,
	cstr sOperation
)
{
	if ( !xrtTypeMoveValue(pCore->ItemType, pCell, pItem) ) {
		__xrtTypedQueueWrap(
			XERR_STATE, XTYPED_QUEUE_ERROR_OPERATION, sOperation,
			"the queue item could not be moved into a value slot"
		);
		return false;
	}
	return true;
}



/* 失败原子地把内部槽移动到外部已初始化值。 */
bool __xrtTypedQueueMoveOut(
	const xtypedqueuecore* pCore,
	ptr pValue,
	ptr pCell,
	cstr sOperation
)
{
	if ( !xrtTypeMoveValue(pCore->ItemType, pValue, pCell) ) {
		__xrtTypedQueueWrap(
			XERR_STATE, XTYPED_QUEUE_ERROR_OPERATION, sOperation,
			"the queue item could not be moved to the output value"
		);
		return false;
	}
	return true;
}



/* 合并两个并发近似数量并限制在固定容量内。 */
size_t __xrtTypedQueueCount(
	const xtypedqueuecore* pCore,
	size_t iReady,
	size_t iRetry
)
{
	if ( iReady >= pCore->Capacity ) {
		return pCore->Capacity;
	}
	return iRetry < (pCore->Capacity - iReady) ?
		iReady + iRetry : pCore->Capacity;
}



/* 验证对象队列类型描述中的元素、容量和实例操作。 */
bool __xrtTypedQueueTypeValidate(
	const xrttype* pType,
	size_t iInstanceSize,
	size_t iInstanceAlign,
	const xrtinstanceops* pInstanceOps,
	cstr sOperation
)
{
	const xtypedqueuemeta* pMeta;

	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedQueueWrap(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"the typed queue object type is invalid"
		);
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_LIST) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 1u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != iInstanceSize) ||
		(pType->InstanceAlign < iInstanceAlign) ||
		(pType->InstanceOps != pInstanceOps) ||
		(pType->Metadata == NULL)
	) {
		__xrtTypedQueueError(
			XERR_TYPE, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"the typed queue object type contract is invalid"
		);
		return false;
	}
	pMeta = (const xtypedqueuemeta*)pType->Metadata;
	if ( (pMeta->Capacity == 0u) ||
		 (pMeta->Capacity > XRT_QUEUE_MAX_CAPACITY) ) {
		__xrtTypedQueueError(
			XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, sOperation,
			"the typed queue object capacity is invalid"
		);
		return false;
	}
	return __xrtTypedQueueItemTypeValidate(
		pType->Arguments[0], sOperation
	);
}



/* 在独占状态下追踪全部固定值槽。 */
bool __xrtTypedQueueTrace(
	xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	xrtobjectvisitor pVisit,
	ptr pContext,
	cstr sOperation
)
{
	uint32 iPrevious;
	bool bSuccess = true;

	if ( (pVisit == NULL) ||
		 !__xrtTypedQueueCoreValid(
			pCore, pOwner, iOwnerSize, sOperation
		 ) ||
		 !__xrtTypedQueueCoreExclusive(
			pCore, false, &iPrevious, sOperation
		 ) ) {
		if ( pVisit == NULL ) {
			__xrtTypedQueueError(
				XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, sOperation,
				"the object visitor is null"
			);
		}
		return false;
	}
	for ( size_t i = 0u; i < pCore->Capacity; i++ ) {
		if ( !xrtTypeTraceValue(
			pCore->ItemType,
			pCore->Values + (i * pCore->Stride),
			pVisit,
			pContext
		) ) {
			bSuccess = false;
			break;
		}
	}
	__xrtTypedQueueCoreShared(pCore, iPrevious);
	if ( !bSuccess ) {
		__xrtTypedQueueWrap(
			XERR_STATE, XTYPED_QUEUE_ERROR_OPERATION, sOperation,
			"a queue value slot could not be traced"
		);
	}
	return bSuccess;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_queue_spsc.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_SPSC)



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_SPSC)

/* 清理初始化中途失败的 SPSC 类型队列并保留根错误。 */
static void __xrtTypedSPSCQueueRollback(xtypedspscqueue* pQueue)
{
	xerror* pError = xrtTakeError();

	xrtSPSCQueueUnit(&pQueue->Free);
	xrtSPSCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把全部固定值槽发布到反向 SPSC 空闲环。 */
static bool __xrtTypedSPSCQueueFillFree(xtypedspscqueue* pQueue)
{
	for ( size_t i = 0u; i < pQueue->Core.Capacity; i++ ) {
		if ( xrtSPSCQueueTryPush(
			&pQueue->Free, __xrtTypedQueueCell(&pQueue->Core, i)
		) != XQUEUE_OK ) {
			__xrtTypedQueueWrap(
				XERR_STATE, XTYPED_QUEUE_ERROR_STATE, "init",
				"an SPSC free value slot could not be published"
			);
			return false;
		}
	}
	return true;
}



/* 结束一个已初始化 SPSC 类型队列，失败表示仍有并发访问。 */
static bool __xrtTypedSPSCQueueUnit(xtypedspscqueue* pQueue)
{
	uint32 iPrevious;

	if ( pQueue == NULL ) {
		return true;
	}
	if ( pQueue->Core.ItemType == NULL ) {
		memset(pQueue, 0, sizeof(*pQueue));
		return true;
	}
	if ( !__xrtTypedQueueCoreExclusive(
		&pQueue->Core, true, &iPrevious, "unit"
	) ) {
		return false;
	}
	(void)iPrevious;
	xrtSPSCQueueUnit(&pQueue->Free);
	xrtSPSCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	return true;
}



/* 共用复制和移动两条 SPSC 入队路径。 */
static xqueueresult __xrtTypedSPSCQueuePush(
	xtypedspscqueue* pQueue,
	ptr pItem,
	bool bTake,
	cstr sOperation
)
{
	ptr pCell;
	xqueueresult Result;
	bool bStored;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), sOperation
	) || !__xrtTypedQueueValueValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pItem, sOperation
	) || !__xrtTypedQueueCoreEnter(
		&pQueue->Core, pQueue, sizeof(*pQueue), sOperation
	) ) {
		return XQUEUE_ERROR;
	}
	if ( xrtSPSCQueueIsClosed(&pQueue->Ready) ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_CLOSED;
	}
	pCell = xrtAtomicPtrExchange(
		&pQueue->PushCell, NULL, XMEMORY_ACQ_REL
	);
	if ( pCell == NULL ) {
		Result = xrtSPSCQueueTryPop(&pQueue->Free, &pCell);
		if ( Result != XQUEUE_OK ) {
			__xrtTypedQueueCoreLeave(&pQueue->Core);
			if ( Result == XQUEUE_EMPTY ) {
				return XQUEUE_FULL;
			}
			__xrtTypedQueueCoreBreak(
				&pQueue->Core, sOperation,
				"the SPSC free value-slot queue is invalid"
			);
			return XQUEUE_ERROR;
		}
	}
	bStored = bTake ?
		__xrtTypedQueueMoveIn(&pQueue->Core, pCell, pItem, sOperation) :
		__xrtTypedQueueCopyIn(&pQueue->Core, pCell, pItem, sOperation);
	if ( !bStored ) {
		xrtAtomicPtrStore(
			&pQueue->PushCell, pCell, XMEMORY_RELEASE
		);
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	Result = xrtSPSCQueueTryPush(&pQueue->Ready, pCell);
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, sOperation,
			"a prepared SPSC value slot could not be published"
		);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 初始化一个拥有固定值槽的 SPSC 类型队列。 */
XRT_API bool xrtTypedSPSCQueueInit(
	xtypedspscqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
)
{
	if ( pQueue == NULL ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, "init",
			"the SPSC typed queue is null"
		);
		return false;
	}
	memset(pQueue, 0, sizeof(*pQueue));
	if ( !xrtSPSCQueueInit(&pQueue->Ready, iCapacity) ) {
		return false;
	}
	if ( !__xrtTypedQueueCoreInit(
		&pQueue->Core, pItemType, pQueue->Ready.Capacity, "init"
	) || !xrtSPSCQueueInit(
		&pQueue->Free, pQueue->Ready.Capacity
	) ) {
		__xrtTypedSPSCQueueRollback(pQueue);
		return false;
	}
	xrtAtomicPtrInit(&pQueue->PushCell, NULL);
	xrtAtomicPtrInit(&pQueue->PopCell, NULL);
	if ( !__xrtTypedSPSCQueueFillFree(pQueue) ) {
		__xrtTypedSPSCQueueRollback(pQueue);
		return false;
	}
	__xrtTypedQueueCoreActivate(&pQueue->Core);
	return true;
}



/* 在堆上创建一个 SPSC 类型队列。 */
XRT_API xtypedspscqueue* xrtTypedSPSCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
)
{
	xtypedspscqueue* pQueue = (xtypedspscqueue*)xrtMalloc(
		sizeof(xtypedspscqueue)
	);

	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtTypedSPSCQueueInit(pQueue, pItemType, iCapacity) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}



/* 释放全部 SPSC 队列值和内部环，但不释放结构。 */
XRT_API void xrtTypedSPSCQueueUnit(xtypedspscqueue* pQueue)
{
	(void)__xrtTypedSPSCQueueUnit(pQueue);
}



/* 释放 Create 返回的 SPSC 类型队列。 */
XRT_API void xrtTypedSPSCQueueDestroy(xtypedspscqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedSPSCQueueUnit(pQueue) ) {
		xrtFree(pQueue);
	}
}



/* 返回 SPSC 队列借用的元素类型。 */
XRT_API const xrttype* xrtTypedSPSCQueueItemType(
	const xtypedspscqueue* pQueue
)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "item-type"
	) ? pQueue->Core.ItemType : NULL;
}



/* 返回 SPSC 队列实际固定容量。 */
XRT_API size_t xrtTypedSPSCQueueCapacity(const xtypedspscqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "capacity"
	) ? pQueue->Core.Capacity : 0u;
}



/* 返回包含移动失败重试槽的并发近似元素数量。 */
XRT_API size_t xrtTypedSPSCQueueCount(const xtypedspscqueue* pQueue)
{
	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "count"
	) ) {
		return 0u;
	}
	return __xrtTypedQueueCount(
		&pQueue->Core,
		xrtSPSCQueueCount(&pQueue->Ready),
		xrtAtomicPtrLoad(&pQueue->PopCell, XMEMORY_ACQUIRE) != NULL ? 1u : 0u
	);
}



/* 失败原子地复制压入一个 SPSC 类型值。 */
XRT_API xqueueresult xrtTypedSPSCQueueTryPush(
	xtypedspscqueue* pQueue,
	const void* pItem
)
{
	return __xrtTypedSPSCQueuePush(
		pQueue, (ptr)pItem, false, "try-push"
	);
}



/* 移动压入一个外部已初始化 SPSC 类型值。 */
XRT_API xqueueresult xrtTypedSPSCQueueTryPushTake(
	xtypedspscqueue* pQueue,
	ptr pItem
)
{
	return __xrtTypedSPSCQueuePush(
		pQueue, pItem, true, "try-push-take"
	);
}



/* 移动弹出一个 SPSC 类型值，移动失败时保留重试槽。 */
XRT_API xqueueresult xrtTypedSPSCQueueTryPop(
	xtypedspscqueue* pQueue,
	ptr pValue
)
{
	ptr pCell;
	xqueueresult Result;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "try-pop"
	) || !__xrtTypedQueueValueValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pValue, "try-pop"
	) || !__xrtTypedQueueCoreEnter(
		&pQueue->Core, pQueue, sizeof(*pQueue), "try-pop"
	) ) {
		return XQUEUE_ERROR;
	}
	pCell = xrtAtomicPtrExchange(
		&pQueue->PopCell, NULL, XMEMORY_ACQ_REL
	);
	if ( pCell == NULL ) {
		Result = xrtSPSCQueueTryPop(&pQueue->Ready, &pCell);
		if ( Result != XQUEUE_OK ) {
			__xrtTypedQueueCoreLeave(&pQueue->Core);
			return Result;
		}
	}
	if ( !__xrtTypedQueueMoveOut(
		&pQueue->Core, pValue, pCell, "try-pop"
	) ) {
		xrtAtomicPtrStore(&pQueue->PopCell, pCell, XMEMORY_RELEASE);
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	Result = xrtSPSCQueueTryPush(&pQueue->Free, pCell);
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "try-pop",
			"a consumed SPSC value slot could not be recycled"
		);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 复制压入连续 SPSC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedSPSCQueuePushBatch(
	xtypedspscqueue* pQueue,
	const void* pItems,
	size_t iCount
)
{
	xqueuebatchresult Batch = { XQUEUE_OK, 0u };
	const bytes pValues = (const bytes)pItems;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "push-batch"
	) || !__xrtTypedQueueValuesValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pItems, iCount,
		"push-batch"
	) ) {
		Batch.Result = XQUEUE_ERROR;
		return Batch;
	}
	while ( Batch.Count < iCount ) {
		xqueueresult Result = xrtTypedSPSCQueueTryPush(
			pQueue,
			pValues + (Batch.Count * pQueue->Core.ItemType->Size)
		);

		if ( Result != XQUEUE_OK ) {
			Batch.Result = Batch.Count != 0u ? XQUEUE_OK : Result;
			return Batch;
		}
		Batch.Count++;
	}
	return Batch;
}



/* 移动弹出到连续已初始化 SPSC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedSPSCQueuePopBatch(
	xtypedspscqueue* pQueue,
	ptr pValues,
	size_t iCapacity
)
{
	xqueuebatchresult Batch = { XQUEUE_OK, 0u };
	bytes pOutput = (bytes)pValues;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "pop-batch"
	) || !__xrtTypedQueueValuesValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pValues, iCapacity,
		"pop-batch"
	) ) {
		Batch.Result = XQUEUE_ERROR;
		return Batch;
	}
	while ( Batch.Count < iCapacity ) {
		xqueueresult Result = xrtTypedSPSCQueueTryPop(
			pQueue,
			pOutput + (Batch.Count * pQueue->Core.ItemType->Size)
		);

		if ( Result != XQUEUE_OK ) {
			Batch.Result = Batch.Count != 0u ? XQUEUE_OK : Result;
			return Batch;
		}
		Batch.Count++;
	}
	return Batch;
}



/* 由唯一生产者停止写入后关闭 SPSC 类型队列。 */
XRT_API void xrtTypedSPSCQueueClose(xtypedspscqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "close"
	) ) {
		xrtSPSCQueueClose(&pQueue->Ready);
	}
}



/* 判断 SPSC 类型队列是否已经关闭写端。 */
XRT_API bool xrtTypedSPSCQueueIsClosed(const xtypedspscqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "is-closed"
	) && xrtSPSCQueueIsClosed(&pQueue->Ready);
}



/* 判断 SPSC 类型队列是否关闭、排空且无进行中的值回调。 */
XRT_API bool xrtTypedSPSCQueueIsDrained(const xtypedspscqueue* pQueue)
{
	return xrtTypedSPSCQueueIsClosed(pQueue) &&
		(xrtTypedSPSCQueueCount(pQueue) == 0u) &&
		(xrtAtomic32Load(&pQueue->Core.Active, XMEMORY_ACQUIRE) == 0u);
}



/* 在独占且排空后重置全部 SPSC 环并重新开放。 */
XRT_API bool xrtTypedSPSCQueueReset(xtypedspscqueue* pQueue)
{
	uint32 iPrevious;
	ptr pCell;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "reset"
	) || !__xrtTypedQueueCoreExclusive(
		&pQueue->Core, false, &iPrevious, "reset"
	) ) {
		return false;
	}
	if ( (xrtSPSCQueueCount(&pQueue->Ready) != 0u) ||
		 (xrtAtomicPtrLoad(&pQueue->PopCell, XMEMORY_ACQUIRE) != NULL) ) {
		__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
		__xrtTypedQueueError(
			XERR_AGAIN, XTYPED_QUEUE_ERROR_STATE, "reset",
			"the SPSC typed queue must be drained before reset"
		);
		return false;
	}
	while ( xrtSPSCQueueTryPop(&pQueue->Free, &pCell) == XQUEUE_OK ) {
	}
	if ( !xrtSPSCQueueReset(&pQueue->Ready) ||
		 !xrtSPSCQueueReset(&pQueue->Free) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the SPSC pointer rings could not be reset"
		);
		return false;
	}
	xrtAtomicPtrStore(&pQueue->PushCell, NULL, XMEMORY_RELAXED);
	xrtAtomicPtrStore(&pQueue->PopCell, NULL, XMEMORY_RELAXED);
	if ( !__xrtTypedSPSCQueueFillFree(pQueue) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the SPSC free value slots could not be restored"
		);
		return false;
	}
	__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
	return true;
}



/* 从对象类型参数和元数据初始化 SPSC 队列负载。 */
static bool __xrtTypedSPSCQueueInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	const xtypedqueuemeta* pMeta;

	if ( !xrtTypedSPSCQueueTypeValidate(pType) ) {
		return false;
	}
	pMeta = (const xtypedqueuemeta*)pType->Metadata;
	return xrtTypedSPSCQueueInit(
		(xtypedspscqueue*)pInstance, pType->Arguments[0], pMeta->Capacity
	);
}



/* 销毁对象负载中的 SPSC 类型队列。 */
static void __xrtTypedSPSCQueueInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedSPSCQueueUnit((xtypedspscqueue*)pInstance);
}



/* 枚举 SPSC 固定值槽直接拥有的全部强对象引用。 */
static bool __xrtTypedSPSCQueueInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	(void)pType;
	return __xrtTypedQueueTrace(
		&((xtypedspscqueue*)pInstance)->Core,
		pInstance,
		sizeof(xtypedspscqueue),
		pVisit,
		pContext,
		"instance-trace"
	);
}



/* 返回 SPSC 类型队列共享实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedSPSCQueueInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedSPSCQueueInstanceInit,
		.Drop = __xrtTypedSPSCQueueInstanceDrop,
		.Trace = __xrtTypedSPSCQueueInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的 SPSC 类型队列描述。 */
XRT_API bool xrtTypedSPSCQueueTypeValidate(const xrttype* pType)
{
	return __xrtTypedQueueTypeValidate(
		pType,
		sizeof(xtypedspscqueue),
		XRT_INTERNAL_OBJECT_ALIGNOF(xtypedspscqueue),
		xrtTypedSPSCQueueInstanceOps(),
		"type-validate"
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_queue_mpsc.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC)



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC)

/* 清理初始化中途失败的 MPSC 类型队列并保留根错误。 */
static void __xrtTypedMPSCQueueRollback(xtypedmpscqueue* pQueue)
{
	xerror* pError = xrtTakeError();

	xrtMPMCQueueUnit(&pQueue->Free);
	xrtMPSCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把全部固定值槽发布到并发空闲环。 */
static bool __xrtTypedMPSCQueueFillFree(xtypedmpscqueue* pQueue)
{
	for ( size_t i = 0u; i < pQueue->Core.Capacity; i++ ) {
		if ( xrtMPMCQueueTryPush(
			&pQueue->Free, __xrtTypedQueueCell(&pQueue->Core, i)
		) != XQUEUE_OK ) {
			__xrtTypedQueueWrap(
				XERR_STATE, XTYPED_QUEUE_ERROR_STATE, "init",
				"an MPSC free value slot could not be published"
			);
			return false;
		}
	}
	return true;
}



/* 把空值槽归还给允许多个生产者领取的空闲环。 */
static bool __xrtTypedMPSCQueueRecycle(
	xtypedmpscqueue* pQueue,
	ptr pCell,
	cstr sOperation
)
{
	xqueueresult Result;

	for ( ;; ) {
		Result = xrtMPMCQueueTryPush(&pQueue->Free, pCell);
		if ( Result == XQUEUE_OK ) {
			return true;
		}
		if ( Result != XQUEUE_FULL ) {
			break;
		}
		xrtAtomicPause();
	}
	__xrtTypedQueueCoreBreak(
		&pQueue->Core, sOperation,
		"an MPSC value slot could not be recycled"
	);
	return false;
}



/* 结束一个已初始化 MPSC 类型队列，失败表示仍有并发访问。 */
static bool __xrtTypedMPSCQueueUnit(xtypedmpscqueue* pQueue)
{
	uint32 iPrevious;

	if ( pQueue == NULL ) {
		return true;
	}
	if ( pQueue->Core.ItemType == NULL ) {
		memset(pQueue, 0, sizeof(*pQueue));
		return true;
	}
	if ( !__xrtTypedQueueCoreExclusive(
		&pQueue->Core, true, &iPrevious, "unit"
	) ) {
		return false;
	}
	(void)iPrevious;
	xrtMPMCQueueUnit(&pQueue->Free);
	xrtMPSCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	return true;
}



/* 共用复制和移动两条 MPSC 入队路径。 */
static xqueueresult __xrtTypedMPSCQueuePush(
	xtypedmpscqueue* pQueue,
	ptr pItem,
	bool bTake,
	cstr sOperation
)
{
	ptr pCell = NULL;
	xqueueresult Result;
	bool bStored;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), sOperation
	) || !__xrtTypedQueueValueValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pItem, sOperation
	) || !__xrtTypedQueueCoreEnter(
		&pQueue->Core, pQueue, sizeof(*pQueue), sOperation
	) ) {
		return XQUEUE_ERROR;
	}
	if ( xrtMPSCQueueIsClosed(&pQueue->Ready) ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_CLOSED;
	}
	Result = xrtMPMCQueueTryPop(&pQueue->Free, &pCell);
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		if ( Result == XQUEUE_EMPTY ) {
			return XQUEUE_FULL;
		}
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, sOperation,
			"the MPSC free value-slot queue is invalid"
		);
		return XQUEUE_ERROR;
	}
	bStored = bTake ?
		__xrtTypedQueueMoveIn(&pQueue->Core, pCell, pItem, sOperation) :
		__xrtTypedQueueCopyIn(&pQueue->Core, pCell, pItem, sOperation);
	if ( !bStored ) {
		(void)__xrtTypedMPSCQueueRecycle(pQueue, pCell, sOperation);
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	do {
		Result = xrtMPSCQueueTryPush(&pQueue->Ready, pCell);
		if ( Result == XQUEUE_FULL ) {
			xrtAtomicPause();
		}
	} while ( Result == XQUEUE_FULL );
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, sOperation,
			"a prepared MPSC value slot could not be published"
		);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 初始化一个拥有固定值槽的 MPSC 类型队列。 */
XRT_API bool xrtTypedMPSCQueueInit(
	xtypedmpscqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
)
{
	if ( pQueue == NULL ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, "init",
			"the MPSC typed queue is null"
		);
		return false;
	}
	memset(pQueue, 0, sizeof(*pQueue));
	if ( !xrtMPSCQueueInit(&pQueue->Ready, iCapacity) ) {
		return false;
	}
	if ( !__xrtTypedQueueCoreInit(
		&pQueue->Core, pItemType, pQueue->Ready.Capacity, "init"
	) || !xrtMPMCQueueInit(
		&pQueue->Free, pQueue->Ready.Capacity
	) ) {
		__xrtTypedMPSCQueueRollback(pQueue);
		return false;
	}
	xrtAtomicPtrInit(&pQueue->PopCell, NULL);
	if ( !__xrtTypedMPSCQueueFillFree(pQueue) ) {
		__xrtTypedMPSCQueueRollback(pQueue);
		return false;
	}
	__xrtTypedQueueCoreActivate(&pQueue->Core);
	return true;
}



/* 在堆上创建一个 MPSC 类型队列。 */
XRT_API xtypedmpscqueue* xrtTypedMPSCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
)
{
	xtypedmpscqueue* pQueue = (xtypedmpscqueue*)xrtMalloc(
		sizeof(xtypedmpscqueue)
	);

	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtTypedMPSCQueueInit(pQueue, pItemType, iCapacity) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}



/* 释放全部 MPSC 队列值和内部环，但不释放结构。 */
XRT_API void xrtTypedMPSCQueueUnit(xtypedmpscqueue* pQueue)
{
	(void)__xrtTypedMPSCQueueUnit(pQueue);
}



/* 释放 Create 返回的 MPSC 类型队列。 */
XRT_API void xrtTypedMPSCQueueDestroy(xtypedmpscqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedMPSCQueueUnit(pQueue) ) {
		xrtFree(pQueue);
	}
}



/* 返回 MPSC 队列借用的元素类型。 */
XRT_API const xrttype* xrtTypedMPSCQueueItemType(
	const xtypedmpscqueue* pQueue
)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "item-type"
	) ? pQueue->Core.ItemType : NULL;
}



/* 返回 MPSC 队列实际固定容量。 */
XRT_API size_t xrtTypedMPSCQueueCapacity(const xtypedmpscqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "capacity"
	) ? pQueue->Core.Capacity : 0u;
}



/* 返回包含移动失败重试槽的并发近似元素数量。 */
XRT_API size_t xrtTypedMPSCQueueCount(const xtypedmpscqueue* pQueue)
{
	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "count"
	) ) {
		return 0u;
	}
	return __xrtTypedQueueCount(
		&pQueue->Core,
		xrtMPSCQueueCount(&pQueue->Ready),
		xrtAtomicPtrLoad(&pQueue->PopCell, XMEMORY_ACQUIRE) != NULL ? 1u : 0u
	);
}



/* 失败原子地复制压入一个 MPSC 类型值。 */
XRT_API xqueueresult xrtTypedMPSCQueueTryPush(
	xtypedmpscqueue* pQueue,
	const void* pItem
)
{
	return __xrtTypedMPSCQueuePush(
		pQueue, (ptr)pItem, false, "try-push"
	);
}



/* 移动压入一个外部已初始化 MPSC 类型值。 */
XRT_API xqueueresult xrtTypedMPSCQueueTryPushTake(
	xtypedmpscqueue* pQueue,
	ptr pItem
)
{
	return __xrtTypedMPSCQueuePush(
		pQueue, pItem, true, "try-push-take"
	);
}



/* 由唯一消费者移动弹出，移动失败时保留重试槽。 */
XRT_API xqueueresult xrtTypedMPSCQueueTryPop(
	xtypedmpscqueue* pQueue,
	ptr pValue
)
{
	ptr pCell;
	xqueueresult Result;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "try-pop"
	) || !__xrtTypedQueueValueValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pValue, "try-pop"
	) || !__xrtTypedQueueCoreEnter(
		&pQueue->Core, pQueue, sizeof(*pQueue), "try-pop"
	) ) {
		return XQUEUE_ERROR;
	}
	pCell = xrtAtomicPtrExchange(
		&pQueue->PopCell, NULL, XMEMORY_ACQ_REL
	);
	if ( pCell == NULL ) {
		Result = xrtMPSCQueueTryPop(&pQueue->Ready, &pCell);
		if ( Result != XQUEUE_OK ) {
			__xrtTypedQueueCoreLeave(&pQueue->Core);
			return Result;
		}
	}
	if ( !__xrtTypedQueueMoveOut(
		&pQueue->Core, pValue, pCell, "try-pop"
	) ) {
		xrtAtomicPtrStore(&pQueue->PopCell, pCell, XMEMORY_RELEASE);
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	if ( !__xrtTypedMPSCQueueRecycle(pQueue, pCell, "try-pop") ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 复制压入连续 MPSC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedMPSCQueuePushBatch(
	xtypedmpscqueue* pQueue,
	const void* pItems,
	size_t iCount
)
{
	xqueuebatchresult Batch = { XQUEUE_OK, 0u };
	const bytes pValues = (const bytes)pItems;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "push-batch"
	) || !__xrtTypedQueueValuesValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pItems, iCount,
		"push-batch"
	) ) {
		Batch.Result = XQUEUE_ERROR;
		return Batch;
	}
	while ( Batch.Count < iCount ) {
		xqueueresult Result = xrtTypedMPSCQueueTryPush(
			pQueue,
			pValues + (Batch.Count * pQueue->Core.ItemType->Size)
		);

		if ( Result != XQUEUE_OK ) {
			Batch.Result = Batch.Count != 0u ? XQUEUE_OK : Result;
			return Batch;
		}
		Batch.Count++;
	}
	return Batch;
}



/* 移动弹出到连续已初始化 MPSC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedMPSCQueuePopBatch(
	xtypedmpscqueue* pQueue,
	ptr pValues,
	size_t iCapacity
)
{
	xqueuebatchresult Batch = { XQUEUE_OK, 0u };
	bytes pOutput = (bytes)pValues;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "pop-batch"
	) || !__xrtTypedQueueValuesValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pValues, iCapacity,
		"pop-batch"
	) ) {
		Batch.Result = XQUEUE_ERROR;
		return Batch;
	}
	while ( Batch.Count < iCapacity ) {
		xqueueresult Result = xrtTypedMPSCQueueTryPop(
			pQueue,
			pOutput + (Batch.Count * pQueue->Core.ItemType->Size)
		);

		if ( Result != XQUEUE_OK ) {
			Batch.Result = Batch.Count != 0u ? XQUEUE_OK : Result;
			return Batch;
		}
		Batch.Count++;
	}
	return Batch;
}



/* 在全部生产者退出后关闭 MPSC 类型队列。 */
XRT_API void xrtTypedMPSCQueueClose(xtypedmpscqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "close"
	) ) {
		xrtMPSCQueueClose(&pQueue->Ready);
	}
}



/* 判断 MPSC 类型队列是否已经关闭写端。 */
XRT_API bool xrtTypedMPSCQueueIsClosed(const xtypedmpscqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "is-closed"
	) && xrtMPSCQueueIsClosed(&pQueue->Ready);
}



/* 判断 MPSC 类型队列是否关闭、排空且无进行中的值回调。 */
XRT_API bool xrtTypedMPSCQueueIsDrained(const xtypedmpscqueue* pQueue)
{
	return xrtTypedMPSCQueueIsClosed(pQueue) &&
		(xrtTypedMPSCQueueCount(pQueue) == 0u) &&
		(xrtAtomic32Load(&pQueue->Core.Active, XMEMORY_ACQUIRE) == 0u);
}



/* 在独占且排空后重置全部 MPSC 环并重新开放。 */
XRT_API bool xrtTypedMPSCQueueReset(xtypedmpscqueue* pQueue)
{
	uint32 iPrevious;
	ptr pCell;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "reset"
	) || !__xrtTypedQueueCoreExclusive(
		&pQueue->Core, false, &iPrevious, "reset"
	) ) {
		return false;
	}
	if ( (xrtMPSCQueueCount(&pQueue->Ready) != 0u) ||
		 (xrtAtomicPtrLoad(&pQueue->PopCell, XMEMORY_ACQUIRE) != NULL) ) {
		__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
		__xrtTypedQueueError(
			XERR_AGAIN, XTYPED_QUEUE_ERROR_STATE, "reset",
			"the MPSC typed queue must be drained before reset"
		);
		return false;
	}
	while ( xrtMPMCQueueTryPop(&pQueue->Free, &pCell) == XQUEUE_OK ) {
	}
	if ( !xrtMPSCQueueReset(&pQueue->Ready) ||
		 !xrtMPMCQueueReset(&pQueue->Free) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the MPSC pointer rings could not be reset"
		);
		return false;
	}
	xrtAtomicPtrStore(&pQueue->PopCell, NULL, XMEMORY_RELAXED);
	if ( !__xrtTypedMPSCQueueFillFree(pQueue) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the MPSC free value slots could not be restored"
		);
		return false;
	}
	__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
	return true;
}



/* 从对象类型参数和元数据初始化 MPSC 队列负载。 */
static bool __xrtTypedMPSCQueueInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	const xtypedqueuemeta* pMeta;

	if ( !xrtTypedMPSCQueueTypeValidate(pType) ) {
		return false;
	}
	pMeta = (const xtypedqueuemeta*)pType->Metadata;
	return xrtTypedMPSCQueueInit(
		(xtypedmpscqueue*)pInstance, pType->Arguments[0], pMeta->Capacity
	);
}



/* 销毁对象负载中的 MPSC 类型队列。 */
static void __xrtTypedMPSCQueueInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedMPSCQueueUnit((xtypedmpscqueue*)pInstance);
}



/* 枚举 MPSC 固定值槽直接拥有的全部强对象引用。 */
static bool __xrtTypedMPSCQueueInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	(void)pType;
	return __xrtTypedQueueTrace(
		&((xtypedmpscqueue*)pInstance)->Core,
		pInstance,
		sizeof(xtypedmpscqueue),
		pVisit,
		pContext,
		"instance-trace"
	);
}



/* 返回 MPSC 类型队列共享实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedMPSCQueueInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedMPSCQueueInstanceInit,
		.Drop = __xrtTypedMPSCQueueInstanceDrop,
		.Trace = __xrtTypedMPSCQueueInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的 MPSC 类型队列描述。 */
XRT_API bool xrtTypedMPSCQueueTypeValidate(const xrttype* pType)
{
	return __xrtTypedQueueTypeValidate(
		pType,
		sizeof(xtypedmpscqueue),
		XRT_INTERNAL_OBJECT_ALIGNOF(xtypedmpscqueue),
		xrtTypedMPSCQueueInstanceOps(),
		"type-validate"
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_queue_mpmc.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPMC)



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPMC)

/* 清理初始化中途失败的 MPMC 类型队列并保留根错误。 */
static void __xrtTypedMPMCQueueRollback(xtypedmpmcqueue* pQueue)
{
	xerror* pError = xrtTakeError();

	xrtMPMCQueueUnit(&pQueue->Retry);
	xrtMPMCQueueUnit(&pQueue->Free);
	xrtMPMCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把全部固定值槽发布到并发空闲环。 */
static bool __xrtTypedMPMCQueueFillFree(xtypedmpmcqueue* pQueue)
{
	for ( size_t i = 0u; i < pQueue->Core.Capacity; i++ ) {
		if ( xrtMPMCQueueTryPush(
			&pQueue->Free, __xrtTypedQueueCell(&pQueue->Core, i)
		) != XQUEUE_OK ) {
			__xrtTypedQueueWrap(
				XERR_STATE, XTYPED_QUEUE_ERROR_STATE, "init",
				"an MPMC free value slot could not be published"
			);
			return false;
		}
	}
	return true;
}



/* 把空值槽归还给并发空闲环。 */
static bool __xrtTypedMPMCQueueRecycle(
	xtypedmpmcqueue* pQueue,
	ptr pCell,
	cstr sOperation
)
{
	xqueueresult Result;

	for ( ;; ) {
		Result = xrtMPMCQueueTryPush(&pQueue->Free, pCell);
		if ( Result == XQUEUE_OK ) {
			return true;
		}
		if ( Result != XQUEUE_FULL ) {
			break;
		}
		xrtAtomicPause();
	}
	__xrtTypedQueueCoreBreak(
		&pQueue->Core, sOperation,
		"an MPMC value slot could not be recycled"
	);
	return false;
}



/* 把移动失败后仍拥有值的槽发布到并发重试环。 */
static bool __xrtTypedMPMCQueueRetry(
	xtypedmpmcqueue* pQueue,
	ptr pCell,
	cstr sOperation
)
{
	xqueueresult Result;

	for ( ;; ) {
		Result = xrtMPMCQueueTryPush(&pQueue->Retry, pCell);
		if ( Result == XQUEUE_OK ) {
			return true;
		}
		if ( Result != XQUEUE_FULL ) {
			break;
		}
		xrtAtomicPause();
	}
	__xrtTypedQueueCoreBreak(
		&pQueue->Core, sOperation,
		"an MPMC value slot could not be retained for retry"
	);
	return false;
}



/* 结束一个已初始化 MPMC 类型队列，失败表示仍有并发访问。 */
static bool __xrtTypedMPMCQueueUnit(xtypedmpmcqueue* pQueue)
{
	uint32 iPrevious;

	if ( pQueue == NULL ) {
		return true;
	}
	if ( pQueue->Core.ItemType == NULL ) {
		memset(pQueue, 0, sizeof(*pQueue));
		return true;
	}
	if ( !__xrtTypedQueueCoreExclusive(
		&pQueue->Core, true, &iPrevious, "unit"
	) ) {
		return false;
	}
	(void)iPrevious;
	xrtMPMCQueueUnit(&pQueue->Retry);
	xrtMPMCQueueUnit(&pQueue->Free);
	xrtMPMCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	return true;
}



/* 共用复制和移动两条 MPMC 入队路径。 */
static xqueueresult __xrtTypedMPMCQueuePush(
	xtypedmpmcqueue* pQueue,
	ptr pItem,
	bool bTake,
	cstr sOperation
)
{
	ptr pCell = NULL;
	xqueueresult Result;
	bool bStored;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), sOperation
	) || !__xrtTypedQueueValueValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pItem, sOperation
	) || !__xrtTypedQueueCoreEnter(
		&pQueue->Core, pQueue, sizeof(*pQueue), sOperation
	) ) {
		return XQUEUE_ERROR;
	}
	if ( xrtMPMCQueueIsClosed(&pQueue->Ready) ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_CLOSED;
	}
	Result = xrtMPMCQueueTryPop(&pQueue->Free, &pCell);
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		if ( Result == XQUEUE_EMPTY ) {
			return XQUEUE_FULL;
		}
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, sOperation,
			"the MPMC free value-slot queue is invalid"
		);
		return XQUEUE_ERROR;
	}
	bStored = bTake ?
		__xrtTypedQueueMoveIn(&pQueue->Core, pCell, pItem, sOperation) :
		__xrtTypedQueueCopyIn(&pQueue->Core, pCell, pItem, sOperation);
	if ( !bStored ) {
		(void)__xrtTypedMPMCQueueRecycle(pQueue, pCell, sOperation);
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	do {
		Result = xrtMPMCQueueTryPush(&pQueue->Ready, pCell);
		if ( Result == XQUEUE_FULL ) {
			xrtAtomicPause();
		}
	} while ( Result == XQUEUE_FULL );
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, sOperation,
			"a prepared MPMC value slot could not be published"
		);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 初始化一个拥有固定值槽的 MPMC 类型队列。 */
XRT_API bool xrtTypedMPMCQueueInit(
	xtypedmpmcqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
)
{
	if ( pQueue == NULL ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, "init",
			"the MPMC typed queue is null"
		);
		return false;
	}
	memset(pQueue, 0, sizeof(*pQueue));
	if ( !xrtMPMCQueueInit(&pQueue->Ready, iCapacity) ) {
		return false;
	}
	if ( !__xrtTypedQueueCoreInit(
		&pQueue->Core, pItemType, pQueue->Ready.Capacity, "init"
	) || !xrtMPMCQueueInit(
		&pQueue->Free, pQueue->Ready.Capacity
	) || !xrtMPMCQueueInit(
		&pQueue->Retry, pQueue->Ready.Capacity
	) ) {
		__xrtTypedMPMCQueueRollback(pQueue);
		return false;
	}
	if ( !__xrtTypedMPMCQueueFillFree(pQueue) ) {
		__xrtTypedMPMCQueueRollback(pQueue);
		return false;
	}
	__xrtTypedQueueCoreActivate(&pQueue->Core);
	return true;
}



/* 在堆上创建一个 MPMC 类型队列。 */
XRT_API xtypedmpmcqueue* xrtTypedMPMCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
)
{
	xtypedmpmcqueue* pQueue = (xtypedmpmcqueue*)xrtMalloc(
		sizeof(xtypedmpmcqueue)
	);

	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtTypedMPMCQueueInit(pQueue, pItemType, iCapacity) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}



/* 释放全部 MPMC 队列值和内部环，但不释放结构。 */
XRT_API void xrtTypedMPMCQueueUnit(xtypedmpmcqueue* pQueue)
{
	(void)__xrtTypedMPMCQueueUnit(pQueue);
}



/* 释放 Create 返回的 MPMC 类型队列。 */
XRT_API void xrtTypedMPMCQueueDestroy(xtypedmpmcqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedMPMCQueueUnit(pQueue) ) {
		xrtFree(pQueue);
	}
}



/* 返回 MPMC 队列借用的元素类型。 */
XRT_API const xrttype* xrtTypedMPMCQueueItemType(
	const xtypedmpmcqueue* pQueue
)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "item-type"
	) ? pQueue->Core.ItemType : NULL;
}



/* 返回 MPMC 队列实际固定容量。 */
XRT_API size_t xrtTypedMPMCQueueCapacity(const xtypedmpmcqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "capacity"
	) ? pQueue->Core.Capacity : 0u;
}



/* 返回就绪环与移动失败重试环的并发近似元素数量。 */
XRT_API size_t xrtTypedMPMCQueueCount(const xtypedmpmcqueue* pQueue)
{
	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "count"
	) ) {
		return 0u;
	}
	return __xrtTypedQueueCount(
		&pQueue->Core,
		xrtMPMCQueueCount(&pQueue->Ready),
		xrtMPMCQueueCount(&pQueue->Retry)
	);
}



/* 失败原子地复制压入一个 MPMC 类型值。 */
XRT_API xqueueresult xrtTypedMPMCQueueTryPush(
	xtypedmpmcqueue* pQueue,
	const void* pItem
)
{
	return __xrtTypedMPMCQueuePush(
		pQueue, (ptr)pItem, false, "try-push"
	);
}



/* 移动压入一个外部已初始化 MPMC 类型值。 */
XRT_API xqueueresult xrtTypedMPMCQueueTryPushTake(
	xtypedmpmcqueue* pQueue,
	ptr pItem
)
{
	return __xrtTypedMPMCQueuePush(
		pQueue, pItem, true, "try-push-take"
	);
}



/* 由任意消费者移动弹出，失败时把元素放入重试环。 */
XRT_API xqueueresult xrtTypedMPMCQueueTryPop(
	xtypedmpmcqueue* pQueue,
	ptr pValue
)
{
	ptr pCell = NULL;
	xqueueresult Result;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "try-pop"
	) || !__xrtTypedQueueValueValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pValue, "try-pop"
	) || !__xrtTypedQueueCoreEnter(
		&pQueue->Core, pQueue, sizeof(*pQueue), "try-pop"
	) ) {
		return XQUEUE_ERROR;
	}
	Result = xrtMPMCQueueTryPop(&pQueue->Retry, &pCell);
	if ( Result == XQUEUE_EMPTY ) {
		Result = xrtMPMCQueueTryPop(&pQueue->Ready, &pCell);
	}
	if ( Result == XQUEUE_CLOSED ) {
		Result = xrtMPMCQueueTryPop(&pQueue->Retry, &pCell);
		if ( Result == XQUEUE_EMPTY ) {
			Result = xrtAtomic32Load(
				&pQueue->Core.Active, XMEMORY_ACQUIRE
			) == 1u ? XQUEUE_CLOSED : XQUEUE_EMPTY;
		}
	}
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return Result;
	}
	if ( !__xrtTypedQueueMoveOut(
		&pQueue->Core, pValue, pCell, "try-pop"
	) ) {
		(void)__xrtTypedMPMCQueueRetry(pQueue, pCell, "try-pop");
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	if ( !__xrtTypedMPMCQueueRecycle(pQueue, pCell, "try-pop") ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 复制压入连续 MPMC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedMPMCQueuePushBatch(
	xtypedmpmcqueue* pQueue,
	const void* pItems,
	size_t iCount
)
{
	xqueuebatchresult Batch = { XQUEUE_OK, 0u };
	const bytes pValues = (const bytes)pItems;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "push-batch"
	) || !__xrtTypedQueueValuesValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pItems, iCount,
		"push-batch"
	) ) {
		Batch.Result = XQUEUE_ERROR;
		return Batch;
	}
	while ( Batch.Count < iCount ) {
		xqueueresult Result = xrtTypedMPMCQueueTryPush(
			pQueue,
			pValues + (Batch.Count * pQueue->Core.ItemType->Size)
		);

		if ( Result != XQUEUE_OK ) {
			Batch.Result = Batch.Count != 0u ? XQUEUE_OK : Result;
			return Batch;
		}
		Batch.Count++;
	}
	return Batch;
}



/* 移动弹出到连续已初始化 MPMC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedMPMCQueuePopBatch(
	xtypedmpmcqueue* pQueue,
	ptr pValues,
	size_t iCapacity
)
{
	xqueuebatchresult Batch = { XQUEUE_OK, 0u };
	bytes pOutput = (bytes)pValues;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "pop-batch"
	) || !__xrtTypedQueueValuesValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pValues, iCapacity,
		"pop-batch"
	) ) {
		Batch.Result = XQUEUE_ERROR;
		return Batch;
	}
	while ( Batch.Count < iCapacity ) {
		xqueueresult Result = xrtTypedMPMCQueueTryPop(
			pQueue,
			pOutput + (Batch.Count * pQueue->Core.ItemType->Size)
		);

		if ( Result != XQUEUE_OK ) {
			Batch.Result = Batch.Count != 0u ? XQUEUE_OK : Result;
			return Batch;
		}
		Batch.Count++;
	}
	return Batch;
}



/* 在全部生产者退出后关闭 MPMC 类型队列。 */
XRT_API void xrtTypedMPMCQueueClose(xtypedmpmcqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "close"
	) ) {
		xrtMPMCQueueClose(&pQueue->Ready);
	}
}



/* 判断 MPMC 类型队列是否已经关闭写端。 */
XRT_API bool xrtTypedMPMCQueueIsClosed(const xtypedmpmcqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "is-closed"
	) && xrtMPMCQueueIsClosed(&pQueue->Ready);
}



/* 判断 MPMC 类型队列是否关闭、排空且无进行中的值回调。 */
XRT_API bool xrtTypedMPMCQueueIsDrained(const xtypedmpmcqueue* pQueue)
{
	return xrtTypedMPMCQueueIsClosed(pQueue) &&
		(xrtTypedMPMCQueueCount(pQueue) == 0u) &&
		(xrtAtomic32Load(&pQueue->Core.Active, XMEMORY_ACQUIRE) == 0u);
}



/* 在独占且排空后重置全部 MPMC 环并重新开放。 */
XRT_API bool xrtTypedMPMCQueueReset(xtypedmpmcqueue* pQueue)
{
	uint32 iPrevious;
	ptr pCell;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "reset"
	) || !__xrtTypedQueueCoreExclusive(
		&pQueue->Core, false, &iPrevious, "reset"
	) ) {
		return false;
	}
	if ( (xrtMPMCQueueCount(&pQueue->Ready) != 0u) ||
		 (xrtMPMCQueueCount(&pQueue->Retry) != 0u) ) {
		__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
		__xrtTypedQueueError(
			XERR_AGAIN, XTYPED_QUEUE_ERROR_STATE, "reset",
			"the MPMC typed queue must be drained before reset"
		);
		return false;
	}
	while ( xrtMPMCQueueTryPop(&pQueue->Free, &pCell) == XQUEUE_OK ) {
	}
	if ( !xrtMPMCQueueReset(&pQueue->Ready) ||
		 !xrtMPMCQueueReset(&pQueue->Free) ||
		 !xrtMPMCQueueReset(&pQueue->Retry) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the MPMC pointer rings could not be reset"
		);
		return false;
	}
	if ( !__xrtTypedMPMCQueueFillFree(pQueue) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the MPMC free value slots could not be restored"
		);
		return false;
	}
	__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
	return true;
}



/* 从对象类型参数和元数据初始化 MPMC 队列负载。 */
static bool __xrtTypedMPMCQueueInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	const xtypedqueuemeta* pMeta;

	if ( !xrtTypedMPMCQueueTypeValidate(pType) ) {
		return false;
	}
	pMeta = (const xtypedqueuemeta*)pType->Metadata;
	return xrtTypedMPMCQueueInit(
		(xtypedmpmcqueue*)pInstance, pType->Arguments[0], pMeta->Capacity
	);
}



/* 销毁对象负载中的 MPMC 类型队列。 */
static void __xrtTypedMPMCQueueInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedMPMCQueueUnit((xtypedmpmcqueue*)pInstance);
}



/* 枚举 MPMC 固定值槽直接拥有的全部强对象引用。 */
static bool __xrtTypedMPMCQueueInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	(void)pType;
	return __xrtTypedQueueTrace(
		&((xtypedmpmcqueue*)pInstance)->Core,
		pInstance,
		sizeof(xtypedmpmcqueue),
		pVisit,
		pContext,
		"instance-trace"
	);
}



/* 返回 MPMC 类型队列共享实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedMPMCQueueInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedMPMCQueueInstanceInit,
		.Drop = __xrtTypedMPMCQueueInstanceDrop,
		.Trace = __xrtTypedMPMCQueueInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的 MPMC 类型队列描述。 */
XRT_API bool xrtTypedMPMCQueueTypeValidate(const xrttype* pType)
{
	return __xrtTypedQueueTypeValidate(
		pType,
		sizeof(xtypedmpmcqueue),
		XRT_INTERNAL_OBJECT_ALIGNOF(xtypedmpmcqueue),
		xrtTypedMPMCQueueInstanceOps(),
		"type-validate"
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_stack.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_STACK)



#if defined(XRUNTIME_FEATURE_TYPED_STACK)

/* 初始化一个拥有类型值的空栈。 */
XRT_API bool xrtTypedStackInit(
	xtypedstack* pStack,
	const xrttype* pItemType
)
{
	return xrtTypedArrayInit(pStack, pItemType);
}



/* 在堆上创建一个拥有类型值的空栈。 */
XRT_API xtypedstack* xrtTypedStackCreate(const xrttype* pItemType)
{
	return (xtypedstack*)xrtTypedArrayCreate(pItemType);
}



/* 释放全部元素和连续存储，但不释放栈结构。 */
XRT_API void xrtTypedStackUnit(xtypedstack* pStack)
{
	xrtTypedArrayUnit(pStack);
}



/* 释放全部元素、连续存储和堆栈结构。 */
XRT_API void xrtTypedStackDestroy(xtypedstack* pStack)
{
	xrtTypedArrayDestroy(pStack);
}



/* 返回栈借用的元素类型描述。 */
XRT_API const xrttype* xrtTypedStackItemType(const xtypedstack* pStack)
{
	return xrtTypedArrayItemType(pStack);
}



/* 返回当前栈深度。 */
XRT_API size_t xrtTypedStackCount(const xtypedstack* pStack)
{
	return xrtTypedArrayCount(pStack);
}



/* 返回当前连续存储容量。 */
XRT_API size_t xrtTypedStackCapacity(const xtypedstack* pStack)
{
	return xrtTypedArrayCapacity(pStack);
}



/* 销毁全部元素并保留容量。 */
XRT_API void xrtTypedStackClear(xtypedstack* pStack)
{
	xrtTypedArrayClear(pStack);
}



/* 保证栈至少具有指定容量。 */
XRT_API bool xrtTypedStackReserve(xtypedstack* pStack, size_t iCapacity)
{
	return xrtTypedArrayReserve(pStack, iCapacity);
}



/* 把容量裁剪到当前深度。 */
XRT_API bool xrtTypedStackTrim(xtypedstack* pStack)
{
	return xrtTypedArrayTrim(pStack);
}



/* 失败原子地复制压入一个类型值。 */
XRT_API bool xrtTypedStackPush(
	xtypedstack* pStack,
	const void* pItem
)
{
	return xrtTypedArrayPush(pStack, pItem);
}



/* 移动或销毁栈顶值，并从栈中删除它。 */
XRT_API bool xrtTypedStackPop(xtypedstack* pStack, ptr pValue)
{
	size_t iCount = xrtTypedArrayCount(pStack);

	if ( iCount == 0u ) {
		return false;
	}
	if ( pValue != NULL ) {
		return xrtTypedArrayPop(pStack, pValue);
	}
	return xrtTypedArrayRemove(pStack, iCount - 1u, 1u);
}



/* 事务压入一批同类型值，来源顺序与逐次 Push 完全一致。 */
XRT_API bool xrtTypedStackPushBatch(
	xtypedstack* pStack,
	const xtypedarray* pItems
)
{
	return xrtTypedArrayAppend(pStack, pItems);
}



/* 原子移交尾部，按逐次 Pop 顺序交付拥有数组。 */
XRT_API xtypedarray* xrtTypedStackPopBatch(
	xtypedstack* pStack,
	size_t iMaxCount
)
{
	return xrtTypedArrayTakeTail(pStack, iMaxCount, true);
}



/* 复制栈顶向下的最多指定数量；起点超过深度由 Slice 报范围错误。 */
XRT_API xtypedarray* xrtTypedStackPeekBatch(
	const xtypedstack* pStack,
	size_t iDepth,
	size_t iMaxCount
)
{
	/* ItemType validates before Count, including the empty/busy case. */
	if ( xrtTypedArrayItemType(pStack) == NULL ) {
		return NULL;
	}
	size_t iCount = xrtTypedArrayCount(pStack);
	if ( iDepth > iCount ) {
		return xrtTypedArraySlice(pStack, iDepth, 0u, true);
	}
	size_t iAvailable = iCount - iDepth;
	size_t iTake = iMaxCount < iAvailable ? iMaxCount : iAvailable;
	return xrtTypedArraySlice(pStack, iAvailable - iTake, iTake, true);
}



/* 按距栈顶深度返回可写借用值。 */
XRT_API ptr xrtTypedStackPeek(xtypedstack* pStack, size_t iDepth)
{
	size_t iCount = xrtTypedArrayCount(pStack);

	return iDepth < iCount ?
		xrtTypedArrayGet(pStack, iCount - iDepth - 1u) : NULL;
}



/* 按距栈顶深度返回只读借用值。 */
XRT_API const void* xrtTypedStackConstPeek(
	const xtypedstack* pStack,
	size_t iDepth
)
{
	size_t iCount = xrtTypedArrayCount(pStack);

	return iDepth < iCount ?
		xrtTypedArrayConstGet(pStack, iCount - iDepth - 1u) : NULL;
}



/* 返回可写栈顶借用值。 */
XRT_API ptr xrtTypedStackTop(xtypedstack* pStack)
{
	return xrtTypedStackPeek(pStack, 0u);
}



/* 返回只读栈顶借用值。 */
XRT_API const void* xrtTypedStackConstTop(const xtypedstack* pStack)
{
	return xrtTypedStackConstPeek(pStack, 0u);
}



/* 深复制一个独立类型栈。 */
XRT_API xtypedstack* xrtTypedStackClone(const xtypedstack* pStack)
{
	return (xtypedstack*)xrtTypedArrayClone(pStack);
}



/* 比较两个栈的精确元素类型、深度和顺序。 */
XRT_API bool xrtTypedStackEquals(
	const xtypedstack* pLeft,
	const xtypedstack* pRight
)
{
	return xrtTypedArrayEquals(pLeft, pRight);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_tree.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_TREE)




#if defined(XRUNTIME_FEATURE_TYPED_TREE)

#define XRT_TYPED_TREE_FLAG_READY 0x0001u
#define XRT_TYPED_TREE_FLAG_BUSY  0x0002u
#define XRT_TYPED_TREE_FLAGS      0x0003u



/* 设置类型树模块结构化错误。 */
static void __xrtTypedTreeError(
	xerrkind Kind,
	xtypedtreeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-tree";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型、AVL 或节点池错误补充类型树上下文。 */
static void __xrtTypedTreeWrap(
	xerrkind DefaultKind,
	xtypedtreeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-tree";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 在清理资源后恢复进入清理阶段时持有的根错误。 */
static void __xrtTypedTreeRestoreError(xerror* pError)
{
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 验证键或值类型能够由类型树安全拥有。 */
static bool __xrtTypedTreeValueTypeValidate(
	const xrttype* pType,
	bool bKey,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedTreeWrap(XERR_ARGUMENT, XTYPED_TREE_ERROR_TYPE,
			sOperation, bKey ? "the tree key type is invalid" :
			"the tree value type is invalid");
		return false;
	}
	if ( pType->Size == 0u ) {
		__xrtTypedTreeError(XERR_TYPE, XTYPED_TREE_ERROR_TYPE,
			sOperation, bKey ? "a tree key must occupy storage" :
			"a tree value must occupy storage");
		return false;
	}
	if ( !xrtTypeIsCopyable(pType) ) {
		__xrtTypedTreeError(XERR_UNSUPPORTED, XTYPED_TREE_ERROR_TYPE,
			sOperation, bKey ? "the tree key type is not copyable" :
			"the tree value type is not copyable");
		return false;
	}
	if ( bKey && !xrtTypeIsComparable(pType) ) {
		__xrtTypedTreeError(XERR_UNSUPPORTED, XTYPED_TREE_ERROR_TYPE,
			sOperation, "the tree key type is not comparable");
		return false;
	}
	return true;
}



/* 向上对齐布局偏移并检查大小溢出。 */
static bool __xrtTypedTreeAlign(
	size_t iValue,
	size_t iAlignment,
	size_t* pResult
)
{
	size_t iMask = iAlignment - 1u;

	if ( iValue > (SIZE_MAX - iMask) ) {
		return false;
	}
	*pResult = (iValue + iMask) & ~iMask;
	return true;
}



/* 计算键和值在一个节点对象中的紧凑对齐布局。 */
static bool __xrtTypedTreeLayout(
	const xrttype* pKeyType,
	const xrttype* pValueType,
	size_t* pValueOffset,
	size_t* pEntrySize,
	size_t* pAlignment
)
{
	size_t iOffset;
	size_t iAlignment = pKeyType->Align > pValueType->Align ?
		pKeyType->Align : pValueType->Align;

	if ( iAlignment < XRT_POOL_ALIGNMENT_DEFAULT ) {
		iAlignment = XRT_POOL_ALIGNMENT_DEFAULT;
	}
	if ( !__xrtTypedTreeAlign(
		pKeyType->Size, pValueType->Align, &iOffset
	) || (pValueType->Size > (SIZE_MAX - iOffset)) ) {
		__xrtTypedTreeError(XERR_RANGE, XTYPED_TREE_ERROR_LAYOUT,
			"init", "the typed tree entry layout overflows");
		return false;
	}
	*pValueOffset = iOffset;
	*pEntrySize = iOffset + pValueType->Size;
	*pAlignment = iAlignment;
	return true;
}



/* 在用户类型回调期间拒绝当前树的全部公开 API，并保留外层门禁状态。 */
static bool __xrtTypedTreeCallbackBegin(const xtypedtree* pTree)
{
	bool bBusy = (pTree->Flags & XRT_TYPED_TREE_FLAG_BUSY) != 0u;

	((xtypedtree*)pTree)->Flags |= XRT_TYPED_TREE_FLAG_BUSY;
	return bBusy;
}



/* 结束当前树的用户类型回调门禁，并恢复进入前的嵌套状态。 */
static void __xrtTypedTreeCallbackEnd(const xtypedtree* pTree, bool bBusy)
{
	if ( !bBusy ) {
		((xtypedtree*)pTree)->Flags &= ~XRT_TYPED_TREE_FLAG_BUSY;
	}
}



/* 返回节点对象中的键槽。 */
static ptr __xrtTypedTreeKey(const xtypedtree* pTree, const void* pEntry)
{
	return pEntry != NULL ?
		(ptr)((const bytes)pEntry + pTree->KeyOffset) : NULL;
}



/* 返回节点对象中的值槽。 */
static ptr __xrtTypedTreeValue(const xtypedtree* pTree, const void* pEntry)
{
	return pEntry != NULL ?
		(ptr)((const bytes)pEntry + pTree->ValueOffset) : NULL;
}



/* 使用运行时键类型比较查询键和节点规范键。 */
static int __xrtTypedTreeCompare(
	const void* pKey,
	const void* pEntry,
	ptr pUserData
)
{
	xtypedtree* pTree = (xtypedtree*)pUserData;
	int iCompare = 0;
	bool bBusy;

	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	(void)xrtTypeCompareValue(
		pTree->KeyType,
		pKey,
		__xrtTypedTreeKey(pTree, pEntry),
		&iCompare
	);
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	return iCompare;
}



/* 销毁节点拥有的完整值和键。 */
static void __xrtTypedTreeDrop(ptr pEntry, ptr pUserData)
{
	xtypedtree* pTree = (xtypedtree*)pUserData;
	bool bBusy;

	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	xrtTypeDropValue(
		pTree->ValueType, __xrtTypedTreeValue(pTree, pEntry)
	);
	xrtTypeDropValue(
		pTree->KeyType, __xrtTypedTreeKey(pTree, pEntry)
	);
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
}



/* 检查公开类型树状态、布局和底层回调是否一致。 */
static bool __xrtTypedTreeValid(
	const xtypedtree* pTree,
	cstr sOperation
)
{
	if ( (pTree == NULL) || (pTree->KeyType == NULL) ||
		 (pTree->ValueType == NULL) ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, "the typed tree is null or uninitialized");
		return false;
	}
	if (
		((pTree->Flags & XRT_TYPED_TREE_FLAG_READY) == 0u) ||
		((pTree->Flags & XRT_TYPED_TREE_FLAG_BUSY) != 0u) ||
		((pTree->Flags & ~XRT_TYPED_TREE_FLAGS) != 0u)
	) {
		__xrtTypedTreeError(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree is not available for API access");
		return false;
	}
	if ( !__xrtAVLTreeValid(&pTree->Storage) ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree storage is invalid");
		return false;
	}
	if (
		(pTree->KeyOffset != 0u) ||
		(pTree->ValueOffset < pTree->KeyType->Size) ||
		(pTree->EntrySize != pTree->Storage.ItemSize) ||
		(pTree->Alignment != pTree->Storage.Alignment) ||
		(pTree->Storage.Compare != __xrtTypedTreeCompare) ||
		(pTree->Storage.Drop != __xrtTypedTreeDrop) ||
		(pTree->Storage.UserData != pTree)
	) {
		__xrtTypedTreeError(XERR_STATE, XTYPED_TREE_ERROR_LAYOUT,
			sOperation, "the typed tree layout or callbacks are invalid");
		return false;
	}
	return true;
}



/* 检查当前树是否允许结构和生命周期修改。 */
static bool __xrtTypedTreeCanMutate(
	const xtypedtree* pTree,
	cstr sOperation
)
{
	if ( !__xrtTypedTreeValid(pTree, sOperation) ) {
		return false;
	}
	if ( !__xrtAVLTreeCanMutate(&pTree->Storage) ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree is currently being visited");
		return false;
	}
	return true;
}



/* 判断字节区间是否触及类型树结构或节点池。 */
static bool __xrtTypedTreeOwnsRange(
	const xtypedtree* pTree,
	const void* pMemory,
	size_t iSize
)
{
	return __xrtRangesOverlap(pMemory, iSize, pTree, sizeof(*pTree)) ||
		__xrtAVLTreeOwnsRange(&pTree->Storage, pMemory, iSize);
}



/* 验证内部来源正好指向一个活动键槽或值槽。 */
static bool __xrtTypedTreeSourceValid(
	xtypedtree* pTree,
	const void* pSource,
	bool bKey,
	cstr sOperation
)
{
	xavltreeiter Iterator;
	ptr pEntry;
	size_t iSize = bKey ? pTree->KeyType->Size : pTree->ValueType->Size;
	bool bExact = false;

	if ( pSource == NULL ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, bKey ? "the source key is null" :
			"the source value is null");
		return false;
	}
	if ( !__xrtTypedTreeOwnsRange(pTree, pSource, iSize) ) {
		return true;
	}
	if ( !xrtAVLTreeIterBegin(&pTree->Storage, &Iterator) ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree source scan could not start");
		return false;
	}
	while ( (pEntry = xrtAVLTreeIterNext(&Iterator)) != NULL ) {
		ptr pExpected = bKey ? __xrtTypedTreeKey(pTree, pEntry) :
			__xrtTypedTreeValue(pTree, pEntry);

		if ( pExpected == pSource ) {
			bExact = true;
			break;
		}
	}
	xrtAVLTreeIterEnd(&Iterator);
	if ( !bExact ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, bKey ?
			"an internal key source must be an active key boundary" :
			"an internal value source must be an active value boundary");
		return false;
	}
	return true;
}



/* 验证移动来源或输出完全位于类型树拥有的内存之外。 */
static bool __xrtTypedTreeExternalValue(
	const xtypedtree* pTree,
	const void* pValue,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, "the movable value is null");
		return false;
	}
	if ( __xrtTypedTreeOwnsRange(
		pTree, pValue, pTree->ValueType->Size
	) ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, "the movable value must not alias typed tree storage");
		return false;
	}
	return true;
}



/* 验证移动值或输出不会在后续树查询前改写外部查询键。 */
static bool __xrtTypedTreeMoveSeparate(
	const xtypedtree* pTree,
	const void* pKey,
	const void* pValue,
	cstr sOperation
)
{
	if ( __xrtRangesOverlap(
		pKey,
		pTree->KeyType->Size,
		pValue,
		pTree->ValueType->Size
	) ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, "the movable value must not overlap the search key");
		return false;
	}
	return true;
}



/* 节点初始化模式区分默认值、复制值和移动值。 */
typedef enum __xrt_typed_tree_init_mode {
	XRT_TYPED_TREE_INIT_DEFAULT = 0,
	XRT_TYPED_TREE_INIT_COPY,
	XRT_TYPED_TREE_INIT_MOVE
} __xrt_typed_tree_init_mode;



/* 新节点初始化上下文保存来源键值和生命周期模式。 */
typedef struct __xrt_typed_tree_init_context {
	xtypedtree* Tree;
	const void* Key;
	ptr Value;
	__xrt_typed_tree_init_mode Mode;
} __xrt_typed_tree_init_context;



/* 初始化并复制一个尚未提交的新树节点。 */
static bool __xrtTypedTreeInitEntry(
	ptr pEntry,
	const void* pKey,
	ptr pUserData
)
{
	__xrt_typed_tree_init_context* pContext =
		(__xrt_typed_tree_init_context*)pUserData;
	xtypedtree* pTree = pContext->Tree;
	ptr pStoredKey = __xrtTypedTreeKey(pTree, pEntry);
	ptr pStoredValue = __xrtTypedTreeValue(pTree, pEntry);
	xerror* pError;
	bool bSuccess;
	bool bBusy;

	(void)pKey;
	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	if ( !xrtTypeInitValue(pTree->KeyType, pStoredKey) ) {
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		return false;
	}
	if ( !xrtTypeCopyValue(pTree->KeyType, pStoredKey, pContext->Key) ) {
		pError = xrtTakeError();
		xrtTypeDropValue(pTree->KeyType, pStoredKey);
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeRestoreError(pError);
		return false;
	}
	if ( !xrtTypeInitValue(pTree->ValueType, pStoredValue) ) {
		pError = xrtTakeError();
		xrtTypeDropValue(pTree->KeyType, pStoredKey);
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeRestoreError(pError);
		return false;
	}
	bSuccess = pContext->Mode == XRT_TYPED_TREE_INIT_DEFAULT;
	if ( pContext->Mode == XRT_TYPED_TREE_INIT_COPY ) {
		bSuccess = xrtTypeCopyValue(
			pTree->ValueType, pStoredValue, pContext->Value
		);
	} else if ( pContext->Mode == XRT_TYPED_TREE_INIT_MOVE ) {
		bSuccess = xrtTypeMoveValue(
			pTree->ValueType, pStoredValue, pContext->Value
		);
	}
	if ( !bSuccess ) {
		pError = xrtTakeError();
		xrtTypeDropValue(pTree->ValueType, pStoredValue);
		xrtTypeDropValue(pTree->KeyType, pStoredKey);
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeRestoreError(pError);
		return false;
	}
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	return true;
}



/* 回滚一个已经完整初始化但尚未提交的节点。 */
static void __xrtTypedTreeRollbackEntry(ptr pEntry, ptr pUserData)
{
	__xrt_typed_tree_init_context* pContext =
		(__xrt_typed_tree_init_context*)pUserData;

	__xrtTypedTreeDrop(pEntry, pContext->Tree);
}



/* 为缺失键建立节点，并返回值槽。 */
static ptr __xrtTypedTreeInsert(
	xtypedtree* pTree,
	const void* pKey,
	ptr pValue,
	__xrt_typed_tree_init_mode Mode,
	bool* pNew,
	cstr sOperation
)
{
	__xrt_typed_tree_init_context Context = {
		pTree,
		pKey,
		pValue,
		Mode
	};
	ptr pEntry = __xrtAVLTreeGetOrAdd(
		&pTree->Storage,
		pKey,
		__xrtTypedTreeInitEntry,
		&Context,
		__xrtTypedTreeRollbackEntry,
		&Context,
		pNew
	);

	if ( pEntry == NULL ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			sOperation, "the typed tree node could not be inserted");
		return NULL;
	}
	return __xrtTypedTreeValue(pTree, pEntry);
}



/* 修复类型树按值交换后保存的内部自引用。 */
static void __xrtTypedTreeRepair(xtypedtree* pTree)
{
	pTree->Storage.UserData = pTree;
}



/* 递增结构版本并跳过外置迭代器保留的零值。 */
static uint64 __xrtTypedTreeNextVersion(uint64 iVersion)
{
	iVersion++;
	return iVersion != 0u ? iVersion : 1u;
}



/* 交换两个静止且有效的类型树，并修复底层自引用。 */
static void __xrtTypedTreeSwap(xtypedtree* pLeft, xtypedtree* pRight)
{
	xtypedtree Tree = *pLeft;

	*pLeft = *pRight;
	*pRight = Tree;
	__xrtTypedTreeRepair(pLeft);
	__xrtTypedTreeRepair(pRight);
}



/* 初始化一个拥有类型键值的空树。 */
XRT_API bool xrtTypedTreeInit(
	xtypedtree* pTree,
	const xrttype* pKeyType,
	const xrttype* pValueType
)
{
	size_t iValueOffset;
	size_t iEntrySize;
	size_t iAlignment;

	if ( pTree == NULL ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			"init", "the typed tree is null");
		return false;
	}
	memset(pTree, 0, sizeof(*pTree));
	if ( !__xrtTypedTreeValueTypeValidate(pKeyType, true, "init") ||
		 !__xrtTypedTreeValueTypeValidate(pValueType, false, "init") ||
		 !__xrtTypedTreeLayout(
			pKeyType, pValueType, &iValueOffset, &iEntrySize, &iAlignment
		) ) {
		return false;
	}
	pTree->KeyType = pKeyType;
	pTree->ValueType = pValueType;
	pTree->KeyOffset = 0u;
	pTree->ValueOffset = iValueOffset;
	pTree->EntrySize = iEntrySize;
	pTree->Alignment = iAlignment;
	if ( !xrtAVLTreeInitAligned(
		&pTree->Storage,
		iEntrySize,
		iAlignment,
		__xrtTypedTreeCompare,
		pTree
	) || !xrtAVLTreeSetDrop(&pTree->Storage, __xrtTypedTreeDrop) ) {
		if ( pTree->Storage.Compare != NULL ) {
			xrtAVLTreeUnit(&pTree->Storage);
		}
		memset(pTree, 0, sizeof(*pTree));
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			"init", "the typed tree storage could not be initialized");
		return false;
	}
	pTree->Flags = XRT_TYPED_TREE_FLAG_READY;
	return true;
}



/* 在堆上创建一个拥有类型键值的空树。 */
XRT_API xtypedtree* xrtTypedTreeCreate(
	const xrttype* pKeyType,
	const xrttype* pValueType
)
{
	xtypedtree* pTree = (xtypedtree*)xrtMalloc(sizeof(*pTree));

	if ( pTree == NULL ) {
		return NULL;
	}
	if ( !xrtTypedTreeInit(pTree, pKeyType, pValueType) ) {
		xrtFree(pTree);
		return NULL;
	}
	return pTree;
}



/* 释放全部键值和节点池，但不释放树结构。 */
XRT_API void xrtTypedTreeUnit(xtypedtree* pTree)
{
	if ( pTree == NULL ) {
		return;
	}
	if ( (pTree->KeyType == NULL) && (pTree->ValueType == NULL) &&
		 (pTree->Flags == 0u) ) {
		return;
	}
	if ( !__xrtTypedTreeCanMutate(pTree, "unit") ) {
		return;
	}
	xrtAVLTreeUnit(&pTree->Storage);
	memset(pTree, 0, sizeof(*pTree));
}



/* 释放全部键值、节点池和堆树结构。 */
XRT_API void xrtTypedTreeDestroy(xtypedtree* pTree)
{
	if ( pTree == NULL ) {
		return;
	}
	if ( !__xrtTypedTreeCanMutate(pTree, "destroy") ) {
		return;
	}
	xrtTypedTreeUnit(pTree);
	xrtFree(pTree);
}



/* 返回树借用的键类型描述。 */
XRT_API const xrttype* xrtTypedTreeKeyType(const xtypedtree* pTree)
{
	return __xrtTypedTreeValid(pTree, "key-type") ? pTree->KeyType : NULL;
}



/* 返回树借用的值类型描述。 */
XRT_API const xrttype* xrtTypedTreeValueType(const xtypedtree* pTree)
{
	return __xrtTypedTreeValid(pTree, "value-type") ? pTree->ValueType : NULL;
}



/* 返回当前键值数量。 */
XRT_API size_t xrtTypedTreeCount(const xtypedtree* pTree)
{
	return __xrtTypedTreeValid(pTree, "count") ?
		xrtAVLTreeCount(&pTree->Storage) : 0u;
}



/* 销毁全部键值并保留节点池供复用。 */
XRT_API bool xrtTypedTreeClear(xtypedtree* pTree)
{
	if ( !__xrtTypedTreeCanMutate(pTree, "clear") ) {
		return false;
	}
	xrtAVLTreeClear(&pTree->Storage);
	return true;
}



/* 释放多余空节点池页并返回实际释放页数。 */
XRT_API size_t xrtTypedTreeTrim(xtypedtree* pTree, size_t iRetainEmpty)
{
	if ( !__xrtTypedTreeCanMutate(pTree, "trim") ) {
		return 0u;
	}
	return xrtPoolTrim(&pTree->Storage.Pool, iRetainEmpty);
}



/* 返回已有值槽，或复制键并默认初始化一个新值。 */
XRT_API ptr xrtTypedTreeGetOrAdd(
	xtypedtree* pTree,
	const void* pKey,
	bool* pNew
)
{
	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtTypedTreeCanMutate(pTree, "get-or-add") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "get-or-add") ) {
		return NULL;
	}
	return __xrtTypedTreeInsert(
		pTree, pKey, NULL, XRT_TYPED_TREE_INIT_DEFAULT, pNew, "get-or-add"
	);
}



/* 失败原子地复制插入或替换一个键值。 */
XRT_API bool xrtTypedTreeSet(
	xtypedtree* pTree,
	const void* pKey,
	const void* pValue
)
{
	ptr pStored;
	bool bNew;
	bool bBusy;

	if ( !__xrtTypedTreeCanMutate(pTree, "set") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "set") ||
		 !__xrtTypedTreeSourceValid(pTree, pValue, false, "set") ) {
		return false;
	}
	pStored = __xrtTypedTreeInsert(
		pTree,
		pKey,
		(ptr)pValue,
		XRT_TYPED_TREE_INIT_COPY,
		&bNew,
		"set"
	);
	if ( pStored == NULL ) {
		return false;
	}
	if ( bNew ) {
		return true;
	}
	if ( pStored == pValue ) {
		return true;
	}
	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	if ( !xrtTypeCopyValue(pTree->ValueType, pStored, pValue) ) {
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			"set", "the existing typed tree value could not be replaced");
		return false;
	}
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	return true;
}



/* 移动外部已初始化值插入或替换一个键值。 */
XRT_API bool xrtTypedTreeSetTake(
	xtypedtree* pTree,
	const void* pKey,
	ptr pValue
)
{
	ptr pStored;
	bool bNew;
	bool bBusy;

	if ( !__xrtTypedTreeCanMutate(pTree, "set-take") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "set-take") ||
		 !__xrtTypedTreeExternalValue(pTree, pValue, "set-take") ||
		 !__xrtTypedTreeMoveSeparate(
			pTree, pKey, pValue, "set-take"
		) ) {
		return false;
	}
	pStored = __xrtTypedTreeInsert(
		pTree,
		pKey,
		pValue,
		XRT_TYPED_TREE_INIT_MOVE,
		&bNew,
		"set-take"
	);
	if ( pStored == NULL ) {
		return false;
	}
	if ( bNew ) {
		return true;
	}
	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	if ( !xrtTypeMoveValue(pTree->ValueType, pStored, pValue) ) {
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			"set-take", "the existing typed tree value could not be moved");
		return false;
	}
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	return true;
}



/* 返回指定键的可写借用值槽。 */
XRT_API ptr xrtTypedTreeGet(xtypedtree* pTree, const void* pKey)
{
	ptr pEntry;

	if ( !__xrtTypedTreeValid(pTree, "get") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "get") ) {
		return NULL;
	}
	pEntry = xrtAVLTreeFind(&pTree->Storage, pKey);
	return __xrtTypedTreeValue(pTree, pEntry);
}



/* 返回指定键的只读借用值槽。 */
XRT_API const void* xrtTypedTreeConstGet(
	const xtypedtree* pTree,
	const void* pKey
)
{
	const void* pEntry;

	if ( !__xrtTypedTreeValid(pTree, "const-get") ||
		 !__xrtTypedTreeSourceValid(
			(xtypedtree*)pTree, pKey, true, "const-get"
		) ) {
		return NULL;
	}
	pEntry = xrtAVLTreeConstFind(&pTree->Storage, pKey);
	return __xrtTypedTreeValue(pTree, pEntry);
}



/* 判断指定键是否存在。 */
XRT_API bool xrtTypedTreeHas(
	const xtypedtree* pTree,
	const void* pKey
)
{
	if ( !__xrtTypedTreeValid(pTree, "has") ||
		 !__xrtTypedTreeSourceValid(
			(xtypedtree*)pTree, pKey, true, "has"
		) ) {
		return false;
	}
	return xrtAVLTreeHas(&pTree->Storage, pKey);
}



/* 返回指定查询对应的内部规范键。 */
XRT_API const void* xrtTypedTreeStoredKey(
	const xtypedtree* pTree,
	const void* pKey
)
{
	const void* pEntry;

	if ( !__xrtTypedTreeValid(pTree, "stored-key") ||
		 !__xrtTypedTreeSourceValid(
			(xtypedtree*)pTree, pKey, true, "stored-key"
		) ) {
		return NULL;
	}
	pEntry = xrtAVLTreeConstFind(&pTree->Storage, pKey);
	return __xrtTypedTreeKey(pTree, pEntry);
}



/* 删除指定键并销毁对应键值。 */
XRT_API bool xrtTypedTreeRemove(xtypedtree* pTree, const void* pKey)
{
	if ( !__xrtTypedTreeCanMutate(pTree, "remove") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "remove") ) {
		return false;
	}
	return xrtAVLTreeRemove(&pTree->Storage, pKey);
}



/* 把值移动到外部已初始化输出后删除键值。 */
XRT_API bool xrtTypedTreeTake(
	xtypedtree* pTree,
	const void* pKey,
	ptr pValue
)
{
	ptr pEntry;
	ptr pStored;
	bool bBusy;

	if ( !__xrtTypedTreeCanMutate(pTree, "take") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "take") ||
		 !__xrtTypedTreeExternalValue(pTree, pValue, "take") ||
		 !__xrtTypedTreeMoveSeparate(pTree, pKey, pValue, "take") ) {
		return false;
	}
	pEntry = xrtAVLTreeFind(&pTree->Storage, pKey);
	if ( pEntry == NULL ) {
		return false;
	}
	pStored = __xrtTypedTreeValue(pTree, pEntry);
	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	if ( !xrtTypeMoveValue(pTree->ValueType, pValue, pStored) ) {
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			"take", "the typed tree value could not be moved out");
		return false;
	}
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	if ( !xrtAVLTreeRemove(&pTree->Storage, pKey) ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			"take", "the moved typed tree entry could not be removed");
		return false;
	}
	return true;
}



/* 将底层树条目投影为借用值，并可返回对应的内部规范键。 */
static ptr __xrtTypedTreeResult(
	xtypedtree* pTree,
	ptr pEntry,
	const void** pKey
)
{
	if ( pKey != NULL ) {
		*pKey = pEntry != NULL ? __xrtTypedTreeKey(pTree, pEntry) : NULL;
	}
	return __xrtTypedTreeValue(pTree, pEntry);
}



/* 返回按键升序排列的第一项。 */
XRT_API ptr xrtTypedTreeFirst(xtypedtree* pTree, const void** pKey)
{
	if ( pKey != NULL ) {
		*pKey = NULL;
	}
	if ( !__xrtTypedTreeValid(pTree, "first") ) {
		return NULL;
	}
	return __xrtTypedTreeResult(
		pTree, xrtAVLTreeFirst(&pTree->Storage), pKey
	);
}



/* 返回按键升序排列的最后一项。 */
XRT_API ptr xrtTypedTreeLast(xtypedtree* pTree, const void** pKey)
{
	if ( pKey != NULL ) {
		*pKey = NULL;
	}
	if ( !__xrtTypedTreeValid(pTree, "last") ) {
		return NULL;
	}
	return __xrtTypedTreeResult(
		pTree, xrtAVLTreeLast(&pTree->Storage), pKey
	);
}



/* 返回第一项不小于查询键的值和内部规范键。 */
XRT_API ptr xrtTypedTreeLowerBound(
	xtypedtree* pTree,
	const void* pSearchKey,
	const void** pStoredKey
)
{
	if ( pStoredKey != NULL ) {
		*pStoredKey = NULL;
	}
	if ( !__xrtTypedTreeValid(pTree, "lower-bound") ||
		 !__xrtTypedTreeSourceValid(
			pTree, pSearchKey, true, "lower-bound"
		) ) {
		return NULL;
	}
	return __xrtTypedTreeResult(
		pTree,
		xrtAVLTreeLowerBound(&pTree->Storage, pSearchKey),
		pStoredKey
	);
}



/* 返回第一项严格大于查询键的值和内部规范键。 */
XRT_API ptr xrtTypedTreeUpperBound(
	xtypedtree* pTree,
	const void* pSearchKey,
	const void** pStoredKey
)
{
	if ( pStoredKey != NULL ) {
		*pStoredKey = NULL;
	}
	if ( !__xrtTypedTreeValid(pTree, "upper-bound") ||
		 !__xrtTypedTreeSourceValid(
			pTree, pSearchKey, true, "upper-bound"
		) ) {
		return NULL;
	}
	return __xrtTypedTreeResult(
		pTree,
		xrtAVLTreeUpperBound(&pTree->Storage, pSearchKey),
		pStoredKey
	);
}



/* 启动完整或带包含边界的正反零分配迭代。 */
static bool __xrtTypedTreeIterStart(
	xtypedtree* pTree,
	const void* pKey,
	bool bHasKey,
	bool bReverse,
	xtypedtreeiter* pIterator,
	cstr sOperation
)
{
	bool bSuccess;

	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(*pIterator));
	}
	if ( !__xrtTypedTreeValid(pTree, sOperation) ||
		 (pIterator == NULL) ||
		 (bHasKey && !__xrtTypedTreeSourceValid(
			pTree, pKey, true, sOperation
		)) ) {
		if ( pIterator == NULL ) {
			__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
				sOperation, "the typed tree iterator is null");
		}
		return false;
	}
	bSuccess = bHasKey ?
		(bReverse ?
			xrtAVLTreeIterRFrom(&pTree->Storage, pKey, &pIterator->Base) :
			xrtAVLTreeIterFrom(&pTree->Storage, pKey, &pIterator->Base)) :
		(bReverse ?
			xrtAVLTreeIterRBegin(&pTree->Storage, &pIterator->Base) :
			xrtAVLTreeIterBegin(&pTree->Storage, &pIterator->Base));
	if ( !bSuccess ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree iterator could not start");
		return false;
	}
	pIterator->Tree = pTree;
	return true;
}



/* 启动按键升序的完整迭代。 */
XRT_API bool xrtTypedTreeIterBegin(
	xtypedtree* pTree,
	xtypedtreeiter* pIterator
)
{
	return __xrtTypedTreeIterStart(
		pTree, NULL, false, false, pIterator, "iter-begin"
	);
}



/* 启动按键降序的完整迭代。 */
XRT_API bool xrtTypedTreeIterRBegin(
	xtypedtree* pTree,
	xtypedtreeiter* pIterator
)
{
	return __xrtTypedTreeIterStart(
		pTree, NULL, false, true, pIterator, "iter-rbegin"
	);
}



/* 从第一项不小于查询键的位置开始升序迭代。 */
XRT_API bool xrtTypedTreeIterFrom(
	xtypedtree* pTree,
	const void* pKey,
	xtypedtreeiter* pIterator
)
{
	return __xrtTypedTreeIterStart(
		pTree, pKey, true, false, pIterator, "iter-from"
	);
}



/* 从第一项不大于查询键的位置开始降序迭代。 */
XRT_API bool xrtTypedTreeIterRFrom(
	xtypedtree* pTree,
	const void* pKey,
	xtypedtreeiter* pIterator
)
{
	return __xrtTypedTreeIterStart(
		pTree, pKey, true, true, pIterator, "iter-rfrom"
	);
}



/* 返回下一借用值和可选规范键，并检测结构修改。 */
XRT_API ptr xrtTypedTreeIterNext(
	xtypedtreeiter* pIterator,
	const void** pKey
)
{
	xtypedtree* pTree;
	ptr pEntry;

	if ( pKey != NULL ) {
		*pKey = NULL;
	}
	if ( (pIterator == NULL) || (pIterator->Tree == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
				"iter-next", "the typed tree iterator is null");
		}
		return NULL;
	}
	pTree = pIterator->Tree;
	if ( !__xrtTypedTreeValid(pTree, "iter-next") ) {
		xrtAVLTreeIterEnd(&pIterator->Base);
		pIterator->Tree = NULL;
		return NULL;
	}
	if (
		(pIterator->Base.Tree != &pTree->Storage) ||
		(pIterator->Base.Base.Tree != &pTree->Storage.Base) ||
		(pIterator->Base.Base.Version != pTree->Storage.Base.Version)
	) {
		xrtAVLTreeIterEnd(&pIterator->Base);
		pIterator->Tree = NULL;
		__xrtTypedTreeError(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			"iter-next", "the typed tree changed during iteration");
		return NULL;
	}
	pEntry = xrtAVLTreeIterNext(&pIterator->Base);
	if ( pEntry == NULL ) {
		pIterator->Tree = NULL;
		return NULL;
	}
	return __xrtTypedTreeResult(pTree, pEntry, pKey);
}



/* 提前结束迭代并清除全部借用状态。 */
XRT_API void xrtTypedTreeIterEnd(xtypedtreeiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}
	xrtAVLTreeIterEnd(&pIterator->Base);
	pIterator->Tree = NULL;
}



/* 验证两个树使用完全相同的键和值类型描述。 */
static bool __xrtTypedTreeSameType(
	const xtypedtree* pLeft,
	const xtypedtree* pRight,
	cstr sOperation
)
{
	if ( (pLeft->KeyType != pRight->KeyType) ||
		 (pLeft->ValueType != pRight->ValueType) ) {
		__xrtTypedTreeError(XERR_TYPE, XTYPED_TREE_ERROR_TYPE,
			sOperation, "the typed tree key or value types do not match");
		return false;
	}
	return true;
}



/* 销毁临时堆树，同时保留进入清理阶段前的根错误。 */
static void __xrtTypedTreeDestroyPreserveError(xtypedtree* pTree)
{
	xerror* pError = xrtTakeError();

	xrtTypedTreeDestroy(pTree);
	__xrtTypedTreeRestoreError(pError);
}



/* 在调用方保护来源期间深复制一棵树。 */
static xtypedtree* __xrtTypedTreeCloneProtected(
	const xtypedtree* pTree,
	cstr sOperation
)
{
	xtypedtree* pClone;
	xavltreeiter Iterator;
	ptr pEntry;

	pClone = xrtTypedTreeCreate(pTree->KeyType, pTree->ValueType);
	if ( pClone == NULL ) {
		__xrtTypedTreeWrap(XERR_MEMORY, XTYPED_TREE_ERROR_OPERATION,
			sOperation, "the typed tree clone could not be created");
		return NULL;
	}
	if ( !xrtAVLTreeIterBegin((xavltree*)&pTree->Storage, &Iterator) ) {
		__xrtTypedTreeDestroyPreserveError(pClone);
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree clone iterator could not start");
		return NULL;
	}
	while ( (pEntry = xrtAVLTreeIterNext(&Iterator)) != NULL ) {
		if ( !xrtTypedTreeSet(
			pClone,
			__xrtTypedTreeKey(pTree, pEntry),
			__xrtTypedTreeValue(pTree, pEntry)
		) ) {
			xrtAVLTreeIterEnd(&Iterator);
			__xrtTypedTreeDestroyPreserveError(pClone);
			__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
				sOperation, "a typed tree entry could not be cloned");
			return NULL;
		}
	}
	xrtAVLTreeIterEnd(&Iterator);
	return pClone;
}



/* 深复制一个独立堆类型树。 */
XRT_API xtypedtree* xrtTypedTreeClone(const xtypedtree* pTree)
{
	xtypedtree* pClone;
	bool bBusy;

	if ( !__xrtTypedTreeValid(pTree, "clone") ) {
		return NULL;
	}
	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	pClone = __xrtTypedTreeCloneProtected(pTree, "clone");
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	return pClone;
}



/* 事务合并同类型树，并按策略保留或替换冲突键。 */
XRT_API bool xrtTypedTreeMerge(
	xtypedtree* pTarget,
	const xtypedtree* pSource,
	bool bReplace
)
{
	xtypedtree* pWork;
	xavltreeiter Iterator;
	ptr pEntry;
	bool bTargetBusy;
	bool bSourceBusy;
	bool bSuccess = true;
	uint64 iTargetVersion;

	if ( !__xrtTypedTreeCanMutate(pTarget, "merge") ||
		 !__xrtTypedTreeValid(pSource, "merge") ||
		 !__xrtTypedTreeSameType(pTarget, pSource, "merge") ) {
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	bTargetBusy = __xrtTypedTreeCallbackBegin(pTarget);
	bSourceBusy = __xrtTypedTreeCallbackBegin(pSource);
	pWork = __xrtTypedTreeCloneProtected(pTarget, "merge");
	if ( pWork == NULL ) {
		__xrtTypedTreeCallbackEnd(pSource, bSourceBusy);
		__xrtTypedTreeCallbackEnd(pTarget, bTargetBusy);
		return false;
	}
	if ( !xrtAVLTreeIterBegin((xavltree*)&pSource->Storage, &Iterator) ) {
		bSuccess = false;
	} else {
		while ( (pEntry = xrtAVLTreeIterNext(&Iterator)) != NULL ) {
			const void* pKey = __xrtTypedTreeKey(pSource, pEntry);

			if ( !bReplace && xrtTypedTreeHas(pWork, pKey) ) {
				continue;
			}
			if ( !xrtTypedTreeSet(
				pWork, pKey, __xrtTypedTreeValue(pSource, pEntry)
			) ) {
				bSuccess = false;
				break;
			}
		}
		xrtAVLTreeIterEnd(&Iterator);
	}
	__xrtTypedTreeCallbackEnd(pSource, bSourceBusy);
	__xrtTypedTreeCallbackEnd(pTarget, bTargetBusy);
	if ( !bSuccess ) {
		__xrtTypedTreeDestroyPreserveError(pWork);
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			"merge", "a source typed tree entry could not be merged");
		return false;
	}

	iTargetVersion = pTarget->Storage.Base.Version;
	__xrtTypedTreeSwap(pTarget, pWork);
	pTarget->Storage.Base.Version = __xrtTypedTreeNextVersion(iTargetVersion);
	xrtTypedTreeDestroy(pWork);
	return true;
}



/* 比较两个树的类型、键集合和值内容。 */
XRT_API bool xrtTypedTreeEquals(
	const xtypedtree* pLeft,
	const xtypedtree* pRight
)
{
	xavltreeiter Iterator;
	ptr pLeftEntry;
	const void* pRightEntry;
	bool bLeftBusy;
	bool bRightBusy;
	bool bEqual = true;
	bool bFailed = false;
	int iCompare;

	if ( !__xrtTypedTreeValid(pLeft, "equals") ||
		 !__xrtTypedTreeValid(pRight, "equals") ||
		 !__xrtTypedTreeSameType(pLeft, pRight, "equals") ) {
		return false;
	}
	if ( pLeft == pRight ) {
		return true;
	}
	if ( pLeft->Storage.Base.Count != pRight->Storage.Base.Count ) {
		return false;
	}
	if ( !xrtTypeIsComparable(pLeft->ValueType) ) {
		__xrtTypedTreeError(XERR_UNSUPPORTED, XTYPED_TREE_ERROR_TYPE,
			"equals", "the typed tree value type is not comparable");
		return false;
	}
	bLeftBusy = __xrtTypedTreeCallbackBegin(pLeft);
	bRightBusy = __xrtTypedTreeCallbackBegin(pRight);
	if ( !xrtAVLTreeIterBegin((xavltree*)&pLeft->Storage, &Iterator) ) {
		bEqual = false;
		bFailed = true;
	} else {
		while ( (pLeftEntry = xrtAVLTreeIterNext(&Iterator)) != NULL ) {
			pRightEntry = xrtAVLTreeConstFind(
				&pRight->Storage,
				__xrtTypedTreeKey(pLeft, pLeftEntry)
			);
			if ( pRightEntry == NULL ) {
				bEqual = false;
				break;
			}
			if ( !xrtTypeCompareValue(
				pLeft->ValueType,
				__xrtTypedTreeValue(pLeft, pLeftEntry),
				__xrtTypedTreeValue(pRight, pRightEntry),
				&iCompare
			) ) {
				bEqual = false;
				bFailed = true;
				break;
			}
			if ( iCompare != 0 ) {
				bEqual = false;
				break;
			}
		}
		xrtAVLTreeIterEnd(&Iterator);
	}
	__xrtTypedTreeCallbackEnd(pRight, bRightBusy);
	__xrtTypedTreeCallbackEnd(pLeft, bLeftBusy);
	if ( bFailed ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			"equals", "the typed tree entries could not be compared");
	}
	return bEqual;
}



/* 初始化对象负载中的类型树。 */
static bool __xrtTypedTreeInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	if ( !xrtTypedTreeTypeValidate(pType) ) {
		return false;
	}
	return xrtTypedTreeInit(
		(xtypedtree*)pInstance, pType->Arguments[0], pType->Arguments[1]
	);
}



/* 销毁对象负载中的类型树。 */
static void __xrtTypedTreeInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedTreeUnit((xtypedtree*)pInstance);
}



/* 类型树追踪适配器保存对象访问器和失败状态。 */
typedef struct xtypedtreetracecontext {
	const xtypedtree* Tree;
	xrtobjectvisitor Visit;
	ptr UserData;
	bool Failed;
} xtypedtreetracecontext;



/* 枚举一个树条目的键和值直接拥有的强对象引用。 */
static bool __xrtTypedTreeTraceEntry(ptr pEntry, ptr pUserData)
{
	xtypedtreetracecontext* pContext =
		(xtypedtreetracecontext*)pUserData;
	const xtypedtree* pTree = pContext->Tree;
	bool bBusy;
	bool bTraced;

	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	bTraced = xrtTypeTraceValue(
		pTree->KeyType,
		__xrtTypedTreeKey(pTree, pEntry),
		pContext->Visit,
		pContext->UserData
	) && xrtTypeTraceValue(
		pTree->ValueType,
		__xrtTypedTreeValue(pTree, pEntry),
		pContext->Visit,
		pContext->UserData
	);
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	if ( !bTraced ) {
		pContext->Failed = true;
		return false;
	}
	return true;
}



/* 枚举类型树所有键值直接拥有的强对象引用。 */
static bool __xrtTypedTreeInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xtypedtree* pTree = (xtypedtree*)pInstance;
	xtypedtreetracecontext Context;
	size_t iExpected;
	size_t iVisited;
	(void)pType;

	if ( !__xrtTypedTreeValid(pTree, "instance-trace") ) {
		return false;
	}
	Context.Tree = pTree;
	Context.Visit = pVisit;
	Context.UserData = pContext;
	Context.Failed = false;
	iExpected = pTree->Storage.Base.Count;
	iVisited = xrtAVLTreeVisit(
		&pTree->Storage, __xrtTypedTreeTraceEntry, &Context
	);
	if ( Context.Failed ) {
		return false;
	}
	if ( iVisited != iExpected ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			"instance-trace", "the typed tree trace visit was incomplete");
		return false;
	}
	return true;
}



/* 返回对象树负载共享的实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedTreeInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedTreeInstanceInit,
		.Drop = __xrtTypedTreeInstanceDrop,
		.Trace = __xrtTypedTreeInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的泛型有序字典类型描述。 */
XRT_API bool xrtTypedTreeTypeValidate(const xrttype* pType)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedTreeWrap(XERR_ARGUMENT, XTYPED_TREE_ERROR_TYPE,
			"type-validate", "the typed tree object type is invalid");
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_DICT) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 2u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != sizeof(xtypedtree)) ||
		(pType->InstanceAlign <
		 XRT_INTERNAL_OBJECT_ALIGNOF(xtypedtree)) ||
		(pType->InstanceOps != xrtTypedTreeInstanceOps())
	) {
		__xrtTypedTreeError(XERR_TYPE, XTYPED_TREE_ERROR_TYPE,
			"type-validate", "the typed tree object type contract is invalid");
		return false;
	}
	return __xrtTypedTreeValueTypeValidate(
		pType->Arguments[0], true, "type-validate"
	) && __xrtTypedTreeValueTypeValidate(
		pType->Arguments[1], false, "type-validate"
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_list.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_LIST)



#if defined(XRUNTIME_FEATURE_TYPED_LIST)

#define XRT_TYPED_LIST_FLAG_READY 0x0001u
#define XRT_TYPED_LIST_FLAG_BUSY  0x0002u
#define XRT_TYPED_LIST_FLAGS      0x0003u



/* 设置类型列表模块结构化错误。 */
static void __xrtTypedListError(
	xerrkind Kind,
	xtypedlisterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-list";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型或整数映射错误补充类型列表上下文。 */
static void __xrtTypedListWrap(
	xerrkind DefaultKind,
	xtypedlisterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-list";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 验证元素类型可由稳定节点列表安全拥有。 */
static bool __xrtTypedListItemTypeValidate(
	const xrttype* pItemType,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pItemType) ) {
		__xrtTypedListWrap(XERR_ARGUMENT, XTYPED_LIST_ERROR_TYPE,
			sOperation, "the list item type is invalid");
		return false;
	}
	if ( pItemType->Size == 0u ) {
		__xrtTypedListError(XERR_TYPE, XTYPED_LIST_ERROR_TYPE,
			sOperation, "a typed list item must occupy storage");
		return false;
	}
	if ( !xrtTypeIsCopyable(pItemType) ) {
		__xrtTypedListError(XERR_UNSUPPORTED, XTYPED_LIST_ERROR_TYPE,
			sOperation, "the list item type is not copyable");
		return false;
	}
	return true;
}



/* 销毁整数映射中一个完整初始化的类型值。 */
static void __xrtTypedListDrop(
	int64 iKey,
	ptr pValue,
	ptr pUserData
)
{
	xtypedlist* pList = (xtypedlist*)pUserData;
	(void)iKey;

	if ( (pList != NULL) && (pList->ItemType != NULL) ) {
		xrtTypeDropValue(pList->ItemType, pValue);
	}
}



/* 检查公开类型列表状态、布局和释放回调是否一致。 */
static bool __xrtTypedListValid(
	const xtypedlist* pList,
	cstr sOperation
)
{
	if ( (pList == NULL) || (pList->ItemType == NULL) ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			sOperation, "the typed list is null or uninitialized");
		return false;
	}
	if (
		((pList->Flags & XRT_TYPED_LIST_FLAG_READY) == 0u) ||
		((pList->Flags & XRT_TYPED_LIST_FLAG_BUSY) != 0u) ||
		((pList->Flags & ~XRT_TYPED_LIST_FLAGS) != 0u)
	) {
		__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list is not available for API access");
		return false;
	}
	if ( !__xrtIntMapValid(&pList->Storage) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list storage is invalid");
		return false;
	}
	if ( (pList->Storage.ValueSize != pList->ItemType->Size) ||
		 (pList->Storage.Alignment < pList->ItemType->Align) ||
		 (pList->Storage.Drop != __xrtTypedListDrop) ||
		 (pList->Storage.UserData != pList) ) {
		__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list item layout or ownership is invalid");
		return false;
	}
	return true;
}



/* 在用户类型回调期间拒绝当前列表的全部 API 重入。 */
void __xrtTypedListCallbackBegin(const xtypedlist* pList)
{
	((xtypedlist*)pList)->Flags |= XRT_TYPED_LIST_FLAG_BUSY;
}



/* 结束当前列表的用户类型回调门禁。 */
void __xrtTypedListCallbackEnd(const xtypedlist* pList)
{
	((xtypedlist*)pList)->Flags &= ~XRT_TYPED_LIST_FLAG_BUSY;
}



/* 检查类型列表当前是否允许结构和生命周期修改。 */
static bool __xrtTypedListCanMutate(
	const xtypedlist* pList,
	cstr sOperation
)
{
	if ( !__xrtTypedListValid(pList, sOperation) ) {
		return false;
	}
	if ( !__xrtIntMapCanMutate(&pList->Storage) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list is currently being visited");
		return false;
	}
	return true;
}



/* 判断字节区间是否触及类型列表自身或拥有的节点池。 */
static bool __xrtTypedListOwnsRange(
	const xtypedlist* pList,
	const void* pMemory,
	size_t iSize
)
{
	return __xrtRangesOverlap(pMemory, iSize, pList, sizeof(*pList)) ||
		__xrtIntMapOwnsRange(&pList->Storage, pMemory, iSize);
}



/* 验证来源是外部值或列表中的准确活动值槽。 */
static bool __xrtTypedListSourceValid(
	const xtypedlist* pList,
	const void* pItem,
	cstr sOperation
)
{
	xintmapiter Iterator;
	ptr pValue;
	bool bExact = false;

	if ( pItem == NULL ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			sOperation, "the source item is null");
		return false;
	}
	if ( !__xrtTypedListOwnsRange(
		pList, pItem, pList->ItemType->Size
	) ) {
		return true;
	}
	if ( !xrtIntMapIterBegin((xintmap*)&pList->Storage, &Iterator) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list source scan could not start");
		return false;
	}
	while ( (pValue = xrtIntMapIterNext(&Iterator, NULL)) != NULL ) {
		if ( pValue == pItem ) {
			bExact = true;
			break;
		}
	}
	xrtIntMapIterEnd(&Iterator);
	if ( !bExact ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			sOperation, "an internal source must be an active value boundary");
		return false;
	}
	return true;
}



/* 验证移动输出完全位于类型列表拥有的内存之外。 */
static bool __xrtTypedListOutputExternal(
	const xtypedlist* pList,
	const void* pValue,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			sOperation, "the output value is null");
		return false;
	}
	if ( __xrtTypedListOwnsRange(
		pList, pValue, pList->ItemType->Size
	) ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			sOperation, "the output value must not alias typed list storage");
		return false;
	}
	return true;
}



/* 新节点初始化上下文保存来源值和元素类型。 */
typedef struct xtypedlistinitcontext {
	xtypedlist* List;
	const void* Item;
} xtypedlistinitcontext;



/* 初始化并复制一个尚未提交的新列表值。 */
static bool __xrtTypedListInitValue(
	int64 iKey,
	ptr pValue,
	ptr pUserData
)
{
	xtypedlistinitcontext* pContext = (xtypedlistinitcontext*)pUserData;
	xerror* pError;
	(void)iKey;

	__xrtTypedListCallbackBegin(pContext->List);
	if ( !xrtTypeInitValue(pContext->List->ItemType, pValue) ) {
		__xrtTypedListCallbackEnd(pContext->List);
		return false;
	}
	if ( xrtTypeCopyValue(
		pContext->List->ItemType, pValue, pContext->Item
	) ) {
		__xrtTypedListCallbackEnd(pContext->List);
		return true;
	}
	pError = xrtTakeError();
	xrtTypeDropValue(pContext->List->ItemType, pValue);
	__xrtTypedListCallbackEnd(pContext->List);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 在调用方已经验证列表和来源后复制设置一个键值。 */
static bool __xrtTypedListSetReady(
	xtypedlist* pList,
	int64 iKey,
	const void* pItem,
	cstr sOperation
)
{
	xtypedlistinitcontext Context;
	ptr pStored;
	bool bNew;

	pStored = xrtIntMapGet(&pList->Storage, iKey);
	if ( pStored != NULL ) {
		__xrtTypedListCallbackBegin(pList);
		if ( !xrtTypeCopyValue(pList->ItemType, pStored, pItem) ) {
			__xrtTypedListCallbackEnd(pList);
			__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
				sOperation, "the existing typed list item could not be replaced");
			return false;
		}
		__xrtTypedListCallbackEnd(pList);
		return true;
	}
	Context.List = pList;
	Context.Item = pItem;
	pStored = xrtIntMapGetOrInit(
		&pList->Storage,
		iKey,
		__xrtTypedListInitValue,
		&Context,
		&bNew
	);
	if ( (pStored == NULL) || !bNew ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			sOperation, "the new typed list item could not be inserted");
		return false;
	}
	return true;
}



/* 修复类型列表按值交换后保存的内部自引用。 */
static void __xrtTypedListRepair(xtypedlist* pList)
{
	pList->Storage.Tree.UserData = &pList->Storage;
	pList->Storage.UserData = pList;
}



/* 交换两个静止且有效的类型列表，并修复底层自引用。 */
static void __xrtTypedListSwap(
	xtypedlist* pLeft,
	xtypedlist* pRight
)
{
	xintmap Storage = pLeft->Storage;
	const xrttype* pItemType = pLeft->ItemType;

	pLeft->Storage = pRight->Storage;
	pLeft->ItemType = pRight->ItemType;
	pRight->Storage = Storage;
	pRight->ItemType = pItemType;
	__xrtTypedListRepair(pLeft);
	__xrtTypedListRepair(pRight);
}



/* 递增结构版本并跳过外置迭代器保留的零值。 */
static uint64 __xrtTypedListNextVersion(uint64 iVersion)
{
	iVersion++;
	return iVersion != 0u ? iVersion : 1u;
}



/* 初始化一个拥有类型值的空稀疏列表。 */
XRT_API bool xrtTypedListInit(
	xtypedlist* pList,
	const xrttype* pItemType
)
{
	if ( pList == NULL ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			"init", "the typed list is null");
		return false;
	}
	memset(pList, 0, sizeof(*pList));
	if ( !__xrtTypedListItemTypeValidate(pItemType, "init") ) {
		return false;
	}
	if ( !xrtIntMapInitAligned(
		&pList->Storage, pItemType->Size, pItemType->Align
	) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			"init", "the typed list storage could not be initialized");
		return false;
	}
	pList->ItemType = pItemType;
	if ( !xrtIntMapSetDrop(
		&pList->Storage, __xrtTypedListDrop, pList
	) ) {
		xrtIntMapUnit(&pList->Storage);
		memset(pList, 0, sizeof(*pList));
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			"init", "the typed list ownership callback could not be installed");
		return false;
	}
	pList->Flags = XRT_TYPED_LIST_FLAG_READY;
	return true;
}



/* 在堆上创建一个拥有类型值的空稀疏列表。 */
XRT_API xtypedlist* xrtTypedListCreate(const xrttype* pItemType)
{
	xtypedlist* pList = (xtypedlist*)xrtMalloc(sizeof(*pList));

	if ( pList == NULL ) {
		return NULL;
	}
	if ( !xrtTypedListInit(pList, pItemType) ) {
		xrtFree(pList);
		return NULL;
	}
	return pList;
}



/* 释放全部元素和节点池，但不释放列表结构。 */
XRT_API void xrtTypedListUnit(xtypedlist* pList)
{
	if ( pList == NULL ) {
		return;
	}
	if ( (pList->ItemType == NULL) && (pList->Flags == 0u) ) {
		return;
	}
	if ( !__xrtTypedListCanMutate(pList, "unit") ) {
		return;
	}
	xrtIntMapUnit(&pList->Storage);
	memset(pList, 0, sizeof(*pList));
}



/* 释放全部元素、节点池和堆列表结构。 */
XRT_API void xrtTypedListDestroy(xtypedlist* pList)
{
	if ( pList == NULL ) {
		return;
	}
	if ( !__xrtTypedListCanMutate(pList, "destroy") ) {
		return;
	}
	xrtTypedListUnit(pList);
	xrtFree(pList);
}



/* 返回列表借用的元素类型描述。 */
XRT_API const xrttype* xrtTypedListItemType(const xtypedlist* pList)
{
	return __xrtTypedListValid(pList, "item-type") ? pList->ItemType : NULL;
}



/* 返回列表当前键值数量。 */
XRT_API size_t xrtTypedListCount(const xtypedlist* pList)
{
	return __xrtTypedListValid(pList, "count") ?
		xrtIntMapCount(&pList->Storage) : 0u;
}



/* 释放全部值并保留节点池供复用。 */
XRT_API bool xrtTypedListClear(xtypedlist* pList)
{
	if ( !__xrtTypedListCanMutate(pList, "clear") ) {
		return false;
	}
	xrtIntMapClear(&pList->Storage);
	return true;
}



/* 释放空闲节点池页并返回实际释放页数。 */
XRT_API size_t xrtTypedListTrim(xtypedlist* pList, size_t iRetainEmpty)
{
	if ( !__xrtTypedListCanMutate(pList, "trim") ) {
		return 0u;
	}
	return xrtIntMapTrim(&pList->Storage, iRetainEmpty);
}



/* 复制插入或失败原子地替换指定键的类型值。 */
XRT_API bool xrtTypedListSet(
	xtypedlist* pList,
	int64 iKey,
	const void* pItem
)
{
	if ( !__xrtTypedListCanMutate(pList, "set") ||
		 !__xrtTypedListSourceValid(pList, pItem, "set") ) {
		return false;
	}
	return __xrtTypedListSetReady(pList, iKey, pItem, "set");
}



/* 在当前最大键之后复制追加一个值。 */
XRT_API bool xrtTypedListAppend(
	xtypedlist* pList,
	const void* pItem,
	int64* pKey
)
{
	int64 iKey = 0;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtTypedListCanMutate(pList, "append") ||
		 !__xrtTypedListSourceValid(pList, pItem, "append") ) {
		return false;
	}
	if ( xrtIntMapCount(&pList->Storage) != 0u ) {
		if ( xrtIntMapLast(&pList->Storage, &iKey) == NULL ) {
			__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
				"append", "the typed list maximum key could not be read");
			return false;
		}
		if ( iKey == INT64_MAX ) {
			__xrtTypedListError(XERR_RANGE, XTYPED_LIST_ERROR_KEY,
				"append", "the typed list append key overflows");
			return false;
		}
		iKey++;
	}
	if ( !__xrtTypedListSetReady(pList, iKey, pItem, "append") ) {
		return false;
	}
	if ( pKey != NULL ) {
		*pKey = iKey;
	}
	return true;
}



/* 返回指定键的可写借用值槽。 */
XRT_API ptr xrtTypedListGet(xtypedlist* pList, int64 iKey)
{
	return __xrtTypedListValid(pList, "get") ?
		xrtIntMapGet(&pList->Storage, iKey) : NULL;
}



/* 返回指定键的只读借用值槽。 */
XRT_API const void* xrtTypedListConstGet(
	const xtypedlist* pList,
	int64 iKey
)
{
	return __xrtTypedListValid(pList, "const-get") ?
		xrtIntMapConstGet(&pList->Storage, iKey) : NULL;
}



/* 判断指定整数键是否存在。 */
XRT_API bool xrtTypedListHas(const xtypedlist* pList, int64 iKey)
{
	return __xrtTypedListValid(pList, "has") &&
		xrtIntMapHas(&pList->Storage, iKey);
}



/* 从较近的一端按键顺序取得指定位置的借用值槽。 */
static ptr __xrtTypedListAt(
	xtypedlist* pList,
	size_t iIndex,
	int64* pKey,
	cstr sOperation
)
{
	xintmapiter Iterator;
	size_t iCount;
	size_t iSteps;
	ptr pValue = NULL;
	bool bReverse;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtTypedListValid(pList, sOperation) ) {
		return NULL;
	}
	iCount = xrtIntMapCount(&pList->Storage);
	if ( iIndex >= iCount ) {
		return NULL;
	}
	bReverse = iIndex > (iCount / 2u);
	iSteps = bReverse ? (iCount - iIndex - 1u) : iIndex;
	if ( !(bReverse ?
		xrtIntMapIterRBegin(&pList->Storage, &Iterator) :
		xrtIntMapIterBegin(&pList->Storage, &Iterator)) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list position iterator could not start");
		return NULL;
	}
	for ( size_t i = 0u; i <= iSteps; i++ ) {
		pValue = xrtIntMapIterNext(&Iterator, pKey);
	}
	xrtIntMapIterEnd(&Iterator);
	if ( pValue == NULL ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list position could not be reached");
	}
	return pValue;
}



/* 按键顺序返回指定位置的可写借用值槽。 */
XRT_API ptr xrtTypedListAt(
	xtypedlist* pList,
	size_t iIndex,
	int64* pKey
)
{
	return __xrtTypedListAt(pList, iIndex, pKey, "at");
}



/* 按键顺序返回指定位置的只读借用值槽。 */
XRT_API const void* xrtTypedListConstAt(
	const xtypedlist* pList,
	size_t iIndex,
	int64* pKey
)
{
	return __xrtTypedListAt(
		(xtypedlist*)pList, iIndex, pKey, "const-at"
	);
}



/* 删除指定键并销毁其值。 */
XRT_API bool xrtTypedListRemove(xtypedlist* pList, int64 iKey)
{
	if ( !__xrtTypedListCanMutate(pList, "remove") ) {
		return false;
	}
	return xrtIntMapRemove(&pList->Storage, iKey);
}



/* 把值移动到外部已初始化输出后删除指定键。 */
XRT_API bool xrtTypedListTake(
	xtypedlist* pList,
	int64 iKey,
	ptr pValue
)
{
	ptr pStored;

	if ( !__xrtTypedListCanMutate(pList, "take") ||
		 !__xrtTypedListOutputExternal(pList, pValue, "take") ) {
		return false;
	}
	pStored = xrtIntMapGet(&pList->Storage, iKey);
	if ( pStored == NULL ) {
		return false;
	}
	__xrtTypedListCallbackBegin(pList);
	if ( !xrtTypeMoveValue(pList->ItemType, pValue, pStored) ) {
		__xrtTypedListCallbackEnd(pList);
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			"take", "the typed list item could not be moved");
		return false;
	}
	__xrtTypedListCallbackEnd(pList);
	if ( !xrtIntMapRemove(&pList->Storage, iKey) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"take", "the moved typed list item could not be removed");
		return false;
	}
	return true;
}



/* 查找按键顺序出现的第一个相等值。 */
XRT_API bool xrtTypedListFind(
	const xtypedlist* pList,
	const void* pItem,
	int64* pKey
)
{
	xintmapiter Iterator;
	ptr pValue;
	int64 iKey;
	int iCompare;
	uint64 iVersion;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtTypedListValid(pList, "find") ||
		 !__xrtTypedListSourceValid(pList, pItem, "find") ) {
		return false;
	}
	if ( !xrtTypeIsComparable(pList->ItemType) ) {
		__xrtTypedListError(XERR_UNSUPPORTED, XTYPED_LIST_ERROR_TYPE,
			"find", "the list item type is not comparable");
		return false;
	}
	if ( !xrtIntMapIterBegin((xintmap*)&pList->Storage, &Iterator) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"find", "the typed list iterator could not start");
		return false;
	}
	__xrtTypedListCallbackBegin(pList);
	iVersion = pList->Storage.Tree.Base.Version;
	while ( (pValue = xrtIntMapIterNext(&Iterator, &iKey)) != NULL ) {
		if ( !xrtTypeCompareValue(
			pList->ItemType, pValue, pItem, &iCompare
		) ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pList);
			__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
				"find", "typed list item comparison failed");
			return false;
		}
		if ( pList->Storage.Tree.Base.Version != iVersion ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pList);
			__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
				"find", "the typed list changed during item comparison");
			return false;
		}
		if ( iCompare == 0 ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pList);
			if ( pKey != NULL ) {
				*pKey = iKey;
			}
			return true;
		}
	}
	xrtIntMapIterEnd(&Iterator);
	__xrtTypedListCallbackEnd(pList);
	return false;
}



/* 判断列表中是否包含相等值。 */
XRT_API bool xrtTypedListContains(
	const xtypedlist* pList,
	const void* pItem
)
{
	return xrtTypedListFind(pList, pItem, NULL);
}



/* 把来源全部键值复制到已经初始化的空目标。 */
static bool __xrtTypedListCopyInto(
	xtypedlist* pTarget,
	const xtypedlist* pSource,
	cstr sOperation
)
{
	xintmapiter Iterator;
	ptr pValue;
	int64 iKey;
	uint64 iVersion;

	if ( !xrtIntMapIterBegin((xintmap*)&pSource->Storage, &Iterator) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the source typed list iterator could not start");
		return false;
	}
	__xrtTypedListCallbackBegin(pSource);
	iVersion = pSource->Storage.Tree.Base.Version;
	while ( (pValue = xrtIntMapIterNext(&Iterator, &iKey)) != NULL ) {
		if ( !__xrtTypedListSetReady(
			pTarget, iKey, pValue, sOperation
		) ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pSource);
			return false;
		}
		if ( pSource->Storage.Tree.Base.Version != iVersion ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pSource);
			__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
				sOperation, "the source typed list changed while being copied");
			return false;
		}
	}
	xrtIntMapIterEnd(&Iterator);
	__xrtTypedListCallbackEnd(pSource);
	return true;
}



/* 失败原子地合并同类型列表。 */
XRT_API bool xrtTypedListMerge(
	xtypedlist* pTarget,
	const xtypedlist* pSource,
	bool bReplace
)
{
	xtypedlist Work = { 0 };
	xintmapiter Iterator;
	ptr pValue;
	int64 iKey;
	uint64 iVersion;
	uint64 iTargetVersion;

	if ( !__xrtTypedListCanMutate(pTarget, "merge") ||
		 !__xrtTypedListValid(pSource, "merge") ) {
		return false;
	}
	if ( !xrtTypeSame(pTarget->ItemType, pSource->ItemType) ) {
		__xrtTypedListError(XERR_TYPE, XTYPED_LIST_ERROR_TYPE,
			"merge", "typed lists have different item types");
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( !xrtTypedListInit(&Work, pTarget->ItemType) ||
		 !__xrtTypedListCopyInto(&Work, pTarget, "merge") ) {
		if ( Work.ItemType != NULL ) {
			xrtTypedListUnit(&Work);
		}
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			"merge", "the typed list merge snapshot failed");
		return false;
	}
	if ( !xrtIntMapIterBegin((xintmap*)&pSource->Storage, &Iterator) ) {
		xrtTypedListUnit(&Work);
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"merge", "the source typed list iterator could not start");
		return false;
	}
	__xrtTypedListCallbackBegin(pTarget);
	__xrtTypedListCallbackBegin(pSource);
	iVersion = pSource->Storage.Tree.Base.Version;
	while ( (pValue = xrtIntMapIterNext(&Iterator, &iKey)) != NULL ) {
		if ( !bReplace && xrtTypedListHas(&Work, iKey) ) {
			continue;
		}
		if ( !__xrtTypedListSetReady(
			&Work, iKey, pValue, "merge"
		) ) {
			xerror* pError = xrtTakeError();

			xrtIntMapIterEnd(&Iterator);
			xrtTypedListUnit(&Work);
			__xrtTypedListCallbackEnd(pSource);
			__xrtTypedListCallbackEnd(pTarget);
			if ( pError != NULL ) {
				xrtSetError(pError);
				xrtErrorFree(pError);
			}
			__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
				"merge", "a source typed list item could not be merged");
			return false;
		}
		if ( pSource->Storage.Tree.Base.Version != iVersion ) {
			xrtIntMapIterEnd(&Iterator);
			xrtTypedListUnit(&Work);
			__xrtTypedListCallbackEnd(pSource);
			__xrtTypedListCallbackEnd(pTarget);
			__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
				"merge", "the source typed list changed while being merged");
			return false;
		}
	}
	xrtIntMapIterEnd(&Iterator);
	__xrtTypedListCallbackEnd(pSource);
	__xrtTypedListCallbackEnd(pTarget);
	iTargetVersion = pTarget->Storage.Tree.Base.Version;
	__xrtTypedListSwap(pTarget, &Work);
	pTarget->Storage.Tree.Base.Version =
		__xrtTypedListNextVersion(iTargetVersion);
	xrtTypedListUnit(&Work);
	return true;
}



/* 深复制一个独立堆类型列表。 */
XRT_API xtypedlist* xrtTypedListClone(const xtypedlist* pList)
{
	xtypedlist* pClone;

	if ( !__xrtTypedListValid(pList, "clone") ) {
		return NULL;
	}
	pClone = xrtTypedListCreate(pList->ItemType);
	if ( pClone == NULL ) {
		__xrtTypedListWrap(XERR_MEMORY, XTYPED_LIST_ERROR_OPERATION,
			"clone", "the typed list clone could not be created");
		return NULL;
	}
	if ( !__xrtTypedListCopyInto(pClone, pList, "clone") ) {
		xrtTypedListDestroy(pClone);
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			"clone", "the typed list clone could not be populated");
		return NULL;
	}
	return pClone;
}



/* 比较两个列表的类型、键集合和值内容。 */
XRT_API bool xrtTypedListEquals(
	const xtypedlist* pLeft,
	const xtypedlist* pRight
)
{
	xintmapiter Iterator;
	ptr pLeftValue;
	const void* pRightValue;
	int64 iKey;
	int iCompare;
	uint64 iLeftVersion;
	uint64 iRightVersion;

	if ( !__xrtTypedListValid(pLeft, "equals") ||
		 !__xrtTypedListValid(pRight, "equals") ) {
		return false;
	}
	if ( pLeft == pRight ) {
		return true;
	}
	if ( !xrtTypeSame(pLeft->ItemType, pRight->ItemType) ||
		 (xrtIntMapCount(&pLeft->Storage) !=
		  xrtIntMapCount(&pRight->Storage)) ) {
		return false;
	}
	if ( !xrtTypeIsComparable(pLeft->ItemType) ) {
		__xrtTypedListError(XERR_UNSUPPORTED, XTYPED_LIST_ERROR_TYPE,
			"equals", "the list item type is not comparable");
		return false;
	}
	if ( !xrtIntMapIterBegin((xintmap*)&pLeft->Storage, &Iterator) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"equals", "the typed list iterator could not start");
		return false;
	}
	__xrtTypedListCallbackBegin(pLeft);
	__xrtTypedListCallbackBegin(pRight);
	iLeftVersion = pLeft->Storage.Tree.Base.Version;
	iRightVersion = pRight->Storage.Tree.Base.Version;
	while ( (pLeftValue = xrtIntMapIterNext(&Iterator, &iKey)) != NULL ) {
		pRightValue = xrtIntMapConstGet(&pRight->Storage, iKey);
		if ( pRightValue == NULL ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pRight);
			__xrtTypedListCallbackEnd(pLeft);
			return false;
		}
		if ( !xrtTypeCompareValue(
			pLeft->ItemType, pLeftValue, pRightValue, &iCompare
		) ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pRight);
			__xrtTypedListCallbackEnd(pLeft);
			__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
				"equals", "typed list item comparison failed");
			return false;
		}
		if ( (pLeft->Storage.Tree.Base.Version != iLeftVersion) ||
			 (pRight->Storage.Tree.Base.Version != iRightVersion) ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pRight);
			__xrtTypedListCallbackEnd(pLeft);
			__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
				"equals", "a typed list changed during item comparison");
			return false;
		}
		if ( iCompare != 0 ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pRight);
			__xrtTypedListCallbackEnd(pLeft);
			return false;
		}
	}
	xrtIntMapIterEnd(&Iterator);
	__xrtTypedListCallbackEnd(pRight);
	__xrtTypedListCallbackEnd(pLeft);
	return true;
}



/* 以指定底层起点初始化类型列表迭代器。 */
static bool __xrtTypedListIterStart(
	xtypedlist* pList,
	xtypedlistiter* pIterator,
	int iDirection,
	bool bBounded,
	int64 iKey,
	cstr sOperation
)
{
	bool bSuccess;

	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(*pIterator));
	}
	if ( (pIterator == NULL) || !__xrtTypedListValid(pList, sOperation) ) {
		if ( pIterator == NULL ) {
			__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
				sOperation, "the typed list iterator is null");
		}
		return false;
	}
	if ( iDirection > 0 ) {
		bSuccess = bBounded ?
			xrtIntMapIterFrom(&pList->Storage, iKey, &pIterator->Base) :
			xrtIntMapIterBegin(&pList->Storage, &pIterator->Base);
	} else {
		bSuccess = bBounded ?
			xrtIntMapIterRFrom(&pList->Storage, iKey, &pIterator->Base) :
			xrtIntMapIterRBegin(&pList->Storage, &pIterator->Base);
	}
	if ( !bSuccess ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list iterator could not start");
		return false;
	}
	pIterator->List = pList;
	return true;
}



/* 启动按键升序的完整迭代。 */
XRT_API bool xrtTypedListIterBegin(
	xtypedlist* pList,
	xtypedlistiter* pIterator
)
{
	return __xrtTypedListIterStart(
		pList, pIterator, 1, false, 0, "iter-begin"
	);
}



/* 启动按键降序的完整迭代。 */
XRT_API bool xrtTypedListIterRBegin(
	xtypedlist* pList,
	xtypedlistiter* pIterator
)
{
	return __xrtTypedListIterStart(
		pList, pIterator, -1, false, 0, "iter-rbegin"
	);
}



/* 从第一个不小于边界的键开始升序迭代。 */
XRT_API bool xrtTypedListIterFrom(
	xtypedlist* pList,
	int64 iKey,
	xtypedlistiter* pIterator
)
{
	return __xrtTypedListIterStart(
		pList, pIterator, 1, true, iKey, "iter-from"
	);
}



/* 从第一个不大于边界的键开始降序迭代。 */
XRT_API bool xrtTypedListIterRFrom(
	xtypedlist* pList,
	int64 iKey,
	xtypedlistiter* pIterator
)
{
	return __xrtTypedListIterStart(
		pList, pIterator, -1, true, iKey, "iter-rfrom"
	);
}



/* 返回下一借用值槽及其整数键。 */
XRT_API ptr xrtTypedListIterNext(
	xtypedlistiter* pIterator,
	int64* pKey
)
{
	ptr pValue;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( (pIterator == NULL) || (pIterator->List == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
				"iter-next", "the typed list iterator is null");
		}
		return NULL;
	}
	if ( pIterator->Base.Base.Base.Version !=
		 pIterator->List->Storage.Tree.Base.Version ) {
		xrtIntMapIterEnd(&pIterator->Base);
		pIterator->List = NULL;
		__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"iter-next", "the typed list changed during iteration");
		return NULL;
	}
	pValue = xrtIntMapIterNext(&pIterator->Base, pKey);
	if ( pValue == NULL ) {
		pIterator->List = NULL;
	}
	return pValue;
}



/* 提前结束迭代并清除全部借用状态。 */
XRT_API void xrtTypedListIterEnd(xtypedlistiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}
	xrtIntMapIterEnd(&pIterator->Base);
	pIterator->List = NULL;
}



/* 初始化对象负载中的类型列表。 */
static bool __xrtTypedListInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	if ( !xrtTypedListTypeValidate(pType) ) {
		return false;
	}
	return xrtTypedListInit(
		(xtypedlist*)pInstance, pType->Arguments[0]
	);
}



/* 销毁对象负载中的类型列表。 */
static void __xrtTypedListInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedListUnit((xtypedlist*)pInstance);
}



/* 类型列表追踪适配器上下文保存对象访问器和失败状态。 */
typedef struct xtypedlisttracecontext {
	const xrttype* ItemType;
	xrtobjectvisitor Visit;
	ptr UserData;
	bool Failed;
} xtypedlisttracecontext;



/* 枚举一个列表值直接拥有的强对象引用。 */
static bool __xrtTypedListTraceValue(
	int64 iKey,
	ptr pValue,
	ptr pUserData
)
{
	xtypedlisttracecontext* pContext =
		(xtypedlisttracecontext*)pUserData;
	(void)iKey;

	if ( !xrtTypeTraceValue(
		pContext->ItemType,
		pValue,
		pContext->Visit,
		pContext->UserData
	) ) {
		pContext->Failed = true;
		return false;
	}
	return true;
}



/* 枚举类型列表所有值直接拥有的强对象引用。 */
static bool __xrtTypedListInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xtypedlist* pList = (xtypedlist*)pInstance;
	xtypedlisttracecontext Context;
	size_t iExpected;
	size_t iVisited;
	(void)pType;

	if ( !__xrtTypedListValid(pList, "instance-trace") ) {
		return false;
	}
	Context.ItemType = pList->ItemType;
	Context.Visit = pVisit;
	Context.UserData = pContext;
	Context.Failed = false;
	iExpected = xrtIntMapCount(&pList->Storage);
	__xrtTypedListCallbackBegin(pList);
	iVisited = xrtIntMapVisit(
		&pList->Storage, __xrtTypedListTraceValue, &Context
	);
	__xrtTypedListCallbackEnd(pList);
	if ( Context.Failed ) {
		return false;
	}
	if ( iVisited != iExpected ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"instance-trace", "the typed list trace visit was incomplete");
		return false;
	}
	return true;
}



/* 返回对象列表负载共享的实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedListInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedListInstanceInit,
		.Drop = __xrtTypedListInstanceDrop,
		.Trace = __xrtTypedListInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的泛型列表类型描述。 */
XRT_API bool xrtTypedListTypeValidate(const xrttype* pType)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedListWrap(XERR_ARGUMENT, XTYPED_LIST_ERROR_TYPE,
			"type-validate", "the typed list object type is invalid");
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_LIST) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 1u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != sizeof(xtypedlist)) ||
		(pType->InstanceAlign <
		 XRT_INTERNAL_OBJECT_ALIGNOF(xtypedlist)) ||
		(pType->InstanceOps != xrtTypedListInstanceOps())
	) {
		__xrtTypedListError(XERR_TYPE, XTYPED_LIST_ERROR_TYPE,
			"type-validate", "the typed list object type contract is invalid");
		return false;
	}
	return __xrtTypedListItemTypeValidate(
		pType->Arguments[0], "type-validate"
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_set.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_SET)



#if defined(XRUNTIME_FEATURE_TYPED_SET)

/* 在跨模块用户回调期间拒绝当前类型集合的全部 API 重入。 */
bool __xrtTypedSetCallbackBegin(const xtypedset* pSet)
{
	return (pSet != NULL) && __xrtSetCallbackBegin(&pSet->Storage);
}



/* 结束当前类型集合的跨模块用户回调门禁。 */
void __xrtTypedSetCallbackEnd(const xtypedset* pSet)
{
	if ( pSet != NULL ) {
		__xrtSetCallbackEnd(&pSet->Storage);
	}
}




/* 设置类型集合模块结构化错误。 */
static void __xrtTypedSetError(
	xerrkind Kind,
	xtypedseterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-set";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型或集合错误补充类型集合上下文。 */
static void __xrtTypedSetWrap(
	xerrkind DefaultKind,
	xtypedseterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-set";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 验证元素类型满足唯一值集合的生命周期和键规则。 */
static bool __xrtTypedSetItemTypeValidate(
	const xrttype* pItemType,
	bool bTypedKeys,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pItemType) ) {
		__xrtTypedSetWrap(XERR_ARGUMENT, XTYPED_SET_ERROR_TYPE,
			sOperation, "the set item type is invalid");
		return false;
	}
	if ( pItemType->Size == 0u ) {
		__xrtTypedSetError(XERR_TYPE, XTYPED_SET_ERROR_TYPE,
			sOperation, "a typed set item must occupy storage");
		return false;
	}
	if ( !xrtTypeIsCopyable(pItemType) ) {
		__xrtTypedSetError(XERR_UNSUPPORTED, XTYPED_SET_ERROR_TYPE,
			sOperation, "the set item type is not copyable");
		return false;
	}
	if ( bTypedKeys && !xrtTypeIsComparable(pItemType) ) {
		__xrtTypedSetError(XERR_UNSUPPORTED, XTYPED_SET_ERROR_TYPE,
			sOperation, "the set item type is not comparable");
		return false;
	}
	if ( bTypedKeys && !xrtTypeIsHashable(pItemType) ) {
		__xrtTypedSetError(XERR_UNSUPPORTED, XTYPED_SET_ERROR_TYPE,
			sOperation, "the set item type is not hashable");
		return false;
	}
	return true;
}



/* 按已验证类型的散列操作计算集合键散列。 */
static uint64 __xrtTypedSetHash(const void* pItem, ptr pUserData)
{
	const xrttype* pItemType = (const xrttype*)pUserData;
	uint64 iHash = 0u;

	(void)xrtTypeHashValue(pItemType, pItem, &iHash);
	return iHash;
}



/* 按已验证类型的比较操作判断两个集合键是否相等。 */
static bool __xrtTypedSetEqual(
	const void* pLeft,
	const void* pRight,
	ptr pUserData
)
{
	const xrttype* pItemType = (const xrttype*)pUserData;
	int iCompare = 0;

	return xrtTypeCompareValue(
		pItemType, pLeft, pRight, &iCompare
	) && (iCompare == 0);
}



/* 初始化并复制一个尚未提交的集合值。 */
static bool __xrtTypedSetCopy(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
)
{
	const xrttype* pItemType = (const xrttype*)pUserData;
	xerror* pError;

	if ( !xrtTypeInitValue(pItemType, pTarget) ) {
		return false;
	}
	if ( xrtTypeCopyValue(pItemType, pTarget, pSource) ) {
		return true;
	}
	pError = xrtTakeError();
	xrtTypeDropValue(pItemType, pTarget);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 销毁集合拥有的一个完整初始化类型值。 */
static void __xrtTypedSetDrop(ptr pItem, ptr pUserData)
{
	const xrttype* pItemType = (const xrttype*)pUserData;

	xrtTypeDropValue(pItemType, pItem);
}



/* 把规范集合值移动到调用方已经初始化的外部值。 */
static bool __xrtTypedSetMove(
	ptr pTarget,
	ptr pSource,
	ptr pUserData
)
{
	return xrtTypeMoveValue(
		(const xrttype*)pUserData, pTarget, pSource
	);
}



/* 检查公开类型集合状态、布局和底层策略是否一致。 */
static bool __xrtTypedSetValid(
	const xtypedset* pSet,
	cstr sOperation
)
{
	if ( (pSet == NULL) || (pSet->ItemType == NULL) ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			sOperation, "the typed set is null or uninitialized");
		return false;
	}
	if ( !__xrtSetValid(&pSet->Storage) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set storage is invalid");
		return false;
	}
	if (
		(pSet->KeyPolicy && (!pSet->KeyPolicy->Hash || !pSet->KeyPolicy->Equal)) ||
		(pSet->Storage.ItemSize != pSet->ItemType->Size) ||
		(pSet->Storage.Alignment < pSet->ItemType->Align) ||
		(pSet->Storage.Hash != (pSet->KeyPolicy ? pSet->KeyPolicy->Hash : __xrtTypedSetHash)) ||
		(pSet->Storage.Equal != (pSet->KeyPolicy ? pSet->KeyPolicy->Equal : __xrtTypedSetEqual)) ||
		(pSet->Storage.Copy != __xrtTypedSetCopy) ||
		(pSet->Storage.Drop != __xrtTypedSetDrop) ||
		(pSet->Storage.KeyUserData != (pSet->KeyPolicy ? pSet->KeyPolicy->UserData : (ptr)pSet->ItemType)) ||
		(pSet->Storage.LifecycleUserData != pSet->ItemType)
	) {
		__xrtTypedSetError(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set layout or type policies are invalid");
		return false;
	}
	return true;
}



/* 检查类型集合当前是否允许读取和推进迭代器。 */
static bool __xrtTypedSetCanRead(
	const xtypedset* pSet,
	cstr sOperation
)
{
	if ( !__xrtTypedSetValid(pSet, sOperation) ) {
		return false;
	}
	if ( !__xrtSetCanRead(&pSet->Storage) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set is executing an item callback");
		return false;
	}
	return true;
}



/* 检查类型集合当前是否允许结构和生命周期修改。 */
static bool __xrtTypedSetCanMutate(
	xtypedset* pSet,
	cstr sOperation
)
{
	if ( !__xrtTypedSetValid(pSet, sOperation) ) {
		return false;
	}
	if ( !__xrtSetCanMutate(&pSet->Storage) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set is currently being visited");
		return false;
	}
	return true;
}



/* 判断字节区间是否触及类型集合自身或拥有的底层存储。 */
static bool __xrtTypedSetOwnsRange(
	const xtypedset* pSet,
	const void* pMemory,
	size_t iSize
)
{
	return __xrtRangesOverlap(pMemory, iSize, pSet, sizeof(*pSet)) ||
		__xrtSetOwnsRange(&pSet->Storage, pMemory, iSize);
}



/* 验证来源是外部值或集合中的准确规范值槽。 */
static bool __xrtTypedSetSourceValid(
	const xtypedset* pSet,
	const void* pItem,
	cstr sOperation
)
{
	xsetiter Iterator;
	const void* pStored;
	bool bExact = false;

	if ( pItem == NULL ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			sOperation, "the source item is null");
		return false;
	}
	if ( !__xrtTypedSetOwnsRange(
		pSet, pItem, pSet->ItemType->Size
	) ) {
		return true;
	}
	if ( !xrtSetIterBegin((xset*)&pSet->Storage, &Iterator) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set source scan could not start");
		return false;
	}
	while ( (pStored = xrtSetIterNext(&Iterator)) != NULL ) {
		if ( pStored == pItem ) {
			bExact = true;
			break;
		}
	}
	xrtSetIterEnd(&Iterator);
	if ( !bExact ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			sOperation, "an internal source must be a canonical item boundary");
		return false;
	}
	return true;
}



/* 验证移动输出完全位于类型集合拥有的内存之外。 */
static bool __xrtTypedSetOutputExternal(
	const xtypedset* pSet,
	const void* pValue,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			sOperation, "the output value is null");
		return false;
	}
	if ( __xrtTypedSetOwnsRange(
		pSet, pValue, pSet->ItemType->Size
	) ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			sOperation, "the output value must not alias typed set storage");
		return false;
	}
	return true;
}



/* 验证两个集合借用完全相同的类型描述和生命周期 ABI。 */
static bool __xrtTypedSetSameType(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	cstr sOperation
)
{
	if ( (pLeft->ItemType != pRight->ItemType) ||
		 (pLeft->Storage.Hash != pRight->Storage.Hash) ||
		 (pLeft->Storage.Equal != pRight->Storage.Equal) ||
		 (pLeft->Storage.KeyUserData != pRight->Storage.KeyUserData) ) {
		__xrtTypedSetError(XERR_TYPE, XTYPED_SET_ERROR_TYPE,
			sOperation, "typed set operands must share item type and exact key policy");
		return false;
	}
	return true;
}



/* 在清理底层临时集合期间保留原始失败。 */
static void __xrtTypedSetDestroyRaw(xset* pStorage)
{
	xerror* pError = xrtTakeError();

	xrtSetDestroy(pStorage);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把底层集合结果包装为不再复制元素的类型集合。 */
static xtypedset* __xrtTypedSetFromRaw(
	const xtypedset* pSource,
	xset* pStorage,
	cstr sOperation
)
{
	xtypedset* pResult;

	if ( pStorage == NULL ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			sOperation, "the typed set operation could not build its result");
		return NULL;
	}
	pResult = (xtypedset*)xrtMalloc(sizeof(*pResult));
	if ( pResult == NULL ) {
		__xrtTypedSetDestroyRaw(pStorage);
		__xrtTypedSetWrap(XERR_MEMORY, XTYPED_SET_ERROR_OPERATION,
			sOperation, "the typed set result could not be allocated");
		return NULL;
	}
	if ( !(pSource->KeyPolicy ?
		xrtTypedSetInitWithKeyPolicy(pResult, pSource->ItemType, pSource->KeyPolicy) :
		xrtTypedSetInit(pResult, pSource->ItemType)) ) {
		xrtFree(pResult);
		__xrtTypedSetDestroyRaw(pStorage);
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			sOperation, "the typed set result could not be initialized");
		return NULL;
	}
	if ( !__xrtSetAdoptHeap(&pResult->Storage, pStorage) ) {
		xrtTypedSetUnit(pResult);
		xrtFree(pResult);
		__xrtTypedSetDestroyRaw(pStorage);
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			sOperation, "the typed set result storage could not be adopted");
		return NULL;
	}
	return pResult;
}



/* 初始化一个拥有类型值的空集合。 */
static bool __xrtTypedSetInit(
	xtypedset* pSet,
	const xrttype* pItemType,
	const xtypedsetkeypolicy* pPolicy
)
{
	size_t iAlignment;

	if ( pSet == NULL ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			"init", "the typed set is null");
		return false;
	}
	memset(pSet, 0, sizeof(*pSet));
	if ( !__xrtTypedSetItemTypeValidate(pItemType, pPolicy == NULL, "init") ) {
		return false;
	}
	iAlignment = pItemType->Align > XRT_SET_ALIGNMENT_DEFAULT ?
		pItemType->Align : XRT_SET_ALIGNMENT_DEFAULT;
	if ( !xrtSetInitAligned(
		&pSet->Storage, pItemType->Size, iAlignment
	) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"init", "the typed set storage could not be initialized");
		return false;
	}
	pSet->ItemType = pItemType;
	pSet->KeyPolicy = pPolicy;
	if ( !xrtSetSetKeyPolicy(
		&pSet->Storage,
		pPolicy ? pPolicy->Hash : __xrtTypedSetHash,
		pPolicy ? pPolicy->Equal : __xrtTypedSetEqual,
		pPolicy ? pPolicy->UserData : (ptr)pItemType
	) || !xrtSetSetLifecycle(
		&pSet->Storage,
		__xrtTypedSetCopy,
		__xrtTypedSetDrop,
		(ptr)pItemType
	) ) {
		xrtSetUnit(&pSet->Storage);
		memset(pSet, 0, sizeof(*pSet));
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"init", "the typed set type policies could not be installed");
		return false;
	}
	return true;
}



XRT_API bool xrtTypedSetInit(xtypedset* pSet, const xrttype* pItemType)
{
	return __xrtTypedSetInit(pSet, pItemType, NULL);
}



XRT_API bool xrtTypedSetInitWithKeyPolicy(
	xtypedset* pSet, const xrttype* pItemType, const xtypedsetkeypolicy* pPolicy
)
{
	/* Policy is borrowed immutable metadata, never an alias of the structure
	 * about to be cleared. Refuse without destroying the overlapping record. */
	if ( pSet && pPolicy && __xrtRangesOverlap(pSet, sizeof(*pSet), pPolicy, sizeof(*pPolicy)) ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			"init-key-policy", "the key policy must not alias set storage");
		return false;
	}
	if ( (pPolicy == NULL) || (pPolicy->Hash == NULL) || (pPolicy->Equal == NULL) ) {
		if ( pSet != NULL ) memset(pSet, 0, sizeof(*pSet));
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			"init-key-policy", "a complete immutable key policy is required");
		return false;
	}
	return __xrtTypedSetInit(pSet, pItemType, pPolicy);
}



/* 创建一个堆分配的空类型集合。 */
XRT_API xtypedset* xrtTypedSetCreate(const xrttype* pItemType)
{
	xtypedset* pSet = (xtypedset*)xrtMalloc(sizeof(*pSet));

	if ( pSet == NULL ) {
		return NULL;
	}
	if ( !xrtTypedSetInit(pSet, pItemType) ) {
		xrtFree(pSet);
		return NULL;
	}
	return pSet;
}



XRT_API xtypedset* xrtTypedSetCreateWithKeyPolicy(
	const xrttype* pItemType, const xtypedsetkeypolicy* pPolicy
)
{
	xtypedset* pSet = (xtypedset*)xrtMalloc(sizeof(*pSet));
	if ( pSet == NULL ) return NULL;
	if ( !xrtTypedSetInitWithKeyPolicy(pSet, pItemType, pPolicy) ) {
		xrtFree(pSet); return NULL;
	}
	return pSet;
}



/* 释放全部元素和存储，但不释放集合结构。 */
XRT_API void xrtTypedSetUnit(xtypedset* pSet)
{
	if ( pSet == NULL ) {
		return;
	}
	if ( !__xrtTypedSetCanMutate(pSet, "unit") ) {
		return;
	}
	xrtSetUnit(&pSet->Storage);
	pSet->ItemType = NULL;
	pSet->KeyPolicy = NULL;
}



/* 释放类型集合持有的全部资源和堆结构。 */
XRT_API void xrtTypedSetDestroy(xtypedset* pSet)
{
	if ( pSet == NULL ) {
		return;
	}
	if ( !__xrtTypedSetCanMutate(pSet, "destroy") ) {
		return;
	}
	xrtTypedSetUnit(pSet);
	xrtFree(pSet);
}



/* 返回集合借用的元素类型描述。 */
XRT_API const xrttype* xrtTypedSetItemType(const xtypedset* pSet)
{
	return __xrtTypedSetCanRead(pSet, "item-type") ?
		pSet->ItemType : NULL;
}



/* 返回集合当前元素数量。 */
XRT_API size_t xrtTypedSetCount(const xtypedset* pSet)
{
	return __xrtTypedSetCanRead(pSet, "count") ?
		xrtSetCount(&pSet->Storage) : 0u;
}



/* 返回集合再次扩容前可容纳的元素数量。 */
XRT_API size_t xrtTypedSetCapacity(const xtypedset* pSet)
{
	return __xrtTypedSetCanRead(pSet, "capacity") ?
		xrtSetCapacity(&pSet->Storage) : 0u;
}



/* 清空全部元素并保留桶数组供后续复用。 */
XRT_API bool xrtTypedSetClear(xtypedset* pSet)
{
	if ( !__xrtTypedSetCanMutate(pSet, "clear") ) {
		return false;
	}
	xrtSetClear(&pSet->Storage);
	return true;
}



/* 确保集合无需扩容即可容纳指定数量的元素。 */
XRT_API bool xrtTypedSetReserve(xtypedset* pSet, size_t iCapacity)
{
	if ( !__xrtTypedSetCanMutate(pSet, "reserve") ) {
		return false;
	}
	if ( !xrtSetReserve(&pSet->Storage, iCapacity) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"reserve", "the typed set capacity could not be reserved");
		return false;
	}
	return true;
}



/* 把桶数组收缩到当前元素数量需要的最小容量。 */
XRT_API bool xrtTypedSetTrim(xtypedset* pSet)
{
	if ( !__xrtTypedSetCanMutate(pSet, "trim") ) {
		return false;
	}
	if ( !xrtSetTrim(&pSet->Storage) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"trim", "the typed set storage could not be trimmed");
		return false;
	}
	return true;
}



/* 返回已有或失败原子地复制加入的只读规范值。 */
XRT_API const void* xrtTypedSetGetOrAdd(
	xtypedset* pSet,
	const void* pItem,
	bool* pNew
)
{
	const void* pStored;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtTypedSetCanMutate(pSet, "get-or-add") ||
		 !__xrtTypedSetSourceValid(pSet, pItem, "get-or-add") ) {
		return NULL;
	}
	pStored = xrtSetGetOrAdd(&pSet->Storage, pItem, pNew);
	if ( pStored == NULL ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"get-or-add", "the typed set item could not be inserted");
	}
	return pStored;
}



/* 复制加入元素，已有等价元素时成功且不替换规范值。 */
XRT_API bool xrtTypedSetAdd(xtypedset* pSet, const void* pItem)
{
	return xrtTypedSetGetOrAdd(pSet, pItem, NULL) != NULL;
}



/* 返回集合内部的只读规范值，缺失是正常结果。 */
XRT_API const void* xrtTypedSetGet(
	const xtypedset* pSet,
	const void* pItem
)
{
	if ( !__xrtTypedSetCanRead(pSet, "get") ||
		 !__xrtTypedSetSourceValid(pSet, pItem, "get") ) {
		return NULL;
	}
	return xrtSetGet(&pSet->Storage, pItem);
}



/* 判断集合是否拥有等价值。 */
XRT_API bool xrtTypedSetHas(
	const xtypedset* pSet,
	const void* pItem
)
{
	return xrtTypedSetGet(pSet, pItem) != NULL;
}



/* 删除等价值并执行类型资源释放。 */
XRT_API bool xrtTypedSetRemove(xtypedset* pSet, const void* pItem)
{
	if ( !__xrtTypedSetCanMutate(pSet, "remove") ||
		 !__xrtTypedSetSourceValid(pSet, pItem, "remove") ) {
		return false;
	}
	return xrtSetRemove(&pSet->Storage, pItem);
}



/* 把规范值移动到外部已初始化输出后删除。 */
XRT_API bool xrtTypedSetTake(
	xtypedset* pSet,
	const void* pItem,
	ptr pValue
)
{
	const void* pStored;

	if ( !__xrtTypedSetCanMutate(pSet, "take") ||
		 !__xrtTypedSetSourceValid(pSet, pItem, "take") ||
		 !__xrtTypedSetOutputExternal(pSet, pValue, "take") ) {
		return false;
	}
	pStored = xrtSetGet(&pSet->Storage, pItem);
	if ( pStored == NULL ) {
		return false;
	}
	if ( !__xrtSetMoveOut(
		&pSet->Storage,
		pStored,
		pValue,
		__xrtTypedSetMove,
		(ptr)pSet->ItemType
	) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"take", "the typed set item could not be moved out");
		return false;
	}
	return true;
}



/* 按最短插入顺序方向查找指定位置的规范值。 */
XRT_API const void* xrtTypedSetAt(
	const xtypedset* pSet,
	size_t iIndex
)
{
	xsetiter Iterator;
	const void* pItem = NULL;
	size_t iSteps;

	if ( !__xrtTypedSetCanRead(pSet, "at") ) {
		return NULL;
	}
	if ( iIndex >= pSet->Storage.Count ) {
		__xrtTypedSetError(XERR_RANGE, XTYPED_SET_ERROR_RANGE,
			"at", "the typed set index is out of range");
		return NULL;
	}
	if ( iIndex <= ((pSet->Storage.Count - 1u) >> 1u) ) {
		iSteps = iIndex;
		if ( !xrtSetIterBegin((xset*)&pSet->Storage, &Iterator) ) {
			__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
				"at", "the typed set iterator could not start");
			return NULL;
		}
	} else {
		iSteps = pSet->Storage.Count - iIndex - 1u;
		if ( !xrtSetIterRBegin((xset*)&pSet->Storage, &Iterator) ) {
			__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
				"at", "the typed set reverse iterator could not start");
			return NULL;
		}
	}
	for ( size_t i = 0u; i <= iSteps; i++ ) {
		pItem = xrtSetIterNext(&Iterator);
	}
	xrtSetIterEnd(&Iterator);
	if ( pItem == NULL ) {
		__xrtTypedSetError(XERR_STATE, XTYPED_SET_ERROR_STATE,
			"at", "the typed set ended before the requested index");
	}
	return pItem;
}



/* 使用指定底层方向初始化类型集合迭代器。 */
static bool __xrtTypedSetIterStart(
	xtypedset* pSet,
	xtypedsetiter* pIterator,
	bool bReverse,
	cstr sOperation
)
{
	bool bSuccess;

	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(*pIterator));
	}
	if ( (pIterator == NULL) ||
		 !__xrtTypedSetCanRead(pSet, sOperation) ) {
		if ( pIterator == NULL ) {
			__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
				sOperation, "the typed set iterator is null");
		}
		return false;
	}
	bSuccess = bReverse ?
		xrtSetIterRBegin(&pSet->Storage, &pIterator->Base) :
		xrtSetIterBegin(&pSet->Storage, &pIterator->Base);
	if ( !bSuccess ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set iterator could not start");
		return false;
	}
	pIterator->Set = pSet;
	return true;
}



/* 启动按插入顺序的完整迭代。 */
XRT_API bool xrtTypedSetIterBegin(
	xtypedset* pSet,
	xtypedsetiter* pIterator
)
{
	return __xrtTypedSetIterStart(
		pSet, pIterator, false, "iter-begin"
	);
}



/* 启动按插入顺序逆序的完整迭代。 */
XRT_API bool xrtTypedSetIterRBegin(
	xtypedset* pSet,
	xtypedsetiter* pIterator
)
{
	return __xrtTypedSetIterStart(
		pSet, pIterator, true, "iter-rbegin"
	);
}



/* 返回下一只读规范值，并检测结构修改。 */
XRT_API const void* xrtTypedSetIterNext(xtypedsetiter* pIterator)
{
	const void* pItem;

	if ( (pIterator == NULL) || (pIterator->Set == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
				"iter-next", "the typed set iterator is null");
		}
		return NULL;
	}
	if (
		(pIterator->Base.Set != &pIterator->Set->Storage) ||
		(pIterator->Base.Version != pIterator->Set->Storage.Version)
	) {
		xrtSetIterEnd(&pIterator->Base);
		pIterator->Set = NULL;
		__xrtTypedSetError(XERR_STATE, XTYPED_SET_ERROR_STATE,
			"iter-next", "the typed set changed during iteration");
		return NULL;
	}
	pItem = xrtSetIterNext(&pIterator->Base);
	if ( pItem == NULL ) {
		pIterator->Set = NULL;
	}
	return pItem;
}



/* 提前结束迭代并清除全部借用状态。 */
XRT_API void xrtTypedSetIterEnd(xtypedsetiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}
	xrtSetIterEnd(&pIterator->Base);
	pIterator->Set = NULL;
}



/* 失败原子地把源集合缺失值合并到目标集合。 */
XRT_API bool xrtTypedSetMerge(
	xtypedset* pTarget,
	const xtypedset* pSource
)
{
	if ( !__xrtTypedSetCanMutate(pTarget, "merge") ||
		 !__xrtTypedSetCanRead(pSource, "merge") ||
		 !__xrtTypedSetSameType(pTarget, pSource, "merge") ) {
		return false;
	}
	if ( !xrtSetMerge(&pTarget->Storage, &pSource->Storage) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"merge", "the typed set merge could not be completed");
		return false;
	}
	return true;
}



/* 深度复制集合结构和值。 */
XRT_API xtypedset* xrtTypedSetClone(const xtypedset* pSet)
{
	if ( !__xrtTypedSetCanRead(pSet, "clone") ) {
		return NULL;
	}
	return __xrtTypedSetFromRaw(
		pSet, xrtSetClone(&pSet->Storage), "clone"
	);
}



/* 底层集合二元构造器共享统一调用形态。 */
typedef xset* (*xtypedsetbinaryproc)(
	const xset* pLeft,
	const xset* pRight
);



/* 验证类型 ABI 后执行一个底层集合二元构造器。 */
static xtypedset* __xrtTypedSetBinary(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	xtypedsetbinaryproc pOperation,
	cstr sOperation
)
{
	if ( !__xrtTypedSetCanRead(pLeft, sOperation) ||
		 !__xrtTypedSetCanRead(pRight, sOperation) ||
		 !__xrtTypedSetSameType(pLeft, pRight, sOperation) ) {
		return NULL;
	}
	return __xrtTypedSetFromRaw(
		pLeft,
		pOperation(&pLeft->Storage, &pRight->Storage),
		sOperation
	);
}



/* 创建两个同类型集合的并集。 */
XRT_API xtypedset* xrtTypedSetUnion(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetBinary(
		pLeft, pRight, xrtSetUnion, "union"
	);
}



/* 创建两个同类型集合的交集。 */
XRT_API xtypedset* xrtTypedSetIntersection(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetBinary(
		pLeft, pRight, xrtSetIntersection, "intersection"
	);
}



/* 创建左集合相对右集合的差集。 */
XRT_API xtypedset* xrtTypedSetDifference(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetBinary(
		pLeft, pRight, xrtSetDifference, "difference"
	);
}



/* 创建两个同类型集合的对称差集。 */
XRT_API xtypedset* xrtTypedSetSymmetricDifference(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetBinary(
		pLeft, pRight,
		xrtSetSymmetricDifference,
		"symmetric-difference"
	);
}



/* 验证两个只读集合操作数和共享类型描述。 */
static bool __xrtTypedSetOperandsValid(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	cstr sOperation
)
{
	return __xrtTypedSetCanRead(pLeft, sOperation) &&
		__xrtTypedSetCanRead(pRight, sOperation) &&
		__xrtTypedSetSameType(pLeft, pRight, sOperation);
}



/* 判断左集合是否为右集合的子集。 */
XRT_API bool xrtTypedSetIsSubset(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	bool bProper
)
{
	return __xrtTypedSetOperandsValid(pLeft, pRight, "is-subset") &&
		xrtSetIsSubset(&pLeft->Storage, &pRight->Storage, bProper);
}



/* 判断左集合是否为右集合的超集。 */
XRT_API bool xrtTypedSetIsSuperset(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	bool bProper
)
{
	return __xrtTypedSetOperandsValid(pLeft, pRight, "is-superset") &&
		xrtSetIsSuperset(&pLeft->Storage, &pRight->Storage, bProper);
}



/* 判断两个集合是否没有任何共同值。 */
XRT_API bool xrtTypedSetIsDisjoint(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetOperandsValid(pLeft, pRight, "is-disjoint") &&
		xrtSetIsDisjoint(&pLeft->Storage, &pRight->Storage);
}



/* 判断两个集合是否拥有相同的唯一值。 */
XRT_API bool xrtTypedSetEquals(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetOperandsValid(pLeft, pRight, "equals") &&
		xrtSetEqual(&pLeft->Storage, &pRight->Storage);
}



/* 初始化对象负载中的类型集合。 */
static bool __xrtTypedSetInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	if ( !xrtTypedSetTypeValidate(pType) ) {
		return false;
	}
	return xrtTypedSetInit(
		(xtypedset*)pInstance, pType->Arguments[0]
	);
}



/* 销毁对象负载中的类型集合。 */
static void __xrtTypedSetInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedSetUnit((xtypedset*)pInstance);
}



/* 类型集合追踪适配器保存对象访问器和失败状态。 */
typedef struct xtypedsettracecontext {
	const xtypedset* Set;
	const xrttype* ItemType;
	xrtobjectvisitor Visit;
	ptr UserData;
	bool Failed;
} xtypedsettracecontext;



/* 枚举一个集合值直接拥有的强对象引用。 */
static bool __xrtTypedSetTraceValue(
	const void* pItem,
	ptr pUserData
	)
{
	xtypedsettracecontext* pContext =
		(xtypedsettracecontext*)pUserData;
	bool bTraced;

	if ( !__xrtSetCallbackBegin(&pContext->Set->Storage) ) {
		pContext->Failed = true;
		return false;
	}
	bTraced = xrtTypeTraceValue(
		pContext->ItemType,
		pItem,
		pContext->Visit,
		pContext->UserData
	);
	__xrtSetCallbackEnd(&pContext->Set->Storage);
	if ( !bTraced ) {
		pContext->Failed = true;
		return false;
	}
	return true;
}



/* 枚举类型集合所有值直接拥有的强对象引用。 */
static bool __xrtTypedSetInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xtypedset* pSet = (xtypedset*)pInstance;
	xtypedsettracecontext Context;
	size_t iExpected;
	size_t iVisited;
	(void)pType;

	if ( !__xrtTypedSetCanRead(pSet, "instance-trace") ) {
		return false;
	}
	Context.Set = pSet;
	Context.ItemType = pSet->ItemType;
	Context.Visit = pVisit;
	Context.UserData = pContext;
	Context.Failed = false;
	iExpected = pSet->Storage.Count;
	iVisited = xrtSetVisit(
		&pSet->Storage, __xrtTypedSetTraceValue, &Context
	);
	if ( Context.Failed ) {
		return false;
	}
	if ( iVisited != iExpected ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			"instance-trace", "the typed set trace visit was incomplete");
		return false;
	}
	return true;
}



/* 返回对象集合负载共享的实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedSetInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedSetInstanceInit,
		.Drop = __xrtTypedSetInstanceDrop,
		.Trace = __xrtTypedSetInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的泛型集合类型描述。 */
XRT_API bool xrtTypedSetTypeValidate(const xrttype* pType)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedSetWrap(XERR_ARGUMENT, XTYPED_SET_ERROR_TYPE,
			"type-validate", "the typed set object type is invalid");
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_SET) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 1u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != sizeof(xtypedset)) ||
		(pType->InstanceAlign <
		 XRT_INTERNAL_OBJECT_ALIGNOF(xtypedset)) ||
		(pType->InstanceOps != xrtTypedSetInstanceOps())
	) {
		__xrtTypedSetError(XERR_TYPE, XTYPED_SET_ERROR_TYPE,
			"type-validate", "the typed set object type contract is invalid");
		return false;
	}
	return __xrtTypedSetItemTypeValidate(
		pType->Arguments[0], true, "type-validate"
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/typed_dict.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_TYPED_DICT)
#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)
#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT)

/* 在跨模块用户回调期间拒绝当前类型字典的全部 API 重入。 */
bool __xrtTypedDictCallbackBegin(const xtypeddict* pDict)
{
	return (pDict != NULL) && __xrtMapCallbackBegin(&pDict->Storage);
}



/* 结束当前类型字典的跨模块用户回调门禁。 */
void __xrtTypedDictCallbackEnd(const xtypeddict* pDict)
{
	if ( pDict != NULL ) {
		__xrtMapCallbackEnd(&pDict->Storage);
	}
}




/* 设置类型字典模块结构化错误。 */
static void __xrtTypedDictError(
	xerrkind Kind,
	xtypeddicterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-dict";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型或映射错误补充类型字典上下文。 */
static void __xrtTypedDictWrap(
	xerrkind DefaultKind,
	xtypeddicterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-dict";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 验证元素类型可由稳定条目字典安全拥有。 */
static bool __xrtTypedDictItemTypeValidate(
	const xrttype* pItemType,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pItemType) ) {
		__xrtTypedDictWrap(XERR_ARGUMENT, XTYPED_DICT_ERROR_TYPE,
			sOperation, "the dictionary item type is invalid");
		return false;
	}
	if ( pItemType->Size == 0u ) {
		__xrtTypedDictError(XERR_TYPE, XTYPED_DICT_ERROR_TYPE,
			sOperation, "a typed dictionary item must occupy storage");
		return false;
	}
	if ( !xrtTypeIsCopyable(pItemType) ) {
		__xrtTypedDictError(XERR_UNSUPPORTED, XTYPED_DICT_ERROR_TYPE,
			sOperation, "the dictionary item type is not copyable");
		return false;
	}
	return true;
}



/* 检查文本键视图形态并保留长度明确的内嵌零。 */
static bool __xrtTypedDictKeyValid(xstrview Key, cstr sOperation)
{
	if ( (Key.Data == NULL) && (Key.Size != 0u) ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "the dictionary key view is invalid");
		return false;
	}
	return true;
}



/* 把文本键视图转换为不改变字节边界的底层键。 */
static xbytesview __xrtTypedDictKey(xstrview Key)
{
	xbytesview Result = { (cbytes)Key.Data, Key.Size };

	return Result;
}



/* 把底层规范键转换为文本键视图。 */
static xstrview __xrtTypedDictTextKey(xbytesview Key)
{
	xstrview Result = { (cstr)Key.Data, Key.Size };

	return Result;
}



/* 销毁映射拥有的一个完整初始化类型值。 */
static void __xrtTypedDictDrop(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	(void)Key;
	xrtTypeDropValue((const xrttype*)pUserData, pValue);
}

bool __xrtTypedDictOwnershipStorage(const xtypeddict* pDict, const xrttype* pType)
{
	return pDict != NULL && pType != NULL && pDict->ItemType == pType &&
		pDict->Storage.ValueSize == pType->Size &&
		pDict->Storage.Drop == __xrtTypedDictDrop &&
		pDict->Storage.DropUserData == (ptr)pType &&
		__xrtMapUsesDefaultKeyPolicy(&pDict->Storage) &&
		__xrtMapCanMutate((xmap*)&pDict->Storage);
}



/* 默认初始化一个尚未提交的新字典值。 */
static bool __xrtTypedDictInitValue(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	(void)Key;
	return xrtTypeInitValue((const xrttype*)pUserData, pValue);
}



/* 新值复制上下文保存元素类型和借用来源。 */
typedef struct xtypeddictcopycontext {
	const xrttype* ItemType;
	const void* Item;
} xtypeddictcopycontext;



/* 移入上下文保存元素类型和调用方持有的已初始化来源。 */
typedef struct xtypeddictmovecontext {
	const xrttype* ItemType;
	ptr Item;
} xtypeddictmovecontext;



/* 初始化并复制一个尚未提交的新字典值。 */
static bool __xrtTypedDictInitCopy(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	xtypeddictcopycontext* pContext =
		(xtypeddictcopycontext*)pUserData;
	xerror* pError;
	(void)Key;

	if ( !xrtTypeInitValue(pContext->ItemType, pValue) ) {
		return false;
	}
	if ( xrtTypeCopyValue(
		pContext->ItemType, pValue, pContext->Item
	) ) {
		return true;
	}
	pError = xrtTakeError();
	xrtTypeDropValue(pContext->ItemType, pValue);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 初始化新值槽，并仅在全部存储准备完成后移入来源值。 */
static bool __xrtTypedDictInitMove(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	xtypeddictmovecontext* pContext =
		(xtypeddictmovecontext*)pUserData;
	xerror* pError;
	(void)Key;

	if ( !xrtTypeInitValue(pContext->ItemType, pValue) ) {
		return false;
	}
	if ( xrtTypeMoveValue(
		pContext->ItemType, pValue, pContext->Item
	) ) {
		return true;
	}
	pError = xrtTakeError();
	xrtTypeDropValue(pContext->ItemType, pValue);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 失败原子地替换一个完整初始化字典值。 */
static bool __xrtTypedDictReplace(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
)
{
	return xrtTypeCopyValue(
		(const xrttype*)pUserData, pTarget, pSource
	);
}



/* 适配 map 的只读回调形态，把调用方明确提供的可写来源移入旧值槽。 */
static bool __xrtTypedDictReplaceMove(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
)
{
	return xrtTypeMoveValue(
		(const xrttype*)pUserData, pTarget, (ptr)pSource
	);
}



/* 把字典值移动到调用方已经初始化的外部值。 */
static bool __xrtTypedDictMove(
	ptr pTarget,
	ptr pSource,
	ptr pUserData
)
{
	return xrtTypeMoveValue(
		(const xrttype*)pUserData, pTarget, pSource
	);
}



/* 检查公开类型字典状态、布局和底层策略是否一致。 */
static bool __xrtTypedDictValid(
	const xtypeddict* pDict,
	cstr sOperation
)
{
	if ( (pDict == NULL) || (pDict->ItemType == NULL) ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "the typed dictionary is null or uninitialized");
		return false;
	}
	if ( !__xrtMapValid(&pDict->Storage) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary storage is invalid");
		return false;
	}
	if (
		!__xrtMapUsesDefaultKeyPolicy(&pDict->Storage) ||
		(pDict->Storage.ValueSize != pDict->ItemType->Size) ||
		(pDict->Storage.Alignment < pDict->ItemType->Align) ||
		(pDict->Storage.Drop != __xrtTypedDictDrop) ||
		(pDict->Storage.DropUserData != pDict->ItemType)
	) {
		__xrtTypedDictError(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary layout or policies are invalid");
		return false;
	}
	return true;
}



/* 检查类型字典当前是否允许读取和推进迭代器。 */
static bool __xrtTypedDictCanRead(
	const xtypeddict* pDict,
	cstr sOperation
)
{
	if ( !__xrtTypedDictValid(pDict, sOperation) ) {
		return false;
	}
	if ( !__xrtMapCanRead(&pDict->Storage) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary is executing an item callback");
		return false;
	}
	return true;
}



/* 检查类型字典当前是否允许结构和生命周期修改。 */
static bool __xrtTypedDictCanMutate(
	xtypeddict* pDict,
	cstr sOperation
)
{
	if ( !__xrtTypedDictValid(pDict, sOperation) ) {
		return false;
	}
	if ( !__xrtMapCanMutate(&pDict->Storage) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary is currently being visited");
		return false;
	}
	return true;
}



/* 判断字节区间是否触及类型字典自身或拥有的底层存储。 */
static bool __xrtTypedDictOwnsRange(
	const xtypeddict* pDict,
	const void* pMemory,
	size_t iSize
)
{
	return __xrtRangesOverlap(pMemory, iSize, pDict, sizeof(*pDict)) ||
		__xrtMapOwnsRange(&pDict->Storage, pMemory, iSize);
}



/* 验证来源是外部值或字典中的准确活动值槽。 */
static bool __xrtTypedDictSourceValid(
	const xtypeddict* pDict,
	const void* pItem,
	cstr sOperation
)
{
	xmapiter Iterator;
	ptr pStored;
	bool bExact = false;

	if ( pItem == NULL ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "the source item is null");
		return false;
	}
	if ( !__xrtTypedDictOwnsRange(
		pDict, pItem, pDict->ItemType->Size
	) ) {
		return true;
	}
	if ( !xrtMapIterBegin((xmap*)&pDict->Storage, &Iterator) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary source scan could not start");
		return false;
	}
	while ( (pStored = xrtMapIterNext(&Iterator, NULL)) != NULL ) {
		if ( pStored == pItem ) {
			bExact = true;
			break;
		}
	}
	xrtMapIterEnd(&Iterator);
	if ( !bExact ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "an internal source must be an active value boundary");
		return false;
	}
	return true;
}



/* 验证移动输出完全位于类型字典拥有的内存之外。 */
static bool __xrtTypedDictOutputExternal(
	const xtypeddict* pDict,
	const void* pValue,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "the output value is null");
		return false;
	}
	if ( __xrtTypedDictOwnsRange(
		pDict, pValue, pDict->ItemType->Size
	) ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "the output value must not alias dictionary storage");
		return false;
	}
	return true;
}



/* 验证两个字典借用完全相同的值类型生命周期描述。 */
static bool __xrtTypedDictSameType(
	const xtypeddict* pLeft,
	const xtypeddict* pRight,
	cstr sOperation
)
{
	if ( pLeft->ItemType != pRight->ItemType ) {
		__xrtTypedDictError(XERR_TYPE, XTYPED_DICT_ERROR_TYPE,
			sOperation, "typed dictionary operands must share one item type descriptor");
		return false;
	}
	return true;
}



/* 在清理临时字典期间保留原始失败。 */
static void __xrtTypedDictDestroyPreserveError(xtypeddict* pDict)
{
	xerror* pError = xrtTakeError();

	xrtTypedDictDestroy(pDict);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 初始化一个拥有类型值的空字典。 */
XRT_API bool xrtTypedDictInit(
	xtypeddict* pDict,
	const xrttype* pItemType
)
{
	size_t iAlignment;

	if ( pDict == NULL ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			"init", "the typed dictionary is null");
		return false;
	}
	memset(pDict, 0, sizeof(*pDict));
	if ( !__xrtTypedDictItemTypeValidate(pItemType, "init") ) {
		return false;
	}
	iAlignment = pItemType->Align > XRT_MAP_ALIGNMENT_DEFAULT ?
		pItemType->Align : XRT_MAP_ALIGNMENT_DEFAULT;
	if ( !xrtMapInitAligned(
		&pDict->Storage, pItemType->Size, iAlignment
	) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"init", "the typed dictionary storage could not be initialized");
		return false;
	}
	pDict->ItemType = pItemType;
	if ( !xrtMapSetDrop(
		&pDict->Storage, __xrtTypedDictDrop, (ptr)pItemType
	) ) {
		xrtMapUnit(&pDict->Storage);
		memset(pDict, 0, sizeof(*pDict));
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"init", "the typed dictionary ownership policy could not be installed");
		return false;
	}
	return true;
}



/* 创建一个堆分配的空类型字典。 */
XRT_API xtypeddict* xrtTypedDictCreate(const xrttype* pItemType)
{
	xtypeddict* pDict = (xtypeddict*)xrtMalloc(sizeof(*pDict));

	if ( pDict == NULL ) {
		return NULL;
	}
	if ( !xrtTypedDictInit(pDict, pItemType) ) {
		xrtFree(pDict);
		return NULL;
	}
	return pDict;
}



/* 释放全部键值和存储，但不释放字典结构。 */
XRT_API void xrtTypedDictUnit(xtypeddict* pDict)
{
	if ( pDict == NULL ) {
		return;
	}
	if ( !__xrtTypedDictCanMutate(pDict, "unit") ) {
		return;
	}
	xrtMapUnit(&pDict->Storage);
	pDict->ItemType = NULL;
}



/* 释放类型字典持有的全部资源和堆结构。 */
XRT_API void xrtTypedDictDestroy(xtypeddict* pDict)
{
	if ( pDict == NULL ) {
		return;
	}
	if ( !__xrtTypedDictCanMutate(pDict, "destroy") ) {
		return;
	}
	xrtTypedDictUnit(pDict);
	xrtFree(pDict);
}



/* 返回字典借用的元素类型描述。 */
XRT_API const xrttype* xrtTypedDictItemType(const xtypeddict* pDict)
{
	return __xrtTypedDictCanRead(pDict, "item-type") ?
		pDict->ItemType : NULL;
}



/* 返回字典当前键值数量。 */
XRT_API size_t xrtTypedDictCount(const xtypeddict* pDict)
{
	return __xrtTypedDictCanRead(pDict, "count") ?
		xrtMapCount(&pDict->Storage) : 0u;
}



/* 返回字典再次扩容前可容纳的键值数量。 */
XRT_API size_t xrtTypedDictCapacity(const xtypeddict* pDict)
{
	return __xrtTypedDictCanRead(pDict, "capacity") ?
		xrtMapCapacity(&pDict->Storage) : 0u;
}



/* 清空全部键值并保留桶数组供后续复用。 */
XRT_API bool xrtTypedDictClear(xtypeddict* pDict)
{
	if ( !__xrtTypedDictCanMutate(pDict, "clear") ) {
		return false;
	}
	xrtMapClear(&pDict->Storage);
	return true;
}



/* 确保字典无需扩容即可容纳指定数量的键。 */
XRT_API bool xrtTypedDictReserve(xtypeddict* pDict, size_t iCapacity)
{
	if ( !__xrtTypedDictCanMutate(pDict, "reserve") ) {
		return false;
	}
	if ( !xrtMapReserve(&pDict->Storage, iCapacity) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"reserve", "the typed dictionary capacity could not be reserved");
		return false;
	}
	return true;
}



/* 把桶数组收缩到当前键数需要的最小容量。 */
XRT_API bool xrtTypedDictTrim(xtypeddict* pDict)
{
	if ( !__xrtTypedDictCanMutate(pDict, "trim") ) {
		return false;
	}
	if ( !xrtMapTrim(&pDict->Storage) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"trim", "the typed dictionary storage could not be trimmed");
		return false;
	}
	return true;
}



/* 返回已有值槽，或按元素类型默认初始化一个新值。 */
XRT_API ptr xrtTypedDictGetOrAdd(
	xtypeddict* pDict,
	xstrview Key,
	bool* pNew
)
{
	ptr pValue;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtTypedDictCanMutate(pDict, "get-or-add") ||
		 !__xrtTypedDictKeyValid(Key, "get-or-add") ) {
		return NULL;
	}
	pValue = xrtMapGetOrInit(
		&pDict->Storage,
		__xrtTypedDictKey(Key),
		__xrtTypedDictInitValue,
		(ptr)pDict->ItemType,
		pNew
	);
	if ( pValue == NULL ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"get-or-add", "the typed dictionary value could not be initialized");
	}
	return pValue;
}



/* 失败原子地复制插入或替换一个键值。 */
XRT_API bool xrtTypedDictSet(
	xtypeddict* pDict,
	xstrview Key,
	const void* pItem
)
{
	xtypeddictcopycontext Context;
	ptr pStored;
	bool bNew;

	if ( !__xrtTypedDictCanMutate(pDict, "set") ||
		 !__xrtTypedDictKeyValid(Key, "set") ||
		 !__xrtTypedDictSourceValid(pDict, pItem, "set") ) {
		return false;
	}
	Context.ItemType = pDict->ItemType;
	Context.Item = pItem;
	pStored = __xrtMapSetOrInit(
		&pDict->Storage,
		__xrtTypedDictKey(Key),
		pItem,
		__xrtTypedDictReplace,
		(ptr)pDict->ItemType,
		__xrtTypedDictInitCopy,
		&Context,
		&bNew
	);
	if ( pStored == NULL ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"set", "the typed dictionary value could not be set");
		return false;
	}
	(void)bNew;
	return true;
}



/* 失败原子地把外部已初始化值移入指定键。 */
XRT_API bool xrtTypedDictSetTake(
	xtypeddict* pDict,
	xstrview Key,
	ptr pItem
)
{
	xtypeddictmovecontext Context;
	ptr pStored;
	bool bNew;

	if ( !__xrtTypedDictCanMutate(pDict, "set-take") ||
		 !__xrtTypedDictKeyValid(Key, "set-take") ||
		 !__xrtTypedDictOutputExternal(pDict, pItem, "set-take") ) {
		return false;
	}
	Context.ItemType = pDict->ItemType;
	Context.Item = pItem;
	pStored = __xrtMapSetOrInit(
		&pDict->Storage,
		__xrtTypedDictKey(Key),
		pItem,
		__xrtTypedDictReplaceMove,
		(ptr)pDict->ItemType,
		__xrtTypedDictInitMove,
		&Context,
		&bNew
	);
	if ( pStored == NULL ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"set-take", "the typed dictionary value could not receive the source");
		return false;
	}
	(void)bNew;
	return true;
}



/* 返回指定键的可写借用值槽。 */
XRT_API ptr xrtTypedDictGet(xtypeddict* pDict, xstrview Key)
{
	if ( !__xrtTypedDictCanRead(pDict, "get") ||
		 !__xrtTypedDictKeyValid(Key, "get") ) {
		return NULL;
	}
	return xrtMapGet(&pDict->Storage, __xrtTypedDictKey(Key));
}



/* 返回指定键的只读借用值槽。 */
XRT_API const void* xrtTypedDictConstGet(
	const xtypeddict* pDict,
	xstrview Key
)
{
	if ( !__xrtTypedDictCanRead(pDict, "const-get") ||
		 !__xrtTypedDictKeyValid(Key, "const-get") ) {
		return NULL;
	}
	return xrtMapConstGet(&pDict->Storage, __xrtTypedDictKey(Key));
}



/* 判断指定文本键是否存在。 */
XRT_API bool xrtTypedDictHas(const xtypeddict* pDict, xstrview Key)
{
	if ( !__xrtTypedDictCanRead(pDict, "has") ||
		 !__xrtTypedDictKeyValid(Key, "has") ) {
		return false;
	}
	return xrtMapHas(&pDict->Storage, __xrtTypedDictKey(Key));
}



/* 返回与查询等价的内部规范键视图。 */
XRT_API bool xrtTypedDictStoredKey(
	const xtypeddict* pDict,
	xstrview Key,
	xstrview* pStoredKey
)
{
	xbytesview Stored;

	if ( pStoredKey != NULL ) {
		pStoredKey->Data = NULL;
		pStoredKey->Size = 0u;
	}
	if ( (pStoredKey == NULL) ||
		 !__xrtTypedDictCanRead(pDict, "stored-key") ||
		 !__xrtTypedDictKeyValid(Key, "stored-key") ) {
		if ( pStoredKey == NULL ) {
			__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
				"stored-key", "the stored key output is null");
		}
		return false;
	}
	if ( !xrtMapStoredKey(
		&pDict->Storage, __xrtTypedDictKey(Key), &Stored
	) ) {
		return false;
	}
	*pStoredKey = __xrtTypedDictTextKey(Stored);
	return true;
}



/* 删除指定键并执行类型资源释放。 */
XRT_API bool xrtTypedDictRemove(xtypeddict* pDict, xstrview Key)
{
	if ( !__xrtTypedDictCanMutate(pDict, "remove") ||
		 !__xrtTypedDictKeyValid(Key, "remove") ) {
		return false;
	}
	return xrtMapRemove(&pDict->Storage, __xrtTypedDictKey(Key));
}



/* 把值移动到外部已初始化输出后删除指定键。 */
XRT_API bool xrtTypedDictTake(
	xtypeddict* pDict,
	xstrview Key,
	ptr pValue
)
{
	if ( !__xrtTypedDictCanMutate(pDict, "take") ||
		 !__xrtTypedDictKeyValid(Key, "take") ||
		 !__xrtTypedDictOutputExternal(pDict, pValue, "take") ) {
		return false;
	}
	if ( !xrtMapHas(&pDict->Storage, __xrtTypedDictKey(Key)) ) {
		return false;
	}
	if ( !__xrtMapMoveOut(
		&pDict->Storage,
		__xrtTypedDictKey(Key),
		pValue,
		__xrtTypedDictMove,
		(ptr)pDict->ItemType
	) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"take", "the typed dictionary value could not be moved out");
		return false;
	}
	return true;
}



/* 按最短插入顺序方向查找指定位置的值。 */
static ptr __xrtTypedDictAt(
	const xtypeddict* pDict,
	size_t iIndex,
	xstrview* pKey,
	cstr sOperation
)
{
	xmapiter Iterator;
	xbytesview Key;
	ptr pValue = NULL;
	size_t iSteps;

	if ( pKey != NULL ) {
		pKey->Data = NULL;
		pKey->Size = 0u;
	}
	if ( !__xrtTypedDictCanRead(pDict, sOperation) ) {
		return NULL;
	}
	if ( iIndex >= pDict->Storage.Count ) {
		__xrtTypedDictError(XERR_RANGE, XTYPED_DICT_ERROR_RANGE,
			sOperation, "the typed dictionary index is out of range");
		return NULL;
	}
	if ( iIndex <= ((pDict->Storage.Count - 1u) >> 1u) ) {
		iSteps = iIndex;
		if ( !xrtMapIterBegin((xmap*)&pDict->Storage, &Iterator) ) {
			__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
				sOperation, "the typed dictionary iterator could not start");
			return NULL;
		}
	} else {
		iSteps = pDict->Storage.Count - iIndex - 1u;
		if ( !xrtMapIterRBegin((xmap*)&pDict->Storage, &Iterator) ) {
			__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
				sOperation, "the typed dictionary reverse iterator could not start");
			return NULL;
		}
	}
	for ( size_t i = 0u; i <= iSteps; i++ ) {
		pValue = xrtMapIterNext(&Iterator, &Key);
	}
	xrtMapIterEnd(&Iterator);
	if ( pValue == NULL ) {
		__xrtTypedDictError(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary ended before the requested index");
		return NULL;
	}
	if ( pKey != NULL ) {
		*pKey = __xrtTypedDictTextKey(Key);
	}
	return pValue;
}



/* 按插入顺序返回指定位置的可写值槽和可选键。 */
XRT_API ptr xrtTypedDictAt(
	xtypeddict* pDict,
	size_t iIndex,
	xstrview* pKey
)
{
	return __xrtTypedDictAt(pDict, iIndex, pKey, "at");
}



/* 按插入顺序返回指定位置的只读值槽和可选键。 */
XRT_API const void* xrtTypedDictConstAt(
	const xtypeddict* pDict,
	size_t iIndex,
	xstrview* pKey
)
{
	return __xrtTypedDictAt(pDict, iIndex, pKey, "const-at");
}



/* 使用指定底层方向初始化类型字典迭代器。 */
static bool __xrtTypedDictIterStart(
	xtypeddict* pDict,
	xtypeddictiter* pIterator,
	bool bReverse,
	cstr sOperation
)
{
	bool bSuccess;

	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(*pIterator));
	}
	if ( (pIterator == NULL) ||
		 !__xrtTypedDictCanRead(pDict, sOperation) ) {
		if ( pIterator == NULL ) {
			__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
				sOperation, "the typed dictionary iterator is null");
		}
		return false;
	}
	bSuccess = bReverse ?
		xrtMapIterRBegin(&pDict->Storage, &pIterator->Base) :
		xrtMapIterBegin(&pDict->Storage, &pIterator->Base);
	if ( !bSuccess ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary iterator could not start");
		return false;
	}
	pIterator->Dict = pDict;
	return true;
}



/* 启动按插入顺序的完整迭代。 */
XRT_API bool xrtTypedDictIterBegin(
	xtypeddict* pDict,
	xtypeddictiter* pIterator
)
{
	return __xrtTypedDictIterStart(
		pDict, pIterator, false, "iter-begin"
	);
}



/* 启动按插入顺序逆序的完整迭代。 */
XRT_API bool xrtTypedDictIterRBegin(
	xtypeddict* pDict,
	xtypeddictiter* pIterator
)
{
	return __xrtTypedDictIterStart(
		pDict, pIterator, true, "iter-rbegin"
	);
}



/* 返回下一借用值槽和可选规范键，并检测结构修改。 */
XRT_API ptr xrtTypedDictIterNext(
	xtypeddictiter* pIterator,
	xstrview* pKey
)
{
	xbytesview Key;
	ptr pValue;

	if ( pKey != NULL ) {
		pKey->Data = NULL;
		pKey->Size = 0u;
	}
	if ( (pIterator == NULL) || (pIterator->Dict == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
				"iter-next", "the typed dictionary iterator is null");
		}
		return NULL;
	}
	if (
		(pIterator->Base.Map != &pIterator->Dict->Storage) ||
		(pIterator->Base.Version != pIterator->Dict->Storage.Version)
	) {
		xrtMapIterEnd(&pIterator->Base);
		pIterator->Dict = NULL;
		__xrtTypedDictError(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"iter-next", "the typed dictionary changed during iteration");
		return NULL;
	}
	pValue = xrtMapIterNext(&pIterator->Base, &Key);
	if ( pValue == NULL ) {
		pIterator->Dict = NULL;
		return NULL;
	}
	if ( pKey != NULL ) {
		*pKey = __xrtTypedDictTextKey(Key);
	}
	return pValue;
}



/* 提前结束迭代并清除全部借用状态。 */
XRT_API void xrtTypedDictIterEnd(xtypeddictiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}
	xrtMapIterEnd(&pIterator->Base);
	pIterator->Dict = NULL;
}



/* 在调用方保护来源结构期间深复制字典。 */
static xtypeddict* __xrtTypedDictCloneProtected(
	const xtypeddict* pDict,
	cstr sOperation
)
{
	xtypeddict* pClone;
	xmapiter Iterator;
	xbytesview Key;
	ptr pValue;

	pClone = xrtTypedDictCreate(pDict->ItemType);
	if ( pClone == NULL ) {
		__xrtTypedDictWrap(XERR_MEMORY, XTYPED_DICT_ERROR_OPERATION,
			sOperation, "the typed dictionary clone could not be created");
		return NULL;
	}
	if ( !xrtTypedDictReserve(pClone, pDict->Storage.Count) ||
		 !xrtMapIterBegin((xmap*)&pDict->Storage, &Iterator) ) {
		__xrtTypedDictDestroyPreserveError(pClone);
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			sOperation, "the typed dictionary clone could not be prepared");
		return NULL;
	}
	while ( (pValue = xrtMapIterNext(&Iterator, &Key)) != NULL ) {
		if ( !xrtTypedDictSet(
			pClone, __xrtTypedDictTextKey(Key), pValue
		) ) {
			xrtMapIterEnd(&Iterator);
			__xrtTypedDictDestroyPreserveError(pClone);
			__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
				sOperation, "a typed dictionary value could not be cloned");
			return NULL;
		}
	}
	xrtMapIterEnd(&Iterator);
	return pClone;
}



/* 深复制一个独立堆类型字典。 */
XRT_API xtypeddict* xrtTypedDictClone(const xtypeddict* pDict)
{
	xtypeddict* pClone;
	bool bProtected;

	if ( !__xrtTypedDictCanRead(pDict, "clone") ) {
		return NULL;
	}
	if ( !__xrtMapProtectRead(&pDict->Storage, &bProtected) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"clone", "the typed dictionary source could not be protected");
		return NULL;
	}
	pClone = __xrtTypedDictCloneProtected(pDict, "clone");
	__xrtMapUnprotectRead(&pDict->Storage, bProtected);
	return pClone;
}



/* 递增结构版本并跳过外置迭代器保留的零值。 */
static uint64 __xrtTypedDictNextVersion(uint64 iVersion)
{
	iVersion++;
	return iVersion != 0u ? iVersion : 1u;
}



/* 事务合并同类型字典，并按策略处理冲突键。 */
XRT_API bool xrtTypedDictMerge(
	xtypeddict* pTarget,
	const xtypeddict* pSource,
	bool bReplace
)
{
	xtypeddict* pWork;
	xmapiter Iterator;
	xbytesview Key;
	ptr pValue;
	bool bTargetProtected;
	bool bSourceProtected;
	bool bSuccess = true;
	uint64 iTargetVersion;

	if ( !__xrtTypedDictCanMutate(pTarget, "merge") ||
		 !__xrtTypedDictCanRead(pSource, "merge") ||
		 !__xrtTypedDictSameType(pTarget, pSource, "merge") ) {
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( !__xrtMapProtectRead(
		&pTarget->Storage, &bTargetProtected
	) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"merge", "the target dictionary could not be protected");
		return false;
	}
	if ( !__xrtMapProtectRead(
		&pSource->Storage, &bSourceProtected
	) ) {
		__xrtMapUnprotectRead(&pTarget->Storage, bTargetProtected);
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"merge", "the source dictionary could not be protected");
		return false;
	}
	pWork = __xrtTypedDictCloneProtected(pTarget, "merge");
	if ( pWork == NULL ) {
		__xrtMapUnprotectRead(&pSource->Storage, bSourceProtected);
		__xrtMapUnprotectRead(&pTarget->Storage, bTargetProtected);
		return false;
	}
	if ( !xrtMapIterBegin((xmap*)&pSource->Storage, &Iterator) ) {
		bSuccess = false;
	} else {
		while ( (pValue = xrtMapIterNext(&Iterator, &Key)) != NULL ) {
			xstrview TextKey = __xrtTypedDictTextKey(Key);

			if ( !bReplace && xrtTypedDictHas(pWork, TextKey) ) {
				continue;
			}
			if ( !xrtTypedDictSet(pWork, TextKey, pValue) ) {
				bSuccess = false;
				break;
			}
		}
		xrtMapIterEnd(&Iterator);
	}
	__xrtMapUnprotectRead(&pSource->Storage, bSourceProtected);
	__xrtMapUnprotectRead(&pTarget->Storage, bTargetProtected);
	if ( !bSuccess ) {
		__xrtTypedDictDestroyPreserveError(pWork);
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"merge", "a source dictionary value could not be merged");
		return false;
	}

	iTargetVersion = pTarget->Storage.Version;
	{
		xmap Temporary = pTarget->Storage;

		pTarget->Storage = pWork->Storage;
		pWork->Storage = Temporary;
	}
	pTarget->Storage.Version = __xrtTypedDictNextVersion(iTargetVersion);
	xrtTypedDictDestroy(pWork);
	return true;
}



/* 比较两个字典的类型、键集合和值内容。 */
XRT_API bool xrtTypedDictEquals(
	const xtypeddict* pLeft,
	const xtypeddict* pRight
)
{
	xmapiter Iterator;
	xbytesview Key;
	ptr pLeftValue;
	const void* pRightValue;
	bool bLeftProtected;
	bool bRightProtected;
	bool bEqual = true;
	bool bFailed = false;
	int iCompare;

	if ( !__xrtTypedDictCanRead(pLeft, "equals") ||
		 !__xrtTypedDictCanRead(pRight, "equals") ||
		 !__xrtTypedDictSameType(pLeft, pRight, "equals") ) {
		return false;
	}
	if ( pLeft == pRight ) {
		return true;
	}
	if ( pLeft->Storage.Count != pRight->Storage.Count ) {
		return false;
	}
	if ( !xrtTypeIsComparable(pLeft->ItemType) ) {
		__xrtTypedDictError(XERR_UNSUPPORTED, XTYPED_DICT_ERROR_TYPE,
			"equals", "the dictionary item type is not comparable");
		return false;
	}
	if ( !__xrtMapProtectRead(&pLeft->Storage, &bLeftProtected) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"equals", "the left dictionary could not be protected");
		return false;
	}
	if ( !__xrtMapProtectRead(&pRight->Storage, &bRightProtected) ) {
		__xrtMapUnprotectRead(&pLeft->Storage, bLeftProtected);
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"equals", "the right dictionary could not be protected");
		return false;
	}
	if ( !xrtMapIterBegin((xmap*)&pLeft->Storage, &Iterator) ) {
		bEqual = false;
		bFailed = true;
	} else {
		while ( (pLeftValue = xrtMapIterNext(&Iterator, &Key)) != NULL ) {
			pRightValue = xrtMapConstGet(&pRight->Storage, Key);
			if ( pRightValue == NULL ) {
				bEqual = false;
				break;
			}
			if ( !__xrtMapCallbackBegin(&pLeft->Storage) ) {
				bEqual = false;
				bFailed = true;
				break;
			}
			if ( !__xrtMapCallbackBegin(&pRight->Storage) ) {
				__xrtMapCallbackEnd(&pLeft->Storage);
				bEqual = false;
				bFailed = true;
				break;
			}
			if ( !xrtTypeCompareValue(
				pLeft->ItemType,
				pLeftValue,
				pRightValue,
				&iCompare
			) ) {
				__xrtMapCallbackEnd(&pRight->Storage);
				__xrtMapCallbackEnd(&pLeft->Storage);
				bEqual = false;
				bFailed = true;
				break;
			}
			__xrtMapCallbackEnd(&pRight->Storage);
			__xrtMapCallbackEnd(&pLeft->Storage);
			if ( iCompare != 0 ) {
				bEqual = false;
				break;
			}
		}
		xrtMapIterEnd(&Iterator);
	}
	__xrtMapUnprotectRead(&pRight->Storage, bRightProtected);
	__xrtMapUnprotectRead(&pLeft->Storage, bLeftProtected);
	if ( bFailed ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"equals", "the dictionary values could not be compared");
	}
	return bEqual;
}



/* 初始化对象负载中的类型字典。 */
static bool __xrtTypedDictInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	if ( !xrtTypedDictTypeValidate(pType) ) {
		return false;
	}
	return xrtTypedDictInit(
		(xtypeddict*)pInstance, pType->Arguments[0]
	);
}



/* 销毁对象负载中的类型字典。 */
static void __xrtTypedDictInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedDictUnit((xtypeddict*)pInstance);
}



/* 类型字典追踪适配器保存对象访问器和失败状态。 */
typedef struct xtypeddicttracecontext {
	const xtypeddict* Dict;
	const xrttype* ItemType;
	xrtobjectvisitor Visit;
	ptr UserData;
	bool Failed;
} xtypeddicttracecontext;



/* 枚举一个字典值直接拥有的强对象引用。 */
static bool __xrtTypedDictTraceValue(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	xtypeddicttracecontext* pContext =
		(xtypeddicttracecontext*)pUserData;
	bool bTraced;
	(void)Key;

	if ( !__xrtMapCallbackBegin(&pContext->Dict->Storage) ) {
		pContext->Failed = true;
		return false;
	}
	bTraced = xrtTypeTraceValue(
		pContext->ItemType,
		pValue,
		pContext->Visit,
		pContext->UserData
	);
	__xrtMapCallbackEnd(&pContext->Dict->Storage);
	if ( !bTraced ) {
		pContext->Failed = true;
		return false;
	}
	return true;
}



/* 枚举类型字典所有值直接拥有的强对象引用。 */
static bool __xrtTypedDictInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xtypeddict* pDict = (xtypeddict*)pInstance;
	xtypeddicttracecontext Context;
	size_t iExpected;
	size_t iVisited;
	(void)pType;

	if ( !__xrtTypedDictCanRead(pDict, "instance-trace") ) {
		return false;
	}
	Context.Dict = pDict;
	Context.ItemType = pDict->ItemType;
	Context.Visit = pVisit;
	Context.UserData = pContext;
	Context.Failed = false;
	iExpected = pDict->Storage.Count;
	iVisited = xrtMapVisit(
		&pDict->Storage, __xrtTypedDictTraceValue, &Context
	);
	if ( Context.Failed ) {
		return false;
	}
	if ( iVisited != iExpected ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"instance-trace", "the typed dictionary trace visit was incomplete");
		return false;
	}
	return true;
}



/* 返回对象字典负载共享的实例操作表。 */
const xrtinstanceops __xrtTypedDictInstanceOperations = {
	.Init = __xrtTypedDictInstanceInit,
	.Drop = __xrtTypedDictInstanceDrop,
	.Trace = __xrtTypedDictInstanceTrace
};



/* 返回对象字典载荷共享的实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedDictInstanceOps(void)
{
	return &__xrtTypedDictInstanceOperations;
}



/* 验证可由对象系统承载的泛型字典类型描述。 */
XRT_API bool xrtTypedDictTypeValidate(const xrttype* pType)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedDictWrap(XERR_ARGUMENT, XTYPED_DICT_ERROR_TYPE,
			"type-validate", "the typed dictionary object type is invalid");
		return false;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)
	/* This exact built-in type retains a typed-dictionary payload prefix and
	 * adds resident retirement storage. Do not admit structural lookalikes. */
	if (pType == xrtDynamicFieldsType()) return true;
#endif
	if (
		(pType->Kind != XRT_TYPE_DICT) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 1u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != sizeof(xtypeddict)) ||
		(pType->InstanceAlign <
		 XRT_INTERNAL_OBJECT_ALIGNOF(xtypeddict)) ||
		(pType->InstanceOps != xrtTypedDictInstanceOps())
	) {
		__xrtTypedDictError(XERR_TYPE, XTYPED_DICT_ERROR_TYPE,
			"type-validate", "the typed dictionary object type contract is invalid");
		return false;
	}
	return __xrtTypedDictItemTypeValidate(
		pType->Arguments[0], "type-validate"
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xruntime/src/runtime/runtime_dynamic_field.c */
/* ========================================================================== */

#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)



#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)

/* The dictionary remains the complete typed-dictionary payload. Clear makes
 * its existing owning slots inaccessible/retired, just like core phased
 * handles; Finish detaches the header before running destructors. No second
 * header, sidecar allocation, registry or per-field retirement array. */
typedef struct xrtdynamicfieldpayload {
	xtypeddict Live;
} xrtdynamicfieldpayload;

static bool __xrtDynamicFieldInstanceInit(ptr, const xrttype*);
static void __xrtDynamicFieldInstanceDrop(ptr, const xrttype*);
static bool __xrtDynamicFieldInstanceTrace(const void*, const xrttype*,
	xrtobjectvisitor, ptr);
static const xrtinstanceops __xrtDynamicFieldInstanceOperations = {
	.Init = __xrtDynamicFieldInstanceInit,
	.Drop = __xrtDynamicFieldInstanceDrop,
	.Trace = __xrtDynamicFieldInstanceTrace
};

/* Complete transitions participate, including nested direct XRT callers.
 * These wrappers do not make this container concurrently mutable: callers
 * still serialize access. The freeze excludes every public storage mutation
 * and real strong retain, not merely the individual Value counter changes. */
#define xrtDynamicFieldsCreate __xrtDynamicBodyCreate
#define xrtDynamicFieldsRef __xrtDynamicBodyRef
#define xrtDynamicFieldsCount __xrtDynamicBodyCount
#define xrtDynamicFieldsCapacity __xrtDynamicBodyCapacity
#define xrtDynamicFieldsClear __xrtDynamicBodyClear
#define xrtDynamicFieldsReserve __xrtDynamicBodyReserve
#define xrtDynamicFieldsTrim __xrtDynamicBodyTrim
#define xrtDynamicFieldsHas __xrtDynamicBodyHas
#define xrtDynamicFieldsGet __xrtDynamicBodyGet
#define xrtDynamicFieldsGetRef __xrtDynamicBodyGetRef
#define xrtDynamicFieldsCopy __xrtDynamicBodyCopy
#define xrtDynamicFieldsStoredName __xrtDynamicBodyStoredName
#define xrtDynamicFieldsSet __xrtDynamicBodySet
#define xrtDynamicFieldsSetTake __xrtDynamicBodySetTake
#define xrtDynamicFieldsSetNew __xrtDynamicBodySetNew
#define xrtDynamicFieldsSetRef __xrtDynamicBodySetRef
#define xrtDynamicFieldsSetRefTake __xrtDynamicBodySetRefTake
#define xrtDynamicFieldsSetRefNew __xrtDynamicBodySetRefNew
#define xrtDynamicFieldsRemove __xrtDynamicBodyRemove
#define xrtDynamicFieldsTake __xrtDynamicBodyTake
#define xrtDynamicFieldsIterBegin __xrtDynamicBodyIterBegin
#define xrtDynamicFieldsIterRBegin __xrtDynamicBodyIterRBegin
#define xrtDynamicFieldsIterNext __xrtDynamicBodyIterNext
#define xrtDynamicFieldsVisitV1 __xrtDynamicBodyVisitV1
#define xrtDynamicFieldsMerge __xrtDynamicBodyMerge
#define xrtDynamicFieldsMergeRefV1 __xrtDynamicBodyMergeRefV1
#define xrtDynamicFieldsClone __xrtDynamicBodyClone
#define xrtDynamicFieldsKeys __xrtDynamicBodyKeys
#define xrtDynamicFieldsValues __xrtDynamicBodyValues
#define xrtDynamicFieldsItems __xrtDynamicBodyItems
#define xrtDynamicFieldsToValue __xrtDynamicBodyToValue
#define xrtDynamicFieldsFromValue __xrtDynamicBodyFromValue

/* 动态字段的唯一泛型实参是拥有 xvalue 指针的运行时 Value 类型。 */
static const xrttype* const __xrtDynamicFieldArguments[] = {
	&__xrtTypeValueDescriptor
};



/* 动态字段表是可追踪引用对象，值语义统一复用对象强引用操作。 */
static const xrttype __xrtDynamicFieldTypeDescriptor = {
	.Id = UINT64_C(0x34E2328DABCB19D3),
	.Kind = XRT_TYPE_DICT,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_REFERENCE |
		XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL |
		XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("DynamicFields"),
	.AbiName = XRT_STR_INIT("xrt.DynamicFields"),
	.Size = sizeof(xrtdynamicfields*),
	.Align = XRT_INTERNAL_ALIGNOF(xrtdynamicfields*),
	.InstanceSize = sizeof(xrtdynamicfieldpayload),
	.InstanceAlign = XRT_INTERNAL_ALIGNOF(xrtdynamicfieldpayload),
	.Ops = &__xrtObjectValueOperations,
	.InstanceOps = &__xrtDynamicFieldInstanceOperations,
	.ArgumentCount = 1u,
	.Arguments = __xrtDynamicFieldArguments
};

static bool __xrtDynamicFieldInstanceInit(ptr pInstance, const xrttype* pType)
{
	if (pType != &__xrtDynamicFieldTypeDescriptor) return false;
	memset(pInstance, 0, sizeof(xrtdynamicfieldpayload));
	return xrtTypedDictInit(&((xrtdynamicfieldpayload*)pInstance)->Live,
		&__xrtTypeValueDescriptor);
}

static void __xrtDynamicFieldInstanceDrop(ptr pInstance, const xrttype* pType)
{
	xrtdynamicfieldpayload* pPayload = pInstance;
	xrtownershipscope Mutation = {0};
	xtypeddict Live;
	(void)pType;
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	Live = pPayload->Live;
	memset(pPayload, 0, sizeof(*pPayload));
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	/* Never call an arbitrary Value/handle destructor under graph freeze. */
	if (Live.ItemType != NULL) xrtTypedDictUnit(&Live);
}

static bool __xrtDynamicFieldInstanceTrace(const void* pInstance,
	const xrttype* pType, xrtobjectvisitor pVisit, ptr pContext)
{
	const xrtdynamicfieldpayload* pPayload = pInstance;
	if (pPayload->Live.ItemType == NULL) return true;
	return __xrtTypedDictInstanceOperations.Trace(&pPayload->Live, pType,
		pVisit, pContext);
}



/* 设置动态字段模块结构化错误。 */
static void __xrtDynamicFieldError(
	xerrkind Kind,
	xdynamicfielderror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.dynamic-field";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层对象、类型字典或 Value 错误补充动态字段上下文。 */
static void __xrtDynamicFieldWrap(
	xerrkind DefaultKind,
	xdynamicfielderror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.dynamic-field";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 验证对象的精确动态字段类型并返回可写字典载荷。 */
static xtypeddict* __xrtDynamicFieldDict(
	xrtdynamicfields* pFields,
	cstr sOperation
)
{
	if ( pFields == NULL ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, sOperation,
			"the dynamic field object is null");
		return NULL;
	}
	if ( xrtObjectType(pFields) != &__xrtDynamicFieldTypeDescriptor ) {
		__xrtDynamicFieldError(XERR_TYPE,
			XDYNAMIC_FIELD_ERROR_TYPE, sOperation,
			"the object is not an xrt.DynamicFields instance");
		return NULL;
	}
	if (pFields->OwnershipCleared) {
		__xrtErrorSetInvalidState(); return NULL;
	}
	return (xtypeddict*)xrtObjectData(pFields);
}



/* 验证对象的精确动态字段类型并返回只读字典载荷。 */
static const xtypeddict* __xrtDynamicFieldConstDict(
	const xrtdynamicfields* pFields,
	cstr sOperation
)
{
	if ( pFields == NULL ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, sOperation,
			"the dynamic field object is null");
		return NULL;
	}
	if ( xrtObjectType(pFields) != &__xrtDynamicFieldTypeDescriptor ) {
		__xrtDynamicFieldError(XERR_TYPE,
			XDYNAMIC_FIELD_ERROR_TYPE, sOperation,
			"the object is not an xrt.DynamicFields instance");
		return NULL;
	}
	if (pFields->OwnershipCleared) {
		__xrtErrorSetInvalidState(); return NULL;
	}
	return (const xtypeddict*)xrtObjectConstData(pFields);
}



/* 验证长度明确的字段名视图，允许空名称但拒绝悬空非空区间。 */
static bool __xrtDynamicFieldNameValid(
	xstrview Name,
	cstr sOperation
)
{
	if ( (Name.Data == NULL) && (Name.Size != 0u) ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, sOperation,
			"the dynamic field name view is invalid");
		return false;
	}
	return true;
}



/* 在清理临时对象期间保留原始失败。 */
static void __xrtDynamicFieldUnrefPreserveError(
	xrtdynamicfields* pFields
)
{
	xerror* pError = xrtTakeError();

	xrtObjectUnref(pFields);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 深复制一个 Value 图并失败原子地提交到字段字典。 */
static bool __xrtDynamicFieldSetClone(
	xtypeddict* pDict,
	xstrview Name,
	const xvalue* pValue,
	cstr sOperation,
	cstr sFailure
)
{
	xvalue* pCopy = xrtValueDeepClone(pValue);

	if ( pCopy == NULL ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
			"the dynamic field source could not be cloned");
		return false;
	}
	if ( !xrtTypedDictSetTake(pDict, Name, &pCopy) ) {
		xrtValueRelease(pCopy);
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation, sFailure);
		return false;
	}
	xrtValueRelease(pCopy);
	return true;
}



/* 返回进程期稳定的动态字段运行时类型。 */
XRT_API const xrttype* xrtDynamicFieldsType(void)
{
	return &__xrtDynamicFieldTypeDescriptor;
}



/* 创建一个空动态字段对象。 */
static xrtdynamicfields* xrtDynamicFieldsCreate(void)
{
	xrtdynamicfields* pFields = xrtObjectCreate(
		&__xrtDynamicFieldTypeDescriptor
	);

	if ( pFields == NULL ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, "create",
			"the dynamic field object could not be created");
	}
	return pFields;
}



/* 保留一个动态字段对象。 */
static xrtdynamicfields* xrtDynamicFieldsRef(xrtdynamicfields* pFields)
{
	if ( __xrtDynamicFieldDict(pFields, "ref") == NULL ) {
		return NULL;
	}
	if ( xrtObjectRef(pFields) == NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, "ref",
			"the dynamic field object could not be retained");
		return NULL;
	}
	return pFields;
}



/* 释放一个动态字段对象，允许空指针。 */
XRT_API void xrtDynamicFieldsUnref(xrtdynamicfields* pFields)
{
	if ( pFields == NULL ) {
		return;
	}
	/* Dropping a real pin remains valid after phased Clear. */
	if ( xrtObjectType(pFields) != &__xrtDynamicFieldTypeDescriptor ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	xrtObjectUnref(pFields);
}



/* 返回当前动态字段数量。 */
static size_t xrtDynamicFieldsCount(const xrtdynamicfields* pFields)
{
	const xtypeddict* pDict = __xrtDynamicFieldConstDict(pFields, "count");

	return pDict != NULL ? xrtTypedDictCount(pDict) : 0u;
}



/* 返回动态字段表再次扩容前的容量。 */
static size_t xrtDynamicFieldsCapacity(const xrtdynamicfields* pFields)
{
	const xtypeddict* pDict = __xrtDynamicFieldConstDict(pFields, "capacity");

	return pDict != NULL ? xrtTypedDictCapacity(pDict) : 0u;
}



/* 清空动态字段并保留存储供后续复用。 */
static bool xrtDynamicFieldsClear(xrtdynamicfields* pFields)
{
	xtypeddict* pDict = __xrtDynamicFieldDict(pFields, "clear");

	if ( (pDict != NULL) && xrtTypedDictClear(pDict) ) {
		return true;
	}
	if ( pDict != NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, "clear",
			"the dynamic fields could not be cleared");
	}
	return false;
}



/* 预留指定数量的动态字段。 */
static bool xrtDynamicFieldsReserve(
	xrtdynamicfields* pFields,
	size_t iCapacity
)
{
	xtypeddict* pDict = __xrtDynamicFieldDict(pFields, "reserve");

	if ( (pDict != NULL) && xrtTypedDictReserve(pDict, iCapacity) ) {
		return true;
	}
	if ( pDict != NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, "reserve",
			"the dynamic field capacity could not be reserved");
	}
	return false;
}



/* 释放动态字段表的多余容量。 */
static bool xrtDynamicFieldsTrim(xrtdynamicfields* pFields)
{
	xtypeddict* pDict = __xrtDynamicFieldDict(pFields, "trim");

	if ( (pDict != NULL) && xrtTypedDictTrim(pDict) ) {
		return true;
	}
	if ( pDict != NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, "trim",
			"the dynamic fields could not be trimmed");
	}
	return false;
}



/* 判断指定名称是否存在。 */
static bool xrtDynamicFieldsHas(
	const xrtdynamicfields* pFields,
	xstrview Name
)
{
	const xtypeddict* pDict = __xrtDynamicFieldConstDict(pFields, "has");

	return (pDict != NULL) &&
		__xrtDynamicFieldNameValid(Name, "has") &&
		xrtTypedDictHas(pDict, Name);
}



/* 返回字段拥有的只读借用 Value。 */
static const xvalue* xrtDynamicFieldsGet(
	const xrtdynamicfields* pFields,
	xstrview Name
)
{
	const xtypeddict* pDict = __xrtDynamicFieldConstDict(pFields, "get");
	xvalue* const* ppValue;

	if ( pDict == NULL ) {
		return NULL;
	}
	if ( !__xrtDynamicFieldNameValid(Name, "get") ) {
		return NULL;
	}
	ppValue = (xvalue* const*)xrtTypedDictConstGet(pDict, Name);
	return ppValue != NULL ? *ppValue : NULL;
}



/* 保留并返回字段当前拥有的同一 Value。 */
static xvalue* xrtDynamicFieldsGetRef(
	const xrtdynamicfields* pFields,
	xstrview Name
)
{
	const xvalue* pValue = xrtDynamicFieldsGet(pFields, Name);

	return pValue != NULL ? xrtValueRetain(pValue) : NULL;
}



/* 深复制字段值，使调用方拥有独立的可变值图。 */
static xvalue* xrtDynamicFieldsCopy(
	const xrtdynamicfields* pFields,
	xstrview Name
)
{
	const xvalue* pValue = xrtDynamicFieldsGet(pFields, Name);
	xvalue* pCopy;

	if ( pValue == NULL ) {
		return NULL;
	}
	pCopy = xrtValueDeepClone(pValue);
	if ( pCopy == NULL ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, "copy",
			"the dynamic field value could not be copied");
	}
	return pCopy;
}



/* 返回与查询等价的内部规范字段名视图。 */
static bool xrtDynamicFieldsStoredName(
	const xrtdynamicfields* pFields,
	xstrview Name,
	xstrview* pStoredName
)
{
	const xtypeddict* pDict;

	if ( pStoredName == NULL ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, "stored-name",
			"the stored dynamic field name output is null");
		return false;
	}
	pStoredName->Data = NULL;
	pStoredName->Size = 0u;
	if ( !__xrtDynamicFieldNameValid(Name, "stored-name") ) {
		return false;
	}
	pDict = __xrtDynamicFieldConstDict(pFields, "stored-name");
	return (pDict != NULL) &&
		xrtTypedDictStoredKey(pDict, Name, pStoredName);
}



/* 深复制来源并失败原子地设置字段。 */
static bool xrtDynamicFieldsSet(
	xrtdynamicfields* pFields,
	xstrview Name,
	const xvalue* pValue
)
{
	xtypeddict* pDict;

	if ( (pValue == NULL) ||
		 !__xrtDynamicFieldNameValid(Name, "set") ) {
		if ( pValue != NULL ) {
			return false;
		}
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, "set",
			"the dynamic field value is null");
		return false;
	}
	pDict = __xrtDynamicFieldDict(pFields, "set");
	return (pDict != NULL) && __xrtDynamicFieldSetClone(
		pDict,
		Name,
		pValue,
		"set",
		"the cloned dynamic field value could not be committed"
	);
}



/* 成功时把来源 Value 移交给字段表并清空调用方槽位。 */
static bool xrtDynamicFieldsSetTake(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue** ppValue
)
{
	xtypeddict* pDict;

	if ( (ppValue == NULL) || (*ppValue == NULL) ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, "set-take",
			"the dynamic field source slot is null or empty");
		return false;
	}
	if ( !__xrtDynamicFieldNameValid(Name, "set-take") ) {
		return false;
	}
	pDict = __xrtDynamicFieldDict(pFields, "set-take");
	if ( pDict == NULL ) {
		return false;
	}
	if ( !__xrtDynamicFieldSetClone(
		pDict,
		Name,
		*ppValue,
		"set-take",
		"the isolated dynamic field value could not be committed"
	) ) {
		return false;
	}
	xrtValueRelease(*ppValue);
	*ppValue = NULL;
	return true;
}



/* 设置并无条件消费适合单行构造的临时 Value。 */
static bool xrtDynamicFieldsSetNew(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue* pValue
)
{
	bool bResult = xrtDynamicFieldsSetTake(pFields, Name, &pValue);

	xrtValueRelease(pValue);
	return bResult;
}



/* 移交一个已经保留的同一 Value，并把类型槽恢复为空值。 */
static bool __xrtDynamicFieldsSetRefOwned(
	xtypeddict* pDict,
	xstrview Name,
	xvalue** ppValue,
	cstr sOperation
)
{
	xvalue* pMoved;

	if ( !xrtTypedDictSetTake(pDict, Name, ppValue) ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
			"the shared dynamic field value could not be committed");
		return false;
	}
	pMoved = *ppValue;
	*ppValue = NULL;
	xrtValueRelease(pMoved);
	return true;
}



/* 保留同一 Value 身份并失败原子地设置字段。 */
static bool xrtDynamicFieldsSetRef(
	xrtdynamicfields* pFields,
	xstrview Name,
	const xvalue* pValue
)
{
	xtypeddict* pDict;
	xvalue* pReference;

	if ( (pValue == NULL) ||
		 !__xrtDynamicFieldNameValid(Name, "set-ref") ) {
		if ( pValue == NULL ) {
			__xrtDynamicFieldError(XERR_ARGUMENT,
				XDYNAMIC_FIELD_ERROR_ARGUMENT, "set-ref",
				"the shared dynamic field value is null");
		}
		return false;
	}
	pDict = __xrtDynamicFieldDict(pFields, "set-ref");
	if ( pDict == NULL ) {
		return false;
	}
	pReference = xrtValueRetain(pValue);
	if ( pReference == NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, "set-ref",
			"the shared dynamic field value could not be retained");
		return false;
	}
	if ( !__xrtDynamicFieldsSetRefOwned(
		pDict, Name, &pReference, "set-ref"
	) ) {
		xrtValueRelease(pReference);
		return false;
	}
	return true;
}



/* 成功时把来源 Value 的同一身份移交给字段表。 */
static bool xrtDynamicFieldsSetRefTake(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue** ppValue
)
{
	xtypeddict* pDict;

	if ( (ppValue == NULL) || (*ppValue == NULL) ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, "set-ref-take",
			"the shared dynamic field source slot is null or empty");
		return false;
	}
	if ( !__xrtDynamicFieldNameValid(Name, "set-ref-take") ) {
		return false;
	}
	pDict = __xrtDynamicFieldDict(pFields, "set-ref-take");
	return (pDict != NULL) && __xrtDynamicFieldsSetRefOwned(
		pDict, Name, ppValue, "set-ref-take"
	);
}



/* 无论成功失败都消费共享 Value 临时值。 */
static bool xrtDynamicFieldsSetRefNew(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue* pValue
)
{
	bool bResult = xrtDynamicFieldsSetRefTake(pFields, Name, &pValue);

	xrtValueRelease(pValue);
	return bResult;
}



/* 删除并释放指定字段。 */
static bool xrtDynamicFieldsRemove(
	xrtdynamicfields* pFields,
	xstrview Name
)
{
	xtypeddict* pDict = __xrtDynamicFieldDict(pFields, "remove");

	return (pDict != NULL) &&
		__xrtDynamicFieldNameValid(Name, "remove") &&
		xrtTypedDictRemove(pDict, Name);
}



/* 把指定字段值移交给调用方，字段缺失时返回空指针。 */
static xvalue* xrtDynamicFieldsTake(
	xrtdynamicfields* pFields,
	xstrview Name
)
{
	xtypeddict* pDict = __xrtDynamicFieldDict(pFields, "take");
	xvalue* pValue = xrtValueNull();

	if ( (pDict == NULL) ||
		 !__xrtDynamicFieldNameValid(Name, "take") ||
		 !xrtTypedDictTake(pDict, Name, &pValue) ) {
		xrtValueRelease(pValue);
		return NULL;
	}
	return pValue;
}



/* 启动动态字段外置迭代并保留字段对象。 */
static bool __xrtDynamicFieldsIterStart(
	xrtdynamicfields* pFields,
	xrtdynamicfielditer* pIterator,
	bool bReverse,
	cstr sOperation
)
{
	xtypeddict* pDict;

	if ( pIterator == NULL ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, sOperation,
			"the dynamic field iterator is null");
		return false;
	}
	memset(pIterator, 0, sizeof(*pIterator));
	pDict = __xrtDynamicFieldDict(pFields, sOperation);
	if ( (pDict == NULL) || (xrtObjectRef(pFields) == NULL) ) {
		return false;
	}
	if ( !(bReverse ?
		xrtTypedDictIterRBegin(pDict, &pIterator->Base) :
		xrtTypedDictIterBegin(pDict, &pIterator->Base)) ) {
		xrtObjectUnref(pFields);
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, sOperation,
			"the dynamic field iteration could not start");
		return false;
	}
	pIterator->Fields = pFields;
	return true;
}



/* 启动按插入顺序的动态字段迭代。 */
static bool xrtDynamicFieldsIterBegin(
	xrtdynamicfields* pFields,
	xrtdynamicfielditer* pIterator
)
{
	return __xrtDynamicFieldsIterStart(
		pFields, pIterator, false, "iter-begin"
	);
}



/* 启动按插入顺序逆序的动态字段迭代。 */
static bool xrtDynamicFieldsIterRBegin(
	xrtdynamicfields* pFields,
	xrtdynamicfielditer* pIterator
)
{
	return __xrtDynamicFieldsIterStart(
		pFields, pIterator, true, "iter-rbegin"
	);
}



/* 返回下一字段的借用名称和值。 */
static const xvalue* xrtDynamicFieldsIterNext(
	xrtdynamicfielditer* pIterator,
	xstrview* pName
)
{
	xvalue** ppValue;

	if ( (pIterator == NULL) || (pIterator->Fields == NULL) ) {
		__xrtDynamicFieldError(
			pIterator == NULL ? XERR_ARGUMENT : XERR_STATE,
			pIterator == NULL ? XDYNAMIC_FIELD_ERROR_ARGUMENT :
				XDYNAMIC_FIELD_ERROR_STATE,
			"iter-next",
			"the dynamic field iterator is not active");
		return NULL;
	}
	if (__xrtDynamicFieldDict(pIterator->Fields, "iter-next") == NULL) return NULL;
	ppValue = (xvalue**)xrtTypedDictIterNext(
		&pIterator->Base, pName
	);
	return ppValue != NULL ? *ppValue : NULL;
}



/* 结束迭代并释放字段对象保留。 */
XRT_API void xrtDynamicFieldsIterEnd(xrtdynamicfielditer* pIterator)
{
	xrtdynamicfields* pFields;

	if ( pIterator == NULL ) {
		return;
	}
	pFields = pIterator->Fields;
	xrtTypedDictIterEnd(&pIterator->Base);
	pIterator->Fields = NULL;
	xrtObjectUnref(pFields);
}



/* 同步访问使用类型字典既有门禁保护借用键和值；普通迭代器不锁住它们。
 * 独立字段对象强引用覆盖整个迭代与门禁退出，不保存回调或 context。 */
static bool xrtDynamicFieldsVisitV1(xrtdynamicfields* pFields,
	xdynamicfieldvisitv1 Visit, ptr UserData)
{
	xrtdynamicfielditer Iterator;
	xtypeddict* pDict;
	const xvalue* pValue;
	xstrview Name;
	bool bResult = false;
	bool bIterating = false;
	xerror* pPrevious = xrtTakeError();
	xerror* pFailure;
	xerror* pCleanup;

	if (Visit == NULL) {
		__xrtDynamicFieldError(XERR_ARGUMENT, XDYNAMIC_FIELD_ERROR_ARGUMENT,
			"visit", "the dynamic field visitor is null");
		goto done;
	}
	pDict = __xrtDynamicFieldDict(pFields, "visit");
	if (pDict == NULL || !xrtDynamicFieldsIterBegin(pFields, &Iterator)) goto done;
	bIterating = true;
	bResult = true;
	while ((pValue = xrtDynamicFieldsIterNext(&Iterator, &Name)) != NULL) {
		if (!__xrtTypedDictCallbackBegin(pDict)) { bResult = false; break; }
		bResult = Visit(Name, pValue, UserData);
		__xrtTypedDictCallbackEnd(pDict);
		if (xrtGetError() != NULL) bResult = false;
		if (!bResult) {
			if (xrtGetError() == NULL)
				__xrtDynamicFieldError(XERR_STATE, XDYNAMIC_FIELD_ERROR_STATE,
					"visit", "the dynamic field visitor failed without an error");
			break;
		}
	}
	if (xrtGetError() != NULL) bResult = false;
done:
	/* Owner retirement can run native drops; never replace the first failure.
     * Physical false remains authoritative even if no error can allocate. */
	pFailure = xrtTakeError();
	if (bIterating) xrtDynamicFieldsIterEnd(&Iterator);
	pCleanup = xrtTakeError();
	if (pFailure != NULL) {
		xrtErrorFree(pCleanup); xrtErrorFree(pPrevious); xrtSetErrorTake(pFailure);
		return false;
	}
	if (!bResult || pCleanup != NULL) {
		xrtErrorFree(pPrevious); xrtSetErrorTake(pCleanup); return false;
	}
	xrtSetErrorTake(pPrevious); return true;
}

/* 返回递增且跳过外置迭代器保留零值的结构版本。 */
static uint64 __xrtDynamicFieldNextVersion(uint64 iVersion)
{
	iVersion++;
	return iVersion != 0u ? iVersion : 1u;
}



/* 按插入顺序把来源字段快照或深复制到独立工作字典。 */
static bool __xrtDynamicFieldMergeEntries(
	xtypeddict* pTarget,
	const xtypeddict* pSource,
	bool bReplace,
	bool bDeepClone
)
{
	xtypeddictiter Iterator = { 0 };
	xstrview Name;
	xvalue** ppValue;

	if ( !xrtTypedDictIterBegin(
		(xtypeddict*)pSource, &Iterator
	) ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, "merge",
			"the dynamic field source iteration could not start");
		return false;
	}
	while ( (ppValue = (xvalue**)xrtTypedDictIterNext(
		&Iterator, &Name
	)) != NULL ) {
		bool bStored;

		if ( !bReplace && xrtTypedDictHas(pTarget, Name) ) {
			continue;
		}
		if ( bDeepClone ) {
			bStored = __xrtDynamicFieldSetClone(
				pTarget,
				Name,
				*ppValue,
				"merge",
				"a cloned dynamic field value could not be committed"
			);
		} else {
			/* Snapshot actual owning edges, not independently copied Value
			 * shells. Set(Value) invokes the element Copy policy and can split
			 * shared container backing when one result is later edited. */
			xvalue* pOwned = xrtValueRetain(*ppValue);
			bStored = pOwned != NULL && xrtTypedDictSetTake(pTarget, Name, &pOwned);
			xrtValueRelease(pOwned);
		}
		if ( !bStored ) {
			if ( !bDeepClone ) {
				__xrtDynamicFieldWrap(XERR_STATE,
					XDYNAMIC_FIELD_ERROR_OPERATION, "merge",
					"an existing dynamic field value could not be snapshotted");
			}
			xrtTypedDictIterEnd(&Iterator);
			return false;
		}
	}
	if ( xrtGetError() != NULL ) {
		xrtTypedDictIterEnd(&Iterator);
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, "merge",
			"the dynamic field source iteration failed");
		return false;
	}
	xrtTypedDictIterEnd(&Iterator);
	return true;
}



/* 事务合并两个动态字段对象。 */
static bool __xrtDynamicFieldMerge(
	xrtdynamicfields* pTarget,
	const xrtdynamicfields* pSource,
	bool bReplace,
	bool bDeepClone
)
{
	xtypeddict* pTargetDict = __xrtDynamicFieldDict(pTarget, "merge");
	const xtypeddict* pSourceDict = __xrtDynamicFieldConstDict(
		pSource, "merge"
	);
	xtypeddict Work;
	xmap Previous;
	xerror* pPreviousError;
	xerror* pDiscard;
	xerror* pPrimary;
	size_t iCapacity;
	bool bReady = false;
	bool bSuccess = false;

	if ( (pTargetDict == NULL) || (pSourceDict == NULL) ) {
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( xrtTypedDictCount(pSourceDict) == 0u ) {
		return true;
	}
	if ( xrtTypedDictCount(pTargetDict) >
		 (SIZE_MAX - xrtTypedDictCount(pSourceDict)) ) {
		__xrtDynamicFieldError(XERR_RANGE,
			XDYNAMIC_FIELD_ERROR_OPERATION, "merge",
			"the merged dynamic field count overflows");
		return false;
	}
	iCapacity = xrtTypedDictCount(pTargetDict) +
		xrtTypedDictCount(pSourceDict);
	pPreviousError = __xrtErrorSwapOwned(NULL);
	memset(&Work, 0, sizeof(Work));
	if ( !xrtTypedDictInit(&Work, xrtTypeValue()) ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, "merge",
			"the dynamic field work dictionary could not be initialized");
		goto cleanup;
	}
	bReady = true;
	if ( !xrtTypedDictReserve(&Work, iCapacity) ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, "merge",
			"the dynamic field work dictionary could not be reserved");
		goto cleanup;
	}
	if ( !__xrtDynamicFieldMergeEntries(
		&Work, pTargetDict, true, false
	) || !__xrtDynamicFieldMergeEntries(
		&Work, pSourceDict, bReplace, bDeepClone
	) ) {
		goto cleanup;
	}
	Previous = pTargetDict->Storage;
	pTargetDict->Storage = Work.Storage;
	Work.Storage = Previous;
	pTargetDict->Storage.Version = __xrtDynamicFieldNextVersion(
		Previous.Version
	);
	bSuccess = true;

cleanup:
	pPrimary = xrtTakeError();
	if ( bReady ) {
		xrtTypedDictUnit(&Work);
	}
	pDiscard = xrtTakeError();
	if ( pPrimary == NULL ) { pPrimary = pDiscard; pDiscard = NULL; }
	xrtErrorFree(pDiscard);
	if ( bSuccess && pPrimary == NULL ) {
		xrtSetErrorTake(pPreviousError);
	} else {
		xrtErrorFree(pPreviousError);
		xrtSetErrorTake(pPrimary);
		if ( xrtGetError() == NULL ) {
			__xrtDynamicFieldError(XERR_STATE,
				XDYNAMIC_FIELD_ERROR_OPERATION, "merge",
				"the dynamic fields could not be merged");
		}
	}
	return bSuccess;
}

static bool xrtDynamicFieldsMerge(xrtdynamicfields* pTarget,
	const xrtdynamicfields* pSource, bool bReplace)
{ return __xrtDynamicFieldMerge(pTarget, pSource, bReplace, true); }

static bool xrtDynamicFieldsMergeRefV1(xrtdynamicfields* pTarget,
	const xrtdynamicfields* pSource, bool bReplace)
{ return __xrtDynamicFieldMerge(pTarget, pSource, bReplace, false); }


/* 深复制一个动态字段对象和它拥有的 Value 图。 */
static xrtdynamicfields* xrtDynamicFieldsClone(
	const xrtdynamicfields* pFields
)
{
	xrtdynamicfields* pClone;

	if ( __xrtDynamicFieldConstDict(pFields, "clone") == NULL ) {
		return NULL;
	}
	pClone = xrtDynamicFieldsCreate();
	if ( pClone == NULL ) {
		return NULL;
	}
	if ( !xrtDynamicFieldsMerge(pClone, pFields, true) ) {
		__xrtDynamicFieldUnrefPreserveError(pClone);
		return NULL;
	}
	return pClone;
}



typedef enum xdynamicfieldcollectkind {
	XDYNAMIC_FIELD_COLLECT_KEYS,
	XDYNAMIC_FIELD_COLLECT_VALUES,
	XDYNAMIC_FIELD_COLLECT_ITEMS,
	XDYNAMIC_FIELD_COLLECT_OBJECT
} xdynamicfieldcollectkind;



/* 构造一个字段名 Value。 */
static xvalue* __xrtDynamicFieldNameValue(xstrview Name)
{
	return xrtValueString(Name);
}



/* 构造一个独立字段值。 */
static xvalue* __xrtDynamicFieldValueCopy(const xvalue* pValue)
{
	return xrtValueDeepClone(pValue);
}



/* 构造一个拥有独立名称和值的 [name, value] 二元项。 */
static xvalue* __xrtDynamicFieldPair(
	xstrview Name,
	const xvalue* pValue
)
{
	xvalue* pPair = xrtValueArray();
	xvalue* pName;
	xvalue* pCopy;

	if ( (pPair == NULL) || !xrtValueReserve(pPair, 2u) ) {
		xrtValueRelease(pPair);
		return NULL;
	}
	pName = __xrtDynamicFieldNameValue(Name);
	if ( (pName == NULL) ||
		 !xrtValueArrayAppendNew(pPair, pName) ) {
		xrtValueRelease(pPair);
		return NULL;
	}
	pCopy = __xrtDynamicFieldValueCopy(pValue);
	if ( (pCopy == NULL) ||
		 !xrtValueArrayAppendNew(pPair, pCopy) ) {
		xrtValueRelease(pPair);
		return NULL;
	}
	return pPair;
}



/* 构造语言绑定常用的名称、值或二元项数组。 */
static xvalue* __xrtDynamicFieldsCollect(
	const xrtdynamicfields* pFields,
	xdynamicfieldcollectkind Kind,
	cstr sOperation
)
{
	const xtypeddict* pDict;
	xrtdynamicfielditer Iterator = { 0 };
	xvalue* pResult = NULL;
	const xvalue* pValue;
	xstrview Name;
	size_t iCount;
	xerror* pPrevious;
	xerror* pFailure;
	xerror* pDiscard;
	bool bIterating = false;

	pDict = __xrtDynamicFieldConstDict(pFields, sOperation);
	if ( pDict == NULL ) {
		return NULL;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	iCount = xrtTypedDictCount(pDict);
	pResult = Kind == XDYNAMIC_FIELD_COLLECT_OBJECT ?
		xrtValueObject() : xrtValueArray();
	if ( pResult == NULL ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
			"the dynamic field result could not be created");
		goto failure;
	}
	if ( !xrtValueReserve(pResult, iCount) ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
			"the dynamic field result could not be reserved");
		goto failure;
	}
	if ( !xrtDynamicFieldsIterBegin(
		(xrtdynamicfields*)pFields, &Iterator
	) ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, sOperation,
			"the dynamic field result iteration could not start");
		goto failure;
	}
	bIterating = true;
	while ( (pValue = xrtDynamicFieldsIterNext(
		&Iterator, &Name
	)) != NULL ) {
		xvalue* pItem;

		if ( Kind == XDYNAMIC_FIELD_COLLECT_KEYS ) {
			pItem = __xrtDynamicFieldNameValue(Name);
		} else if ( Kind == XDYNAMIC_FIELD_COLLECT_VALUES ) {
			pItem = __xrtDynamicFieldValueCopy(pValue);
		} else if ( Kind == XDYNAMIC_FIELD_COLLECT_ITEMS ) {
			pItem = __xrtDynamicFieldPair(Name, pValue);
		} else {
			pItem = __xrtDynamicFieldValueCopy(pValue);
		}
		if ( (pItem == NULL) || !(Kind == XDYNAMIC_FIELD_COLLECT_OBJECT ?
			xrtValueObjectSetNew(pResult, Name, pItem) :
			xrtValueArrayAppendNew(pResult, pItem)) ) {
			__xrtDynamicFieldWrap(XERR_MEMORY,
				XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
				"a dynamic field result item could not be created");
			goto failure;
		}
	}
	if ( xrtGetError() != NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, sOperation,
			"the dynamic field result iteration failed");
		goto failure;
	}
	xrtDynamicFieldsIterEnd(&Iterator);
	pDiscard = __xrtErrorSwapOwned(pPrevious);
	xrtErrorFree(pDiscard);
	return pResult;

failure:
	pFailure = xrtTakeError();
	if ( bIterating ) {
		xrtDynamicFieldsIterEnd(&Iterator);
	}
	xrtValueRelease(pResult);
	if ( pFailure != NULL ) {
		xrtSetError(pFailure);
		xrtErrorFree(pFailure);
	}
	xrtErrorFree(pPrevious);
	if ( xrtGetError() == NULL ) {
		__xrtDynamicFieldError(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
			"the dynamic field result could not be completed");
	}
	return NULL;
}



/* 返回按插入顺序排列的字段名数组。 */
static xvalue* xrtDynamicFieldsKeys(const xrtdynamicfields* pFields)
{
	return __xrtDynamicFieldsCollect(
		pFields, XDYNAMIC_FIELD_COLLECT_KEYS, "keys"
	);
}



/* 返回按插入顺序排列的独立字段值数组。 */
static xvalue* xrtDynamicFieldsValues(const xrtdynamicfields* pFields)
{
	return __xrtDynamicFieldsCollect(
		pFields, XDYNAMIC_FIELD_COLLECT_VALUES, "values"
	);
}



/* 返回按插入顺序排列的 [name, value] 二元项数组。 */
static xvalue* xrtDynamicFieldsItems(const xrtdynamicfields* pFields)
{
	return __xrtDynamicFieldsCollect(
		pFields, XDYNAMIC_FIELD_COLLECT_ITEMS, "items"
	);
}



/* 返回按插入顺序保存字段的独立 Value Object。 */
static xvalue* xrtDynamicFieldsToValue(const xrtdynamicfields* pFields)
{
	return __xrtDynamicFieldsCollect(
		pFields, XDYNAMIC_FIELD_COLLECT_OBJECT, "to-value"
	);
}



/* 从 Value Object 深复制名称和值并创建动态字段对象。 */
static xrtdynamicfields* xrtDynamicFieldsFromValue(const xvalue* pValue)
{
	xrtdynamicfields* pFields;
	size_t iCount;
	size_t i;

	if ( xrtValueType(pValue) != XVALUE_OBJECT ) {
		__xrtDynamicFieldError(XERR_TYPE,
			XDYNAMIC_FIELD_ERROR_TYPE, "from-value",
			"the dynamic field source is not a Value Object");
		return NULL;
	}
	pFields = xrtDynamicFieldsCreate();
	if ( pFields == NULL ) {
		return NULL;
	}
	iCount = xrtValueCount(pValue);
	if ( !xrtDynamicFieldsReserve(pFields, iCount) ) {
		__xrtDynamicFieldUnrefPreserveError(pFields);
		return NULL;
	}
	for ( i = 0u; i < iCount; ++i ) {
		xstrview Name;
		xvalue* pItem = xrtValueObjectAt(pValue, i, &Name);

		if ( (pItem == NULL) ||
			 !xrtDynamicFieldsSet(pFields, Name, pItem) ) {
			__xrtDynamicFieldUnrefPreserveError(pFields);
			return NULL;
		}
	}
	return pFields;
}

#undef xrtDynamicFieldsCreate
#undef xrtDynamicFieldsRef
#undef xrtDynamicFieldsCount
#undef xrtDynamicFieldsCapacity
#undef xrtDynamicFieldsClear
#undef xrtDynamicFieldsReserve
#undef xrtDynamicFieldsTrim
#undef xrtDynamicFieldsHas
#undef xrtDynamicFieldsGet
#undef xrtDynamicFieldsGetRef
#undef xrtDynamicFieldsCopy
#undef xrtDynamicFieldsStoredName
#undef xrtDynamicFieldsSet
#undef xrtDynamicFieldsSetTake
#undef xrtDynamicFieldsSetNew
#undef xrtDynamicFieldsSetRef
#undef xrtDynamicFieldsSetRefTake
#undef xrtDynamicFieldsSetRefNew
#undef xrtDynamicFieldsRemove
#undef xrtDynamicFieldsTake
#undef xrtDynamicFieldsIterBegin
#undef xrtDynamicFieldsIterRBegin
#undef xrtDynamicFieldsIterNext
#undef xrtDynamicFieldsVisitV1
#undef xrtDynamicFieldsMerge
#undef xrtDynamicFieldsMergeRefV1
#undef xrtDynamicFieldsClone
#undef xrtDynamicFieldsKeys
#undef xrtDynamicFieldsValues
#undef xrtDynamicFieldsItems
#undef xrtDynamicFieldsToValue
#undef xrtDynamicFieldsFromValue

#define XRUNTIME_DYNAMIC_WRAP(Result, Name, Parameters, Arguments) \
XRT_API Result xrtDynamicFields##Name Parameters { \
	xrtownershipscope Mutation = {0}; \
	if (!xrtOwnershipMutationBegin(&Mutation)) abort(); \
	Result ResultValue = __xrtDynamicBody##Name Arguments; \
	if (!xrtOwnershipScopeEnd(&Mutation)) abort(); \
	return ResultValue; \
}
XRUNTIME_DYNAMIC_WRAP(xrtdynamicfields*, Create, (void), ())
XRUNTIME_DYNAMIC_WRAP(xrtdynamicfields*, Ref, (xrtdynamicfields* f), (f))
XRUNTIME_DYNAMIC_WRAP(size_t, Count, (const xrtdynamicfields* f), (f))
XRUNTIME_DYNAMIC_WRAP(size_t, Capacity, (const xrtdynamicfields* f), (f))
XRUNTIME_DYNAMIC_WRAP(bool, Clear, (xrtdynamicfields* f), (f))
XRUNTIME_DYNAMIC_WRAP(bool, Reserve, (xrtdynamicfields* f, size_t n), (f,n))
XRUNTIME_DYNAMIC_WRAP(bool, Trim, (xrtdynamicfields* f), (f))
XRUNTIME_DYNAMIC_WRAP(bool, Has, (const xrtdynamicfields* f, xstrview n), (f,n))
XRUNTIME_DYNAMIC_WRAP(const xvalue*, Get, (const xrtdynamicfields* f, xstrview n), (f,n))
XRUNTIME_DYNAMIC_WRAP(xvalue*, GetRef, (const xrtdynamicfields* f, xstrview n), (f,n))
XRUNTIME_DYNAMIC_WRAP(xvalue*, Copy, (const xrtdynamicfields* f, xstrview n), (f,n))
XRUNTIME_DYNAMIC_WRAP(bool, StoredName, (const xrtdynamicfields* f, xstrview n, xstrview* s), (f,n,s))
XRUNTIME_DYNAMIC_WRAP(bool, Set, (xrtdynamicfields* f, xstrview n, const xvalue* v), (f,n,v))
XRUNTIME_DYNAMIC_WRAP(bool, SetTake, (xrtdynamicfields* f, xstrview n, xvalue** v), (f,n,v))
XRUNTIME_DYNAMIC_WRAP(bool, SetNew, (xrtdynamicfields* f, xstrview n, xvalue* v), (f,n,v))
XRUNTIME_DYNAMIC_WRAP(bool, SetRef, (xrtdynamicfields* f, xstrview n, const xvalue* v), (f,n,v))
XRUNTIME_DYNAMIC_WRAP(bool, SetRefTake, (xrtdynamicfields* f, xstrview n, xvalue** v), (f,n,v))
XRUNTIME_DYNAMIC_WRAP(bool, SetRefNew, (xrtdynamicfields* f, xstrview n, xvalue* v), (f,n,v))
XRUNTIME_DYNAMIC_WRAP(bool, Remove, (xrtdynamicfields* f, xstrview n), (f,n))
XRUNTIME_DYNAMIC_WRAP(xvalue*, Take, (xrtdynamicfields* f, xstrview n), (f,n))
XRUNTIME_DYNAMIC_WRAP(bool, IterBegin, (xrtdynamicfields* f, xrtdynamicfielditer* i), (f,i))
XRUNTIME_DYNAMIC_WRAP(bool, IterRBegin, (xrtdynamicfields* f, xrtdynamicfielditer* i), (f,i))
XRUNTIME_DYNAMIC_WRAP(const xvalue*, IterNext, (xrtdynamicfielditer* i, xstrview* n), (i,n))
XRUNTIME_DYNAMIC_WRAP(bool, VisitV1, (xrtdynamicfields* f, xdynamicfieldvisitv1 v, ptr d), (f,v,d))
XRUNTIME_DYNAMIC_WRAP(bool, Merge, (xrtdynamicfields* f, const xrtdynamicfields* s, bool r), (f,s,r))
XRUNTIME_DYNAMIC_WRAP(bool, MergeRefV1, (xrtdynamicfields* f, const xrtdynamicfields* s, bool r), (f,s,r))
XRUNTIME_DYNAMIC_WRAP(xrtdynamicfields*, Clone, (const xrtdynamicfields* f), (f))
XRUNTIME_DYNAMIC_WRAP(xvalue*, Keys, (const xrtdynamicfields* f), (f))
XRUNTIME_DYNAMIC_WRAP(xvalue*, Values, (const xrtdynamicfields* f), (f))
XRUNTIME_DYNAMIC_WRAP(xvalue*, Items, (const xrtdynamicfields* f), (f))
XRUNTIME_DYNAMIC_WRAP(xvalue*, ToValue, (const xrtdynamicfields* f), (f))
XRUNTIME_DYNAMIC_WRAP(xrtdynamicfields*, FromValue, (const xvalue* v), (v))
#undef XRUNTIME_DYNAMIC_WRAP

static bool __xrtDynamicFieldOwnershipValid(const xrtobject* pObject)
{
	if (pObject == NULL || pObject->Type != &__xrtDynamicFieldTypeDescriptor ||
		pObject->OwnershipCleared || xrtObjectRefCount(pObject) == 0u ||
		__xrtAtomicRefLoad(&pObject->State) != XRT_OBJECT_STATE_ACTIVE || pObject->Graph != NULL) return false;
	const xrtdynamicfieldpayload* pPayload = xrtObjectConstData(pObject);
	return __xrtTypedDictOwnershipStorage(&pPayload->Live, &__xrtTypeValueDescriptor);
}

static bool __xrtDynamicFieldOwnershipCount(const void* pData, size_t* pCount)
{
	if (!__xrtDynamicFieldOwnershipValid(pData)) return false;
	*pCount = xrtObjectRefCount(pData);
	return true;
}

static bool __xrtDynamicFieldOwnershipTrace(const void* pData,
	xrtownershipvisitor pVisit, ptr pContext)
{
	if (pVisit == NULL || !__xrtDynamicFieldOwnershipValid(pData)) return false;
	const xrtdynamicfieldpayload* pPayload = xrtObjectConstData(pData);
	xmapiter Iterator = {0};
	xvalue** ppValue;
	size_t iVisited = 0u;
	/* Native slot traversal: no retain, allocation, binding, class callback,
	 * deep copy or Value-backing flattening. Repeated slots stay repeated. */
	if (!xrtMapIterBegin((xmap*)&pPayload->Live.Storage, &Iterator)) return false;
	while ((ppValue = xrtMapIterNext(&Iterator, NULL)) != NULL) {
		iVisited++;
		if (*ppValue == NULL || !pVisit(xrtValueOwnership(*ppValue), pContext)) {
			xrtMapIterEnd(&Iterator); return false;
		}
	}
	xrtMapIterEnd(&Iterator);
	return iVisited == pPayload->Live.Storage.Count;
}

static const xrtownershipops __xrtDynamicFieldOwnershipOps = {
	__xrtDynamicFieldOwnershipCount, __xrtDynamicFieldOwnershipTrace
};

XRT_API xrtownershipref xrtDynamicFieldsOwnership(const xrtdynamicfields* pFields)
{
	return (xrtownershipref){pFields, &__xrtDynamicFieldOwnershipOps};
}

static bool __xrtDynamicFieldHold(const void* pData)
{ return xrtObjectRef((xrtobject*)pData) != NULL; }
static void __xrtDynamicFieldDrop(const void* pData)
{ xrtObjectUnref((xrtobject*)pData); }
static bool __xrtDynamicFieldClaim(const void* pData, const void* pToken)
{
	xrtobject* pObject = (xrtobject*)pData;
	if (pToken == NULL || !__xrtDynamicFieldOwnershipValid(pObject) ||
		(pObject->OwnershipClaim != NULL && pObject->OwnershipClaim != pToken)) return false;
	pObject->OwnershipClaim = pToken; return true;
}
static void __xrtDynamicFieldRestore(const void* pData, const void* pToken)
{
	xrtobject* pObject = (xrtobject*)pData;
	if (pToken == NULL || pObject->OwnershipClaim != pToken || pObject->OwnershipCleared) abort();
	pObject->OwnershipClaim = NULL;
}
static void __xrtDynamicFieldClear(const void* pData, const void* pToken)
{
	xrtobject* pObject = (xrtobject*)pData;
	if (pToken == NULL || pObject->OwnershipClaim != pToken || pObject->OwnershipCleared) abort();
	/* Keep the existing physical owning slots until Finish. All API/native
	 * admission rejects this receiver; no post-commit graph snapshot can read
	 * them as live edges. Every actual target remains held by the collector. */
	pObject->OwnershipCleared = true;
}
static bool __xrtDynamicFieldFinish(const void* pData, const void* pToken)
{
	xrtobject* pObject = (xrtobject*)pData;
	xrtdynamicfieldpayload* pPayload = xrtObjectData(pObject);
	xrtownershipscope Mutation = {0};
	xtypeddict Retired;
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (pToken == NULL || pObject->OwnershipClaim != pToken || !pObject->OwnershipCleared) abort();
	Retired = pPayload->Live;
	memset(&pPayload->Live, 0, sizeof(pPayload->Live));
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (Retired.ItemType == NULL) return true;
	/* Detach before callback: failure/retry never repeats a physical drop.
	 * Preserve an existing primary diagnostic instead of hiding it in cleanup. */
	xerror* pPrior = xrtTakeError();
	xrtTypedDictUnit(&Retired);
	xerror* pFailure = xrtTakeError();
	bool bResult = pFailure == NULL;
	if (pPrior != NULL) { xrtErrorFree(pFailure); xrtSetErrorTake(pPrior); }
	else xrtSetErrorTake(pFailure);
	return bResult;
}

XRT_API const xrtownershipadapterv1* xrtDynamicFieldsOwnershipAdapterV1(xrtownershipref Reference)
{
	static const xrtownershipadapterv1 Adapter = {
		sizeof(Adapter), __xrtDynamicFieldHold, __xrtDynamicFieldDrop,
		__xrtDynamicFieldClaim, __xrtDynamicFieldRestore, NULL,
		__xrtDynamicFieldClear, __xrtDynamicFieldFinish
	};
	/* Exact resident identity BEFORE any native payload access. An arbitrary
	 * runtime Object/bound Trace, forged TypeId, or borrowed dict is not enough. */
	if (Reference.Ops != &__xrtDynamicFieldOwnershipOps || Reference.Data == NULL ||
		!__xrtDynamicFieldOwnershipValid(Reference.Data)) return NULL;
	return &Adapter;
}

#endif
#endif

#endif
