#ifndef XL_RUNTIME_H
#define XL_RUNTIME_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xruntime_decl.h"
#include "xl_extlib_abi.h"

/* xlang 的脚本级错误文本入口。XRT 的 xrtGetError 返回结构化 xerror，
   不能被旧扩展库误声明为 const char*。 */
static inline const char* xl6_xrt_last_error_text(void)
{
    const xerror* pError = xrtGetError();
    return pError != NULL ? xrtErrorMessage(pError) : "";
}

/*
	类型化容器的所有权模式属于基础运行时，不依赖 async/task。
	普通 array<Class> 在模块/全局初始化时同样会生成这两个标识。
*/

/* xlang 只在私有适配层保存 XRT 日期时间布局，避免向语言 ABI 暴露实现结构。 */
typedef xdatetime xl6_xrt_datetime;

/* xlang 的 Random 句柄私有保存当前 XRT xrng 的稳定布局。 */
typedef xrng xl6_xrt_rng;

#ifndef XL6_WEAK_TYPEDEF
#define XL6_WEAK_TYPEDEF
typedef struct xl6_weak {
    ptr Control;
} xl6_weak;
#endif

#ifndef XL6_XRT_CALL_TYPEDEF
#define XL6_XRT_CALL_TYPEDEF
typedef struct xl6_type_ops xl6_type_ops;
typedef struct xl6_type_desc xl6_type_desc;
typedef struct xl6_generic_type xl6_generic_type;
typedef struct xl6_method_desc xl6_method_desc;
typedef struct xl6_method_table xl6_method_table;
typedef struct xl6_func_param xl6_func_param;
typedef struct xl6_func_sig xl6_func_sig;
typedef struct xl6_enum_field xl6_enum_field;
typedef struct xl6_enum_variant xl6_enum_variant;
typedef struct xl6_enum_info xl6_enum_info;
typedef struct xl6_protocol_requirement xl6_protocol_requirement;
typedef struct xl6_protocol_entry xl6_protocol_entry;
typedef struct xl6_protocol xl6_protocol;
typedef struct xl6_protocol_witness xl6_protocol_witness;
typedef struct xl6_call_frame xl6_call_frame;
typedef struct xl6_call_result xl6_call_result;
typedef bool (*xl6_xcall_proc)(xl6_call_frame* pFrame, xl6_call_result* pResult);

#define XL6_XRT_TYPE_KIND_INVALID 0u
#define XL6_XRT_TYPE_KIND_NULL 1u
#define XL6_XRT_TYPE_KIND_BOOL 2u
#define XL6_XRT_TYPE_KIND_INT 3u
#define XL6_XRT_TYPE_KIND_FLOAT 4u
#define XL6_XRT_TYPE_KIND_STRING 5u
#define XL6_XRT_TYPE_KIND_TIME 6u
#define XL6_XRT_TYPE_KIND_POINTER 7u
#define XL6_XRT_TYPE_KIND_FUNCTION 8u
#define XL6_XRT_TYPE_KIND_ARRAY 9u
#define XL6_XRT_TYPE_KIND_LIST 10u
#define XL6_XRT_TYPE_KIND_SET 11u
#define XL6_XRT_TYPE_KIND_DICT 12u
#define XL6_XRT_TYPE_KIND_RECORD 13u
#define XL6_XRT_TYPE_KIND_HANDLE 14u
#define XL6_XRT_TYPE_KIND_TYPE 15u
#define XL6_XRT_TYPE_KIND_FUTURE 16u
#define XL6_XRT_TYPE_KIND_CLASS 17u
#define XL6_XRT_TYPE_KIND_PROTOCOL 18u
#define XL6_XRT_TYPE_KIND_ENUM 19u
#define XL6_XRT_TYPE_KIND_NULLABLE 20u
#define XL6_XRT_HANDLE_FLAG_OWNED 0x00000001u
#define XL6_XRT_HANDLE_FLAG_BORROWED 0x00000002u
#define XL6_XRT_HANDLE_FLAG_NULLABLE 0x00000004u
#define XL6_XRT_FUNC_FLAG_VARARGS 0x00000001u
#define XL6_XRT_FUNC_FLAG_KWARGS 0x00000002u
#define XL6_XRT_FUNC_PARAM_OPTIONAL 0x00000001u
#define XL6_ENUM_FLAG_NUMERIC 0x00000001u
#define XL6_ENUM_FLAG_TAGGED 0x00000002u
#define XL6_ENUM_VARIANT_HAS_PAYLOAD 0x00000001u
#ifndef XL6_XRT_FUNC_SIG_TYPEDEF
#define XL6_XRT_FUNC_SIG_TYPEDEF
#endif

struct xl6_type_ops {
    void (*init)(void* pObj);
    void (*copy)(void* pDst, const void* pSrc);
    void (*move)(void* pDst, void* pSrc);
    void (*drop)(void* pObj);
    int (*compare)(const void* pA, const void* pB);
    uint64 (*hash)(const void* pObj);
    str (*to_string)(const void* pObj, uint32* pSize);
    xvalue* (*box)(const void* pObj, const xl6_type_desc* pType);
    bool (*unbox)(xvalue* pVal, void* pOut, const xl6_type_desc* pType);
    void (*clone)(void* pDst, const void* pSrc);
};

struct xl6_type_desc {
    uint32 TypeId;
    uint32 Kind;
    const char* Name;
    uint32 NameSize;
    const char* AbiName;
    uint32 AbiNameSize;
    size_t Size;
    size_t Align;
    const xl6_type_ops* Ops;
    const xl6_method_table* Methods;
    const void* Extra;
    const xl6_type_desc* BaseType;
    xatomicptr RuntimeType;
};

struct xl6_generic_type {
    uint32 ArgCount;
    const xl6_type_desc* const* Args;
};

struct xl6_method_desc {
    const char* Name;
    uint32 NameSize;
    const xl6_func_sig* Sig;
    ptr NativeEntry;
    uint32 NativeAbi;
    xl6_xcall_proc XCallEntry;
};

struct xl6_method_table {
    uint32 Count;
    const xl6_method_desc* Methods;
};

struct xl6_func_param {
    const char* Name;
    uint32 NameSize;
    const xl6_type_desc* Type;
    xvalue* DefaultValue;
    uint32 Flags;
};

struct xl6_func_sig {
    uint64 TypeId;
    const char* Name;
    uint32 NameSize;
    uint32 ParamCount;
    const xl6_func_param* Params;
    uint32 ReturnCount;
    const xl6_type_desc* const* ReturnTypes;
    uint32 Flags;
};

struct xl6_enum_field {
    const char* Name;
    uint32 NameSize;
    const xl6_type_desc* Type;
    size_t Offset;
};

struct xl6_enum_variant {
    const char* Name;
    uint32 NameSize;
    uint64 Value;
    uint32 FieldCount;
    const xl6_enum_field* Fields;
    uint32 Flags;
};

struct xl6_enum_info {
    const xl6_type_desc* Type;
    const xl6_type_desc* BackingType;
    uint32 VariantCount;
    const xl6_enum_variant* Variants;
    uint32 Flags;
};

struct xl6_call_frame {
    xvalue* Self;
    ptr Env;
    uint32 Argc;
    xvalue** Argv;
    xvalue* Kwargs;
    xvalue* Varargs;
    const xl6_func_sig* Sig;
};

struct xl6_call_result {
    uint32 Count;
    xvalue* Values[XRT_CALL_RESULT_INLINE_COUNT];
    xvalue* Overflow;
};

struct xl6_protocol_requirement {
    const char* Name;
    uint32 NameSize;
    const xl6_func_sig* Signature;
};

struct xl6_protocol {
    const xl6_type_desc* Type;
    uint32 RequirementCount;
    const xl6_protocol_requirement* Requirements;
};

struct xl6_protocol_entry {
    const char* Name;
    uint32 NameSize;
    const xl6_func_sig* Signature;
    ptr Entry;
};

struct xl6_protocol_witness {
    const xl6_protocol* Protocol;
    const xl6_type_desc* ConcreteType;
    uint32 EntryCount;
    const xl6_protocol_entry* Entries;
};
#endif

#define XL6_XVO_DT_NULL 0
#define XL6_XVO_DT_BOOL 1
#define XL6_XVO_DT_INT 2
#define XL6_XVO_DT_FLOAT 3
#define XL6_XVO_DT_TEXT 4
#define XL6_XVO_DT_TIME 5
#define XL6_XVO_DT_POINT 6
#define XL6_XVO_DT_FUNC 7
#define XL6_XVO_DT_ARRAY 8
#define XL6_XVO_DT_LIST 9
#define XL6_XVO_DT_COLL 10
#define XL6_XVO_DT_TABLE 11
#define XL6_XVO_DT_CLASS 12
#define XL6_XVO_DT_NATIVE 15

bool xl6ProtocolWitnessValidate(const xl6_protocol_witness* witness);
const xl6_protocol_entry* xl6ProtocolWitnessFind(
    const xl6_protocol_witness* witness,
    const char* name,
    uint32 nameSize,
    uint64 signatureId);
bool xl6ProtocolRegister(const xl6_protocol_witness* witness);
bool xl6ProtocolUnregister(const xl6_protocol_witness* witness);
const xl6_protocol_witness* xl6ProtocolLookup(
    uint32 protocolTypeId,
    uint32 concreteTypeId);

static inline const xl6_type_desc* xl6TypeNull(void);
static inline const xl6_type_desc* xl6TypeBool(void);
static inline const xl6_type_desc* xl6TypeBool32(void);
static inline const xl6_type_desc* xl6TypeInt(void);
static inline const xl6_type_desc* xl6TypeInt8(void);
static inline const xl6_type_desc* xl6TypeUInt8(void);
static inline const xl6_type_desc* xl6TypeInt16(void);
static inline const xl6_type_desc* xl6TypeUInt16(void);
static inline const xl6_type_desc* xl6TypeInt32(void);
static inline const xl6_type_desc* xl6TypeUInt32(void);
static inline const xl6_type_desc* xl6TypeInt64(void);
static inline const xl6_type_desc* xl6TypeUInt64(void);
static inline const xl6_type_desc* xl6TypeFloat(void);
static inline const xl6_type_desc* xl6TypeFloat32(void);
static inline const xl6_type_desc* xl6TypeFloat64(void);
static inline const xl6_type_desc* xl6TypeString(void);
static inline const xl6_type_desc* xl6TypeTime(void);
static inline const xl6_type_desc* xl6TypePointer(void);
static inline const xl6_type_desc* xl6TypeFunction(void);
static inline const xl6_type_desc* xl6TypeFuture(void);
static inline const xl6_type_desc* xl6TypeArray(void);
static inline const xl6_type_desc* xl6TypeList(void);
static inline const xl6_type_desc* xl6TypeSet(void);
static inline const xl6_type_desc* xl6TypeDict(void);
static inline const xl6_type_desc* xl6TypeHandle(void);
static inline const xl6_type_desc* xl6TypeType(void);
static inline const xl6_type_desc* xl6TypeValue(void);
static inline bool xl6_xrt_value_is_null(const xvalue* pValue);
static inline bool xl6TypeSame(const xl6_type_desc* pA, const xl6_type_desc* pB);
static inline bool xl6TypeIsA(const xl6_type_desc* pType, const xl6_type_desc* pTarget);
static inline bool xl6TypeCanWiden(const xl6_type_desc* pSource, const xl6_type_desc* pTarget);
static inline uint32 xl6TypeGenericArgCount(const xl6_type_desc* pType);
static inline const xl6_type_desc* xl6TypeGenericArg(const xl6_type_desc* pType, uint32 iIndex);
static inline xvalue* xl6TypeBoxValue(const xl6_type_desc* pType, const void* pObj);
static inline bool xl6TypeUnboxValue(const xl6_type_desc* pType, xvalue* pVal, ptr pOut);
static inline bool xl6TypeInitValue(const xl6_type_desc* pType, void* pObj);
static inline bool xl6TypeCopyValue(const xl6_type_desc* pType, void* pDst, const void* pSrc);
static inline bool xl6TypeMoveValue(const xl6_type_desc* pType, void* pDst, void* pSrc);
static inline bool xl6TypeCloneValue(const xl6_type_desc* pType, void* pDst, const void* pSrc);
static inline void xl6TypeDropValue(const xl6_type_desc* pType, ptr pObj);
static inline int xl6TypeCompareValue(const xl6_type_desc* pType, const void* pA, const void* pB);
static inline uint64_t xl6TypeHashValue(const xl6_type_desc* pType, const void* pObj);
#if defined(XL_RUNTIME_NEED_TYPED_CONTAINER) || \
	defined(XL_RUNTIME_NEED_TYPE_STRING_VALUE)
static inline str xl6TypeToStringValue(const xl6_type_desc* pType, const void* pObj, uint32* pSize);
#endif
static inline const xl6_enum_info* xl6EnumInfo(const xl6_type_desc* pType);
static inline const xl6_enum_variant* xl6EnumVariantAt(
    const xl6_enum_info* pEnum,
    uint32 iIndex);
static inline const xl6_enum_variant* xl6EnumFindVariant(
    const xl6_enum_info* pEnum,
    const char* sName,
    uint32 iNameSize);
static inline const xl6_enum_variant* xl6EnumFindValue(
    const xl6_enum_info* pEnum,
    uint64 iValue);
static inline const xl6_enum_field* xl6EnumFieldAt(
    const xl6_enum_variant* pVariant,
    uint32 iIndex);
static inline const xl6_enum_field* xl6EnumFindField(
    const xl6_enum_variant* pVariant,
    const char* sName,
    uint32 iNameSize);
uint64 xl6FuncSigTypeId(const xl6_func_sig* pSig);

static inline xvalue* xl6ValueRecord(const xl6_type_desc* pType, const void* pData);
static inline xvalue* xl6ValueObject(const xl6_type_desc* pType);
static inline xvalue* xl6ValueRecordMove(const xl6_type_desc* pType, void* pData);
static inline xvalue* xl6ValueRecordClone(const xvalue* pValue, bool bDeep);
static inline xvalue* xl6ValueRecordCloneRelease(xvalue* pValue, bool bDeep);
static inline xvalue* xl6ValueHandle(const xl6_type_desc* pType, ptr pHandle, uint32 iFlags);
#ifdef XL_RUNTIME_NEED_WEAK
static inline xl6_weak xl6WeakCreate(xvalue* pValue);
static inline void xl6WeakInit(xl6_weak* pWeak);
static inline bool xl6WeakSet(xl6_weak* pWeak, xvalue* pValue);
static inline bool xl6WeakCopy(xl6_weak* pTarget, const xl6_weak* pSource);
static inline xl6_weak xl6WeakClone(const xl6_weak* pSource);
static inline void xl6WeakMove(xl6_weak* pTarget, xl6_weak* pSource);
static inline void xl6WeakTake(xl6_weak* pTarget, xl6_weak Value);
static inline void xl6WeakUnit(xl6_weak* pWeak);
static inline bool xl6WeakExpired(const xl6_weak* pWeak);
static inline xvalue* xl6WeakLock(const xl6_weak* pWeak);
static inline xvalue* xl6WeakLockValue(xl6_weak Weak);
#endif
static inline const xl6_type_desc* xl6ValueType(const xvalue* pVal);
static inline bool xl6ValueSetType(xvalue* pVal, const xl6_type_desc* pType);
static inline ptr xl6ValueRecordData(xvalue* pVal);
static inline xvalue* xl6ValueRecordOwnerRef(const ptr pData);
static inline ptr xl6ValueHandleData(xvalue* pVal);
static inline ptr* xl6ValueHandleSlot(xvalue* pVal);
static inline xvalue* xl6ValueHandleOwner(const ptr* pSlot);

static inline void xl6CallResultInit(xl6_call_result* pResult);
static inline void xl6CallResultUnit(xl6_call_result* pResult);
static inline bool xl6CallResultSetValue(xl6_call_result* pResult,
    uint32 iIndex, xvalue* pValue, bool bTake);
static inline xrtcallable* xl6XrtCallableCreate(const xl6_func_sig* pSignature,
    xl6_xcall_proc pEntry, ptr pEnvironment, void (*pDropEnvironment)(ptr));
static inline const xl6_func_sig* xl6XrtCallableSourceSignature(
    const xrtcallable* pCallable);
static inline uint64 xl6XrtCallableSignatureId(const xrtcallable* pCallable);
static inline bool xl6XrtCallableInvoke(const xrtcallable* pCallable,
    xl6_call_frame* pFrame, xl6_call_result* pResult);
static inline bool xl6XrtValueInvoke(const xvalue* pCallable,
    xl6_call_frame* pFrame, xl6_call_result* pResult);

/* 私有桥接源可只读取稳定 ABI 声明，避免重复生成单头运行时实现。 */
#ifndef XL_RUNTIME_DECLARATIONS_ONLY

#ifndef XL6_XRT_CALL_BRIDGE_IMPL
#define XL6_XRT_CALL_BRIDGE_IMPL

typedef struct xl6_xrt_callable_bridge {
    const xl6_func_sig* Signature;
    xl6_xcall_proc Entry;
    ptr Environment;
    void (*DropEnvironment)(ptr);
    xrtfunctionsig RuntimeSignature;
    const xrttype** ReturnTypes;
} xl6_xrt_callable_bridge;

#define XL6_XRT_CALLABLE_SIGNATURE_TAG UINT64_C(0x584C3643414C4C42)

static inline void xl6CallResultInit(xl6_call_result* pResult)
{
    if ( pResult != 0 ) {
        memset(pResult, 0, sizeof(*pResult));
    }
}

static inline void xl6CallResultUnit(xl6_call_result* pResult)
{
    uint32 i;
    if ( pResult == 0 ) {
        return;
    }
    for ( i = 0; i < XRT_CALL_RESULT_INLINE_COUNT; i++ ) {
        xrtValueRelease(pResult->Values[i]);
        pResult->Values[i] = 0;
    }
    xrtValueRelease(pResult->Overflow);
    pResult->Overflow = 0;
    pResult->Count = 0;
}

static inline bool xl6CallResultSetValue(xl6_call_result* pResult,
    uint32 iIndex, xvalue* pValue, bool bTake)
{
    xvalue* retained;
    xvalue* previous;
    size_t overflowIndex;
    if ( pResult == 0 || pValue == 0 ) {
        return false;
    }
    if ( iIndex < XRT_CALL_RESULT_INLINE_COUNT ) {
        retained = bTake ? pValue : xrtValueRetain(pValue);
        if ( retained == 0 ) {
            return false;
        }
        previous = pResult->Values[iIndex];
        pResult->Values[iIndex] = retained;
        xrtValueRelease(previous);
    } else {
        overflowIndex = (size_t)(iIndex - XRT_CALL_RESULT_INLINE_COUNT);
        if ( pResult->Overflow == 0 ) {
            pResult->Overflow = xrtValueArray();
            if ( pResult->Overflow == 0 ) {
                return false;
            }
        }
        while ( xrtValueCount(pResult->Overflow) <= overflowIndex ) {
            if ( !xrtValueArrayAppend(pResult->Overflow, xrtValueNull()) ) {
                return false;
            }
        }
        if ( bTake ) {
            retained = pValue;
            if ( !xrtValueArraySetTake(pResult->Overflow,
                    overflowIndex, &retained) ) {
                return false;
            }
        } else if ( !xrtValueArraySet(pResult->Overflow,
                overflowIndex, pValue) ) {
            return false;
        }
    }
    if ( pResult->Count <= iIndex ) {
        pResult->Count = iIndex + 1u;
    }
    return true;
}

static inline xvalue* xl6CallResultGet(const xl6_call_result* pResult,
    uint32 iIndex)
{
    if ( pResult == 0 || iIndex >= pResult->Count ) {
        return 0;
    }
    return iIndex < XRT_CALL_RESULT_INLINE_COUNT
        ? pResult->Values[iIndex]
        : xrtValueArrayGet(pResult->Overflow,
            (size_t)(iIndex - XRT_CALL_RESULT_INLINE_COUNT));
}

static inline bool xl6XrtCallResultExport(xl6_call_result* pSource,
    xrtcallresult* pTarget)
{
    uint32 i;
    xvalue* value;
    for ( i = 0; i < pSource->Count; i++ ) {
        if ( i < XRT_CALL_RESULT_INLINE_COUNT ) {
            value = pSource->Values[i];
            pSource->Values[i] = 0;
        } else {
            value = xrtValueArrayTake(pSource->Overflow, 0);
        }
        if ( value == 0 || !xrtCallResultPushTake(pTarget, &value) ) {
            xrtValueRelease(value);
            return false;
        }
    }
    return true;
}

static inline bool xl6XrtCallableEntry(ptr pEnvironment,
    const xrtcallframe* pFrame, xrtcallresult* pResult)
{
    xl6_xrt_callable_bridge* bridge =
        (xl6_xrt_callable_bridge*)pEnvironment;
    xl6_call_frame frame;
    xl6_call_result result;
    size_t i;
    bool success;
    if ( bridge == 0 || bridge->Entry == 0 || pFrame == 0 ||
         pResult == 0 || pFrame->ArgumentCount > UINT32_MAX ) {
        return false;
    }
    memset(&frame, 0, sizeof(frame));
    frame.Self = pFrame->Self;
    frame.Env = bridge->Environment;
    frame.Argc = (uint32)pFrame->ArgumentCount;
    frame.Argv = (xvalue**)pFrame->Arguments;
    frame.Sig = bridge->Signature;
    if ( pFrame->KeywordCount != 0 ) {
        frame.Kwargs = xrtValueObject();
        if ( frame.Kwargs == 0 ) {
            return false;
        }
        for ( i = 0; i < pFrame->KeywordCount; i++ ) {
            if ( !xrtValueObjectSet(frame.Kwargs,
                    pFrame->KeywordNames[i], pFrame->KeywordValues[i]) ) {
                xrtValueRelease(frame.Kwargs);
                return false;
            }
        }
    }
    xl6CallResultInit(&result);
    success = bridge->Entry(&frame, &result);
    if ( success ) {
        success = xl6XrtCallResultExport(&result, pResult);
    }
    xl6CallResultUnit(&result);
    xrtValueRelease(frame.Kwargs);
    return success;
}

static inline void xl6XrtCallableBridgeFree(
    xl6_xrt_callable_bridge* bridge, bool dropEnvironment)
{
    if ( bridge == 0 ) {
        return;
    }
    if ( dropEnvironment && bridge->DropEnvironment != 0 ) {
        bridge->DropEnvironment(bridge->Environment);
    }
    xrtFree(bridge->ReturnTypes);
    xrtFree(bridge);
}

static inline void xl6XrtCallableDrop(ptr pEnvironment)
{
    xl6XrtCallableBridgeFree(
        (xl6_xrt_callable_bridge*)pEnvironment, true);
}

static inline xrtcallable* xl6XrtCallableCreate(
    const xl6_func_sig* pSignature, xl6_xcall_proc pEntry,
    ptr pEnvironment, void (*pDropEnvironment)(ptr))
{
    xl6_xrt_callable_bridge* bridge;
    xrtcallable* callable;
    const xrttype* valueType;
    size_t i;
    if ( pEntry == 0 ) {
        return 0;
    }
    bridge = (xl6_xrt_callable_bridge*)xrtCalloc(1, sizeof(*bridge));
    if ( bridge == 0 ) {
        return 0;
    }
    bridge->Signature = pSignature;
    bridge->Entry = pEntry;
    bridge->Environment = pEnvironment;
    bridge->DropEnvironment = pDropEnvironment;
    if ( pSignature != 0 ) {
        bridge->RuntimeSignature.Id = 0;
        bridge->RuntimeSignature.Name.Data = pSignature->Name != 0
            ? pSignature->Name : "";
        bridge->RuntimeSignature.Name.Size = pSignature->Name != 0
            ? (size_t)pSignature->NameSize : 0;
        bridge->RuntimeSignature.Flags = 0x00000003u;
        bridge->RuntimeSignature.UserTag = XL6_XRT_CALLABLE_SIGNATURE_TAG;
        bridge->RuntimeSignature.UserData = (ptr)pSignature;
        bridge->RuntimeSignature.ReturnCount = pSignature->ReturnCount;
        if ( pSignature->ReturnCount != 0 ) {
            valueType = xrtTypeValue();
            bridge->ReturnTypes = (const xrttype**)xrtMalloc(
                sizeof(*bridge->ReturnTypes) * pSignature->ReturnCount);
            if ( valueType == 0 || bridge->ReturnTypes == 0 ) {
                xl6XrtCallableBridgeFree(bridge, false);
                return 0;
            }
            for ( i = 0; i < pSignature->ReturnCount; i++ ) {
                bridge->ReturnTypes[i] = valueType;
            }
            bridge->RuntimeSignature.ReturnTypes = bridge->ReturnTypes;
        }
    }
    callable = xrtCallableCreate(pSignature != 0
        ? &bridge->RuntimeSignature : 0,
        xl6XrtCallableEntry, bridge, xl6XrtCallableDrop);
    if ( callable == 0 ) {
        xl6XrtCallableBridgeFree(bridge, false);
    }
    return callable;
}

static inline const xl6_func_sig* xl6XrtCallableSourceSignature(
    const xrtcallable* pCallable)
{
    const xrtfunctionsig* signature = xrtCallableSignature(pCallable);
    if ( signature == 0 ||
         signature->UserTag != XL6_XRT_CALLABLE_SIGNATURE_TAG ) {
        return 0;
    }
    return (const xl6_func_sig*)signature->UserData;
}

static inline uint64 xl6XrtCallableSignatureId(const xrtcallable* pCallable)
{
    const xl6_func_sig* signature =
        xl6XrtCallableSourceSignature(pCallable);
    return signature != 0
        ? signature->TypeId
        : xrtCallableSignatureId(pCallable);
}

static inline bool xl6XrtCallableInvoke(const xrtcallable* pCallable,
    xl6_call_frame* pFrame, xl6_call_result* pResult)
{
    xrtcallframe frame;
    xrtcallresult result;
    xl6_call_result converted;
    xstrview* names = 0;
    xvalue** values = 0;
    size_t keywordCount = 0;
    size_t i;
    bool success = false;
    if ( pCallable == 0 || pResult == 0 ) {
        return false;
    }
    memset(&frame, 0, sizeof(frame));
    memset(&result, 0, sizeof(result));
    xl6CallResultInit(&converted);
    frame.Signature = xrtCallableSignature(pCallable);
    if ( pFrame != 0 ) {
        frame.Self = pFrame->Self;
        frame.ArgumentCount = pFrame->Argc;
        frame.Arguments = pFrame->Argv;
        if ( pFrame->Kwargs != 0 ) {
            keywordCount = xrtValueCount(pFrame->Kwargs);
            if ( keywordCount != 0 ) {
                names = (xstrview*)xrtMalloc(sizeof(*names) * keywordCount);
                values = (xvalue**)xrtMalloc(sizeof(*values) * keywordCount);
                if ( names == 0 || values == 0 ) {
                    goto cleanup;
                }
                for ( i = 0; i < keywordCount; i++ ) {
                    values[i] = xrtValueObjectAt(
                        pFrame->Kwargs, i, &names[i]);
                    if ( values[i] == 0 ) {
                        goto cleanup;
                    }
                }
                frame.KeywordCount = keywordCount;
                frame.KeywordNames = names;
                frame.KeywordValues = values;
            }
        }
    }
    if ( !xrtCallableInvoke(pCallable, &frame, &result) ) {
        goto cleanup;
    }
    for ( i = 0; i < xrtCallResultCount(&result); i++ ) {
        if ( !xl6CallResultSetValue(&converted, (uint32)i,
                xrtCallResultGet(&result, i), false) ) {
            goto cleanup;
        }
    }
    xl6CallResultUnit(pResult);
    *pResult = converted;
    memset(&converted, 0, sizeof(converted));
    success = true;

cleanup:
    xl6CallResultUnit(&converted);
    xrtCallResultUnit(&result);
    xrtFree(values);
    xrtFree(names);
    return success;
}

static inline bool xl6XrtValueInvoke(const xvalue* pCallable,
    xl6_call_frame* pFrame, xl6_call_result* pResult)
{
    xrtcallable* callable = xrtValueGetCallable(pCallable);
    return callable != 0 &&
        xl6XrtCallableInvoke(callable, pFrame, pResult);
}

#endif

#ifndef XL6_RUNTIME_CORE_IMPL
#define XL6_RUNTIME_CORE_IMPL
static inline void xl6_runtime_fatal(int code)
{
    exit(code);
}

static inline void xl6_runtime_fatal_message(int code, const char* message)
{
    if ( message != 0 && message[0] != '\0' ) {
        fprintf(stderr, "xlang runtime fatal: %s (code %d)\n", message, code);
        fflush(stderr);
    }
    exit(code);
}

static inline size_t xl6_xrt_utf8_char_size(const char* text, size_t size)
{
    uint32 scalar = 0;
    size_t read = 0;

    return xrtUtf8Decode(xrtStrViewN(text, size), &scalar, &read) == XUTF_OK ?
        read : 0;
}

static inline int64 xl6_xrt_value_int(const xvalue* value)
{
    int64 result = 0;

    (void)xrtValueGetInt(value, &result);
    return result;
}

static inline double xl6_xrt_value_float(const xvalue* value)
{
    double result = 0.0;

    (void)xrtValueGetFloat(value, &result);
    return result;
}

static inline bool xl6_xrt_value_bool(const xvalue* value)
{
    bool result = false;

    (void)xrtValueGetBool(value, &result);
    return result;
}

/* 为 xlang Random 类创建一个由语言对象独占的显式随机状态。 */
static inline void* xl6_xrt_rng_create(uint64 seed, uint64 stream)
{
    xl6_xrt_rng* rng = (xl6_xrt_rng*)xrtMalloc(sizeof(*rng));

    if ( rng != NULL ) {
        xrtRngSeed(rng, seed, stream);
    }
    return rng;
}

/* 释放 xlang Random 类持有的显式随机状态。 */
static inline int64 xl6_xrt_rng_destroy(void* self)
{
    xrtFree(self);
    return 0;
}

/* 重置一个 xlang Random 实例的可复现序列。 */
static inline void xl6_xrt_rng_seed(void* self, uint64 seed, uint64 stream)
{
    if ( self != NULL ) {
        xrtRngSeed((xl6_xrt_rng*)self, seed, stream);
    }
}

/* 从显式随机状态生成 32 位整数。 */
static inline uint32 xl6_xrt_rng_u32(void* self)
{
    return self != NULL ? xrtRng32((xl6_xrt_rng*)self) : 0;
}

/* 从显式随机状态生成 64 位整数。 */
static inline uint64 xl6_xrt_rng_u64(void* self)
{
    return self != NULL ? xrtRng64((xl6_xrt_rng*)self) : 0;
}

/* 保留 xlang 的闭区间随机整数契约。 */
static inline int xl6_xrt_rng_range(void* self, int min, int max)
{
    if ( self == NULL ) {
        return min;
    }
    return (int)xrtRngRangeClosed((xl6_xrt_rng*)self, min, max);
}

/* 使用显式状态和可选字母表创建随机字符串。 */
static inline void* xl6_xrt_rng_string(
    void* self,
    const char* chars,
    size_t size,
    size_t length)
{
    xstrview alphabet = size != 0 ?
        (xstrview){ chars, size } : xrtStrView(chars);

    if ( self == NULL ) {
        return NULL;
    }
    return alphabet.Size != 0 ?
        xrtRngStringFrom((xl6_xrt_rng*)self, alphabet, length) :
        xrtRngString((xl6_xrt_rng*)self, length);
}

/* 使用当前线程状态生成 xlang 闭区间随机整数。 */
static inline int xl6_xrt_random_range(int min, int max)
{
    return (int)xrtRandRangeClosed(min, max);
}

/* 重置当前线程的 xlang 便捷随机序列。 */
static inline void xl6_xrt_random_seed(uint64 seed, uint64 stream)
{
    xrtRandSeed(seed, stream);
}

/* 使用当前线程状态和可选字母表创建随机字符串。 */
static inline void* xl6_xrt_random_string(
    const char* chars,
    size_t size,
    size_t length)
{
    xstrview alphabet = size != 0 ?
        (xstrview){ chars, size } : xrtStrView(chars);

    return alphabet.Size != 0 ?
        xrtRandStringFrom(alphabet, length) : xrtRandString(length);
}

/* xlang 的 time 与当前 XRT 一致使用微秒；此处只保留语言层的易用语义。 */
#define XL6_XRT_TIME_SECOND XRT_TIME_SECOND
#define XL6_XRT_TIME_MINUTE XRT_TIME_MINUTE
#define XL6_XRT_TIME_HOUR XRT_TIME_HOUR
#define XL6_XRT_TIME_DAY XRT_TIME_DAY
#define XL6_XRT_TIME_WEEK XRT_TIME_WEEK

enum {
    XL6_XRT_TIME_UNIT_SECOND = XTIME_UNIT_SECOND,
    XL6_XRT_TIME_UNIT_MINUTE = XTIME_UNIT_MINUTE,
    XL6_XRT_TIME_UNIT_HOUR = XTIME_UNIT_HOUR,
    XL6_XRT_TIME_UNIT_DAY = XTIME_UNIT_DAY,
    XL6_XRT_TIME_UNIT_WEEK = XTIME_UNIT_WEEK,
    XL6_XRT_TIME_UNIT_MONTH = XTIME_UNIT_MONTH,
    XL6_XRT_TIME_UNIT_QUARTER = XTIME_UNIT_QUARTER,
    XL6_XRT_TIME_UNIT_YEAR = XTIME_UNIT_YEAR
};

/* 把标准库保留的紧凑区间编号映射到 XRT 唯一的时间单位枚举。 */
static inline int xl6_xrt_time_unit(int interval)
{
    switch ( interval ) {
    case 1:
        return XL6_XRT_TIME_UNIT_YEAR;
    case 2:
        return XL6_XRT_TIME_UNIT_MONTH;
    case 3:
        return XL6_XRT_TIME_UNIT_DAY;
    case 4:
        return XL6_XRT_TIME_UNIT_HOUR;
    case 5:
        return XL6_XRT_TIME_UNIT_MINUTE;
    case 6:
        return XL6_XRT_TIME_UNIT_SECOND;
    case 7:
        return XL6_XRT_TIME_UNIT_WEEK;
    case 8:
        return XL6_XRT_TIME_UNIT_QUARTER;
    default:
        return -1;
    }
}

/* 时间构造和运算失败时返回 time 零值，具体错误仍保留在 XRT 错误上下文。 */
static inline xtime xl6_xrt_time_datetime(
    int64 year,
    int month,
    int day,
    int hour,
    int minute,
    int second)
{
    xtime result = 0;

    (void)xrtDateTime(year, month, day, hour, minute, second, 0, &result);
    return result;
}

static inline xtime xl6_xrt_time_date(int64 year, int month, int day)
{
    xtime result = 0;

    (void)xrtDate(year, month, day, &result);
    return result;
}

static inline xtime xl6_xrt_time_clock_value(int hour, int minute, int second)
{
    return xl6_xrt_time_datetime(1970, 1, 1, hour, minute, second);
}

static inline xtime xl6_xrt_time_add(int interval, int64 amount, xtime value)
{
    xtime result = 0;
    int unit = xl6_xrt_time_unit(interval);

    if ( unit < 0 || !xrtTimeAdd(value, amount, unit, &result) ) {
        return 0;
    }
    return result;
}

static inline int64 xl6_xrt_time_diff(int interval, xtime left, xtime right)
{
    int64 result = 0;
    int unit = xl6_xrt_time_unit(interval);

    if ( unit < 0 || !xrtTimeDiff(left, right, unit, &result) ) {
        return 0;
    }
    return result;
}

/* xlang 的 UTC/local 转换返回墙上时间值，绝对时刻只在转换过程中存在。 */
static inline xtime xl6_xrt_time_utc_to_local(xtime value)
{
    xl6_xrt_datetime local;
    xtime result = 0;

    if ( !xrtTimeLocal(value, &local) ) {
        return 0;
    }
    local.Offset = 0;
    if ( !xrtTimeMake(&local, &result) ) {
        return 0;
    }
    return result;
}

static inline xtime xl6_xrt_time_local_to_utc(xtime value)
{
    xl6_xrt_datetime local;
    xtime result = 0;

    if ( !xrtTimeSplit(value, &local) ) {
        return 0;
    }
    if ( !xrtTimeFromLocal(&local, XTIME_FOLD_EARLIER, &result) ) {
        return 0;
    }
    return result;
}

static inline xtime xl6_xrt_time_now_local(void)
{
    return xl6_xrt_time_utc_to_local(xrtNow());
}

static inline xtime xl6_xrt_time_today(void)
{
    return xrtDatePart(xl6_xrt_time_now_local());
}

static inline xtime xl6_xrt_time_clock(void)
{
    return xrtTimePart(xl6_xrt_time_now_local());
}

static inline xtime xl6_xrt_time_now_utc(void)
{
    return xrtNow();
}

static inline int xl6_xrt_time_timezone_offset(void)
{
    xl6_xrt_datetime local;

    return xrtTimeLocal(xrtNow(), &local) ? local.Offset : 0;
}

static inline xtime xl6_xrt_time_from_unix(int64 seconds)
{
    xtime result = 0;

    (void)xrtTimeFromUnix(seconds, &result);
    return result;
}

static inline xtime xl6_xrt_time_first_day_of_month(xtime value)
{
    xtime start = 0;
    xtime end = 0;

    return xrtMonthRange(value, &start, &end) ? start : 0;
}

static inline xtime xl6_xrt_time_last_day_of_month(xtime value)
{
    xtime start = 0;
    xtime end = 0;

    return xrtMonthRange(value, &start, &end) ? end - XL6_XRT_TIME_DAY : 0;
}

static inline xtime xl6_xrt_time_first_day_of_year(xtime value)
{
    xtime start = 0;
    xtime end = 0;

    return xrtYearRange(value, &start, &end) ? start : 0;
}

static inline xtime xl6_xrt_time_last_day_of_year(xtime value)
{
    xtime start = 0;
    xtime end = 0;

    return xrtYearRange(value, &start, &end) ? end - XL6_XRT_TIME_DAY : 0;
}

static inline xtime xl6_xrt_time_first_day_of_week(xtime value, int startDay)
{
    xtime start = 0;
    xtime end = 0;

    return xrtWeekRange(value, startDay, &start, &end) ? start : 0;
}

static inline xtime xl6_xrt_time_last_day_of_week(xtime value, int startDay)
{
    xtime start = 0;
    xtime end = 0;

    return xrtWeekRange(value, startDay, &start, &end) ? end - XL6_XRT_TIME_DAY : 0;
}

static inline int xl6_xrt_time_week_of_year(xtime value)
{
    int64 weekYear = 0;
    int week = 0;
    int weekday = 0;

    return xrtISOWeek(value, &weekYear, &week, &weekday) ? week : 0;
}

static inline int xl6_xrt_time_week_of_month(xtime value)
{
    xl6_xrt_datetime dateTime;
    xtime first = 0;
    int firstWeekday;
    int firstDayOffset;

    if ( !xrtTimeSplit(value, &dateTime) ||
         !xrtDate(dateTime.Year, dateTime.Month, 1, &first) ) {
        return 0;
    }
    firstWeekday = xrtWeekday(first);
    firstDayOffset = firstWeekday == 0 ? 6 : firstWeekday - 1;
    return ((dateTime.Day - 1 + firstDayOffset) / 7) + 1;
}

/* 约等于规则固定在语言契约中，不再依赖旧 XRT 全局可变配置。 */
static inline bool xl6_xrt_int_approx(int64 left, int64 right)
{
    long double leftValue;
    long double rightValue;
    long double difference;
    long double maximum;

    if ( left == right ) {
        return true;
    }
    leftValue = (long double)left;
    rightValue = (long double)right;
    difference = leftValue > rightValue
        ? leftValue - rightValue
        : rightValue - leftValue;
    leftValue = leftValue < 0.0L ? -leftValue : leftValue;
    rightValue = rightValue < 0.0L ? -rightValue : rightValue;
    maximum = leftValue > rightValue ? leftValue : rightValue;
    return maximum != 0.0L && (difference / maximum) <= 0.0001L;
}

static inline bool xl6_xrt_num_approx(double left, double right)
{
    double difference;

    if ( left == right ) {
        return true;
    }
    difference = left > right ? left - right : right - left;
    return difference <= 0.01;
}

static inline bool xl6_xrt_time_approx(xtime left, xtime right)
{
    return xrtTimeNear(left, right, (uint64)(10 * XL6_XRT_TIME_SECOND));
}

/* 返回由 XRT 分配器持有的 UTF-8 相对时间文本，调用端负责接管并释放。 */
static inline void* xl6_xrt_time_relative(xtime value, xtime base)
{
    bool future = value >= base;
    uint64 distance = future
        ? (uint64)value - (uint64)base
        : (uint64)base - (uint64)value;
    uint64 count;
    const char* unit;
    const char* suffix = future ? "后" : "前";
    char* result = (char*)xrtMalloc(64);

    if ( result == NULL ) {
        return NULL;
    }
    if ( distance < (uint64)XL6_XRT_TIME_MINUTE ) {
        count = distance / (uint64)XL6_XRT_TIME_SECOND;
        unit = "秒";
    } else if ( distance < (uint64)XL6_XRT_TIME_HOUR ) {
        count = distance / (uint64)XL6_XRT_TIME_MINUTE;
        unit = "分钟";
    } else if ( distance < (uint64)XL6_XRT_TIME_DAY ) {
        count = distance / (uint64)XL6_XRT_TIME_HOUR;
        unit = "小时";
    } else if ( distance < (uint64)(30 * XL6_XRT_TIME_DAY) ) {
        count = distance / (uint64)XL6_XRT_TIME_DAY;
        unit = "天";
    } else if ( distance < (uint64)(365 * XL6_XRT_TIME_DAY) ) {
        count = distance / (uint64)(30 * XL6_XRT_TIME_DAY);
        unit = "个月";
    } else {
        count = distance / (uint64)(365 * XL6_XRT_TIME_DAY);
        unit = "年";
    }
    (void)snprintf(result, 64, "%llu%s%s",
        (unsigned long long)count, unit, suffix);
    return result;
}

static inline void* xl6_xrt_time_string_owned(xtime value, int format)
{
    const char* pattern;

    switch ( format ) {
    case 0:
        pattern = "%Y-%m-%d %H:%M:%S";
        break;
    case 1:
        pattern = "%Y-%m-%d";
        break;
    case 2:
        pattern = "%H:%M:%S";
        break;
    default:
        return xrtStrDup("null");
    }
    return xrtTimeFormat(value, 0, xrtStrView(pattern));
}

/* XLang 保留易读模式，运行时只负责把它映射到 XRT 的唯一时间格式契约。 */
static inline char* xl6_xrt_time_pattern(const char* pattern)
{
    size_t size;
    size_t position = 0;
    size_t outputPosition = 0;
    bool afterHour = false;
    char* output;

    if ( pattern == NULL ) {
        return NULL;
    }
    size = strlen(pattern);
    if ( size > ((SIZE_MAX - 1) / 3) ) {
        return NULL;
    }
    output = (char*)xrtMalloc((size * 3) + 1);
    if ( output == NULL ) {
        return NULL;
    }
    while ( position < size ) {
        char byte = pattern[position];
        const char* replacement = NULL;
        size_t count = 1;

        while ( ((position + count) < size) &&
                (pattern[position + count] == byte) ) {
            count++;
        }
        if ( byte == 'y' ) {
            replacement = count == 4 ? "%Y" : (count == 2 ? "%y" : NULL);
            afterHour = false;
        } else if ( byte == 'm' ) {
            if ( count == 4 ) {
                replacement = "%B";
            } else if ( count == 3 ) {
                replacement = "%b";
            } else if ( count == 2 ) {
                replacement = afterHour ? "%M" : "%m";
            } else if ( count == 1 ) {
                replacement = "%-m";
            }
            afterHour = false;
        } else if ( byte == 'd' ) {
            replacement = count == 2 ? "%d" : (count == 1 ? "%-d" : NULL);
            afterHour = false;
        } else if ( byte == 'h' ) {
            replacement = count == 2 ? "%H" : (count == 1 ? "%-H" : NULL);
            afterHour = replacement != NULL;
        } else if ( byte == 'H' ) {
            replacement = count == 2 ? "%I" : (count == 1 ? "%-I" : NULL);
            afterHour = replacement != NULL;
        } else if ( byte == 'n' ) {
            replacement = count == 2 ? "%M" : (count == 1 ? "%-M" : NULL);
            afterHour = false;
        } else if ( byte == 's' ) {
            replacement = count == 2 ? "%S" : (count == 1 ? "%-S" : NULL);
            afterHour = false;
        } else if ( (byte == 'a') && ((position + 1) < size) &&
                    (pattern[position + 1] == 'p') ) {
            replacement = "%P";
            count = 2;
            afterHour = false;
        } else if ( (byte == 'A') && ((position + 1) < size) &&
                    (pattern[position + 1] == 'P') ) {
            replacement = "%p";
            count = 2;
            afterHour = false;
        } else if ( byte == 'w' ) {
            replacement = count == 3 ? "%A" :
                (count == 2 ? "%a" : (count == 1 ? "%w" : NULL));
            afterHour = false;
        } else if ( byte == 'q' ) {
            replacement = "%q";
            count = 1;
            afterHour = false;
        } else if ( byte == '%' ) {
            replacement = "%%";
            count = 1;
        }
        if ( replacement != NULL ) {
            size_t replacementSize = strlen(replacement);

            memcpy(output + outputPosition, replacement, replacementSize);
            outputPosition += replacementSize;
            position += count;
            continue;
        }
        if ( (byte == 'y') || (byte == 'm') || (byte == 'd') ||
             (byte == 'h') || (byte == 'H') || (byte == 'n') ||
             (byte == 's') || (byte == 'w') ) {
            xrtFree(output);
            return NULL;
        }
        output[outputPosition++] = byte;
        position++;
    }
    output[outputPosition] = '\0';
    return output;
}

static inline void* xl6_xrt_time_format_owned(xtime value, const char* pattern)
{
    char* xrtPattern = xl6_xrt_time_pattern(pattern);
    void* result;

    if ( xrtPattern == NULL ) {
        return NULL;
    }
    result = xrtTimeFormat(value, 0, xrtStrView(xrtPattern));
    xrtFree(xrtPattern);
    return result;
}

static inline xtime xl6_xrt_time_parse_value(const char* text, const char* pattern)
{
    xtime value = 0;
    char* xrtPattern = xl6_xrt_time_pattern(pattern);

    if ( (text != NULL) && (xrtPattern != NULL) ) {
        (void)xrtTimeParse(xrtStrView(text), xrtStrView(xrtPattern), &value);
    }
    xrtFree(xrtPattern);
    return value;
}

static inline bool xl6_xrt_time_parse_try(
    const char* text,
    const char* pattern,
    void* outValue)
{
    xtime localValue = 0;
    xtime* value = outValue != NULL ? (xtime*)outValue : &localValue;
    char* xrtPattern = xl6_xrt_time_pattern(pattern);
    bool success = text != NULL && xrtPattern != NULL &&
        xrtTimeParse(xrtStrView(text), xrtStrView(xrtPattern), value);

    xrtFree(xrtPattern);
    return success;
}

static inline xtime xl6_xrt_time_parse_any_value(const char* text, size_t size)
{
    xtime value = 0;

    (void)xrtTimeParseAny(
        size != 0 ? xrtStrViewN(text, size) : xrtStrView(text),
        &value);
    return value;
}

static inline bool xl6_xrt_time_parse_any_try(
    const char* text,
    size_t size,
    void* outValue)
{
    xtime localValue = 0;
    xtime* value = outValue != NULL ? (xtime*)outValue : &localValue;

    return text != NULL && xrtTimeParseAny(
        size != 0 ? xrtStrViewN(text, size) : xrtStrView(text),
        value);
}

/*
    生成器按表达式所有权选择借用或移交；辅助层只负责把该选择映射到当前 XRT API。
*/
static inline bool xl6_xrt_value_array_append(xvalue* array, xvalue* value, bool take)
{
    return take ? xrtValueArrayAppendTake(array, &value) : xrtValueArrayAppend(array, value);
}

static inline bool xl6_xrt_value_array_insert(
    xvalue* array,
    size_t index,
    xvalue* value,
    bool take)
{
    return take
        ? xrtValueArrayInsertTake(array, index, &value)
        : xrtValueArrayInsert(array, index, value);
}

static inline bool xl6_xrt_value_array_set(
    xvalue* array,
    size_t index,
    xvalue* value,
    bool take)
{
    return take
        ? xrtValueArraySetTake(array, index, &value)
        : xrtValueArraySet(array, index, value);
}

static inline xstrview xl6_xrt_value_key(const void* key, size_t size)
{
    const char* text = key != 0 ? (const char*)key : "";

    return size != 0 ? xrtStrViewN(text, size) : xrtStrView(text);
}

static inline xvalue* xl6_xrt_value_object_get(
    const xvalue* object,
    const void* key,
    size_t size)
{
    return xrtValueObjectGet(object, xl6_xrt_value_key(key, size));
}

static inline xvalue* xl6_xrt_value_object_edit(
    xvalue* object,
    const void* key,
    size_t size)
{
    return xrtValueObjectEdit(object, xl6_xrt_value_key(key, size));
}

static inline bool xl6_xrt_value_object_set(
    xvalue* object,
    const void* key,
    size_t size,
    xvalue* value,
    bool take)
{
    xstrview name = xl6_xrt_value_key(key, size);

    return take
        ? xrtValueObjectSetTake(object, name, &value)
        : xrtValueObjectSet(object, name, value);
}

static inline bool xl6_xrt_value_object_has(
    const xvalue* object,
    const void* key,
    size_t size)
{
    return xrtValueObjectHas(object, xl6_xrt_value_key(key, size));
}

static inline bool xl6_xrt_value_object_remove(
    xvalue* object,
    const void* key,
    size_t size)
{
    return xrtValueObjectRemove(object, xl6_xrt_value_key(key, size));
}

static inline xvalue* xl6_xrt_value_object_take(
    xvalue* object,
    const void* key,
    size_t size)
{
    return xrtValueObjectTake(object, xl6_xrt_value_key(key, size));
}



/* 创建一个由 xruntime 管理的动态字段对象。 */
static inline ptr xl6_xrt_dynamic_fields_create(void)
{
    return (ptr)xrtDynamicFieldsCreate();
}



/* 接管已经拥有的动态字段对象句柄。 */
static inline ptr xl6_xrt_dynamic_fields_wrap(ptr handle)
{
    return handle;
}



/* 从 Value Object 深复制并创建动态字段对象。 */
static inline ptr xl6_xrt_dynamic_fields_from_value(xvalue* value)
{
    return (ptr)xrtDynamicFieldsFromValue(value);
}



/* 释放动态字段对象句柄。 */
static inline long long xl6_xrt_dynamic_fields_destroy(ptr handle)
{
    xrtDynamicFieldsUnref((xrtdynamicfields*)handle);
    return 0;
}



/* 返回原生动态字段对象句柄。 */
static inline ptr xl6_xrt_dynamic_fields_native_handle(ptr handle)
{
    return handle;
}



/* 返回动态字段数量和容量。 */
static inline long long xl6_xrt_dynamic_fields_count(ptr handle)
{
    return (long long)xrtDynamicFieldsCount((const xrtdynamicfields*)handle);
}



static inline long long xl6_xrt_dynamic_fields_capacity(ptr handle)
{
    return (long long)xrtDynamicFieldsCapacity((const xrtdynamicfields*)handle);
}



/* 管理动态字段存储。 */
static inline int xl6_xrt_dynamic_fields_reserve(ptr handle, long long capacity)
{
    return (capacity >= 0) && xrtDynamicFieldsReserve(
        (xrtdynamicfields*)handle,
        (size_t)capacity);
}



static inline int xl6_xrt_dynamic_fields_trim(ptr handle)
{
    return xrtDynamicFieldsTrim((xrtdynamicfields*)handle);
}



static inline int xl6_xrt_dynamic_fields_clear(ptr handle)
{
    return xrtDynamicFieldsClear((xrtdynamicfields*)handle);
}



/* 查询并按语言别名语义保留动态字段值。 */
static inline int xl6_xrt_dynamic_fields_has(ptr handle, const char* name)
{
    return xrtDynamicFieldsHas(
        (const xrtdynamicfields*)handle,
        xl6_xrt_value_key(name, 0));
}



static inline xvalue* xl6_xrt_dynamic_fields_get(ptr handle, const char* name)
{
    xvalue* value = xrtDynamicFieldsGetRef(
        (const xrtdynamicfields*)handle,
        xl6_xrt_value_key(name, 0));

    return value != 0 ? value : xrtValueNull();
}



static inline xvalue* xl6_xrt_dynamic_fields_copy(ptr handle, const char* name)
{
    xvalue* value = xrtDynamicFieldsCopy(
        (const xrtdynamicfields*)handle,
        xl6_xrt_value_key(name, 0));

    return value != 0 ? value : xrtValueNull();
}



/* 共享同一 Value 身份设置字段；参数只在调用期间借用。 */
static inline int xl6_xrt_dynamic_fields_set(
    ptr handle,
    const char* name,
    xvalue* value)
{
    return xrtDynamicFieldsSetRef(
        (xrtdynamicfields*)handle,
        xl6_xrt_value_key(name, 0),
        value);
}



/* 删除字段，或把字段值所有权移交给语言。 */
static inline int xl6_xrt_dynamic_fields_remove(ptr handle, const char* name)
{
    return xrtDynamicFieldsRemove(
        (xrtdynamicfields*)handle,
        xl6_xrt_value_key(name, 0));
}



static inline xvalue* xl6_xrt_dynamic_fields_take(ptr handle, const char* name)
{
    xvalue* value = xrtDynamicFieldsTake(
        (xrtdynamicfields*)handle,
        xl6_xrt_value_key(name, 0));

    return value != 0 ? value : xrtValueNull();
}



/* 返回语言常用的独立字段集合和 Value Object。 */
static inline xvalue* xl6_xrt_dynamic_fields_keys(ptr handle)
{
    xvalue* value = xrtDynamicFieldsKeys((const xrtdynamicfields*)handle);

    return value != 0 ? value : xrtValueNull();
}



static inline xvalue* xl6_xrt_dynamic_fields_values(ptr handle)
{
    xvalue* value = xrtDynamicFieldsValues((const xrtdynamicfields*)handle);

    return value != 0 ? value : xrtValueNull();
}



static inline xvalue* xl6_xrt_dynamic_fields_items(ptr handle)
{
    xvalue* value = xrtDynamicFieldsItems((const xrtdynamicfields*)handle);

    return value != 0 ? value : xrtValueNull();
}



static inline xvalue* xl6_xrt_dynamic_fields_to_value(ptr handle)
{
    xvalue* value = xrtDynamicFieldsToValue((const xrtdynamicfields*)handle);

    return value != 0 ? value : xrtValueNull();
}



/* 合并或深复制动态字段对象。 */
static inline int xl6_xrt_dynamic_fields_merge(
    ptr handle,
    ptr source,
    int replace)
{
    return xrtDynamicFieldsMerge(
        (xrtdynamicfields*)handle,
        (const xrtdynamicfields*)source,
        replace);
}



static inline ptr xl6_xrt_dynamic_fields_clone(ptr handle)
{
    return (ptr)xrtDynamicFieldsClone((const xrtdynamicfields*)handle);
}



/* 把语言 DynamicFields 类句柄转换为独立 Value Object。 */
static inline xvalue* xl6_xrt_dynamic_fields_to_value_handle(xvalue* fields)
{
    return xl6_xrt_dynamic_fields_to_value(xl6ValueHandleData(fields));
}



/* 失败原子地用 Value Object 替换语言 DynamicFields 类的原生句柄。 */
static inline int xl6_xrt_dynamic_fields_assign_value_handle(
    xvalue* fields,
    const xvalue* value)
{
    ptr* slot = xl6ValueHandleSlot(fields);
    xrtdynamicfields* replacement;
    xrtdynamicfields* previous;

    if ( slot == 0 ) { return false; }
    replacement = xrtDynamicFieldsFromValue(value);
    if ( replacement == 0 ) { return false; }
    previous = (xrtdynamicfields*)*slot;
    *slot = replacement;
    xrtDynamicFieldsUnref(previous);
    return true;
}



static inline bool xl6_xrt_value_set_add(xvalue* set, xvalue* value, bool take)
{
    return take ? xrtValueSetAddTake(set, &value) : xrtValueSetAdd(set, value);
}

static inline xtime xl6_xrt_value_time(const xvalue* value)
{
    xtime result = 0;

    (void)xrtValueGetTime(value, &result);
    return result;
}

static inline ptr xl6_xrt_value_pointer(const xvalue* value)
{
    ptr result = 0;

    (void)xrtValueGetPointer(value, &result);
    return result;
}

static inline const char* xl6_xrt_value_text(const xvalue* value)
{
    xstrview result = {0};

    return xrtValueGetString(value, &result) ? result.Data : "";
}

static inline size_t xl6_xrt_value_text_size(const xvalue* value)
{
    xstrview result = {0};

    return xrtValueGetString(value, &result) ? result.Size : 0;
}

static inline xvalue* xl6_xrt_value_text_create(ptr text, uint32 size, bool take)
{
    xstrview view;
    str owned;
    xvalue* result;

    view.Data = text != 0 ? (const char*)text : "";
    view.Size = size != 0 ? size : strlen(view.Data);
    if ( !take || text == 0 ) {
        return xrtValueString(view);
    }

    owned = (str)text;
    result = xrtValueStringTake(&owned, view.Size);
    if ( result == 0 ) {
        xrtFree(owned);
    }
    return result;
}

static inline str xl6_xrt_value_string_owned(const xvalue* value, uint32* outputSize)
{
    xvaluetype type;
    xstrview text = {0};
    xtime time = 0;
    ptr pointer = 0;
    size_t size = 0;
    str result = 0;
    char buffer[64];

    if ( outputSize != 0 ) {
        *outputSize = 0;
    }
    if ( value == 0 ) {
        result = xrtStrDup("null");
        size = 4;
    } else {
        type = xrtValueType(value);
        switch ( type ) {
        case XVALUE_NULL:
            result = xrtStrDup("null");
            size = 4;
            break;
        case XVALUE_BOOL:
            if ( xl6_xrt_value_bool(value) ) {
                result = xrtStrDup("true");
                size = 4;
            } else {
                result = xrtStrDup("false");
                size = 5;
            }
            break;
        case XVALUE_INT:
            result = xrtIntString(xl6_xrt_value_int(value), 10, 0);
            break;
        case XVALUE_FLOAT:
            result = xrtNumString(xl6_xrt_value_float(value), 0);
            break;
        case XVALUE_STRING:
            if ( xrtValueGetString(value, &text) ) {
                result = xrtStrDupN(text.Data, text.Size);
                size = text.Size;
            }
            break;
        case XVALUE_TIME:
            (void)xrtValueGetTime(value, &time);
            result = xl6_xrt_time_string_owned(time, 0);
            break;
        case XVALUE_POINTER:
            (void)xrtValueGetPointer(value, &pointer);
            size = (size_t)snprintf(
                buffer,
                sizeof(buffer),
                "%p",
                pointer);
            if ( size < sizeof(buffer) ) {
                result = xrtStrDupN(buffer, size);
            }
            break;
        default:
            result = xrtXlonStringify(value, false, &size);
            break;
        }
    }

    if ( result == 0 ) {
        return 0;
    }
    if ( size == 0 ) {
        size = strlen(result);
    }
    if ( size > UINT32_MAX ) {
        xrtFree(result);
        return 0;
    }
    if ( outputSize != 0 ) {
        *outputSize = (uint32)size;
    }
    return result;
}

static inline int xl6_xrt_value_kind(const xvalue* value)
{
    switch ( xrtValueType(value) ) {
    case XVALUE_NULL: return XL6_XVO_DT_NULL;
    case XVALUE_BOOL: return XL6_XVO_DT_BOOL;
    case XVALUE_INT: return XL6_XVO_DT_INT;
    case XVALUE_FLOAT: return XL6_XVO_DT_FLOAT;
    case XVALUE_STRING: return XL6_XVO_DT_TEXT;
    case XVALUE_TIME: return XL6_XVO_DT_TIME;
    case XVALUE_POINTER: return XL6_XVO_DT_POINT;
    case XVALUE_ARRAY: return XL6_XVO_DT_ARRAY;
    case XVALUE_INT_MAP: return XL6_XVO_DT_LIST;
    case XVALUE_SET: return XL6_XVO_DT_COLL;
    case XVALUE_OBJECT: return XL6_XVO_DT_TABLE;
    case XVALUE_HANDLE:
        if ( xrtValueIsCallable(value) ) { return XL6_XVO_DT_FUNC; }
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
        if ( xrtValueIsRuntimeObject(value) ) { return XL6_XVO_DT_CLASS; }
#endif
        return XL6_XVO_DT_NATIVE;
    default:
        return -1;
    }
}



/* 把当前 Value 类型映射为唯一的 XLang 类型描述。 */
static inline const xl6_type_desc* xl6_xrt_value_type(const xvalue* value)
{
    const xl6_type_desc* type;

    if ( value == 0 ) { return 0; }
    type = xl6ValueType(value);
    if ( type != 0 ) { return type; }
    switch ( xrtValueType(value) ) {
    case XVALUE_NULL: return xl6TypeNull();
    case XVALUE_BOOL: return xl6TypeBool();
	case XVALUE_INT: return xl6TypeInt64();
	case XVALUE_FLOAT: return xl6TypeFloat64();
    case XVALUE_STRING: return xl6TypeString();
    case XVALUE_TIME: return xl6TypeTime();
    case XVALUE_POINTER: return xl6TypePointer();
    case XVALUE_ARRAY: return xl6TypeArray();
    case XVALUE_INT_MAP: return xl6TypeList();
    case XVALUE_SET: return xl6TypeSet();
    case XVALUE_OBJECT: return xl6TypeDict();
    case XVALUE_HANDLE:
        return xrtValueIsCallable(value) ? xl6TypeFunction() : 0;
    default:
        return 0;
    }
}



/*
    XLang 的类型描述负责语言语义，新版 XRT 的类型描述只负责 C ABI 值操作。
    这里保留唯一一层映射，生成代码不得把两种结构相互强制转换。
*/
static inline bool xl6_type_name_is(
    const xl6_type_desc* type,
    const char* name,
    size_t size)
{
    return type != 0 && type->Name != 0 && type->NameSize == size &&
           memcmp(type->Name, name, size) == 0;
}



static inline const xrttype* xl6_type_builtin_xrt(const xl6_type_desc* type)
{
    if ( type == 0 ) { return 0; }

    switch ( type->Kind ) {
    case XL6_XRT_TYPE_KIND_NULL:
        return xrtTypeNull();
    case XL6_XRT_TYPE_KIND_BOOL:
        return xl6_type_name_is(type, "bool32", 6) ? xrtTypeBool32() : xrtTypeBool();
    case XL6_XRT_TYPE_KIND_INT:
        if ( xl6_type_name_is(type, "int8", 4) ) { return xrtTypeInt8(); }
        if ( xl6_type_name_is(type, "uint8", 5) ) { return xrtTypeUInt8(); }
        if ( xl6_type_name_is(type, "int16", 5) ) { return xrtTypeInt16(); }
        if ( xl6_type_name_is(type, "uint16", 6) ) { return xrtTypeUInt16(); }
        if ( xl6_type_name_is(type, "int32", 5) ) { return xrtTypeInt32(); }
        if ( xl6_type_name_is(type, "uint32", 6) ) { return xrtTypeUInt32(); }
        if ( xl6_type_name_is(type, "uint64", 6) ) { return xrtTypeUInt64(); }
        return xrtTypeInt64();
    case XL6_XRT_TYPE_KIND_FLOAT:
        return xl6_type_name_is(type, "float32", 7) ? xrtTypeFloat32() : xrtTypeFloat64();
    case XL6_XRT_TYPE_KIND_STRING:
        return xrtTypeString();
    case XL6_XRT_TYPE_KIND_TIME:
        return xrtTypeTime();
    case XL6_XRT_TYPE_KIND_POINTER:
        return xrtTypePointer();
    case XL6_XRT_TYPE_KIND_FUNCTION:
        return xrtTypeCallable();
    case XL6_XRT_TYPE_KIND_FUTURE:
#ifdef XL_RUNTIME_NEED_FUTURE_TYPE
        return xrtTypeFuture();
#else
        return 0;
#endif
    case XL6_XRT_TYPE_KIND_TYPE:
        return xrtTypeUInt32();
    default:
        return xl6_type_name_is(type, "var", 3) ? xrtTypeValue() : 0;
    }
}



/* 字符串是基础类型中唯一必须通过生命周期操作管理的所有权值。 */
static inline void xl6_type_string_init(void* value)
{
    (void)xrtTypeInitValue(xrtTypeString(), value);
}



static inline void xl6_type_string_copy(void* target, const void* source)
{
    (void)xrtTypeCopyValue(xrtTypeString(), target, source);
}



static inline void xl6_type_string_move(void* target, void* source)
{
    (void)xrtTypeMoveValue(xrtTypeString(), target, source);
}



static inline void xl6_type_string_drop(void* value)
{
    xrtTypeDropValue(xrtTypeString(), value);
}



static const xl6_type_ops xl6_type_string_ops = {
    xl6_type_string_init,
    xl6_type_string_copy,
    xl6_type_string_move,
    xl6_type_string_drop,
    0,
    0,
    0,
    0,
    0,
    0
};



#define XL6_DEFINE_PRIVATE_TYPE_GETTER(functionName, typeId, typeKind, displayName, abiName, valueSize, valueAlign, typeOps) \
    static inline const xl6_type_desc* functionName(void) \
    { \
        static const xl6_type_desc type = { \
            typeId, typeKind, displayName, (uint32)(sizeof(displayName) - 1u), \
            abiName, (uint32)(sizeof(abiName) - 1u), valueSize, valueAlign, \
            typeOps, 0, 0, 0, XRT_ATOMICPTR_INIT(NULL) \
        }; \
        return &type; \
    }

XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeNull, 1u, XL6_XRT_TYPE_KIND_NULL,
    "null", "xvalue*", 0u, 0u, 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeBool, 2u, XL6_XRT_TYPE_KIND_BOOL,
    "bool", "bool", sizeof(bool), sizeof(bool), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeBool32, 0x00010002u, XL6_XRT_TYPE_KIND_BOOL,
    "bool32", "int32", sizeof(int32_t), sizeof(int32_t), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeInt, 3u, XL6_XRT_TYPE_KIND_INT,
    "int", "int64", sizeof(int64), sizeof(int64), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeInt8, 0x00010010u, XL6_XRT_TYPE_KIND_INT,
    "int8", "int8", sizeof(int8_t), sizeof(int8_t), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeUInt8, 0x00010011u, XL6_XRT_TYPE_KIND_INT,
    "uint8", "uint8", sizeof(uint8_t), sizeof(uint8_t), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeInt16, 0x00010012u, XL6_XRT_TYPE_KIND_INT,
    "int16", "int16", sizeof(int16_t), sizeof(int16_t), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeUInt16, 0x00010013u, XL6_XRT_TYPE_KIND_INT,
    "uint16", "uint16", sizeof(uint16_t), sizeof(uint16_t), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeInt32, 0x00010014u, XL6_XRT_TYPE_KIND_INT,
    "int32", "int32", sizeof(int32_t), sizeof(int32_t), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeUInt32, 0x00010015u, XL6_XRT_TYPE_KIND_INT,
    "uint32", "uint32", sizeof(uint32), sizeof(uint32), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeInt64, 0x00010016u, XL6_XRT_TYPE_KIND_INT,
    "int64", "int64", sizeof(int64), sizeof(int64), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeUInt64, 0x00010017u, XL6_XRT_TYPE_KIND_INT,
    "uint64", "uint64", sizeof(uint64), sizeof(uint64), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeFloat, 4u, XL6_XRT_TYPE_KIND_FLOAT,
    "float", "double", sizeof(double), sizeof(double), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeFloat32, 0x00010020u, XL6_XRT_TYPE_KIND_FLOAT,
    "float32", "float", sizeof(float), sizeof(float), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeFloat64, 0x00010021u, XL6_XRT_TYPE_KIND_FLOAT,
    "float64", "double", sizeof(double), sizeof(double), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeString, 5u, XL6_XRT_TYPE_KIND_STRING,
    "string", "str", sizeof(str), sizeof(str), &xl6_type_string_ops)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeTime, 6u, XL6_XRT_TYPE_KIND_TIME,
    "time", "xtime", sizeof(xtime), sizeof(xtime), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypePointer, 7u, XL6_XRT_TYPE_KIND_POINTER,
    "point", "ptr", sizeof(ptr), sizeof(ptr), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeFunction, 8u, XL6_XRT_TYPE_KIND_FUNCTION,
    "function", "xrtcallable*", sizeof(ptr), sizeof(ptr), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeArray, 9u, XL6_XRT_TYPE_KIND_ARRAY,
    "array", "xvalue*", sizeof(ptr), sizeof(ptr), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeList, 10u, XL6_XRT_TYPE_KIND_LIST,
    "list", "xvalue*", sizeof(ptr), sizeof(ptr), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeSet, 11u, XL6_XRT_TYPE_KIND_SET,
    "set", "xvalue*", sizeof(ptr), sizeof(ptr), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeDict, 12u, XL6_XRT_TYPE_KIND_DICT,
    "dict", "xvalue*", sizeof(ptr), sizeof(ptr), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeHandle, 14u, XL6_XRT_TYPE_KIND_HANDLE,
    "handle", "ptr", sizeof(ptr), sizeof(ptr), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeType, 15u, XL6_XRT_TYPE_KIND_TYPE,
    "type", "uint32", sizeof(uint32), sizeof(uint32), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeFuture, 16u, XL6_XRT_TYPE_KIND_FUTURE,
    "future", "xfuture*", sizeof(ptr), sizeof(ptr), 0)
XL6_DEFINE_PRIVATE_TYPE_GETTER(xl6TypeValue, 0x00010030u, XL6_XRT_TYPE_KIND_HANDLE,
    "var", "xvalue*", sizeof(ptr), sizeof(ptr), 0)

#undef XL6_DEFINE_PRIVATE_TYPE_GETTER



/* 类型相等同时校验稳定 ID 和名称，避免 32 位语言类型 ID 碰撞。 */
static inline bool xl6TypeSame(const xl6_type_desc* left, const xl6_type_desc* right)
{
    if ( left == right ) { return true; }
    if ( left == 0 || right == 0 ) { return false; }
    if ( left->TypeId != 0 && right->TypeId != 0 && left->TypeId != right->TypeId ) {
        return false;
    }
    if ( left->Name == 0 || right->Name == 0 || left->NameSize != right->NameSize ) {
        return false;
    }
    return memcmp(left->Name, right->Name, left->NameSize) == 0;
}



static inline bool xl6TypeIsA(const xl6_type_desc* type, const xl6_type_desc* target)
{
    uint32 depth = 0;

    if ( target == 0 ) { return false; }
    while ( type != 0 && depth < 256u ) {
        if ( xl6TypeSame(type, target) ) { return true; }
        type = type->BaseType;
        ++depth;
    }
    return false;
}



static inline bool xl6TypeCanWiden(
    const xl6_type_desc* source,
    const xl6_type_desc* target)
{
    if ( source == 0 || target == 0 ) { return false; }
    if ( xl6TypeSame(source, target) ) { return true; }
    if ( source->Kind == XL6_XRT_TYPE_KIND_NULL &&
         target->Kind == XL6_XRT_TYPE_KIND_POINTER ) {
        return true;
    }
    return source->Kind == XL6_XRT_TYPE_KIND_BOOL &&
           (target->Kind == XL6_XRT_TYPE_KIND_INT ||
            target->Kind == XL6_XRT_TYPE_KIND_FLOAT);
}



static inline uint32 xl6TypeGenericArgCount(const xl6_type_desc* type)
{
    const xl6_generic_type* generic;

    if ( type == 0 || type->Extra == 0 ) { return 0; }
    generic = (const xl6_generic_type*)type->Extra;
    if ( generic->ArgCount > 64u || generic->Args == 0 ) { return 0; }
    return generic->ArgCount;
}



static inline const xl6_type_desc* xl6TypeGenericArg(
    const xl6_type_desc* type,
    uint32 index)
{
    const xl6_generic_type* generic;
    uint32 count = xl6TypeGenericArgCount(type);

    if ( index >= count ) { return 0; }
    generic = (const xl6_generic_type*)type->Extra;
    return generic->Args[index];
}



static inline bool xl6TypeInitValue(const xl6_type_desc* type, void* value)
{
    const xrttype* xrtType;

    if ( type == 0 || (value == 0 && type->Size != 0) ) { return false; }
    if ( type->Ops != 0 && type->Ops->init != 0 ) {
        type->Ops->init(value);
        return true;
    }
    xrtType = xl6_type_builtin_xrt(type);
    if ( xrtType != 0 ) { return xrtTypeInitValue(xrtType, value); }
    if ( type->Size != 0 ) { memset(value, 0, type->Size); }
    return true;
}



static inline bool xl6TypeCopyValue(
    const xl6_type_desc* type,
    void* target,
    const void* source)
{
    const xrttype* xrtType;

    if ( type == 0 || (type->Size != 0 && (target == 0 || source == 0)) ) { return false; }
    if ( type->Ops != 0 && type->Ops->copy != 0 ) {
        type->Ops->copy(target, source);
        return true;
    }
    xrtType = xl6_type_builtin_xrt(type);
    if ( xrtType != 0 ) { return xrtTypeCopyValue(xrtType, target, source); }
    if ( type->Size != 0 ) { memcpy(target, source, type->Size); }
    return true;
}



static inline bool xl6TypeMoveValue(
    const xl6_type_desc* type,
    void* target,
    void* source)
{
    const xrttype* xrtType;

    if ( type == 0 || (type->Size != 0 && (target == 0 || source == 0)) ) { return false; }
    if ( type->Ops != 0 && type->Ops->move != 0 ) {
        type->Ops->move(target, source);
        return true;
    }
    xrtType = xl6_type_builtin_xrt(type);
    if ( xrtType != 0 ) { return xrtTypeMoveValue(xrtType, target, source); }
    if ( type->Size != 0 ) {
        memmove(target, source, type->Size);
        memset(source, 0, type->Size);
    }
    return true;
}



static inline bool xl6TypeCloneValue(
    const xl6_type_desc* type,
    void* target,
    const void* source)
{
    const xrttype* xrtType;

    if ( type == 0 || (type->Size != 0 && (target == 0 || source == 0)) ) { return false; }
    if ( type->Ops != 0 && type->Ops->clone != 0 ) {
        type->Ops->clone(target, source);
        return true;
    }
    xrtType = xl6_type_builtin_xrt(type);
    if ( xrtType != 0 ) { return xrtTypeCloneValue(xrtType, target, source); }
    return xl6TypeCopyValue(type, target, source);
}



static inline void xl6TypeDropValue(const xl6_type_desc* type, ptr value)
{
    const xrttype* xrtType;

    if ( type == 0 || (value == 0 && type->Size != 0) ) { return; }
    if ( type->Ops != 0 && type->Ops->drop != 0 ) {
        type->Ops->drop(value);
        return;
    }
    xrtType = xl6_type_builtin_xrt(type);
    if ( xrtType != 0 ) { xrtTypeDropValue(xrtType, value); }
}



static inline int xl6TypeCompareValue(
    const xl6_type_desc* type,
    const void* left,
    const void* right)
{
    const xrttype* xrtType;
    int result = 0;

    if ( type == 0 || (type->Size != 0 && (left == 0 || right == 0)) ) { return 0; }
    if ( type->Ops != 0 && type->Ops->compare != 0 ) {
        return type->Ops->compare(left, right);
    }
    xrtType = xl6_type_builtin_xrt(type);
    if ( xrtType != 0 && xrtTypeCompareValue(xrtType, left, right, &result) ) {
        return result;
    }
    if ( type->Size != 0 ) { result = memcmp(left, right, type->Size); }
    return (result > 0) - (result < 0);
}



static inline uint64_t xl6TypeHashValue(const xl6_type_desc* type, const void* value)
{
    const xrttype* xrtType;
    uint64 hash = 0;

    if ( type == 0 || (value == 0 && type->Size != 0) ) { return 0; }
    if ( type->Ops != 0 && type->Ops->hash != 0 ) { return type->Ops->hash(value); }
    xrtType = xl6_type_builtin_xrt(type);
    if ( xrtType != 0 && xrtTypeHashValue(xrtType, value, &hash) ) { return hash; }
    return type->Size != 0 ? xrtHash64(value, type->Size) : 0;
}



#if defined(XL_RUNTIME_NEED_TYPED_CONTAINER) || \
	defined(XL_RUNTIME_NEED_TYPE_STRING_VALUE)
static inline str xl6TypeToStringValue(
    const xl6_type_desc* type,
    const void* value,
    uint32* size)
{
    const xrttype* xrtType;
    str result;
    size_t length;

    if ( size != 0 ) { *size = 0; }
    if ( type == 0 || (value == 0 && type->Size != 0) ) { return 0; }
    if ( type->Ops != 0 && type->Ops->to_string != 0 ) {
        return type->Ops->to_string(value, size);
    }
    xrtType = xl6_type_builtin_xrt(type);
    if ( xrtType == 0 ) { return 0; }
    result = xrtTypeToString(xrtType, value);
    if ( result == 0 ) { return 0; }
    length = strlen((const char*)result);
    if ( length > UINT32_MAX ) {
        xrtFree(result);
        return 0;
    }
    if ( size != 0 ) { *size = (uint32)length; }
    return result;
}
#endif



/* 返回枚举类型携带的借用反射信息。 */
static inline const xl6_enum_info* xl6EnumInfo(const xl6_type_desc* type)
{
    return type != 0 && type->Kind == XL6_XRT_TYPE_KIND_ENUM
        ? (const xl6_enum_info*)type->Extra : 0;
}



/* 按声明顺序返回借用的枚举分支。 */
static inline const xl6_enum_variant* xl6EnumVariantAt(
    const xl6_enum_info* info,
    uint32 index)
{
    return info != 0 && index < info->VariantCount
        ? &info->Variants[index] : 0;
}



/* 按名称查找借用的枚举分支，长度为零时自动读取 C 字符串长度。 */
static inline const xl6_enum_variant* xl6EnumFindVariant(
    const xl6_enum_info* info,
    const char* name,
    uint32 nameSize)
{
    size_t length;
    uint32 index;

    if ( info == 0 || name == 0 ) { return 0; }
    length = nameSize != 0 ? (size_t)nameSize : strlen(name);
    if ( length > UINT32_MAX ) { return 0; }
    for ( index = 0; index < info->VariantCount; ++index ) {
        const xl6_enum_variant* variant = &info->Variants[index];

        if ( variant->NameSize == (uint32)length &&
             (length == 0 || memcmp(variant->Name, name, length) == 0) ) {
            return variant;
        }
    }
    return 0;
}



/* 按数值查找借用的枚举分支。 */
static inline const xl6_enum_variant* xl6EnumFindValue(
    const xl6_enum_info* info,
    uint64 value)
{
    uint32 index;

    if ( info == 0 ) { return 0; }
    for ( index = 0; index < info->VariantCount; ++index ) {
        if ( info->Variants[index].Value == value ) {
            return &info->Variants[index];
        }
    }
    return 0;
}



/* 按声明顺序返回借用的分支字段。 */
static inline const xl6_enum_field* xl6EnumFieldAt(
    const xl6_enum_variant* variant,
    uint32 index)
{
    return variant != 0 && index < variant->FieldCount
        ? &variant->Fields[index] : 0;
}



/* 按名称查找借用的分支字段，长度为零时自动读取 C 字符串长度。 */
static inline const xl6_enum_field* xl6EnumFindField(
    const xl6_enum_variant* variant,
    const char* name,
    uint32 nameSize)
{
    size_t length;
    uint32 index;

    if ( variant == 0 || name == 0 ) { return 0; }
    length = nameSize != 0 ? (size_t)nameSize : strlen(name);
    if ( length > UINT32_MAX ) { return 0; }
    for ( index = 0; index < variant->FieldCount; ++index ) {
        const xl6_enum_field* field = &variant->Fields[index];

        if ( field->NameSize == (uint32)length &&
             (length == 0 || memcmp(field->Name, name, length) == 0) ) {
            return field;
        }
    }
    return 0;
}



static inline xvalue* xl6TypeBoxValue(const xl6_type_desc* type, const void* value)
{
    const xrttype* xrtType;

    if ( type == 0 || (value == 0 && type->Size != 0) ) { return 0; }
    if ( type->Ops != 0 && type->Ops->box != 0 ) { return type->Ops->box(value, type); }
    xrtType = xl6_type_builtin_xrt(type);
    return xrtType != 0 ? xrtValueFromTyped(xrtType, value, 0) : 0;
}



static inline bool xl6TypeUnboxValue(
    const xl6_type_desc* type,
    xvalue* value,
    ptr output)
{
    const xrttype* xrtType;

    if ( type == 0 || (output == 0 && type->Size != 0) ) { return false; }
    /*
        xlang 的 null 可以安全赋给 point。XRT 的 pointer 转换只处理真实
        pointer xvalue，因此在语言适配层把 null 显式还原成空指针。
        这条规则必须位于所有类型化容器共用的 unbox 入口，避免数组、列表、
        字典和动态回调结果出现彼此不一致的空值行为。
    */
    if ( xl6_xrt_value_is_null(value) &&
         type->Kind == XL6_XRT_TYPE_KIND_POINTER ) {
        memset(output, 0, type->Size);
        return true;
    }
    if ( type->Ops != 0 && type->Ops->unbox != 0 ) {
        return type->Ops->unbox(value, output, type);
    }
    xrtType = xl6_type_builtin_xrt(type);
    return xrtType != 0 && xrtValueToTyped(value, xrtType, output, 0);
}



#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
/*
    XRT Typed Container 拥有独占生命周期，xlang 在这里补充语言句柄的共享语义。
    底层容器始终由当前 XRT API 管理，引用计数和语言类型映射不会进入 XRT ABI。
*/
#define XL6_XRT_TYPE_FLAG_COPYABLE 0x00000004u
#define XL6_XRT_TYPE_FLAG_REFERENCE 0x00000008u
#define XL6_XRT_TYPE_FLAG_NULLABLE 0x00000010u
#define XL6_XRT_TYPE_FLAG_FINAL 0x00000020u
#define XL6_XRT_TYPE_FLAG_RELOCATABLE 0x00000040u
#define XL6_XRT_NATIVE_TYPE_RECORD 15
#define XL6_XRT_NATIVE_TYPE_HANDLE 16

typedef bool (*xl6_xrt_type_writer)(xstrview text, ptr context);

typedef struct xl6_xrt_type_ops_layout {
    bool (*Init)(ptr value, const xrttype* type);
    bool (*Copy)(ptr target, const void* source, const xrttype* type);
    bool (*Move)(ptr target, ptr source, const xrttype* type);
    void (*Drop)(ptr value, const xrttype* type);
    bool (*Clone)(ptr target, const void* source, const xrttype* type);
    int (*Compare)(const void* left, const void* right, const xrttype* type);
    uint64 (*Hash)(const void* value, const xrttype* type);
    bool (*Format)(const void* value, const xrttype* type,
        xl6_xrt_type_writer write, ptr context);
    ptr Trace;
} xl6_xrt_type_ops_layout;

typedef struct xl6_xrt_type_layout {
    uint64 Id;
    int Kind;
    uint32 Flags;
    xstrview Name;
    xstrview AbiName;
    size_t Size;
    size_t Align;
    size_t InstanceSize;
    size_t InstanceAlign;
    const xl6_xrt_type_ops_layout* Ops;
    const void* InstanceOps;
    const xrttype* Base;
    size_t ArgumentCount;
    const xrttype* const* Arguments;
    const void* Fields;
    const void* Methods;
    const void* Metadata;
} xl6_xrt_type_layout;

typedef struct xl6_typed_type_bridge {
    volatile int32_t RefCount;
    const xl6_type_desc* LanguageType;
    char AbiName[32];
    xl6_xrt_type_layout Type;
} xl6_typed_type_bridge;

typedef struct xl6_xvalue_converter_layout {
    ptr Context;
    bool (*ToTyped)(const xvalue* source, const xrttype* targetType,
        ptr target, ptr context);
    xvalue* (*FromTyped)(const xrttype* sourceType, const void* source,
        ptr context);
} xl6_xvalue_converter_layout;

static inline const xl6_type_desc* xl6_typed_language_type(const xrttype* type)
{
    const xl6_xrt_type_layout* layout = (const xl6_xrt_type_layout*)type;

    return layout != 0 ? (const xl6_type_desc*)layout->Metadata : 0;
}



static inline bool xl6_typed_type_init(ptr value, const xrttype* type)
{
    return xl6TypeInitValue(xl6_typed_language_type(type), value);
}



static inline bool xl6_typed_type_copy(
    ptr target,
    const void* source,
    const xrttype* type)
{
    return xl6TypeCopyValue(xl6_typed_language_type(type), target, source);
}



static inline bool xl6_typed_type_move(ptr target, ptr source, const xrttype* type)
{
    return xl6TypeMoveValue(xl6_typed_language_type(type), target, source);
}



static inline void xl6_typed_type_drop(ptr value, const xrttype* type)
{
    xl6TypeDropValue(xl6_typed_language_type(type), value);
}



static inline bool xl6_typed_type_clone(
    ptr target,
    const void* source,
    const xrttype* type)
{
    return xl6TypeCloneValue(xl6_typed_language_type(type), target, source);
}



static inline int xl6_typed_type_compare(
    const void* left,
    const void* right,
    const xrttype* type)
{
    return xl6TypeCompareValue(xl6_typed_language_type(type), left, right);
}



static inline uint64 xl6_typed_type_hash(const void* value, const xrttype* type)
{
    return xl6TypeHashValue(xl6_typed_language_type(type), value);
}



static inline bool xl6_typed_type_format(
    const void* value,
    const xrttype* type,
    xl6_xrt_type_writer write,
    ptr context)
{
    const xl6_type_desc* languageType = xl6_typed_language_type(type);
    uint32 size = 0;
    str text;
    bool result;

    if ( languageType == 0 || write == 0 ) { return false; }
    text = xl6TypeToStringValue(languageType, value, &size);
    if ( text == 0 ) { return false; }
    result = write(xrtStrViewN((const char*)text, size), context);
    xrtFree(text);
    return result;
}



static const xl6_xrt_type_ops_layout xl6_typed_type_ops = {
    xl6_typed_type_init,
    xl6_typed_type_copy,
    xl6_typed_type_move,
    xl6_typed_type_drop,
    xl6_typed_type_clone,
    xl6_typed_type_compare,
    xl6_typed_type_hash,
    xl6_typed_type_format,
    0
};



static inline bool xl6_typed_type_is_reference(const xl6_type_desc* type)
{
	return type != 0 && type->Size == sizeof(void*) &&
		(type->Kind == XL6_XRT_TYPE_KIND_CLASS ||
		 type->Kind == XL6_XRT_TYPE_KIND_PROTOCOL ||
		 type->Kind == XL6_XRT_TYPE_KIND_HANDLE ||
		 type->Kind == XL6_XRT_TYPE_KIND_FUTURE);
}



static inline bool xl6_typed_bridge_acquire(
    const xl6_type_desc* languageType,
    xl6_typed_type_bridge** bridge,
    const xrttype** xrtType)
{
    const xrttype* builtin;
    xl6_typed_type_bridge* result;
    xstrview abiName;
    bool reference;
    int abiNameSize;

    if ( bridge == 0 || xrtType == 0 || languageType == 0 ) { return false; }
    *bridge = 0;
    *xrtType = 0;
    builtin = xl6_type_builtin_xrt(languageType);
    if ( builtin != 0 ) {
        *xrtType = builtin;
        return true;
    }
    if ( languageType->Name == 0 || languageType->NameSize == 0 ||
         languageType->Size == 0 || languageType->Align == 0 ) {
        return false;
    }
    result = (xl6_typed_type_bridge*)xrtCalloc(1, sizeof(*result));
    if ( result == 0 ) { return false; }
    abiNameSize = snprintf(
        result->AbiName,
        sizeof(result->AbiName),
        "xlang.type.%08X",
        (unsigned int)languageType->TypeId);
    if ( abiNameSize <= 0 || (size_t)abiNameSize >= sizeof(result->AbiName) ) {
        xrtFree(result);
        return false;
    }
    abiName = xrtStrViewN(result->AbiName, (size_t)abiNameSize);
    reference = xl6_typed_type_is_reference(languageType);
    result->RefCount = 1;
    result->LanguageType = languageType;
    result->Type.Id = xrtTypeId(abiName);
    result->Type.Kind = reference ?
        XL6_XRT_NATIVE_TYPE_HANDLE : XL6_XRT_NATIVE_TYPE_RECORD;
    result->Type.Flags = XL6_XRT_TYPE_FLAG_COPYABLE | XL6_XRT_TYPE_FLAG_RELOCATABLE;
    if ( reference ) {
        result->Type.Flags |= XL6_XRT_TYPE_FLAG_REFERENCE |
            XL6_XRT_TYPE_FLAG_NULLABLE | XL6_XRT_TYPE_FLAG_FINAL;
    }
    result->Type.Name = xrtStrViewN(languageType->Name, languageType->NameSize);
    result->Type.AbiName = abiName;
    result->Type.Size = languageType->Size;
    result->Type.Align = languageType->Align;
    result->Type.InstanceSize = reference ? 0u : languageType->Size;
    result->Type.InstanceAlign = reference ? 1u : languageType->Align;
    result->Type.Ops = &xl6_typed_type_ops;
    result->Type.Metadata = languageType;
    if ( result->Type.Id == 0 ) {
        xrtFree(result);
        return false;
    }
    *bridge = result;
    *xrtType = (const xrttype*)&result->Type;
    return true;
}



static inline void xl6_typed_bridge_retain(xl6_typed_type_bridge* bridge)
{
    if ( bridge != 0 ) { (void)xrtRefRetain(&bridge->RefCount); }
}



static inline void xl6_typed_bridge_release(xl6_typed_type_bridge* bridge)
{
    if ( bridge != 0 && xrtRefRelease(&bridge->RefCount) == 0 ) {
        xrtFree(bridge);
    }
}



static inline bool xl6_typed_value_to(
    const xvalue* source,
    const xrttype* targetType,
    ptr target,
    ptr context)
{
    (void)context;
    return xl6TypeUnboxValue(
        xl6_typed_language_type(targetType),
        (xvalue*)source,
        target);
}



static inline xvalue* xl6_typed_value_from(
    const xrttype* sourceType,
    const void* source,
    ptr context)
{
    (void)context;
    return xl6TypeBoxValue(xl6_typed_language_type(sourceType), source);
}



static const xl6_xvalue_converter_layout xl6_typed_value_converter = {
    0,
    xl6_typed_value_to,
    xl6_typed_value_from
};



static inline const xvalueconverter* xl6_typed_converter(void)
{
    return (const xvalueconverter*)&xl6_typed_value_converter;
}



static inline struct xl6_typed_container* xl6_typed_container_alloc(
    uint32 kind,
    const xl6_type_desc* itemType)
{
    struct xl6_typed_container* result;

    result = (struct xl6_typed_container*)xrtCalloc(1, sizeof(*result));
    if ( result == 0 ) { return 0; }
    result->RefCount = 1;
    result->Kind = kind;
    result->ItemType = itemType;
    if ( !xl6_typed_bridge_acquire(itemType, &result->Bridge, &result->XrtType) ) {
        xrtFree(result);
        return 0;
    }
    return result;
}



static inline struct xl6_typed_container* xl6_typed_container_wrap(
    const struct xl6_typed_container* source,
    ptr value)
{
    struct xl6_typed_container* result;

    if ( source == 0 || value == 0 ) { return 0; }
    result = (struct xl6_typed_container*)xrtCalloc(1, sizeof(*result));
    if ( result == 0 ) { return 0; }
    result->RefCount = 1;
    result->Kind = source->Kind;
    result->ItemType = source->ItemType;
    result->XrtType = source->XrtType;
    result->Bridge = source->Bridge;
    result->Value = value;
    xl6_typed_bridge_retain(result->Bridge);
    return result;
}



static inline struct xl6_typed_container* xl6_typed_container_retain(
    struct xl6_typed_container* value,
    uint32 kind)
{
    if ( value != 0 && value->Kind == kind ) {
        (void)xrtRefRetain(&value->RefCount);
        return value;
    }
    return 0;
}



static inline void xl6_typed_container_release(
    struct xl6_typed_container* value,
    uint32 kind)
{
    if ( value == 0 || value->Kind != kind ||
         xrtRefRelease(&value->RefCount) != 0 ) {
        return;
    }
    if ( value->Value != 0 ) {
        if ( kind == XL6_TYPED_CONTAINER_ARRAY ) {
            xrtTypedArrayDestroy((xtypedarray*)value->Value);
        } else if ( kind == XL6_TYPED_CONTAINER_LIST ) {
            xrtTypedListDestroy((xtypedlist*)value->Value);
        } else if ( kind == XL6_TYPED_CONTAINER_SET ) {
            xrtTypedSetDestroy((xtypedset*)value->Value);
        } else if ( kind == XL6_TYPED_CONTAINER_DICT ) {
            xrtTypedDictDestroy((xtypeddict*)value->Value);
        }
    }
    xl6_typed_bridge_release(value->Bridge);
    xrtFree(value);
}



static inline bool xl6_typed_same(
    const struct xl6_typed_container* left,
    const struct xl6_typed_container* right,
    uint32 kind)
{
    return left != 0 && right != 0 && left->Kind == kind && right->Kind == kind &&
        xl6TypeSame(left->ItemType, right->ItemType);
}



/* Typed Array 语言句柄。 */
static inline xtarray xl6TypedArrayCreate(const xl6_type_desc* itemType)
{
    xtarray result = xl6_typed_container_alloc(XL6_TYPED_CONTAINER_ARRAY, itemType);

    if ( result == 0 ) { return 0; }
    result->Value = xrtTypedArrayCreate(result->XrtType);
    if ( result->Value == 0 ) {
        xl6_typed_container_release(result, XL6_TYPED_CONTAINER_ARRAY);
        return 0;
    }
    return result;
}



static inline xtarray xl6TypedArrayFromValue(
    xvalue* value,
    const xl6_type_desc* itemType)
{
    xtarray result = xl6_typed_container_alloc(XL6_TYPED_CONTAINER_ARRAY, itemType);

    if ( result == 0 ) { return 0; }
    result->Value = xrtTypedArrayFromValue(
        value, result->XrtType, xl6_typed_converter());
    if ( result->Value == 0 ) {
        xl6_typed_container_release(result, XL6_TYPED_CONTAINER_ARRAY);
        return 0;
    }
    return result;
}



static inline xtarray xl6TypedArrayRetain(xtarray array)
{
    return xl6_typed_container_retain(array, XL6_TYPED_CONTAINER_ARRAY);
}



static inline void xl6TypedArrayRelease(xtarray array)
{
    xl6_typed_container_release(array, XL6_TYPED_CONTAINER_ARRAY);
}



static inline xtarray xl6TypedArrayClone(xtarray array)
{
    xtypedarray* value;
    xtarray result;

    if ( array == 0 || array->Kind != XL6_TYPED_CONTAINER_ARRAY ) { return 0; }
    value = xrtTypedArrayClone((xtypedarray*)array->Value);
    if ( value == 0 ) { return 0; }
    result = xl6_typed_container_wrap(array, value);
    if ( result == 0 ) { xrtTypedArrayDestroy(value); }
    return result;
}



static inline xvalue* xl6TypedArrayToValue(xtarray array)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY ?
        xrtTypedArrayToValue(
            (const xtypedarray*)array->Value, xl6_typed_converter()) : 0;
}



static inline bool xl6TypedArrayAppend(xtarray array, const void* item)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        xrtTypedArrayPush((xtypedarray*)array->Value, item);
}



static inline bool xl6TypedArrayAppendValue(xtarray array, xvalue* value)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        xrtTypedArrayPushValue(
            (xtypedarray*)array->Value, value, xl6_typed_converter());
}



static inline bool xl6TypedArrayInsertValue(
    xtarray array,
    size_t index,
    xvalue* value)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        xrtTypedArrayInsertValue(
            (xtypedarray*)array->Value, index, value, xl6_typed_converter());
}



static inline bool xl6TypedArraySetValue(
    xtarray array,
    size_t index,
    xvalue* value)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        xrtTypedArraySetValue(
            (xtypedarray*)array->Value, index, value, xl6_typed_converter());
}



static inline xvalue* xl6TypedArrayTakeValue(xtarray array, size_t index)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY ?
        xrtTypedArrayTakeValue(
            (xtypedarray*)array->Value, index, xl6_typed_converter()) : 0;
}



static inline xvalue* xl6TypedArrayPopValue(xtarray array)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY ?
        xrtTypedArrayPopValue(
            (xtypedarray*)array->Value, xl6_typed_converter()) : 0;
}



static inline ptr xl6TypedArrayGet(xtarray array, size_t index)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY ?
        xrtTypedArrayGet((xtypedarray*)array->Value, index) : 0;
}



static inline size_t xl6TypedArrayCount(xtarray array)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY ?
        xrtTypedArrayCount((const xtypedarray*)array->Value) : 0;
}



/* 返回活动元素连续区的可写借用。 */
static inline ptr xl6TypedArrayData(xtarray array)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY ?
        xrtTypedArrayData((xtypedarray*)array->Value) : 0;
}



/* 返回活动元素连续区的只读借用。 */
static inline const void* xl6TypedArrayConstData(xtarray array)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY ?
        xrtTypedArrayConstData((const xtypedarray*)array->Value) : 0;
}



/* 返回连续存储容量，不改变数组。 */
static inline size_t xl6TypedArrayCapacity(xtarray array)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY ?
        xrtTypedArrayCapacity((const xtypedarray*)array->Value) : 0;
}



/* 预留连续存储，负容量直接失败。 */
static inline bool xl6TypedArrayReserve(xtarray array, long long capacity)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        capacity >= 0 && (uint64)capacity <= (uint64)SIZE_MAX &&
        xrtTypedArrayReserve((xtypedarray*)array->Value, (size_t)capacity);
}



/* 调整元素数量；新增槽按元素类型初始化。 */
static inline bool xl6TypedArrayResize(xtarray array, long long count)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        count >= 0 && (uint64)count <= (uint64)SIZE_MAX &&
        xrtTypedArrayResize((xtypedarray*)array->Value, (size_t)count);
}



/* 把数组容量裁剪到当前元素数量。 */
static inline bool xl6TypedArrayTrim(xtarray array)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        xrtTypedArrayTrim((xtypedarray*)array->Value);
}



static inline bool xl6TypedArrayRemove(xtarray array, size_t index, size_t count)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        xrtTypedArrayRemove((xtypedarray*)array->Value, index, count);
}



static inline bool xl6TypedArrayClear(xtarray array)
{
    if ( array == 0 || array->Kind != XL6_TYPED_CONTAINER_ARRAY ) { return false; }
    xrtTypedArrayClear((xtypedarray*)array->Value);
    return true;
}



static inline bool xl6TypedArrayReverse(xtarray array)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        xrtTypedArrayReverse((xtypedarray*)array->Value);
}



static inline bool xl6TypedArraySwap(xtarray array, long long left, long long right)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        left >= 0 && right >= 0 &&
        (uint64)left <= (uint64)SIZE_MAX &&
        (uint64)right <= (uint64)SIZE_MAX &&
        xrtTypedArraySwap(
            (xtypedarray*)array->Value, (size_t)left, (size_t)right);
}



static inline int64 xl6TypedArrayIndexOfValue(xtarray array, xvalue* value)
{
    size_t index;

    if ( array == 0 || array->Kind != XL6_TYPED_CONTAINER_ARRAY ) { return -1; }
    index = xrtTypedArrayFindValue(
        (const xtypedarray*)array->Value, value, xl6_typed_converter());
    return index == SIZE_MAX || index > (size_t)INT64_MAX ? -1 : (int64)index;
}



static inline bool xl6TypedArrayContainsValue(xtarray array, xvalue* value)
{
    return array != 0 && array->Kind == XL6_TYPED_CONTAINER_ARRAY &&
        xrtTypedArrayContainsValue(
            (const xtypedarray*)array->Value, value, xl6_typed_converter());
}



static inline bool xl6TypedArrayExtend(xtarray target, xtarray source)
{
    return xl6_typed_same(target, source, XL6_TYPED_CONTAINER_ARRAY) &&
        xrtTypedArrayAppend(
            (xtypedarray*)target->Value, (const xtypedarray*)source->Value);
}



static inline xtarray xl6TypedArrayConcat(xtarray left, xtarray right)
{
    xtypedarray* value;
    xtarray result;

    if ( !xl6_typed_same(left, right, XL6_TYPED_CONTAINER_ARRAY) ) { return 0; }
    value = xrtTypedArrayConcat(
        (const xtypedarray*)left->Value, (const xtypedarray*)right->Value);
    if ( value == 0 ) { return 0; }
    result = xl6_typed_container_wrap(left, value);
    if ( result == 0 ) { xrtTypedArrayDestroy(value); }
    return result;
}



static inline bool xl6TypedArrayEquals(xtarray left, xtarray right)
{
    return xl6_typed_same(left, right, XL6_TYPED_CONTAINER_ARRAY) &&
        xrtTypedArrayEquals(
            (const xtypedarray*)left->Value, (const xtypedarray*)right->Value);
}



/* Typed List 语言句柄。 */
static inline xtlist xl6TypedListCreate(const xl6_type_desc* itemType)
{
    xtlist result = xl6_typed_container_alloc(XL6_TYPED_CONTAINER_LIST, itemType);

    if ( result == 0 ) { return 0; }
    result->Value = xrtTypedListCreate(result->XrtType);
    if ( result->Value == 0 ) {
        xl6_typed_container_release(result, XL6_TYPED_CONTAINER_LIST);
        return 0;
    }
    return result;
}



static inline xtlist xl6TypedListFromValue(
    xvalue* value,
    const xl6_type_desc* itemType)
{
    xtlist result = xl6_typed_container_alloc(XL6_TYPED_CONTAINER_LIST, itemType);

    if ( result == 0 ) { return 0; }
    result->Value = xrtTypedListFromValue(
        value, result->XrtType, xl6_typed_converter());
    if ( result->Value == 0 ) {
        xl6_typed_container_release(result, XL6_TYPED_CONTAINER_LIST);
        return 0;
    }
    return result;
}



static inline xtlist xl6TypedListRetain(xtlist list)
{
    return xl6_typed_container_retain(list, XL6_TYPED_CONTAINER_LIST);
}



static inline void xl6TypedListRelease(xtlist list)
{
    xl6_typed_container_release(list, XL6_TYPED_CONTAINER_LIST);
}



static inline xtlist xl6TypedListClone(xtlist list)
{
    xtypedlist* value;
    xtlist result;

    if ( list == 0 || list->Kind != XL6_TYPED_CONTAINER_LIST ) { return 0; }
    value = xrtTypedListClone((const xtypedlist*)list->Value);
    if ( value == 0 ) { return 0; }
    result = xl6_typed_container_wrap(list, value);
    if ( result == 0 ) { xrtTypedListDestroy(value); }
    return result;
}



static inline xvalue* xl6TypedListToValue(xtlist list)
{
    return list != 0 && list->Kind == XL6_TYPED_CONTAINER_LIST ?
        xrtTypedListToValue(
            (const xtypedlist*)list->Value, xl6_typed_converter()) : 0;
}



static inline bool xl6TypedListSetValue(xtlist list, int64 key, xvalue* value)
{
    return list != 0 && list->Kind == XL6_TYPED_CONTAINER_LIST &&
        xrtTypedListSetValue(
            (xtypedlist*)list->Value, key, value, xl6_typed_converter());
}



static inline ptr xl6TypedListGet(xtlist list, int64 key)
{
    return list != 0 && list->Kind == XL6_TYPED_CONTAINER_LIST ?
        xrtTypedListGet((xtypedlist*)list->Value, key) : 0;
}



static inline ptr xl6TypedListItemAt(xtlist list, size_t index, int64* key)
{
    return list != 0 && list->Kind == XL6_TYPED_CONTAINER_LIST ?
        xrtTypedListAt((xtypedlist*)list->Value, index, key) : 0;
}



static inline size_t xl6TypedListCount(xtlist list)
{
    return list != 0 && list->Kind == XL6_TYPED_CONTAINER_LIST ?
        xrtTypedListCount((const xtypedlist*)list->Value) : 0;
}



/* 释放稀疏列表空节点池，并返回实际释放数量。 */
static inline long long xl6TypedListTrim(xtlist list, long long retainEmpty)
{
    if ( list == 0 || list->Kind != XL6_TYPED_CONTAINER_LIST ||
         retainEmpty < 0 || (uint64)retainEmpty > (uint64)SIZE_MAX ) {
        return -1;
    }
    return (long long)xrtTypedListTrim(
        (xtypedlist*)list->Value, (size_t)retainEmpty);
}



static inline bool xl6TypedListRemove(xtlist list, int64 key)
{
    return list != 0 && list->Kind == XL6_TYPED_CONTAINER_LIST &&
        xrtTypedListRemove((xtypedlist*)list->Value, key);
}



static inline xvalue* xl6TypedListTakeValue(xtlist list, int64 key)
{
    return list != 0 && list->Kind == XL6_TYPED_CONTAINER_LIST ?
        xrtTypedListTakeValue(
            (xtypedlist*)list->Value, key, xl6_typed_converter()) : 0;
}



static inline bool xl6TypedListClear(xtlist list)
{
    return list != 0 && list->Kind == XL6_TYPED_CONTAINER_LIST &&
        xrtTypedListClear((xtypedlist*)list->Value);
}



static inline bool xl6TypedListContainsValue(xtlist list, xvalue* value)
{
    return list != 0 && list->Kind == XL6_TYPED_CONTAINER_LIST &&
        xrtTypedListContainsValue(
            (const xtypedlist*)list->Value, value, xl6_typed_converter());
}



static inline bool xl6TypedListMerge(xtlist target, xtlist source)
{
    return xl6_typed_same(target, source, XL6_TYPED_CONTAINER_LIST) &&
        xrtTypedListMerge(
            (xtypedlist*)target->Value,
            (const xtypedlist*)source->Value,
            true);
}



static inline bool xl6TypedListEquals(xtlist left, xtlist right)
{
    return xl6_typed_same(left, right, XL6_TYPED_CONTAINER_LIST) &&
        xrtTypedListEquals(
            (const xtypedlist*)left->Value, (const xtypedlist*)right->Value);
}



/* Typed Set 语言句柄。 */
static inline xtset xl6TypedSetCreate(const xl6_type_desc* itemType)
{
    xtset result = xl6_typed_container_alloc(XL6_TYPED_CONTAINER_SET, itemType);

    if ( result == 0 ) { return 0; }
    result->Value = xrtTypedSetCreate(result->XrtType);
    if ( result->Value == 0 ) {
        xl6_typed_container_release(result, XL6_TYPED_CONTAINER_SET);
        return 0;
    }
    return result;
}



static inline xtset xl6TypedSetFromValue(
    xvalue* value,
    const xl6_type_desc* itemType)
{
    xtset result = xl6_typed_container_alloc(XL6_TYPED_CONTAINER_SET, itemType);

    if ( result == 0 ) { return 0; }
    result->Value = xrtTypedSetFromValue(
        value, result->XrtType, xl6_typed_converter());
    if ( result->Value == 0 ) {
        xl6_typed_container_release(result, XL6_TYPED_CONTAINER_SET);
        return 0;
    }
    return result;
}



static inline xtset xl6TypedSetRetain(xtset set)
{
    return xl6_typed_container_retain(set, XL6_TYPED_CONTAINER_SET);
}



static inline void xl6TypedSetRelease(xtset set)
{
    xl6_typed_container_release(set, XL6_TYPED_CONTAINER_SET);
}



static inline xtset xl6TypedSetClone(xtset set)
{
    xtypedset* value;
    xtset result;

    if ( set == 0 || set->Kind != XL6_TYPED_CONTAINER_SET ) { return 0; }
    value = xrtTypedSetClone((const xtypedset*)set->Value);
    if ( value == 0 ) { return 0; }
    result = xl6_typed_container_wrap(set, value);
    if ( result == 0 ) { xrtTypedSetDestroy(value); }
    return result;
}



static inline xvalue* xl6TypedSetToValue(xtset set)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET ?
        xrtTypedSetToValue(
            (const xtypedset*)set->Value, xl6_typed_converter()) : 0;
}



static inline bool xl6TypedSetAddValue(xtset set, xvalue* value)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET &&
        xrtTypedSetAddValue(
            (xtypedset*)set->Value, value, xl6_typed_converter());
}



static inline bool xl6TypedSetExists(xtset set, const void* item)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET &&
        xrtTypedSetHas((const xtypedset*)set->Value, item);
}



static inline bool xl6TypedSetExistsValue(xtset set, xvalue* value)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET &&
        xrtTypedSetHasValue(
            (const xtypedset*)set->Value, value, xl6_typed_converter());
}



static inline bool xl6TypedSetRemove(xtset set, const void* item)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET &&
        xrtTypedSetRemove((xtypedset*)set->Value, item);
}



static inline bool xl6TypedSetRemoveValue(xtset set, xvalue* value)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET &&
        xrtTypedSetRemoveValue(
            (xtypedset*)set->Value, value, xl6_typed_converter());
}



static inline xvalue* xl6TypedSetTakeValue(xtset set, xvalue* value)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET ?
        xrtTypedSetTakeValue(
            (xtypedset*)set->Value, value, xl6_typed_converter()) : 0;
}



static inline bool xl6TypedSetClear(xtset set)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET &&
        xrtTypedSetClear((xtypedset*)set->Value);
}



static inline size_t xl6TypedSetCount(xtset set)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET ?
        xrtTypedSetCount((const xtypedset*)set->Value) : 0;
}



/* 返回集合再次扩容前可容纳的元素数量。 */
static inline size_t xl6TypedSetCapacity(xtset set)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET ?
        xrtTypedSetCapacity((const xtypedset*)set->Value) : 0;
}



/* 预留散列表容量，负容量直接失败。 */
static inline bool xl6TypedSetReserve(xtset set, long long capacity)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET &&
        capacity >= 0 && (uint64)capacity <= (uint64)SIZE_MAX &&
        xrtTypedSetReserve((xtypedset*)set->Value, (size_t)capacity);
}



/* 裁剪集合的空闲散列表存储。 */
static inline bool xl6TypedSetTrim(xtset set)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET &&
        xrtTypedSetTrim((xtypedset*)set->Value);
}



static inline ptr xl6TypedSetItemAt(xtset set, size_t index)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET ?
        (ptr)xrtTypedSetAt((const xtypedset*)set->Value, index) : 0;
}



static inline const xl6_type_desc* xl6TypedSetItemType(xtset set)
{
    return set != 0 && set->Kind == XL6_TYPED_CONTAINER_SET ? set->ItemType : 0;
}



static inline bool xl6TypedSetMerge(xtset target, xtset source)
{
    if ( !xl6_typed_same(target, source, XL6_TYPED_CONTAINER_SET) ) { return false; }
    if ( target == source ) { return true; }
    return xrtTypedSetMerge(
        (xtypedset*)target->Value, (const xtypedset*)source->Value);
}



static inline xtset xl6_typed_set_wrap_result(xtset source, xtypedset* value)
{
    xtset result;

    if ( value == 0 ) { return 0; }
    result = xl6_typed_container_wrap(source, value);
    if ( result == 0 ) { xrtTypedSetDestroy(value); }
    return result;
}



static inline xtset xl6TypedSetUnion(xtset left, xtset right)
{
    if ( !xl6_typed_same(left, right, XL6_TYPED_CONTAINER_SET) ) { return 0; }
    return xl6_typed_set_wrap_result(
        left,
        xrtTypedSetUnion(
            (const xtypedset*)left->Value,
            (const xtypedset*)right->Value));
}



static inline xtset xl6TypedSetIntersection(xtset left, xtset right)
{
    if ( !xl6_typed_same(left, right, XL6_TYPED_CONTAINER_SET) ) { return 0; }
    return xl6_typed_set_wrap_result(
        left,
        xrtTypedSetIntersection(
            (const xtypedset*)left->Value,
            (const xtypedset*)right->Value));
}



static inline xtset xl6TypedSetDifference(xtset left, xtset right)
{
    if ( !xl6_typed_same(left, right, XL6_TYPED_CONTAINER_SET) ) { return 0; }
    return xl6_typed_set_wrap_result(
        left,
        xrtTypedSetDifference(
            (const xtypedset*)left->Value,
            (const xtypedset*)right->Value));
}



static inline xtset xl6TypedSetSymmetricDifference(xtset left, xtset right)
{
    if ( !xl6_typed_same(left, right, XL6_TYPED_CONTAINER_SET) ) { return 0; }
    return xl6_typed_set_wrap_result(
        left,
        xrtTypedSetSymmetricDifference(
            (const xtypedset*)left->Value,
            (const xtypedset*)right->Value));
}



static inline bool xl6TypedSetIsSubset(xtset left, xtset right, bool proper)
{
    return xl6_typed_same(left, right, XL6_TYPED_CONTAINER_SET) &&
        xrtTypedSetIsSubset(
            (const xtypedset*)left->Value,
            (const xtypedset*)right->Value,
            proper);
}



static inline bool xl6TypedSetIsSuperset(xtset left, xtset right, bool proper)
{
    return xl6_typed_same(left, right, XL6_TYPED_CONTAINER_SET) &&
        xrtTypedSetIsSuperset(
            (const xtypedset*)left->Value,
            (const xtypedset*)right->Value,
            proper);
}



static inline bool xl6TypedSetIsDisjoint(xtset left, xtset right)
{
    return xl6_typed_same(left, right, XL6_TYPED_CONTAINER_SET) &&
        xrtTypedSetIsDisjoint(
            (const xtypedset*)left->Value,
            (const xtypedset*)right->Value);
}



static inline bool xl6TypedSetEquals(xtset left, xtset right)
{
    return xl6_typed_same(left, right, XL6_TYPED_CONTAINER_SET) &&
        xrtTypedSetEquals(
            (const xtypedset*)left->Value,
            (const xtypedset*)right->Value);
}



/* Typed Dict 语言句柄。 */
static inline xstrview xl6_typed_dict_key(const void* key, size_t size)
{
    if ( key != 0 && size == 0 ) {
        size = strlen((const char*)key);
    }
    return xrtStrViewN((const char*)key, size);
}



static inline xtdict xl6TypedDictCreate(const xl6_type_desc* itemType)
{
    xtdict result = xl6_typed_container_alloc(XL6_TYPED_CONTAINER_DICT, itemType);

    if ( result == 0 ) { return 0; }
    result->Value = xrtTypedDictCreate(result->XrtType);
    if ( result->Value == 0 ) {
        xl6_typed_container_release(result, XL6_TYPED_CONTAINER_DICT);
        return 0;
    }
    return result;
}



static inline xtdict xl6TypedDictFromValue(
    xvalue* value,
    const xl6_type_desc* itemType)
{
    xtdict result = xl6_typed_container_alloc(XL6_TYPED_CONTAINER_DICT, itemType);

    if ( result == 0 ) { return 0; }
    result->Value = xrtTypedDictFromValue(
        value, result->XrtType, xl6_typed_converter());
    if ( result->Value == 0 ) {
        xl6_typed_container_release(result, XL6_TYPED_CONTAINER_DICT);
        return 0;
    }
    return result;
}



static inline xtdict xl6TypedDictRetain(xtdict dict)
{
    return xl6_typed_container_retain(dict, XL6_TYPED_CONTAINER_DICT);
}



static inline void xl6TypedDictRelease(xtdict dict)
{
    xl6_typed_container_release(dict, XL6_TYPED_CONTAINER_DICT);
}



static inline xtdict xl6TypedDictClone(xtdict dict)
{
    xtypeddict* value;
    xtdict result;

    if ( dict == 0 || dict->Kind != XL6_TYPED_CONTAINER_DICT ) { return 0; }
    value = xrtTypedDictClone((const xtypeddict*)dict->Value);
    if ( value == 0 ) { return 0; }
    result = xl6_typed_container_wrap(dict, value);
    if ( result == 0 ) { xrtTypedDictDestroy(value); }
    return result;
}



static inline xvalue* xl6TypedDictToValue(xtdict dict)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT ?
        xrtTypedDictToValue(
            (const xtypeddict*)dict->Value, xl6_typed_converter()) : 0;
}



static inline bool xl6TypedDictSetValue(
    xtdict dict,
    const void* key,
    size_t keySize,
    xvalue* value)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT &&
        xrtTypedDictSetValue(
            (xtypeddict*)dict->Value,
            xl6_typed_dict_key(key, keySize),
            value,
            xl6_typed_converter());
}



static inline ptr xl6TypedDictGet(xtdict dict, const void* key, size_t keySize)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT ?
        xrtTypedDictGet(
            (xtypeddict*)dict->Value, xl6_typed_dict_key(key, keySize)) : 0;
}



static inline ptr xl6TypedDictItemAt(
    xtdict dict,
    size_t index,
    const char** key,
    uint32* keySize)
{
    xstrview storedKey = { 0, 0 };
    ptr result;

    if ( key != 0 ) { *key = 0; }
    if ( keySize != 0 ) { *keySize = 0; }
    if ( dict == 0 || dict->Kind != XL6_TYPED_CONTAINER_DICT ) { return 0; }
    result = xrtTypedDictAt((xtypeddict*)dict->Value, index, &storedKey);
    if ( result == 0 || storedKey.Size > UINT32_MAX ) { return 0; }
    if ( key != 0 ) { *key = storedKey.Data; }
    if ( keySize != 0 ) { *keySize = (uint32)storedKey.Size; }
    return result;
}



static inline size_t xl6TypedDictCount(xtdict dict)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT ?
        xrtTypedDictCount((const xtypeddict*)dict->Value) : 0;
}



/* 返回字典再次扩容前可容纳的键数量。 */
static inline size_t xl6TypedDictCapacity(xtdict dict)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT ?
        xrtTypedDictCapacity((const xtypeddict*)dict->Value) : 0;
}



/* 预留字典散列表容量，负容量直接失败。 */
static inline bool xl6TypedDictReserve(xtdict dict, long long capacity)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT &&
        capacity >= 0 && (uint64)capacity <= (uint64)SIZE_MAX &&
        xrtTypedDictReserve((xtypeddict*)dict->Value, (size_t)capacity);
}



/* 裁剪字典的空闲散列表存储。 */
static inline bool xl6TypedDictTrim(xtdict dict)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT &&
        xrtTypedDictTrim((xtypeddict*)dict->Value);
}



static inline bool xl6TypedDictExists(
    xtdict dict,
    const void* key,
    size_t keySize)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT &&
        xrtTypedDictHas(
            (const xtypeddict*)dict->Value, xl6_typed_dict_key(key, keySize));
}



static inline bool xl6TypedDictRemove(
    xtdict dict,
    const void* key,
    size_t keySize)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT &&
        xrtTypedDictRemove(
            (xtypeddict*)dict->Value, xl6_typed_dict_key(key, keySize));
}



static inline xvalue* xl6TypedDictTakeValue(
    xtdict dict,
    const void* key,
    size_t keySize)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT ?
        xrtTypedDictTakeValue(
            (xtypeddict*)dict->Value,
            xl6_typed_dict_key(key, keySize),
            xl6_typed_converter()) : 0;
}



static inline bool xl6TypedDictClear(xtdict dict)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT &&
        xrtTypedDictClear((xtypeddict*)dict->Value);
}



static inline bool xl6TypedDictMerge(xtdict target, xtdict source)
{
    if ( !xl6_typed_same(target, source, XL6_TYPED_CONTAINER_DICT) ) { return false; }
    if ( target == source ) { return true; }
    return xrtTypedDictMerge(
        (xtypeddict*)target->Value,
        (const xtypeddict*)source->Value,
        true);
}



static inline bool xl6TypedDictEquals(xtdict left, xtdict right)
{
    return xl6_typed_same(left, right, XL6_TYPED_CONTAINER_DICT) &&
        xrtTypedDictEquals(
            (const xtypeddict*)left->Value,
            (const xtypeddict*)right->Value);
}



static inline const xl6_type_desc* xl6TypedDictItemType(xtdict dict)
{
    return dict != 0 && dict->Kind == XL6_TYPED_CONTAINER_DICT ? dict->ItemType : 0;
}
#endif



/*
    class 直接使用 xruntime 对象模型，record 与 native handle 继续使用轻量私有 box。
    这样 weak<T>、对象身份和后续对象图能力共享同一套底层契约，普通值不承担额外成本。
*/
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
#define XL6_RUNTIME_CLASS_PREFIX_MAGIC UINT64_C(0x584C36434C415353)

typedef struct xl6_runtime_class_prefix {
    xrtobject* Object;
    uint64 Magic;
} xl6_runtime_class_prefix;

typedef struct xl6_runtime_class_bridge {
    const xl6_type_desc* LanguageType;
    size_t DataOffset;
    xrttype Type;
} xl6_runtime_class_bridge;



/* 返回类型适配器中的语言类描述。 */
static inline const xl6_runtime_class_bridge* xl6_runtime_class_bridge_get(
    const xrttype* type);



/* 运行时对象初始化阶段只建立清零负载，语言字段初始化由装箱入口统一完成。 */
static inline bool xl6_runtime_class_instance_init(
    ptr instance,
    const xrttype* type)
{
    if ( instance == 0 || type == 0 ) { return false; }
    if ( type->InstanceSize != 0 ) {
        memset(instance, 0, type->InstanceSize);
    }
    return true;
}



/* 对象最后一个强引用离开时，按语言类型释放全部字段。 */
static inline void xl6_runtime_class_instance_drop(
    ptr instance,
    const xrttype* type)
{
    const xl6_runtime_class_bridge* bridge =
        xl6_runtime_class_bridge_get(type);

    if ( bridge != 0 && bridge->LanguageType != 0 &&
         bridge->LanguageType->Ops != 0 &&
         bridge->LanguageType->Ops->drop != 0 ) {
        bridge->LanguageType->Ops->drop(
            ((unsigned char*)instance) + bridge->DataOffset);
    }
}



static const xrtinstanceops xl6_runtime_class_instance_ops = {
    xl6_runtime_class_instance_init,
    xl6_runtime_class_instance_drop,
    0
};



/* 只接收由本适配层创建的 XRT 类描述，避免误读外部运行时对象元数据。 */
static inline const xl6_runtime_class_bridge* xl6_runtime_class_bridge_get(
    const xrttype* type)
{
    return type != 0 && type->Kind == XRT_TYPE_CLASS &&
        type->InstanceOps == &xl6_runtime_class_instance_ops
        ? (const xl6_runtime_class_bridge*)type->Metadata : 0;
}



/* 为语言类惰性建立进程期稳定的 xrttype 描述。 */
static inline const xrttype* xl6_runtime_class_type(
    const xl6_type_desc* languageType)
{
    xl6_runtime_class_bridge* bridge;
    const xrttype* baseType = 0;
    const xrttype* installed;
    const char* abiName;
    uint32 abiNameSize;
    size_t align;
    size_t prefixOffset;
    ptr expected;

    if ( languageType == 0 ||
         languageType->Kind != XL6_XRT_TYPE_KIND_CLASS ||
         languageType->Name == 0 || languageType->NameSize == 0 ) {
        return 0;
    }
    installed = (const xrttype*)xrtAtomicPtrLoad(
        &languageType->RuntimeType, XMEMORY_ACQUIRE);
    if ( installed != 0 ) { return installed; }
    align = languageType->Align;
    if ( align == 0 || (align & (align - 1)) != 0 ||
         sizeof(xl6_runtime_class_prefix) > (SIZE_MAX - (align - 1)) ) {
        return 0;
    }
    prefixOffset = (sizeof(xl6_runtime_class_prefix) + (align - 1)) &
        ~(align - 1);
    if ( languageType->Size > (SIZE_MAX - prefixOffset) ) { return 0; }
    if ( languageType->BaseType != 0 ) {
        baseType = xl6_runtime_class_type(languageType->BaseType);
        if ( baseType == 0 ) { return 0; }
    }
    bridge = (xl6_runtime_class_bridge*)xrtCalloc(1, sizeof(*bridge));
    if ( bridge == 0 ) { return 0; }
    abiName = languageType->AbiName != 0 && languageType->AbiNameSize != 0
        ? languageType->AbiName : languageType->Name;
    abiNameSize = languageType->AbiName != 0 && languageType->AbiNameSize != 0
        ? languageType->AbiNameSize : languageType->NameSize;
    bridge->LanguageType = languageType;
    bridge->DataOffset = prefixOffset;
    bridge->Type.Kind = XRT_TYPE_CLASS;
    bridge->Type.Flags = XRT_TYPE_FLAG_COPYABLE |
        XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
        XRT_TYPE_FLAG_RELOCATABLE;
    bridge->Type.Name = xrtStrViewN(languageType->Name, languageType->NameSize);
    bridge->Type.AbiName = xrtStrViewN(abiName, abiNameSize);
    bridge->Type.Id = xrtTypeId(bridge->Type.AbiName);
    bridge->Type.Size = sizeof(xrtobject*);
    bridge->Type.Align = _Alignof(xrtobject*);
    bridge->Type.InstanceSize = prefixOffset + languageType->Size;
    bridge->Type.InstanceAlign = align > _Alignof(xl6_runtime_class_prefix)
        ? align : _Alignof(xl6_runtime_class_prefix);
    bridge->Type.Ops = xrtObjectValueOps();
    bridge->Type.InstanceOps = &xl6_runtime_class_instance_ops;
    bridge->Type.Base = baseType;
    bridge->Type.Metadata = bridge;
    if ( bridge->Type.Id == 0 || !xrtTypeValidate(&bridge->Type) ) {
        xrtFree(bridge);
        return 0;
    }
    expected = 0;
    if ( !xrtAtomicPtrCompareExchange(
        (xatomicptr*)&languageType->RuntimeType,
        &expected,
        &bridge->Type,
        XMEMORY_ACQ_REL,
        XMEMORY_ACQUIRE) ) {
        xrtFree(bridge);
        return (const xrttype*)expected;
    }
    return &bridge->Type;
}



/* 用 xrtobject 承载一个类实例，并把对象强引用移交给动态 Value。 */
static inline xvalue* xl6_runtime_class_create(
    const xl6_type_desc* type,
    const void* data,
    bool move,
    bool deep)
{
    const xrttype* runtimeType = xl6_runtime_class_type(type);
    const xl6_runtime_class_bridge* bridge;
    xl6_runtime_class_prefix* prefix;
    xrtobject* object;
    ptr target;
    xvalue* value;

    if ( runtimeType == 0 ) { return 0; }
    bridge = xl6_runtime_class_bridge_get(runtimeType);
    if ( bridge == 0 ) { return 0; }
    object = xrtObjectCreate(runtimeType);
    if ( object == 0 ) { return 0; }
    target = ((unsigned char*)xrtObjectData(object)) + bridge->DataOffset;
    prefix = (xl6_runtime_class_prefix*)((unsigned char*)target -
        sizeof(*prefix));
    prefix->Object = object;
    prefix->Magic = XL6_RUNTIME_CLASS_PREFIX_MAGIC;
    if ( data != 0 ) {
        if ( move && type->Ops != 0 && type->Ops->move != 0 ) {
            type->Ops->move(target, (void*)data);
        } else if ( deep && type->Ops != 0 && type->Ops->clone != 0 ) {
            type->Ops->clone(target, data);
        } else if ( type->Ops != 0 && type->Ops->copy != 0 ) {
            type->Ops->copy(target, data);
        } else if ( type->Size != 0 ) {
            memcpy(target, data, type->Size);
        }
    } else if ( type->Ops != 0 && type->Ops->init != 0 ) {
        type->Ops->init(target);
    }
    value = xrtValueRuntimeObjectTake(&object);
    if ( value == 0 ) { xrtObjectUnref(object); }
    return value;
}
#endif



typedef struct xl6_value_handle_box {
    const xl6_type_desc* Type;
    xvalue* Owner;
    ptr Data;
    uint32 Flags;
} xl6_value_handle_box;

typedef struct xl6_value_record_box xl6_value_record_box;

/* 保持记录数据满足基础 C 类型的最大对齐，不要求宿主编译器提供 C11 对齐类型。 */
typedef union xl6_value_record_align {
    long long Integer;
    long double Floating;
    void* Pointer;
    void (*Function)(void);
} xl6_value_record_align;

typedef struct xl6_value_record_storage {
    xl6_value_record_box* Box;
    xl6_value_record_align Align;
    unsigned char Data[];
} xl6_value_record_storage;

struct xl6_value_record_box {
    const xl6_type_desc* Type;
    xvalue* Owner;
    xl6_value_record_storage* Storage;
    ptr Data;
};



static bool xl6_value_handle_clone(ptr handle, ptr* clone, ptr userData)
{
    xl6_value_handle_box* source = (xl6_value_handle_box*)handle;
    xl6_value_handle_box* target;

    (void)userData;
    if ( source == 0 || clone == 0 ) { return false; }
    target = (xl6_value_handle_box*)xrtCalloc(1, sizeof(*target));
    if ( target == 0 ) { return false; }
    target->Type = source->Type;
    target->Flags = source->Flags;
    if ( source->Data != 0 && source->Type != 0 && source->Type->Ops != 0 &&
         source->Type->Ops->copy != 0 ) {
        source->Type->Ops->copy(&target->Data, &source->Data);
        if ( target->Data == 0 ) { xrtFree(target); return false; }
        target->Flags &= ~XL6_XRT_HANDLE_FLAG_BORROWED;
        target->Flags |= XL6_XRT_HANDLE_FLAG_OWNED;
    } else if ( (source->Flags & XL6_XRT_HANDLE_FLAG_OWNED) != 0 &&
                source->Data != 0 ) {
        xrtFree(target);
        return false;
    } else {
        target->Data = source->Data;
    }
    *clone = target;
    return true;
}



static void xl6_value_handle_drop(ptr handle, ptr userData)
{
    xl6_value_handle_box* box = (xl6_value_handle_box*)handle;

    (void)userData;
    if ( box == 0 ) { return; }
    if ( (box->Flags & XL6_XRT_HANDLE_FLAG_OWNED) != 0 && box->Data != 0 &&
         box->Type != 0 && box->Type->Ops != 0 && box->Type->Ops->drop != 0 ) {
        box->Type->Ops->drop(&box->Data);
    }
    xrtFree(box);
}



static uint64 xl6_value_handle_hash(ptr handle, ptr userData)
{
    xl6_value_handle_box* box = (xl6_value_handle_box*)handle;

    (void)userData;
    if ( box == 0 ) { return 0; }
    if ( box->Type != 0 && box->Type->Ops != 0 && box->Type->Ops->hash != 0 ) {
        return box->Type->Ops->hash(&box->Data);
    }
    return xrtHash64(&box->Data, sizeof(box->Data));
}



static bool xl6_value_handle_equal(ptr left, ptr right, ptr userData)
{
    xl6_value_handle_box* a = (xl6_value_handle_box*)left;
    xl6_value_handle_box* b = (xl6_value_handle_box*)right;

    (void)userData;
    if ( a == b ) { return true; }
    if ( a == 0 || b == 0 || !xl6TypeSame(a->Type, b->Type) ) { return false; }
    if ( a->Type != 0 && a->Type->Ops != 0 && a->Type->Ops->compare != 0 ) {
        return a->Type->Ops->compare(&a->Data, &b->Data) == 0;
    }
    return a->Data == b->Data;
}



static const xvaluehandleops xl6_value_handle_ops = {
    xl6_value_handle_clone,
    xl6_value_handle_drop,
    xl6_value_handle_hash,
    xl6_value_handle_equal
};



static bool xl6_value_record_clone(ptr handle, ptr* clone, ptr userData)
{
    xl6_value_record_box* source = (xl6_value_record_box*)handle;
    xl6_value_record_box* target;

    (void)userData;
    if ( source == 0 || source->Type == 0 || clone == 0 ) { return false; }
    target = (xl6_value_record_box*)xrtCalloc(1, sizeof(*target));
    if ( target == 0 ) { return false; }
    target->Type = source->Type;
    if ( source->Type->Size != 0 ) {
        target->Storage = (xl6_value_record_storage*)xrtCalloc(
            1, sizeof(*target->Storage) + source->Type->Size);
        if ( target->Storage == 0 ) { xrtFree(target); return false; }
        target->Storage->Box = target;
        target->Data = target->Storage->Data;
        if ( source->Type->Ops != 0 && source->Type->Ops->copy != 0 ) {
            source->Type->Ops->copy(target->Data, source->Data);
        } else {
            memcpy(target->Data, source->Data, source->Type->Size);
        }
    }
    *clone = target;
    return true;
}



static void xl6_value_record_drop(ptr handle, ptr userData)
{
    xl6_value_record_box* box = (xl6_value_record_box*)handle;

    (void)userData;
    if ( box == 0 ) { return; }
    if ( box->Data != 0 && box->Type != 0 && box->Type->Ops != 0 &&
         box->Type->Ops->drop != 0 ) {
        box->Type->Ops->drop(box->Data);
    }
    xrtFree(box->Storage);
    xrtFree(box);
}



static uint64 xl6_value_record_hash(ptr handle, ptr userData)
{
    xl6_value_record_box* box = (xl6_value_record_box*)handle;

    (void)userData;
    if ( box == 0 || box->Type == 0 ) { return 0; }
    if ( box->Type->Kind == XL6_XRT_TYPE_KIND_CLASS ) {
        return xrtHash64(&box->Data, sizeof(box->Data));
    }
    return xl6TypeHashValue(box->Type, box->Data);
}



static bool xl6_value_record_equal(ptr left, ptr right, ptr userData)
{
    xl6_value_record_box* a = (xl6_value_record_box*)left;
    xl6_value_record_box* b = (xl6_value_record_box*)right;

    (void)userData;
    if ( a == b ) { return true; }
    if ( a == 0 || b == 0 || !xl6TypeSame(a->Type, b->Type) ) { return false; }
    if ( a->Type->Kind == XL6_XRT_TYPE_KIND_CLASS ) { return false; }
    return xl6TypeCompareValue(a->Type, a->Data, b->Data) == 0;
}



static const xvaluehandleops xl6_value_record_ops = {
    xl6_value_record_clone,
    xl6_value_record_drop,
    xl6_value_record_hash,
    xl6_value_record_equal
};



static inline xl6_value_handle_box* xl6_value_handle_box_get(const xvalue* value)
{
    ptr handle = 0;
    ptr userData = 0;
    const xvaluehandleops* ops = 0;

    return xrtValueGetHandle(value, &handle, &ops, &userData) &&
        ops == &xl6_value_handle_ops && userData == 0
        ? (xl6_value_handle_box*)handle : 0;
}



static inline xl6_value_record_box* xl6_value_record_box_get(const xvalue* value)
{
    ptr handle = 0;
    ptr userData = 0;
    const xvaluehandleops* ops = 0;

    return xrtValueGetHandle(value, &handle, &ops, &userData) &&
        ops == &xl6_value_record_ops && userData == 0
        ? (xl6_value_record_box*)handle : 0;
}



static inline xvalue* xl6ValueHandle(
    const xl6_type_desc* type,
    ptr handle,
    uint32 flags)
{
    xl6_value_handle_box* box;
    ptr owned;
    xvalue* value;

    if ( type == 0 || (handle == 0 &&
         (flags & XL6_XRT_HANDLE_FLAG_NULLABLE) == 0) ) { return 0; }
    box = (xl6_value_handle_box*)xrtCalloc(1, sizeof(*box));
    if ( box == 0 ) { return 0; }
    box->Type = type;
    box->Data = handle;
    box->Flags = flags;
    owned = box;
    value = xrtValueHandleTake(&owned, &xl6_value_handle_ops, 0);
    if ( value == 0 ) { xrtFree(box); return 0; }
    box->Owner = value;
    return value;
}



static inline xvalue* xl6ValueRecordCreate(
    const xl6_type_desc* type,
    const void* data,
    bool move,
    bool deep)
{
    xl6_value_record_box* box;
    ptr owned;
    xvalue* value;

    if ( type == 0 ) { return 0; }
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
    if ( type->Kind == XL6_XRT_TYPE_KIND_CLASS ) {
        return xl6_runtime_class_create(type, data, move, deep);
    }
#endif
    box = (xl6_value_record_box*)xrtCalloc(1, sizeof(*box));
    if ( box == 0 ) { return 0; }
    box->Type = type;
    if ( type->Size != 0 ) {
        box->Storage = (xl6_value_record_storage*)xrtCalloc(
            1, sizeof(*box->Storage) + type->Size);
        if ( box->Storage == 0 ) { xrtFree(box); return 0; }
        box->Storage->Box = box;
        box->Data = box->Storage->Data;
        if ( data != 0 ) {
            if ( move && type->Ops != 0 && type->Ops->move != 0 ) {
                type->Ops->move(box->Data, (void*)data);
            } else if ( deep && type->Ops != 0 && type->Ops->clone != 0 ) {
                type->Ops->clone(box->Data, data);
            } else if ( type->Ops != 0 && type->Ops->copy != 0 ) {
                type->Ops->copy(box->Data, data);
            } else {
                memcpy(box->Data, data, type->Size);
            }
        } else if ( type->Ops != 0 && type->Ops->init != 0 ) {
            type->Ops->init(box->Data);
        }
    }
    owned = box;
    value = xrtValueHandleTake(&owned, &xl6_value_record_ops, 0);
    if ( value == 0 ) { xl6_value_record_drop(box, 0); return 0; }
    box->Owner = value;
    return value;
}



static inline xvalue* xl6ValueRecord(
    const xl6_type_desc* type,
    const void* data)
{
    return xl6ValueRecordCreate(type, data, false, false);
}



static inline xvalue* xl6ValueObject(const xl6_type_desc* type)
{
    return type != 0 && type->Kind == XL6_XRT_TYPE_KIND_CLASS
        ? xl6ValueRecordCreate(type, 0, false, false) : 0;
}



static inline xvalue* xl6ValueRecordMove(
    const xl6_type_desc* type,
    void* data)
{
    return xl6ValueRecordCreate(type, data, true, false);
}



static inline xvalue* xl6ValueRecordClone(
    const xvalue* value,
    bool deep)
{
    xl6_value_record_box* box = xl6_value_record_box_get(value);
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
    const xl6_type_desc* type;
    ptr data;

    if ( xrtValueIsRuntimeObject(value) ) {
        type = xl6ValueType(value);
        data = xl6ValueRecordData((xvalue*)value);
        return type != 0 && data != 0
            ? xl6ValueRecordCreate(type, data, false, deep) : 0;
    }
#endif

    return box != 0
        ? xl6ValueRecordCreate(box->Type, box->Data, false, deep)
        : 0;
}



static inline xvalue* xl6ValueRecordCloneRelease(
    xvalue* value,
    bool deep)
{
    xvalue* result = xl6ValueRecordClone(value, deep);

    if ( value != 0 ) {
        xrtValueRelease(value);
    }
    return result;
}



static inline const xl6_type_desc* xl6ValueType(const xvalue* value)
{
    xl6_value_handle_box* handle = xl6_value_handle_box_get(value);
    xl6_value_record_box* record;
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
    xrtobject* object;
    const xl6_runtime_class_bridge* bridge;
#endif

    if ( handle != 0 ) { return handle->Type; }
    record = xl6_value_record_box_get(value);
    if ( record != 0 ) { return record->Type; }
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
    object = xrtValueGetRuntimeObject(value);
    bridge = object != 0
        ? xl6_runtime_class_bridge_get(xrtObjectType(object)) : 0;
    return bridge != 0 ? bridge->LanguageType : 0;
#else
    return 0;
#endif
}



static inline bool xl6ValueSetType(
    xvalue* value,
    const xl6_type_desc* type)
{
    xl6_value_handle_box* handle = xl6_value_handle_box_get(value);
    xl6_value_record_box* record;

    if ( type == 0 ) { return false; }
    if ( handle != 0 ) { handle->Type = type; return true; }
    record = xl6_value_record_box_get(value);
    if ( record != 0 ) { record->Type = type; return true; }
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
    if ( xrtValueIsRuntimeObject(value) ) {
        return xl6TypeSame(xl6ValueType(value), type);
    }
#endif
    return false;
}



static inline ptr xl6ValueRecordData(xvalue* value)
{
    xl6_value_record_box* box = xl6_value_record_box_get(value);
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
    xrtobject* object;
    const xl6_runtime_class_bridge* bridge;
#endif

    if ( box != 0 ) { return box->Data; }
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
    object = xrtValueGetRuntimeObject(value);
    bridge = object != 0
        ? xl6_runtime_class_bridge_get(xrtObjectType(object)) : 0;
    return bridge != 0
        ? ((unsigned char*)xrtObjectData(object)) + bridge->DataOffset : 0;
#else
    return 0;
#endif
}



static inline xvalue* xl6ValueRecordOwnerRef(const ptr data)
{
    xl6_value_record_storage* storage;
    xl6_value_record_box* box;
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
    xl6_runtime_class_prefix* prefix;
    const xl6_runtime_class_bridge* bridge;
#endif

    if ( data == 0 ) { return 0; }
#ifdef XL_RUNTIME_NEED_CLASS_HANDLE
    prefix = (xl6_runtime_class_prefix*)((unsigned char*)data -
        sizeof(*prefix));
    if ( prefix->Magic == XL6_RUNTIME_CLASS_PREFIX_MAGIC &&
         prefix->Object != 0 ) {
        bridge = xl6_runtime_class_bridge_get(
            xrtObjectType(prefix->Object));
        if ( bridge != 0 &&
             ((unsigned char*)xrtObjectData(prefix->Object)) +
                 bridge->DataOffset == data ) {
            return xrtValueRuntimeObject(prefix->Object);
        }
    }
#endif
    storage = (xl6_value_record_storage*)((unsigned char*)data -
        offsetof(xl6_value_record_storage, Data));
    box = storage->Box;
    return box != 0 && box->Storage == storage && box->Data == data
        ? xrtValueRetain(box->Owner) : 0;
}



static inline ptr xl6ValueHandleData(xvalue* value)
{
    xl6_value_handle_box* box = xl6_value_handle_box_get(value);

    return box != 0 ? box->Data : 0;
}



static inline ptr* xl6ValueHandleSlot(xvalue* value)
{
    xl6_value_handle_box* box = xl6_value_handle_box_get(value);

    return box != 0 ? &box->Data : 0;
}



static inline xvalue* xl6ValueHandleOwner(const ptr* slot)
{
    xl6_value_handle_box* box;

    if ( slot == 0 ) { return 0; }
    box = (xl6_value_handle_box*)((unsigned char*)slot -
        offsetof(xl6_value_handle_box, Data));
    return &box->Data == slot ? box->Owner : 0;
}



#ifdef XL_RUNTIME_NEED_WEAK
/* 初始化一个空弱引用。 */
static inline void xl6WeakInit(xl6_weak* weak)
{
    if ( weak != 0 ) { weak->Control = 0; }
}



/* 从语言对象建立按值返回的弱引用。 */
static inline xl6_weak xl6WeakCreate(xvalue* value)
{
    xl6_weak weak = {0};

    (void)xl6WeakSet(&weak, value);
    return weak;
}



/* 替换弱引用目标；空值表示清除当前目标。 */
static inline bool xl6WeakSet(xl6_weak* weak, xvalue* value)
{
    xrtobject* object = 0;

    if ( weak == 0 ) { return false; }
    if ( value != 0 && !xl6_xrt_value_is_null(value) ) {
        object = xrtValueGetRuntimeObject(value);
        if ( object == 0 ) { return false; }
    }
    return xrtWeakSet((xrtweak*)weak, object);
}



/* 复制弱引用，并替换目标原来持有的控制块。 */
static inline bool xl6WeakCopy(xl6_weak* target, const xl6_weak* source)
{
    return xrtWeakCopy((xrtweak*)target, (const xrtweak*)source);
}



/* 克隆弱引用并按值返回，调用方最终必须执行 xl6WeakUnit。 */
static inline xl6_weak xl6WeakClone(const xl6_weak* source)
{
    xl6_weak weak = {0};

    if ( source != 0 ) { (void)xl6WeakCopy(&weak, source); }
    return weak;
}



/* 把弱引用所有权移入目标，并清空来源。 */
static inline void xl6WeakMove(xl6_weak* target, xl6_weak* source)
{
    (void)xrtWeakMove((xrtweak*)target, (xrtweak*)source);
}



/* 接管按值返回的弱临时值，并释放目标原来持有的控制块。 */
static inline void xl6WeakTake(xl6_weak* target, xl6_weak value)
{
    xl6WeakMove(target, &value);
}



/* 释放弱引用持有的控制块引用并恢复为空值。 */
static inline void xl6WeakUnit(xl6_weak* weak)
{
    xrtWeakUnit((xrtweak*)weak);
}



/* 判断弱引用为空或目标对象已经结束生命周期。 */
static inline bool xl6WeakExpired(const xl6_weak* weak)
{
    return xrtWeakExpired((const xrtweak*)weak);
}



/* 尝试把弱引用提升为新的强 xvalue 引用。 */
static inline xvalue* xl6WeakLock(const xl6_weak* weak)
{
    xrtobject* object;
    xvalue* value;

    object = xrtWeakLock((const xrtweak*)weak);
    if ( object == 0 ) { return xrtValueNull(); }
    value = xrtValueRuntimeObjectTake(&object);
    if ( value == 0 ) {
        xrtObjectUnref(object);
        return xrtValueNull();
    }
    return value;
}



/* 按值接收弱引用并尝试提升，不接管参数内的控制块引用。 */
static inline xvalue* xl6WeakLockValue(xl6_weak weak)
{
    return xl6WeakLock(&weak);
}
#endif

static inline bool xl6_xrt_value_is_null(const xvalue* value)
{
    return (value == 0) || (xrtValueType(value) == XVALUE_NULL);
}

static inline xvalue* xl6_xrt_retain(xvalue* value)
{
    return xrtValueRetain(value);
}

/*
    静态泛型容器放入 var 后仍由原 XRT 容器保存数据。
    这里先声明动态派发入口，具体实现放在 typed container 运行时段。
*/
#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
static inline uint32 xl6_xrt_typed_container_kind(xvalue* value);
static inline const xl6_type_desc* xl6_xrt_typed_container_item_type(xvalue* value);
static inline xvalue* xl6_xrt_typed_container_box_ref(const xl6_type_desc* type, void* handle);
static inline xvalue* xl6_xrt_typed_container_snapshot(xvalue* values);
static inline uint32 xl6_xrt_typed_container_count(xvalue* values);
static inline xvalue* xl6_xrt_typed_container_get_i64(xvalue* values, int64 index);
static inline xvalue* xl6_xrt_typed_container_get_key(xvalue* values, const char* key);
static inline bool xl6_xrt_typed_container_set_i64_owned(xvalue* values, int64 index, xvalue* item);
static inline bool xl6_xrt_typed_container_set_key_owned(xvalue* values, const char* key, xvalue* item);
static inline bool xl6_xrt_typed_container_set_key_borrow(xvalue* values, const char* key, xvalue* item);
static inline int xl6_xrt_typed_container_contains(xvalue* values, xvalue* item);
static inline int xl6_xrt_typed_container_clear(xvalue* values);
static inline int xl6_xrt_typed_array_remove_value_release(xtarray values, xvalue* item);
#else
static inline uint32 xl6_xrt_typed_container_kind(xvalue* value)
{
    (void)value;
    return XL6_XRT_TYPE_KIND_INVALID;
}

static inline xvalue* xl6_xrt_typed_container_snapshot(xvalue* values)
{
    return values;
}

static inline uint32 xl6_xrt_typed_container_count(xvalue* values)
{
    (void)values;
    return 0u;
}

static inline xvalue* xl6_xrt_typed_container_get_i64(xvalue* values, int64 index)
{
    (void)values;
    (void)index;
    return xrtValueNull();
}

static inline xvalue* xl6_xrt_typed_container_get_key(xvalue* values, const char* key)
{
    (void)values;
    (void)key;
    return xrtValueNull();
}

static inline bool xl6_xrt_typed_container_set_i64_owned(xvalue* values, int64 index, xvalue* item)
{
    (void)values;
    (void)index;
    (void)item;
    return false;
}

static inline bool xl6_xrt_typed_container_set_key_owned(xvalue* values, const char* key, xvalue* item)
{
    (void)values;
    (void)key;
    (void)item;
    return false;
}

static inline bool xl6_xrt_typed_container_set_key_borrow(xvalue* values, const char* key, xvalue* item)
{
    (void)values;
    (void)key;
    (void)item;
    return false;
}

static inline int xl6_xrt_typed_container_contains(xvalue* values, xvalue* item)
{
    (void)values;
    (void)item;
    return 0;
}

static inline int xl6_xrt_typed_container_clear(xvalue* values)
{
    (void)values;
    return 0;
}

static inline int xl6_xrt_typed_array_remove_value_release(xtarray values, xvalue* item)
{
    (void)values;
    if ( item != 0 ) { xrtValueRelease(item); }
    return 0;
}
#endif

static inline xvalue* xl6_xrt_value_copy_release(xvalue* value, int deep)
{
    xvalue* result = deep ? xrtValueDeepClone(value) : xrtValueClone(value);
    if ( value != 0 ) {
        xrtValueRelease(value);
    }
    return result;
}
#endif

#ifdef XL_RUNTIME_NEED_POINT_MOVE
static inline void* xl6_point_take(void** slot)
{
    void* value;

    if ( slot == 0 ) {
        return 0;
    }
    value = *slot;
    *slot = 0;
    return value;
}
#endif

#ifdef XL_RUNTIME_NEED_LIFECYCLE
#ifndef XL_RUNTIME_INIT_HOOK
#define XL_RUNTIME_INIT_HOOK() 1
#endif
#ifndef XL_RUNTIME_UNIT_HOOK
#define XL_RUNTIME_UNIT_HOOK() ((void)0)
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

static UINT xl6_runtime_console_output_cp = 0;
static int xl6_runtime_console_output_changed = 0;

static inline int xl6_runtime_handle_is_console(DWORD stdHandle)
{
    HANDLE handle = GetStdHandle(stdHandle);
    DWORD mode = 0;
    return handle != NULL &&
           handle != INVALID_HANDLE_VALUE &&
           GetConsoleMode(handle, &mode);
}

static inline void xl6_runtime_console_begin(void)
{
    xl6_runtime_console_output_changed = 0;

    /*
     * 输入代码页属于父子进程共享状态，不能由生成程序调用 SetConsoleCP。
     * 输出使用 UTF-8 代码页；未来交互输入应走 ReadConsoleW 后转换为 UTF-8。
     */
    if ( xl6_runtime_handle_is_console(STD_OUTPUT_HANDLE) ||
         xl6_runtime_handle_is_console(STD_ERROR_HANDLE) ) {
        xl6_runtime_console_output_cp = GetConsoleOutputCP();
        if ( xl6_runtime_console_output_cp != 0 &&
             xl6_runtime_console_output_cp != CP_UTF8 &&
             SetConsoleOutputCP(CP_UTF8) ) {
            xl6_runtime_console_output_changed = 1;
        }
    }
}

static inline void xl6_runtime_console_end(void)
{
    /*
     * 宿主可能在 xlang 运行期间主动修改代码页。只有代码页仍是本运行时
     * 设置的 UTF-8 时才恢复，避免覆盖宿主的新设置。
     */
    if ( xl6_runtime_console_output_changed && GetConsoleOutputCP() == CP_UTF8 ) {
        (void)SetConsoleOutputCP(xl6_runtime_console_output_cp);
    }
    xl6_runtime_console_output_changed = 0;
}
#else
static inline void xl6_runtime_console_begin(void) { }
static inline void xl6_runtime_console_end(void) { }
#endif

#ifndef XL6_INIT_OPTIONS_DEFINED
#define XL6_INIT_OPTIONS_DEFINED
#ifndef XL_INIT_OPTIONS_VERSION
#define XL_INIT_OPTIONS_VERSION 1u
#endif
typedef struct XL_InitOptions {
    uint32 size;
    uint32 version;
    int argc;
    char** argv;
    void* reserved;
} XL_InitOptions;
#endif

static int xl__runtime_init_count = 0;

XL_InitOptions xlInitOptions(int argc, char** argv)
{
    XL_InitOptions options;
    options.size = (uint32)sizeof(options);
    options.version = XL_INIT_OPTIONS_VERSION;
    options.argc = argc;
    options.argv = argv;
    options.reserved = 0;
    return options;
}

int xlInit(const XL_InitOptions* options)
{
    if ( options != 0 ) {
        if ( options->size < (uint32)sizeof(XL_InitOptions) ) { return 0; }
        if ( options->version != XL_INIT_OPTIONS_VERSION ) { return 0; }
	}
	if ( xl__runtime_init_count == 0 ) {
		if ( !XL_RUNTIME_INIT_HOOK() ) { return 0; }
		xl6_runtime_console_begin();
	}
    xl__runtime_init_count += 1;
    return 1;
}

void xlUnit(void)
{
    if ( xl__runtime_init_count <= 0 ) { return; }
	xl__runtime_init_count -= 1;
	if ( xl__runtime_init_count == 0 ) {
		xl6_runtime_console_end();
		XL_RUNTIME_UNIT_HOOK();
	}
}

void xlDllAttach(void* module)
{
    (void)module;
}

void xlDllDetach(int processTerminating)
{
    (void)processTerminating;
}

XL6_C_API xvalue* xlValueRetain(const xvalue* value)
{
    return xrtValueRetain(value);
}

XL6_C_API void xlValueRelease(xvalue* value)
{
    xrtValueRelease(value);
}

XL6_C_API xvalue* xlValueClone(const xvalue* value, bool deep)
{
    xvalue* result = xl6ValueRecordClone(value, deep);

    return result != 0 ? result : xrtValueClone(value);
}

XL6_C_API bool xlValueIsNull(const xvalue* value)
{
    return xl6_xrt_value_is_null(value);
}

XL6_C_API void* xlValueData(xvalue* value)
{
    void* data = xl6ValueRecordData(value);

    return data != 0 ? data : xl6ValueHandleData(value);
}

#ifdef XL6_BUILD_SHARED
#if defined(_WIN32) || defined(_WIN64)
int __stdcall DllMain(void* module, unsigned long reason, void* reserved)
{
    if ( reason == 1ul ) {
        xlDllAttach(module);
    } else if ( reason == 0ul ) {
        xlDllDetach(reserved != 0);
    }
    return 1;
}
#else
static void __attribute__((constructor)) xl__shared_attach(void)
{
    xlDllAttach(0);
}
static void __attribute__((destructor)) xl__shared_detach(void)
{
    xlDllDetach(0);
}
#endif
#endif
#endif

/*
    XLang 的文本边界只接受严格 UTF-8；空指针只允许表示空字节序列。
*/
static inline bool xl6_xrt_utf8_valid(const void* value, size_t size)
{
    if ( value == 0 ) {
        return size == 0;
    }
    return xrtUtf8Valid(xrtStrViewN((const char*)value, size), 0);
}

#ifdef XL_RUNTIME_NEED_STRING
static inline char* xl6_xrt_string_dup(const char* value, size_t size)
{
    if ( value == 0 ) {
        value = "";
    }
    return size == 0 ? xrtStrDup(value) : xrtStrDupN(value, size);
}

static inline char* xl6_xrt_str_dup(const char* value)
{
    return xl6_xrt_string_dup(value, 0);
}

/*
    nullable string 仍然拥有非空字符串。
    null 必须原样保留，不能像普通 string 一样折叠为空字符串。
*/
static inline const char* xl6_nullable_string_copy(const char* value)
{
    return value != 0 ? xl6_xrt_string_dup(value, 0) : 0;
}

static inline const char* xl6_xrt_str_concat(const char* left, const char* right)
{
    size_t leftLen = left != 0 ? strlen(left) : 0;
    size_t rightLen = right != 0 ? strlen(right) : 0;
    size_t totalLen = leftLen + rightLen;
    char* text;

    /* 拼接表达式属于当前 XRT 临时区，持久化目标只在边界处复制一次。 */
    if ( totalLen == 0 ) {
        return "";
    }
    text = (char*)xrtTemp(totalLen + 1);
    if ( text == 0 ) {
        return "";
    }
    if ( leftLen > 0 ) {
        memcpy(text, left, leftLen);
    }
    if ( rightLen > 0 ) {
        memcpy(text + leftLen, right, rightLen);
    }
    text[totalLen] = 0;
    return text;
}

/*
    C 函数返回的字符串可能指向静态缓冲区。
    跨过 extern 边界时复制到线程临时内存，避免后续 C 调用改写已有 xlang 值。
*/
static inline const char* xl6_xrt_external_string_copy(const char* value)
{
    size_t len;
    char* text;

    if ( value == 0 ) {
        return "";
    }
    len = strlen(value);
    text = (char*)xrtTemp(len + 1);
    if ( text == 0 ) {
        return "";
    }
    memcpy(text, value, len + 1);
    return text;
}

static inline char* xl6_xrt_fstr_join(int count, ...)
{
    va_list ap;
    int i;
    char* result;
    xstrview* parts;
    xstrview separator = { "", 0 };
    if ( count <= 0 ) { return xrtStrDup(""); }
    parts = (xstrview*)xrtMalloc(sizeof(xstrview) * (size_t)count);
    if ( parts == 0 ) { return xrtStrDup(""); }
    va_start(ap, count);
    for ( i = 0; i < count; ++i ) {
        const char* part = va_arg(ap, char*);
        parts[i] = xrtStrView(part);
    }
    va_end(ap);
    result = xrtStrJoin(separator, parts, (size_t)count);
    xrtFree(parts);
    return result;
}
#endif

/* 输出文本并立即刷新标准输出，不接管文本所有权。 */
static inline void xl6_xrt_write(const char* text)
{
    if ( text != 0 && text[0] != '\0' ) {
        fputs(text, stdout);
    }
    fflush(stdout);
}

/* 输出文本和换行并立即刷新标准输出，不接管文本所有权。 */
static inline void xl6_xrt_writeln(const char* text)
{
    xl6_xrt_write(text);
    fputc('\n', stdout);
    fflush(stdout);
}

#ifdef XL_RUNTIME_NEED_XVALUE
static inline uint64 xl6_xrt_type_name_id(const char* name, uint32 size)
{
    uint64 hash = 1469598103934665603ULL;
    const unsigned char* prefix = (const unsigned char*)"type:";
    uint32 i;
    while ( *prefix != 0 ) {
        hash ^= (uint64)(*prefix++);
        hash *= 1099511628211ULL;
    }
    if ( name == 0 ) {
        name = "_";
        size = 1;
    }
    for ( i = 0; i < size && name[i] != 0; ++i ) {
        hash ^= (uint64)((unsigned char)name[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static inline const char* xl6_xrt_type_name_from_id(uint64 typeId)
{
#define XL6_XRT_MATCH_TYPE_NAME(name, size) if ( typeId == xl6_xrt_type_name_id(name, size) ) { return name; }
    XL6_XRT_MATCH_TYPE_NAME("null", 4)
    XL6_XRT_MATCH_TYPE_NAME("bool", 4)
    XL6_XRT_MATCH_TYPE_NAME("int", 3)
    XL6_XRT_MATCH_TYPE_NAME("int8", 4)
    XL6_XRT_MATCH_TYPE_NAME("uint8", 5)
    XL6_XRT_MATCH_TYPE_NAME("int16", 5)
    XL6_XRT_MATCH_TYPE_NAME("uint16", 6)
    XL6_XRT_MATCH_TYPE_NAME("int32", 5)
    XL6_XRT_MATCH_TYPE_NAME("uint32", 6)
    XL6_XRT_MATCH_TYPE_NAME("int64", 5)
    XL6_XRT_MATCH_TYPE_NAME("uint64", 6)
    XL6_XRT_MATCH_TYPE_NAME("float", 5)
    XL6_XRT_MATCH_TYPE_NAME("float32", 7)
    XL6_XRT_MATCH_TYPE_NAME("float64", 7)
    XL6_XRT_MATCH_TYPE_NAME("string", 6)
    XL6_XRT_MATCH_TYPE_NAME("time", 4)
    XL6_XRT_MATCH_TYPE_NAME("point", 5)
    XL6_XRT_MATCH_TYPE_NAME("func", 4)
    XL6_XRT_MATCH_TYPE_NAME("function", 8)
    XL6_XRT_MATCH_TYPE_NAME("array", 5)
    XL6_XRT_MATCH_TYPE_NAME("list", 4)
    XL6_XRT_MATCH_TYPE_NAME("set", 3)
    XL6_XRT_MATCH_TYPE_NAME("dict", 4)
    XL6_XRT_MATCH_TYPE_NAME("type", 4)
    XL6_XRT_MATCH_TYPE_NAME("object", 6)
    XL6_XRT_MATCH_TYPE_NAME("var", 3)
#undef XL6_XRT_MATCH_TYPE_NAME
    return "unknown";
}

static inline xvalue* xl6_xrt_bind_type(xvalue* value, const xl6_type_desc* desc)
{
    if ( value != 0 && desc != 0 && desc->Kind != XL6_XRT_TYPE_KIND_CLASS ) {
        (void)xl6ValueSetType(value, desc);
    }
    return value;
}

static inline uint64 xl6_xrt_type_value_id(xvalue* value, bool release)
{
    uint64 result;
    const xl6_type_desc* desc;
    xrtcallable* callable;
    int type;
    if ( value == 0 ) { return xl6_xrt_type_name_id("null", 4); }
    type = xl6_xrt_value_kind(value);
    switch ( type ) {
    case XL6_XVO_DT_NULL: result = xl6_xrt_type_name_id("null", 4); break;
    case XL6_XVO_DT_BOOL: result = xl6_xrt_type_name_id("bool", 4); break;
    case XL6_XVO_DT_INT: result = xl6_xrt_type_name_id("int", 3); break;
    case XL6_XVO_DT_FLOAT: result = xl6_xrt_type_name_id("float", 5); break;
    case XL6_XVO_DT_TEXT: result = xl6_xrt_type_name_id("string", 6); break;
    case XL6_XVO_DT_TIME: result = xl6_xrt_type_name_id("time", 4); break;
    case XL6_XVO_DT_POINT: result = xl6_xrt_type_name_id("point", 5); break;
    case XL6_XVO_DT_FUNC:
        callable = xrtValueGetCallable(value);
        result = callable != 0 ? xl6XrtCallableSignatureId(callable) : 0;
        if ( result == 0 ) { result = xl6_xrt_type_name_id("function", 8); }
        break;
    case XL6_XVO_DT_ARRAY:
    case XL6_XVO_DT_LIST:
    case XL6_XVO_DT_COLL:
    case XL6_XVO_DT_TABLE:
    case XL6_XVO_DT_CLASS:
    case XL6_XVO_DT_NATIVE:
        desc = xl6_xrt_value_type(value);
        result = (desc != 0 && desc->Name != 0 && desc->NameSize > 0) ? xl6_xrt_type_name_id(desc->Name, desc->NameSize) : xl6_xrt_type_name_id("var", 3);
        break;
    default:
        result = xl6_xrt_type_name_id("var", 3);
        break;
    }
    if ( release ) { xrtValueRelease(value); }
    return result;
}

#ifdef XL_RUNTIME_NEED_PRINT
static inline void xl6_xrt_print_write(const char* text, size_t size)
{
    if ( text == 0 ) { return; }
    if ( size == 0 ) { size = strlen(text); }
    if ( size != 0 ) {
        (void)xrtConsoleWrite(XCONSOLE_STDOUT, (xstrview){text, size});
    }
}

static inline void xl6_xrt_print_one(const xvalue* value)
{
    char buffer[128];
    size_t size = 0;
    xstrview text = {0};
    bool boolean = false;
    int64 integer = 0;
    double number = 0.0;
    ptr pointer = 0;
    str serialized;

    switch ( xrtValueType(value) ) {
    case XVALUE_NULL:
        xl6_xrt_print_write("null", 4);
        break;
    case XVALUE_BOOL:
        (void)xrtValueGetBool(value, &boolean);
        xl6_xrt_print_write(boolean ? "true" : "false", boolean ? 4 : 5);
        break;
    case XVALUE_INT:
        (void)xrtValueGetInt(value, &integer);
        if ( xrtIntFormatTo(integer, (xstrview){"", 0}, buffer, sizeof(buffer), &size) ) {
            xl6_xrt_print_write(buffer, size);
        }
        break;
    case XVALUE_FLOAT:
        (void)xrtValueGetFloat(value, &number);
        if ( xrtNumFormatTo(number, (xstrview){"", 0}, buffer, sizeof(buffer), &size) ) {
            xl6_xrt_print_write(buffer, size);
        }
        break;
    case XVALUE_STRING:
        if ( xrtValueGetString(value, &text) ) {
            xl6_xrt_print_write(text.Data, text.Size);
        }
        break;
    case XVALUE_POINTER:
        (void)xrtValueGetPointer(value, &pointer);
        size = (size_t)snprintf(buffer, sizeof(buffer), "%p", pointer);
        if ( size < sizeof(buffer) ) { xl6_xrt_print_write(buffer, size); }
        break;
    default:
        serialized = xrtXlonStringify(value, false, &size);
        if ( serialized != 0 ) {
            xl6_xrt_print_write((const char*)serialized, size);
            xrtFree(serialized);
        }
        break;
    }
}

void xl6_xrt_print(xvalue* values, bool newline)
{
    size_t count;
    size_t i;

    fflush(stdout);
    if ( xrtValueType(values) == XVALUE_ARRAY ) {
        count = xrtValueCount(values);
        for ( i = 0; i < count; ++i ) {
            if ( i != 0 ) { xl6_xrt_print_write(" ", 1); }
            xl6_xrt_print_one(xrtValueArrayGet(values, i));
        }
    } else {
        xl6_xrt_print_one(values);
    }
    if ( newline ) { xl6_xrt_print_write("\n", 1); }
    fflush(stdout);
}
#endif

/*
    按 xlang 容器语义返回借用值：数组允许负索引，整数表保留完整 int64 键。
    缺失项统一返回 XRT 的静态 null，调用方不需要处理空指针。
*/
static inline xvalue* xl6_xrt_index_get_i64(xvalue* values, int64 index)
{
    xvalue* result = 0;
    xvaluetype type = xrtValueType(values);

    if ( type == XVALUE_ARRAY ) {
        result = xrtValueArrayAt(values, index);
    } else if ( type == XVALUE_INT_MAP ) {
        result = xrtValueIntMapGet(values, index);
    }
    return result != 0 ? result : xrtValueNull();
}



/* 按字符串键返回对象中的借用值，缺失项统一返回静态 null。 */
static inline xvalue* xl6_xrt_index_get_key(xvalue* values, const char* key)
{
    xvalue* result;

    if ( xrtValueType(values) != XVALUE_OBJECT || key == 0 ) {
        return xrtValueNull();
    }
    result = xrtValueObjectGet(values, xrtStrView(key));
    return result != 0 ? result : xrtValueNull();
}



/* 按动态 Value 键返回借用值，字符串走对象键，其余值按整数键解释。 */
static inline xvalue* xl6_xrt_index_get_value(xvalue* values, xvalue* key)
{
    if ( key != 0 && xl6_xrt_value_kind(key) == XL6_XVO_DT_TEXT ) {
        return xl6_xrt_index_get_key(values, xl6_xrt_value_text(key));
    }
    return xl6_xrt_index_get_i64(
        values,
        key != 0 ? xl6_xrt_value_int(key) : 0
    );
}



/* 将语言数组的有符号索引解析为 XRT 数组下标，避免最小负整数取反溢出。 */
static inline bool xl6_xrt_array_index(
    const xvalue* values,
    int64 index,
    size_t* result)
{
    size_t count;
    uint64 distance;

    if ( result == 0 || xrtValueType(values) != XVALUE_ARRAY ) {
        return false;
    }
    count = xrtValueCount(values);
    if ( index < 0 ) {
        distance = (uint64)(-(index + 1)) + 1u;
        if ( distance > (uint64)count ) {
            return false;
        }
        *result = count - (size_t)distance;
        return true;
    }
    if ( (uint64)index >= (uint64)count ) {
        return false;
    }
    *result = (size_t)index;
    return true;
}



/* 沿 XRT COW 路径返回可变子容器；数组和整数表保持各自的索引语义。 */
static inline xvalue* xl6_xrt_index_edit_i64(xvalue* values, int64 index)
{
    size_t resolved;
    xvaluetype type = xrtValueType(values);

    if ( type == XVALUE_ARRAY ) {
        return xl6_xrt_array_index(values, index, &resolved)
            ? xrtValueArrayEdit(values, resolved)
            : 0;
    }
    if ( type == XVALUE_INT_MAP ) {
        return xrtValueIntMapEdit(values, index);
    }
    return 0;
}



/* 使用并消费动态键，字符串编辑对象字段，其余键按整数索引解释。 */
static inline xvalue* xl6_xrt_index_edit_value_take(
    xvalue* values,
    xvalue* key)
{
    xvalue* result;

    if ( key != 0 && xl6_xrt_value_kind(key) == XL6_XVO_DT_TEXT ) {
        result = xl6_xrt_value_object_edit(
            values,
            xl6_xrt_value_text(key),
            xl6_xrt_value_text_size(key)
        );
    } else {
        result = xl6_xrt_index_edit_i64(
            values,
            key != 0 ? xl6_xrt_value_int(key) : 0
        );
    }
    if ( key != 0 ) {
        xrtValueRelease(key);
    }
    return result;
}



/* 成功时消费待写值，失败时保持调用方引用，供统一失败路径释放。 */
static inline bool xl6_xrt_index_set_i64_owned(
    xvalue* values,
    int64 index,
    xvalue* item)
{
    size_t resolved;
    xvaluetype type;

    if ( values == 0 || item == 0 ) {
        return false;
    }
    type = xrtValueType(values);
    if ( type == XVALUE_ARRAY ) {
        return xl6_xrt_array_index(values, index, &resolved) &&
               xrtValueArraySetTake(values, resolved, &item);
    }
    if ( type == XVALUE_INT_MAP ) {
        return xrtValueIntMapSetTake(values, index, &item);
    }
    return false;
}



static inline xvalue* xl6_xrt_index_safe_get_i64(xvalue* values, int64 index)
{
    if ( xl6_xrt_typed_container_kind(values) != XL6_XRT_TYPE_KIND_INVALID ) {
        return xl6_xrt_typed_container_get_i64(values, index);
    }
    return xl6_xrt_retain(xl6_xrt_index_get_i64(values, index));
}

static inline xvalue* xl6_xrt_index_safe_get_key(xvalue* values, const char* key)
{
    if ( xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_DICT ) {
        return xl6_xrt_typed_container_get_key(values, key);
    }
    return xl6_xrt_retain(xl6_xrt_index_get_key(values, key));
}

static inline xvalue* xl6_xrt_dict_get_var_key(xvalue* values, xvalue* key)
{
    xvalue* result = xl6_xrt_index_get_value(values, key);
    if ( key != 0 ) { xrtValueRelease(key); }
    return result;
}

static inline xvalue* xl6_xrt_index_safe_get_var_key(xvalue* values, xvalue* key)
{
    xvalue* result;
    if ( key != 0 && xl6_xrt_value_kind(key) == XL6_XVO_DT_TEXT ) {
        result = xl6_xrt_index_safe_get_key(values, xl6_xrt_value_text(key));
    } else {
        result = xl6_xrt_index_safe_get_i64(values, key != 0 ? xl6_xrt_value_int(key) : 0);
    }
    if ( key != 0 ) { xrtValueRelease(key); }
    return result;
}

#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
static inline void* xl6_xrt_typed_dict_get_var_key(xtdict values, xvalue* key)
{
    void* result = 0;
    if ( key != 0 && xl6_xrt_value_kind(key) == XL6_XVO_DT_TEXT ) {
        result = xl6TypedDictGet(values, xl6_xrt_value_text(key), 0);
    }
    if ( key != 0 ) { xrtValueRelease(key); }
    return result;
}
#endif

static inline xvalue* xl6_xrt_null_release(xvalue* values)
{
    if ( values != 0 ) { xrtValueRelease(values); }
    return xrtValueNull();
}

static inline xvalue* xl6_xrt_array_index_retain_release(xvalue* values, int64 index)
{
    xvalue* result = xl6_xrt_index_safe_get_i64(values, index);
    if ( values != 0 ) { xrtValueRelease(values); }
    return result;
}

static inline xvalue* xl6_xrt_list_index_retain_release(xvalue* values, int64 index)
{
    xvalue* result = xl6_xrt_index_safe_get_i64(values, index);
    if ( values != 0 ) { xrtValueRelease(values); }
    return result;
}

static inline xvalue* xl6_xrt_dict_get_key_retain_release(xvalue* values, const char* key)
{
    xvalue* result = xl6_xrt_index_safe_get_key(values, key);
    if ( values != 0 ) { xrtValueRelease(values); }
    return result;
}

static inline xvalue* xl6_xrt_dict_get_var_key_retain_release(xvalue* values, xvalue* key)
{
    xvalue* result = xl6_xrt_retain(xl6_xrt_index_get_value(values, key));
    if ( key != 0 ) { xrtValueRelease(key); }
    if ( values != 0 ) { xrtValueRelease(values); }
    return result;
}

static inline xvalue* xl6_xrt_index_safe_get_i64_retain_release(xvalue* values, int64 index)
{
    xvalue* result = xl6_xrt_index_safe_get_i64(values, index);
    if ( values != 0 ) { xrtValueRelease(values); }
    return result;
}

static inline xvalue* xl6_xrt_index_safe_get_key_retain_release(xvalue* values, const char* key)
{
    xvalue* result = xl6_xrt_index_safe_get_key(values, key);
    if ( values != 0 ) { xrtValueRelease(values); }
    return result;
}

static inline xvalue* xl6_xrt_index_safe_get_var_key_retain_release(xvalue* values, xvalue* key)
{
    xvalue* result = xl6_xrt_index_safe_get_var_key(values, key);
    if ( values != 0 ) { xrtValueRelease(values); }
    return result;
}

static inline uint32 xl6_xrt_member_length(xvalue* values)
{
    int type;
    size_t count;

    if ( values == 0 || xl6_xrt_value_is_null(values) ) { return 0; }
    type = xl6_xrt_value_kind(values);
    if ( xl6_xrt_typed_container_kind(values) != XL6_XRT_TYPE_KIND_INVALID ) {
        return xl6_xrt_typed_container_count(values);
    }
    if ( type != XL6_XVO_DT_ARRAY && type != XL6_XVO_DT_LIST &&
         type != XL6_XVO_DT_COLL && type != XL6_XVO_DT_TABLE ) { return 0; }
    count = xrtValueCount(values);
    return count > UINT32_MAX ? UINT32_MAX : (uint32)count;
}

static inline xvalue* xl6_xrt_member_safe_length(xvalue* values)
{
    if ( values == 0 || xl6_xrt_value_is_null(values) ) { return xrtValueNull(); }
    return xrtValueInt((int64)xl6_xrt_member_length(values));
}

static inline uint32 xl6_xrt_member_length_release(xvalue* values)
{
    uint32 result = xl6_xrt_member_length(values);
    if ( values != 0 ) { xrtValueRelease(values); }
    return result;
}

static inline xvalue* xl6_xrt_member_safe_length_release(xvalue* values)
{
    xvalue* result = xl6_xrt_member_safe_length(values);
    if ( values != 0 ) { xrtValueRelease(values); }
    return result;
}

static inline int xl6_xrt_type_is_numeric(int type)
{
    return type == XL6_XVO_DT_BOOL || type == XL6_XVO_DT_INT || type == XL6_XVO_DT_FLOAT;
}

static inline double xl6_xrt_value_to_double(xvalue* value, int type)
{
    return type == XL6_XVO_DT_FLOAT ? xl6_xrt_value_float(value) : (double)xl6_xrt_value_int(value);
}

static inline int xl6_xrt_basic_compare(xvalue* left, xvalue* right, int* outResult)
{
    int leftType;
    int rightType;
    double leftNumber;
    double rightNumber;

    if ( outResult == 0 ) { return 0; }
    leftType = xl6_xrt_value_kind(left);
    rightType = xl6_xrt_value_kind(right);
    if ( leftType == XL6_XVO_DT_NULL && rightType == XL6_XVO_DT_NULL ) {
        *outResult = 0;
        return 1;
    }
    if ( xl6_xrt_type_is_numeric(leftType) &&
         xl6_xrt_type_is_numeric(rightType) ) {
        leftNumber = xl6_xrt_value_to_double(left, leftType);
        rightNumber = xl6_xrt_value_to_double(right, rightType);
        *outResult = leftNumber < rightNumber ? -1 : (leftNumber > rightNumber ? 1 : 0);
        return 1;
    }
    if ( leftType != rightType ) {
        *outResult = 0;
        return 0;
    }
    if ( leftType == XL6_XVO_DT_TEXT ) {
        int compare = xrtStrCompare(
            xrtStrView(xl6_xrt_value_text(left)),
            xrtStrView(xl6_xrt_value_text(right))
        );
        *outResult = compare < 0 ? -1 : (compare > 0 ? 1 : 0);
        return 1;
    }
    if ( leftType == XL6_XVO_DT_TIME ) {
        xtime leftTime = xl6_xrt_value_time(left);
        xtime rightTime = xl6_xrt_value_time(right);
        *outResult = leftTime < rightTime ? -1 : (leftTime > rightTime ? 1 : 0);
        return 1;
    }
    *outResult = 0;
    return 0;
}

static inline int xl6_xrt_basic_equal(xvalue* left, xvalue* right)
{
	int leftNull;
	int rightNull;
	uint32 leftKind;
	uint32 rightKind;

	/* 可空值允许轻量空指针表示，语言比较时与显式 null 哨兵等价。 */
	leftNull = (left == 0) || xl6_xrt_value_is_null(left);
	rightNull = (right == 0) || xl6_xrt_value_is_null(right);
	if ( leftNull || rightNull ) {
		return leftNull && rightNull;
	}
	leftKind = xl6_xrt_typed_container_kind(left);
	rightKind = xl6_xrt_typed_container_kind(right);
    if ( leftKind != XL6_XRT_TYPE_KIND_INVALID || rightKind != XL6_XRT_TYPE_KIND_INVALID ) {
        return leftKind == rightKind &&
               xl6TypeSame(xl6ValueType(left), xl6ValueType(right)) &&
               xl6ValueHandleData(left) == xl6ValueHandleData(right);
    }
    return xrtValueEqual(left, right) ? 1 : (left == right);
}

static inline int xl6_xrt_basic_approx(xvalue* left, xvalue* right)
{
    int leftType = xl6_xrt_value_kind(left);
    int rightType = xl6_xrt_value_kind(right);
    if ( leftType == XL6_XVO_DT_TEXT || rightType == XL6_XVO_DT_TEXT ) {
        return leftType == XL6_XVO_DT_TEXT && rightType == XL6_XVO_DT_TEXT &&
               xrtUtf8Similarity(xrtStrView(xl6_xrt_value_text(left)),
                   xrtStrView(xl6_xrt_value_text(right))) >= 0.95;
    }
    if ( leftType == XL6_XVO_DT_TIME || rightType == XL6_XVO_DT_TIME ) {
        return leftType == XL6_XVO_DT_TIME && rightType == XL6_XVO_DT_TIME &&
               xl6_xrt_time_approx(
                   xl6_xrt_value_time(left),
                   xl6_xrt_value_time(right));
    }
    if ( xl6_xrt_type_is_numeric(leftType) && xl6_xrt_type_is_numeric(rightType) ) {
        if ( leftType == XL6_XVO_DT_FLOAT || rightType == XL6_XVO_DT_FLOAT ) {
            return xl6_xrt_num_approx(
                xl6_xrt_value_to_double(left, leftType),
                xl6_xrt_value_to_double(right, rightType));
        }
        return xl6_xrt_int_approx(
            xl6_xrt_value_int(left),
            xl6_xrt_value_int(right));
    }
    return 0;
}

static inline int xl6_xrt_basic_order(xvalue* left, xvalue* right, int op)
{
    int cmp = 0;
    if ( !xl6_xrt_basic_compare(left, right, &cmp) ) { return 0; }
    if ( op == 0 ) { return cmp < 0; }
    if ( op == 1 ) { return cmp <= 0; }
    if ( op == 2 ) { return cmp > 0; }
    return cmp >= 0;
}

/* 比较表达式接收拥有型参数，并在计算结束后统一归还所有权。 */
static inline int xl6_xrt_basic_equal_release(xvalue* left, xvalue* right)
{
    int result = xl6_xrt_basic_equal(left, right);
    if ( left != 0 ) { xrtValueRelease(left); }
    if ( right != 0 ) { xrtValueRelease(right); }
    return result;
}

static inline int xl6_xrt_basic_approx_release(xvalue* left, xvalue* right)
{
    int result = xl6_xrt_basic_approx(left, right);
    if ( left != 0 ) { xrtValueRelease(left); }
    if ( right != 0 ) { xrtValueRelease(right); }
    return result;
}

static inline int xl6_xrt_basic_order_release(xvalue* left, xvalue* right, int op)
{
    int result = xl6_xrt_basic_order(left, right, op);
    if ( left != 0 ) { xrtValueRelease(left); }
    if ( right != 0 ) { xrtValueRelease(right); }
    return result;
}

#ifdef XL_RUNTIME_NEED_XVALUE_BINARY
static inline xvalue* xl6_xrt_value_text_concat(xvalue* left, xvalue* right)
{
    return xl6_xrt_value_text_create(
        xrtStrConcat(xrtStrView(xl6_xrt_value_text(left)),
            xrtStrView(xl6_xrt_value_text(right))),
        0,
        true);
}

static inline xvalue* xl6_xrt_array_concat(xvalue* left, xvalue* right)
{
    xvalue* result = xrtValueArray();
    size_t leftCount;
    size_t rightCount;
    size_t i;

    if ( result == 0 ) {
        return xrtValueNull();
    }
    leftCount = xrtValueCount(left);
    for ( i = 0; i < leftCount; ++i ) {
        if ( !xrtValueArrayAppend(result, xrtValueArrayGet(left, i)) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    rightCount = xrtValueCount(right);
    for ( i = 0; i < rightCount; ++i ) {
        if ( !xrtValueArrayAppend(result, xrtValueArrayGet(right, i)) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline xvalue* xl6_xrt_basic_binary(xvalue* left, xvalue* right, int op)
{
    int leftType = xl6_xrt_value_kind(left);
    int rightType = xl6_xrt_value_kind(right);
    if ( op == 0 && leftType == XL6_XVO_DT_TEXT && rightType == XL6_XVO_DT_TEXT ) {
        return xl6_xrt_value_text_concat(left, right);
    }
    if ( op == 0 && leftType == XL6_XVO_DT_ARRAY && rightType == XL6_XVO_DT_ARRAY ) {
        return xl6_xrt_array_concat(left, right);
    }
    if ( !xl6_xrt_type_is_numeric(leftType) || !xl6_xrt_type_is_numeric(rightType) ) { return xrtValueNull(); }
    if ( op >= 5 ) {
        int64 l;
        int64 r;
        if ( leftType == XL6_XVO_DT_FLOAT || rightType == XL6_XVO_DT_FLOAT ) { return xrtValueNull(); }
        l = xl6_xrt_value_int(left);
        r = xl6_xrt_value_int(right);
        if ( op == 5 ) { return xrtValueInt(l & r); }
        if ( op == 6 ) { return xrtValueInt(l | r); }
        if ( op == 7 ) { return xrtValueInt(l ^ r); }
        if ( r < 0 || r >= 64 ) { return xrtValueNull(); }
        if ( op == 8 ) { return xrtValueInt((int64)((uint64)l << (uint32)r)); }
        return xrtValueInt((int64)((uint64)l >> (uint32)r));
    }
    if ( leftType == XL6_XVO_DT_FLOAT || rightType == XL6_XVO_DT_FLOAT ) {
        double l = xl6_xrt_value_to_double(left, leftType);
        double r = xl6_xrt_value_to_double(right, rightType);
        if ( op == 0 ) { return xrtValueFloat(l + r); }
        if ( op == 1 ) { return xrtValueFloat(l - r); }
        if ( op == 2 ) { return xrtValueFloat(l * r); }
        if ( op == 3 ) { return r != 0.0 ? xrtValueFloat(l / r) : xrtValueNull(); }
        return xrtValueNull();
    }
    {
        int64 l = xl6_xrt_value_int(left);
        int64 r = xl6_xrt_value_int(right);
        if ( op == 0 ) { return xrtValueInt(l + r); }
        if ( op == 1 ) { return xrtValueInt(l - r); }
        if ( op == 2 ) { return xrtValueInt(l * r); }
        if ( op == 3 ) { return r != 0 ? xrtValueInt(l / r) : xrtValueNull(); }
        if ( op == 4 ) { return r != 0 ? xrtValueInt(l % r) : xrtValueNull(); }
    }
    return xrtValueNull();
}
#endif

static inline int xl6_xrt_value_truthy(xvalue* value)
{
    if ( xl6_xrt_typed_container_kind(value) != XL6_XRT_TYPE_KIND_INVALID ) {
        return xl6_xrt_typed_container_count(value) != 0u;
    }
    return xrtValueTruthy(value) ? 1 : 0;
}

static inline xvalue* xl6_xrt_basic_unary(xvalue* value, int op)
{
    int type = xl6_xrt_value_kind(value);
    if ( !xl6_xrt_type_is_numeric(type) ) { return xrtValueNull(); }
    if ( type == XL6_XVO_DT_FLOAT ) {
        double v = xl6_xrt_value_float(value);
        if ( op == 0 ) { return xrtValueFloat(v); }
        if ( op == 1 ) { return xrtValueFloat(-v); }
        return xrtValueNull();
    }
    {
        int64 v = xl6_xrt_value_int(value);
        if ( op == 0 ) { return xrtValueInt(v); }
        if ( op == 1 ) { return xrtValueInt(-v); }
        return xrtValueInt(~v);
    }
}

#ifndef XL6_XRT_VALUE_EXPR_RELEASE_HELPERS
#define XL6_XRT_VALUE_EXPR_RELEASE_HELPERS
static inline int xl6_xrt_value_truthy_release(xvalue* value)
{
    int result = value != 0 ? xl6_xrt_value_truthy(value) : 0;
    if ( value != 0 ) { xrtValueRelease(value); }
    return result;
}

static inline xvalue* xl6_xrt_basic_unary_release(xvalue* value, int op)
{
    xvalue* result = value != 0 ? xl6_xrt_basic_unary(value, op) : xrtValueNull();
    if ( value != 0 ) { xrtValueRelease(value); }
    return result;
}

#ifdef XL_RUNTIME_NEED_XVALUE_BINARY
static inline xvalue* xl6_xrt_basic_binary_release(xvalue* left, xvalue* right, int op)
{
    xvalue* result = (left != 0 && right != 0) ? xl6_xrt_basic_binary(left, right, op) : xrtValueNull();
    if ( left != 0 ) { xrtValueRelease(left); }
    if ( right != 0 ) { xrtValueRelease(right); }
    return result;
}
#endif
#endif

static inline int xl6_xrt_value_contains(xvalue* values, xvalue* item)
{
    int found = 0;
    int type;

    if ( xl6_xrt_typed_container_kind(values) != XL6_XRT_TYPE_KIND_INVALID ) {
        found = xl6_xrt_typed_container_contains(values, item);
    } else {
        type = xl6_xrt_value_kind(values);
        if ( type == XL6_XVO_DT_COLL ) {
            found = xrtValueSetHas(values, item) ? 1 : 0;
        } else if ( type == XL6_XVO_DT_TABLE &&
                    xl6_xrt_value_kind(item) == XL6_XVO_DT_TEXT ) {
            found = xrtValueObjectHas(
                values,
                xrtStrView(xl6_xrt_value_text(item))
            ) ? 1 : 0;
        } else if ( type == XL6_XVO_DT_TEXT &&
                    xl6_xrt_value_kind(item) == XL6_XVO_DT_TEXT ) {
            found = xrtStrContains(
                xrtStrView(xl6_xrt_value_text(values)),
                xrtStrView(xl6_xrt_value_text(item))
            ) ? 1 : 0;
        } else if ( type == XL6_XVO_DT_ARRAY ||
                    type == XL6_XVO_DT_LIST ) {
            xvalueiter* iterator = xrtValueIterCreate(values);
            xvalue* current;

            if ( iterator != 0 ) {
                while ( (current = xrtValueIterNext(iterator, 0)) != 0 ) {
                    if ( xrtValueEqual(current, item) ) {
                        found = 1;
                        break;
                    }
                }
                xrtValueIterDestroy(iterator);
            }
        }
    }
    if ( item != 0 ) { xrtValueRelease(item); }
    return found;
}

static inline int xl6_xrt_value_contains_release(xvalue* values, xvalue* item)
{
    int found = xl6_xrt_value_contains(values, item);
    if ( values != 0 ) { xrtValueRelease(values); }
    return found;
}

static inline int xl6_xrt_array_contains(xvalue* values, xvalue* item) { return xl6_xrt_value_contains(values, item); }
static inline int xl6_xrt_list_contains(xvalue* values, xvalue* item) { return xl6_xrt_value_contains(values, item); }
static inline int xl6_xrt_set_contains(xvalue* values, xvalue* item) { return xl6_xrt_value_contains(values, item); }
static inline int xl6_xrt_array_contains_release(xvalue* values, xvalue* item) { return xl6_xrt_value_contains_release(values, item); }
static inline int xl6_xrt_list_contains_release(xvalue* values, xvalue* item) { return xl6_xrt_value_contains_release(values, item); }
static inline int xl6_xrt_set_contains_release(xvalue* values, xvalue* item) { return xl6_xrt_value_contains_release(values, item); }

static inline int64 xl6_xrt_array_index_of(xvalue* values, xvalue* item)
{
    uint32 count = 0;
    uint32 i;
    int64 result = -1;

#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
    if ( xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_ARRAY ) {
        result = xl6TypedArrayIndexOfValue((xtarray)xl6ValueHandleData(values), item);
    } else
#endif
    if ( values != 0 && xl6_xrt_value_kind(values) == XL6_XVO_DT_ARRAY ) {
        count = xl6_xrt_member_length(values);
        for ( i = 0; i < count; ++i ) {
            if ( xl6_xrt_basic_equal(xrtValueArrayGet(values, i), item) ) {
                result = (int64)i;
                break;
            }
        }
    }
    if ( item != 0 ) { xrtValueRelease(item); }
    return result;
}

static inline int xl6_xrt_array_remove_value_release(xvalue* values, xvalue* item)
{
    uint32 count;
    uint32 i;
    int ok;

    count = xl6_xrt_member_length(values);
    ok = 0;
    for ( i = 0; i < count; ++i ) {
        if ( xl6_xrt_basic_equal(xrtValueArrayGet(values, i), item) ) {
            ok = xrtValueArrayRemove(values, i, 1) ? 1 : 0;
            break;
        }
    }
    if ( item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline int xl6_xrt_value_remove_value_release(xvalue* values, xvalue* item)
{
    int ok = 0;
    if ( xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_ARRAY ) {
        ok = xl6_xrt_typed_array_remove_value_release((xtarray)xl6ValueHandleData(values), item);
    } else if ( values != 0 && xl6_xrt_value_kind(values) == XL6_XVO_DT_ARRAY ) {
        ok = xl6_xrt_array_remove_value_release(values, item);
    } else if ( item != 0 ) {
        xrtValueRelease(item);
    }
    return ok;
}

static inline int xl6_xrt_table_exists_release(xvalue* values, const char* key)
{
    int found = xrtValueObjectHas(values, xrtStrView(key != 0 ? key : "")) ? 1 : 0;
    if ( values != 0 ) { xrtValueRelease(values); }
    return found;
}

static inline int xl6_xrt_array_push_value(xvalue* values, xvalue* item)
{
    return xrtValueArrayAppendNew(values, item) ? 1 : 0;
}

static inline int xl6_xrt_array_insert_value(xvalue* values, uint32 index, xvalue* item)
{
    return xrtValueArrayInsertNew(values, index, item) ? 1 : 0;
}

static inline int xl6_xrt_set_add_value(xvalue* values, xvalue* item)
{
    return xrtValueSetAddNew(values, item) ? 1 : 0;
}

static inline int xl6_xrt_set_remove_value_release(xvalue* values, xvalue* item)
{
    int ok = xrtValueSetRemove(values, item) ? 1 : 0;
    if ( item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline xvalue* xl6_xrt_array_take_value_or_null(xvalue* values, uint32 index)
{
    xvalue* result = xrtValueArrayTake(values, index);
    return result != 0 ? result : xrtValueNull();
}

static inline xvalue* xl6_xrt_array_pop_value_or_null(xvalue* values)
{
    xvalue* result = xrtValueArrayPop(values);
    return result != 0 ? result : xrtValueNull();
}

static inline xvalue* xl6_xrt_list_take_value_or_null(xvalue* values, int64 key)
{
    xvalue* result = xrtValueIntMapTake(values, key);
    return result != 0 ? result : xrtValueNull();
}

static inline xvalue* xl6_xrt_table_take_value_or_null(xvalue* values, const char* key)
{
    xvalue* result = xrtValueObjectTake(values, xrtStrView(key != 0 ? key : ""));
    return result != 0 ? result : xrtValueNull();
}

#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
static inline int xl6_xrt_typed_array_append_value_release(xtarray values, xvalue* item)
{
    int ok = xl6TypedArrayAppendValue(values, item) ? 1 : 0;
    if ( item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline int xl6_xrt_typed_array_insert_value_release(xtarray values, uint32 index, xvalue* item)
{
    int ok = xl6TypedArrayInsertValue(values, index, item) ? 1 : 0;
    if ( item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline xvalue* xl6_xrt_typed_array_take_value_or_null(xtarray values, uint32 index)
{
    xvalue* result = xl6TypedArrayTakeValue(values, index);
    return result != 0 ? result : xrtValueNull();
}

static inline xvalue* xl6_xrt_typed_array_pop_value_or_null(xtarray values)
{
    xvalue* result = xl6TypedArrayPopValue(values);
    return result != 0 ? result : xrtValueNull();
}

static inline int64 xl6_xrt_typed_array_index_of_value_release(xtarray values, xvalue* item)
{
    int64 result = xl6TypedArrayIndexOfValue(values, item);
    if ( item != 0 ) { xrtValueRelease(item); }
    return result;
}

static inline int xl6_xrt_typed_array_remove_value_release(xtarray values, xvalue* item)
{
    int64 index = xl6TypedArrayIndexOfValue(values, item);
    int ok = 0;
    if ( index >= 0 ) {
        ok = xl6TypedArrayRemove(values, (uint32)index, 1u) ? 1 : 0;
    }
    if ( item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline int xl6_xrt_typed_array_contains_value_release(xtarray values, xvalue* item)
{
    int ok = xl6TypedArrayContainsValue(values, item) ? 1 : 0;
    if ( item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline xvalue* xl6_xrt_typed_list_take_value_or_null(xtlist values, int64 key)
{
    xvalue* result = xl6TypedListTakeValue(values, key);
    return result != 0 ? result : xrtValueNull();
}

static inline int xl6_xrt_typed_list_contains_value_release(xtlist values, xvalue* item)
{
    int ok = xl6TypedListContainsValue(values, item) ? 1 : 0;
    if ( item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline xtlist xl6_xrt_typed_list_concat(xtlist left, xtlist right)
{
    xtlist result = xl6TypedListClone(left);
    if ( result == 0 ) {
        return 0;
    }
    if ( !xl6TypedListMerge(result, right) ) {
        xl6TypedListRelease(result);
        return 0;
    }
    return result;
}

static inline xvalue* xl6_xrt_typed_dict_take_value_or_null(xtdict values, const char* key, uint32 keyLen)
{
    xvalue* result = xl6TypedDictTakeValue(values, key, keyLen);
    return result != 0 ? result : xrtValueNull();
}

static inline xtlist xl6_xrt_typed_list_without_key(xtlist values, int64 key)
{
    xtlist result = xl6TypedListClone(values);
    if ( result != 0 ) {
        xl6TypedListRemove(result, key);
    }
    return result;
}

static inline xtdict xl6_xrt_typed_dict_concat(xtdict left, xtdict right)
{
    xtdict result = xl6TypedDictClone(left);
    if ( result == 0 ) {
        return 0;
    }
    if ( !xl6TypedDictMerge(result, right) ) {
        xl6TypedDictRelease(result);
        return 0;
    }
    return result;
}

static inline xtdict xl6_xrt_typed_dict_without_key(xtdict values, const char* key, uint32 keyLen)
{
    xtdict result = xl6TypedDictClone(values);
    if ( result != 0 ) {
        xl6TypedDictRemove(result, key, keyLen);
    }
    return result;
}

static inline int xl6_xrt_typed_set_relation(xtset left, xtset right, int mode)
{
    if ( mode == 0 ) {
        return xl6TypedSetIsSubset(left, right, false) ? 1 : 0;
    }
    if ( mode == 1 ) {
        return xl6TypedSetIsSubset(left, right, true) ? 1 : 0;
    }
    if ( mode == 2 ) {
        return xl6TypedSetIsSuperset(left, right, false) ? 1 : 0;
    }
    return xl6TypedSetIsSuperset(left, right, true) ? 1 : 0;
}

static inline int xl6_xrt_typed_set_add_value_release(xtset values, xvalue* item)
{
    int ok = xl6TypedSetAddValue(values, item) ? 1 : 0;
    if ( item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline int xl6_xrt_typed_set_value_op(xtset values, xvalue* item, int remove)
{
    const xl6_type_desc* type = values != 0 ? xl6TypedSetItemType(values) : 0;
    ptr raw;
    int ok;
    if ( type == 0 || type->Size == 0u ) {
        if ( item != 0 ) { xrtValueRelease(item); }
        return 0;
    }
    raw = xrtMalloc(type->Size);
    if ( raw == 0 ) {
        if ( item != 0 ) { xrtValueRelease(item); }
        return 0;
    }
    if ( !xl6TypeUnboxValue(type, item, raw) ) {
        xrtFree(raw);
        if ( item != 0 ) { xrtValueRelease(item); }
        return 0;
    }
    ok = remove ? (xl6TypedSetRemove(values, raw) ? 1 : 0) : (xl6TypedSetExists(values, raw) ? 1 : 0);
    xl6TypeDropValue(type, raw);
    xrtFree(raw);
    if ( item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline int xl6_xrt_typed_set_contains_value_release(xtset values, xvalue* item)
{
    return xl6_xrt_typed_set_value_op(values, item, 0);
}

static inline int xl6_xrt_typed_set_remove_value_release(xtset values, xvalue* item)
{
    return xl6_xrt_typed_set_value_op(values, item, 1);
}
#endif

static inline int xl6_xrt_container_push_value(xvalue* values, xvalue* item)
{
#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
    if ( xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_ARRAY ) {
        return xl6_xrt_typed_array_append_value_release((xtarray)xl6ValueHandleData(values), item);
    }
#endif
    if ( values != 0 && xl6_xrt_value_kind(values) == XL6_XVO_DT_ARRAY ) {
        return xl6_xrt_array_push_value(values, item);
    }
    if ( item != 0 ) { xrtValueRelease(item); }
    return 0;
}

static inline int xl6_xrt_container_add_value(xvalue* values, xvalue* item)
{
#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
    if ( xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_SET ) {
        return xl6_xrt_typed_set_add_value_release((xtset)xl6ValueHandleData(values), item);
    }
#endif
    if ( values != 0 && xl6_xrt_value_kind(values) == XL6_XVO_DT_COLL ) {
        return xl6_xrt_set_add_value(values, item);
    }
    if ( item != 0 ) { xrtValueRelease(item); }
    return 0;
}

static inline int xl6_xrt_container_insert_value(xvalue* values, xvalue* index, xvalue* item)
{
    int ok = 0;
#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
    if ( xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_ARRAY &&
         index != 0 && xl6_xrt_value_kind(index) == XL6_XVO_DT_INT ) {
        ok = xl6TypedArrayInsertValue(
            (xtarray)xl6ValueHandleData(values),
            (uint32)xl6_xrt_value_int(index),
            item) ? 1 : 0;
        if ( item != 0 ) { xrtValueRelease(item); }
    } else
#endif
    if ( values != 0 && index != 0 &&
         xl6_xrt_value_kind(values) == XL6_XVO_DT_ARRAY &&
         xl6_xrt_value_kind(index) == XL6_XVO_DT_INT ) {
        ok = xl6_xrt_array_insert_value(values, (uint32)xl6_xrt_value_int(index), item);
    } else if ( item != 0 ) {
        xrtValueRelease(item);
    }
    if ( index != 0 ) { xrtValueRelease(index); }
    return ok;
}

static inline int xl6_xrt_container_remove_value(xvalue* values, xvalue* key, uint32 count, int hasCount)
{
    int ok = 0;
    int valuesType = values != 0 ? xl6_xrt_value_kind(values) : -1;
    int keyType = key != 0 ? xl6_xrt_value_kind(key) : -1;
#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
    uint32 typedKind = xl6_xrt_typed_container_kind(values);
    if ( typedKind == XL6_XRT_TYPE_KIND_ARRAY && keyType == XL6_XVO_DT_INT ) {
        ok = xl6TypedArrayRemove(
            (xtarray)xl6ValueHandleData(values),
            (uint32)xl6_xrt_value_int(key),
            hasCount ? count : 1u) ? 1 : 0;
    } else if ( !hasCount && typedKind == XL6_XRT_TYPE_KIND_LIST && keyType == XL6_XVO_DT_INT ) {
        ok = xl6TypedListRemove((xtlist)xl6ValueHandleData(values), xl6_xrt_value_int(key)) ? 1 : 0;
    } else if ( !hasCount && typedKind == XL6_XRT_TYPE_KIND_SET ) {
        ok = xl6TypedSetRemoveValue((xtset)xl6ValueHandleData(values), key) ? 1 : 0;
    } else if ( !hasCount && typedKind == XL6_XRT_TYPE_KIND_DICT && keyType == XL6_XVO_DT_TEXT ) {
        ok = xl6TypedDictRemove((xtdict)xl6ValueHandleData(values), xl6_xrt_value_text(key), 0) ? 1 : 0;
    } else
#endif
    if ( valuesType == XL6_XVO_DT_ARRAY && keyType == XL6_XVO_DT_INT ) {
        ok = xrtValueArrayRemove(values, (uint32)xl6_xrt_value_int(key), hasCount ? count : 1u) ? 1 : 0;
    } else if ( !hasCount && valuesType == XL6_XVO_DT_LIST && keyType == XL6_XVO_DT_INT ) {
        ok = xrtValueIntMapRemove(values, xl6_xrt_value_int(key)) ? 1 : 0;
    } else if ( !hasCount && valuesType == XL6_XVO_DT_COLL ) {
        ok = xrtValueSetRemove(values, key) ? 1 : 0;
    } else if ( !hasCount && valuesType == XL6_XVO_DT_TABLE && keyType == XL6_XVO_DT_TEXT ) {
        ok = xrtValueObjectRemove(values, (xstrview){
            xl6_xrt_value_text(key),
            xl6_xrt_value_text_size(key)
        }) ? 1 : 0;
    }
    if ( key != 0 ) { xrtValueRelease(key); }
    return ok;
}

bool xl6_xrt_std_table_remove(xvalue* table, const char* key, uint32 keyLen)
{
    return xrtValueObjectRemove(table, (xstrview){
        key != 0 ? key : "",
        keyLen != 0 ? keyLen : (key != 0 ? strlen(key) : 0)
    });
}

static inline xvalue* xl6_xrt_container_take_value(xvalue* values, xvalue* key)
{
    xvalue* result = 0;
    int valuesType = values != 0 ? xl6_xrt_value_kind(values) : -1;
    int keyType = key != 0 ? xl6_xrt_value_kind(key) : -1;
#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
    uint32 typedKind = xl6_xrt_typed_container_kind(values);
    if ( typedKind == XL6_XRT_TYPE_KIND_ARRAY && keyType == XL6_XVO_DT_INT ) {
        result = xl6TypedArrayTakeValue(
            (xtarray)xl6ValueHandleData(values),
            (uint32)xl6_xrt_value_int(key));
    } else if ( typedKind == XL6_XRT_TYPE_KIND_LIST && keyType == XL6_XVO_DT_INT ) {
        result = xl6TypedListTakeValue((xtlist)xl6ValueHandleData(values), xl6_xrt_value_int(key));
    } else if ( typedKind == XL6_XRT_TYPE_KIND_DICT && keyType == XL6_XVO_DT_TEXT ) {
        result = xl6TypedDictTakeValue((xtdict)xl6ValueHandleData(values), xl6_xrt_value_text(key), 0);
    } else
#endif
    if ( valuesType == XL6_XVO_DT_ARRAY && keyType == XL6_XVO_DT_INT ) {
        result = xrtValueArrayTake(values, (uint32)xl6_xrt_value_int(key));
    } else if ( valuesType == XL6_XVO_DT_LIST && keyType == XL6_XVO_DT_INT ) {
        result = xrtValueIntMapTake(values, xl6_xrt_value_int(key));
    } else if ( valuesType == XL6_XVO_DT_TABLE && keyType == XL6_XVO_DT_TEXT ) {
        result = xrtValueObjectTake(values, (xstrview){
            xl6_xrt_value_text(key),
            xl6_xrt_value_text_size(key)
        });
    }
    if ( key != 0 ) { xrtValueRelease(key); }
    return result != 0 ? result : xrtValueNull();
}

static inline xvalue* xl6_xrt_container_pop_value(xvalue* values)
{
    xvalue* result = 0;
#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
    if ( xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_ARRAY ) {
        result = xl6TypedArrayPopValue((xtarray)xl6ValueHandleData(values));
    } else
#endif
    if ( values != 0 && xl6_xrt_value_kind(values) == XL6_XVO_DT_ARRAY ) {
        result = xrtValueArrayPop(values);
    }
    return result != 0 ? result : xrtValueNull();
}

static inline int xl6_xrt_container_clear(xvalue* values)
{
    int type = values != 0 ? xl6_xrt_value_kind(values) : -1;
#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
    if ( xl6_xrt_typed_container_kind(values) != XL6_XRT_TYPE_KIND_INVALID ) {
        return xl6_xrt_typed_container_clear(values);
    }
#endif
    if ( type == XL6_XVO_DT_ARRAY || type == XL6_XVO_DT_LIST ||
         type == XL6_XVO_DT_COLL || type == XL6_XVO_DT_TABLE ) {
        return xrtValueClear(values) ? 1 : 0;
    }
    return 0;
}

static inline int xl6_xrt_container_reserve(xvalue* values, long long count)
{
    if ( count < 0 ) { return 0; }
#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
    if ( xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_ARRAY ) {
        return xl6TypedArrayReserve(
            (xtarray)xl6ValueHandleData(values), count) ? 1 : 0;
    }
#endif
    return values != 0 && count <= (long long)UINT32_MAX &&
        xl6_xrt_value_kind(values) == XL6_XVO_DT_ARRAY ?
        (xrtValueReserve(values, (uint32)count) ? 1 : 0) : 0;
}

static inline int xl6_xrt_container_swap(
    xvalue* values,
    long long index1,
    long long index2)
{
    if ( index1 < 0 || index2 < 0 ) { return 0; }
#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
    if ( xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_ARRAY ) {
        return xl6TypedArraySwap((xtarray)xl6ValueHandleData(values), index1, index2) ? 1 : 0;
    }
#endif
    return values != 0 && index1 <= (long long)UINT32_MAX &&
        index2 <= (long long)UINT32_MAX &&
        xl6_xrt_value_kind(values) == XL6_XVO_DT_ARRAY ?
        (xrtValueArraySwap(values, (uint32)index1, (uint32)index2) ? 1 : 0) : 0;
}

static inline xvalue* xl6_xrt_array_varargs(uint32 count, ...)
{
    xvalue* result = xrtValueArray();
    va_list args;
    uint32 i;
    int ok = result != 0;

    va_start(args, count);
    for ( i = 0; i < count; ++i ) {
        xvalue* item = va_arg(args, xvalue*);
        if ( ok ) {
            ok = xrtValueArrayAppendNew(result, item) ? 1 : 0;
        } else if ( item != 0 ) {
            xrtValueRelease(item);
        }
    }
    va_end(args);
    if ( ok ) { return result; }
    if ( result != 0 ) { xrtValueRelease(result); }
    return xrtValueNull();
}

static inline xvalue* xl6_xrt_list_literal(uint32 count, ...)
{
    xvalue* result = xrtValueIntMap();
    va_list args;
    uint32 i;
    int ok = result != 0;

    va_start(args, count);
    for ( i = 0; i < count; ++i ) {
        int64 key = va_arg(args, int64);
        xvalue* item = va_arg(args, xvalue*);
        if ( ok ) {
            ok = xrtValueIntMapSetNew(result, key, item) ? 1 : 0;
        } else if ( item != 0 ) {
            xrtValueRelease(item);
        }
    }
    va_end(args);
    if ( ok ) { return result; }
    if ( result != 0 ) { xrtValueRelease(result); }
    return xrtValueNull();
}

static inline xvalue* xl6_xrt_set_literal(uint32 count, ...)
{
    xvalue* result = xrtValueSet();
    va_list args;
    uint32 i;
    int ok = result != 0;

    va_start(args, count);
    for ( i = 0; i < count; ++i ) {
        xvalue* item = va_arg(args, xvalue*);
        if ( ok ) {
            ok = xrtValueSetAddNew(result, item) ? 1 : 0;
        } else if ( item != 0 ) {
            xrtValueRelease(item);
        }
    }
    va_end(args);
    if ( ok ) { return result; }
    if ( result != 0 ) { xrtValueRelease(result); }
    return xrtValueNull();
}

static inline xvalue* xl6_xrt_dict_varargs(uint32 count, ...)
{
    xvalue* result = xrtValueObject();
    va_list args;
    uint32 i;
    int ok = result != 0;

    va_start(args, count);
    for ( i = 0; i < count; ++i ) {
        const char* key = va_arg(args, const char*);
        xvalue* item = va_arg(args, xvalue*);
        if ( ok ) {
            ok = xrtValueObjectSetNew(result, xrtStrView(key), item) ? 1 : 0;
        } else if ( item != 0 ) {
            xrtValueRelease(item);
        }
    }
    va_end(args);
    if ( ok ) { return result; }
    if ( result != 0 ) { xrtValueRelease(result); }
    return xrtValueNull();
}

#ifndef XL6_XRT_TAKE_CSTR_HELPERS
#define XL6_XRT_TAKE_CSTR_HELPERS
static inline const char* xl6_xrt_temp_cstr(const char* value)
{
    size_t len;
    char* text;

    if ( value == 0 ) {
        return "";
    }
    len = strlen(value);
    text = (char*)xrtTemp(len + 1);
    if ( text == 0 ) {
        return "";
    }
    memcpy(text, value, len + 1);
    return text;
}

static inline const char* xl6_xrt_take_cstr(void* value)
{
    const char* source;
    size_t len;
    char* text;

    if ( value == 0 ) {
        return "";
    }
    source = (const char*)value;
    len = strlen(source);
    text = (char*)xrtTemp(len + 1);
    if ( text == 0 ) {
        xrtFree(value);
        return "";
    }
    memcpy(text, source, len + 1);
    xrtFree(value);
    return text;
}
#endif

/*
    var 临时值的消费依赖当前生成单元的临时作用域表。
    这些帮助函数由 cgen 在作用域实现之后生成，运行时头不保留第二份定义。
*/

#ifdef XL_RUNTIME_NEED_TYPED_CONTAINER
static inline xvalue* xl6_xrt_typed_box_or_null(const xl6_type_desc* type, const void* item)
{
    if ( item == 0 ) { return xrtValueNull(); }
    {
        xvalue* boxed = xl6TypeBoxValue(type, item);
        return boxed != 0 ? boxed : xrtValueNull();
    }
}

static inline uint32 xl6_xrt_typed_container_kind(xvalue* value)
{
    const xl6_type_desc* type;
    if ( value == 0 || xl6_xrt_value_kind(value) != XL6_XVO_DT_NATIVE ) {
        return XL6_XRT_TYPE_KIND_INVALID;
    }
    type = xl6ValueType(value);
    if ( type == 0 || type->Kind < XL6_XRT_TYPE_KIND_ARRAY || type->Kind > XL6_XRT_TYPE_KIND_DICT ) {
        return XL6_XRT_TYPE_KIND_INVALID;
    }
    return type->Kind;
}

static inline const xl6_type_desc* xl6_xrt_typed_container_item_type(xvalue* value)
{
    const xl6_type_desc* type = value != 0 ? xl6ValueType(value) : 0;
    const xl6_generic_type* generic = type != 0 ? (const xl6_generic_type*)type->Extra : 0;
    return generic != 0 && generic->ArgCount == 1u ? generic->Args[0] : 0;
}

static inline xvalue* xl6_xrt_typed_container_box_ref(const xl6_type_desc* type, void* handle)
{
    void* owned = 0;
    xvalue* result;
    if ( handle == 0 ) { return xrtValueNull(); }
    if ( type == 0 || type->Ops == 0 || type->Ops->copy == 0 || type->Ops->drop == 0 ) {
        return xrtValueNull();
    }
    type->Ops->copy(&owned, &handle);
    if ( owned == 0 ) { return xrtValueNull(); }
    result = xl6ValueHandle(type, owned, XL6_XRT_HANDLE_FLAG_OWNED | XL6_XRT_HANDLE_FLAG_NULLABLE);
    if ( result == 0 ) {
        type->Ops->drop(&owned);
        return xrtValueNull();
    }
    return result;
}

static inline xtarray xl6_xrt_typed_array_from_value_release(const xl6_type_desc* type, xvalue* value)
{
    xtarray result;

    result = xl6TypedArrayFromValue(value, type);
    if ( value != 0 ) { xrtValueRelease(value); }
    return result;
}

static inline xtlist xl6_xrt_typed_list_from_value_release(const xl6_type_desc* type, xvalue* value)
{
    xtlist result;

    result = xl6TypedListFromValue(value, type);
    if ( value != 0 ) { xrtValueRelease(value); }
    return result;
}

static inline xtset xl6_xrt_typed_set_from_value_release(const xl6_type_desc* type, xvalue* value)
{
    xtset result;

    result = xl6TypedSetFromValue(value, type);
    if ( value != 0 ) { xrtValueRelease(value); }
    return result;
}

static inline xtdict xl6_xrt_typed_dict_from_value_release(const xl6_type_desc* type, xvalue* value)
{
    xtdict result;

    result = xl6TypedDictFromValue(value, type);
    if ( value != 0 ) { xrtValueRelease(value); }
    return result;
}

typedef xvalue* (*xl6_xrt_typed_item_box_fn)(void* item);

static inline xvalue* xl6_xrt_typed_map_item(
    void* rawItem,
    xl6_xrt_typed_item_box_fn box,
    bool classHandle)
{
    void* boxItem = rawItem;

    if ( classHandle && rawItem != 0 ) {
        xvalue* handle = *(xvalue**)rawItem;
        boxItem = handle != 0 && !xl6_xrt_value_is_null(handle) ?
            xl6ValueRecordData(handle) : 0;
    }
    return boxItem != 0 && box != 0 ? box(boxItem) : xrtValueNull();
}

/*
	typed 容器越界通过生成单元注入的异常入口报告。
	纯 C 适配层未注入该入口时仍保持明确的 fatal 语义，不依赖未定义的私有函数。
*/
#ifndef XL6_TYPED_INDEX_RAISE
#define XL6_TYPED_INDEX_RAISE(kind, message, file, line, column) xl6_runtime_fatal(2)
#endif

static inline void* xl6_xrt_typed_require_item(
    void* item,
    const xl6_type_desc* type,
    const char* file,
    int line,
    int column)
{
    if ( item == 0 ) {
        size_t size = type != 0 && type->Size > 0 ? type->Size : sizeof(void*);
        void* fallback = xrtTemp(size);
        if ( fallback == 0 ) { xl6_runtime_fatal(2); }
        memset(fallback, 0, size);
		XL6_TYPED_INDEX_RAISE(
			"RangeError",
			"typed container index is out of range",
			file,
			line,
			column
		);
        return fallback;
    }
    return item;
}

static inline bool xl6_xrt_typed_array_resolve_index(xtarray values, int64 index, uint32* resolved)
{
    uint32 count;
    if ( values == 0 || resolved == 0 ) { return false; }
    if ( index < 0 ) {
        count = xl6TypedArrayCount(values);
        if ( index < -(int64)count ) { return false; }
        index += (int64)count;
    }
    if ( index < 0 || (uint64)index > 0xFFFFFFFFULL ) { return false; }
    *resolved = (uint32)index;
    return true;
}

static inline void* xl6_xrt_typed_array_get_at(xtarray values, int64 index)
{
    uint32 resolved;
    return xl6_xrt_typed_array_resolve_index(values, index, &resolved) ?
        xl6TypedArrayGet(values, resolved) : 0;
}

static inline xvalue* xl6_xrt_typed_array_to_xvalue_map(xtarray values, xl6_xrt_typed_item_box_fn box, bool classHandle)
{
    xvalue* result = xrtValueArray();
    uint32 count = xl6TypedArrayCount(values);
    uint32 i;
    if ( result == 0 ) {
        return xrtValueNull();
    }
    for ( i = 0; i < count; ++i ) {
        void* rawItem = xl6_xrt_typed_array_get_at(values, i);
        xvalue* item = xl6_xrt_typed_map_item(rawItem, box, classHandle);
        if ( !xrtValueArrayAppendNew(result, item) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline xvalue* xl6_xrt_typed_list_to_sequence_map(
    xtlist values,
    xl6_xrt_typed_item_box_fn box,
    bool classHandle)
{
    xvalue* result = xrtValueArray();
    uint32 count = xl6TypedListCount(values);
    uint32 i;

    if ( result == 0 ) { return xrtValueNull(); }
    for ( i = 0; i < count; ++i ) {
        void* rawItem = xl6TypedListItemAt(values, i, 0);
        xvalue* item = xl6_xrt_typed_map_item(rawItem, box, classHandle);
        if ( !xrtValueArrayAppendNew(result, item) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline xvalue* xl6_xrt_typed_set_to_sequence_map(
    xtset values,
    xl6_xrt_typed_item_box_fn box,
    bool classHandle)
{
    xvalue* result = xrtValueArray();
    uint32 count = xl6TypedSetCount(values);
    uint32 i;

    if ( result == 0 ) { return xrtValueNull(); }
    for ( i = 0; i < count; ++i ) {
        void* rawItem = xl6TypedSetItemAt(values, i);
        xvalue* item = xl6_xrt_typed_map_item(rawItem, box, classHandle);
        if ( !xrtValueArrayAppendNew(result, item) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline xvalue* xl6_xrt_typed_dict_to_xvalue_map(
    xtdict values,
    xl6_xrt_typed_item_box_fn box,
    bool classHandle)
{
    xvalue* result = xrtValueObject();
    uint32 count = xl6TypedDictCount(values);
    uint32 i;

    if ( result == 0 ) { return xrtValueNull(); }
    for ( i = 0; i < count; ++i ) {
        const char* key = 0;
        uint32 keySize = 0;
        void* rawItem = xl6TypedDictItemAt(values, i, &key, &keySize);
        xvalue* item = xl6_xrt_typed_map_item(rawItem, box, classHandle);
        if ( !xrtValueObjectSetNew(result, (xstrview){ key, keySize }, item) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline bool xl6_xrt_typed_array_set_at(xtarray values, int64 index, xvalue* value)
{
    uint32 resolved;
    return xl6_xrt_typed_array_resolve_index(values, index, &resolved) &&
        xl6TypedArraySetValue(values, resolved, value);
}

static inline xvalue* xl6_xrt_typed_array_to_xvalue(const xl6_type_desc* type, xtarray values)
{
    xvalue* result = xrtValueArray();
    uint32 count = xl6TypedArrayCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        xvalue* item = xl6_xrt_typed_box_or_null(type, xl6_xrt_typed_array_get_at(values, i));
        if ( !xrtValueArrayAppendNew(result, item) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline xtarray xl6_xrt_typed_array_copy(const xl6_type_desc* type, xtarray values)
{
    xtarray result;
    uint32 count = xl6TypedArrayCount(values);
    uint32 i;

    result = xl6TypedArrayCreate(type);
    for ( i = 0; i < count; ++i ) {
        xvalue* item = xl6_xrt_typed_box_or_null(type, xl6_xrt_typed_array_get_at(values, i));
        if ( !xl6TypedArrayAppendValue(result, item) && item != 0 ) { xrtValueRelease(item); }
        else if ( item != 0 ) { xrtValueRelease(item); }
    }
    return result;
}

static inline xvalue* xl6_xrt_typed_list_to_xvalue(const xl6_type_desc* type, xtlist values)
{
    xvalue* result = xrtValueIntMap();
    uint32 count = xl6TypedListCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        int64 key = 0;
        xvalue* item = xl6_xrt_typed_box_or_null(type, xl6TypedListItemAt(values, i, &key));
        if ( !xrtValueIntMapSetNew(result, key, item) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline xvalue* xl6_xrt_typed_list_to_sequence_value(
    const xl6_type_desc* type,
    xtlist values)
{
    xvalue* result = xrtValueArray();
    uint32 count = xl6TypedListCount(values);
    uint32 i;

    if ( result == 0 ) { return xrtValueNull(); }
    for ( i = 0; i < count; ++i ) {
        xvalue* item = xl6_xrt_typed_box_or_null(
            type, xl6TypedListItemAt(values, i, 0));
        if ( !xrtValueArrayAppendNew(result, item) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline xtlist xl6_xrt_typed_list_copy(const xl6_type_desc* type, xtlist values)
{
    xtlist result;
    uint32 count = xl6TypedListCount(values);
    uint32 i;

    result = xl6TypedListCreate(type);
    for ( i = 0; i < count; ++i ) {
        int64 key = 0;
        xvalue* item = xl6_xrt_typed_box_or_null(type, xl6TypedListItemAt(values, i, &key));
        if ( !xl6TypedListSetValue(result, key, item) && item != 0 ) { xrtValueRelease(item); }
        else if ( item != 0 ) { xrtValueRelease(item); }
    }
    return result;
}

static inline xvalue* xl6_xrt_typed_set_to_xvalue(const xl6_type_desc* type, xtset values)
{
    xvalue* result = xrtValueSet();
    uint32 count = xl6TypedSetCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        xvalue* item = xl6_xrt_typed_box_or_null(type, xl6TypedSetItemAt(values, i));
        if ( !xrtValueSetAddNew(result, item) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline xvalue* xl6_xrt_typed_set_to_sequence_value(
    const xl6_type_desc* type,
    xtset values)
{
    xvalue* result = xrtValueArray();
    uint32 count = xl6TypedSetCount(values);
    uint32 i;

    if ( result == 0 ) { return xrtValueNull(); }
    for ( i = 0; i < count; ++i ) {
        xvalue* item = xl6_xrt_typed_box_or_null(
            type, xl6TypedSetItemAt(values, i));
        if ( !xrtValueArrayAppendNew(result, item) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline xtset xl6_xrt_typed_set_copy(const xl6_type_desc* type, xtset values)
{
    xtset result;
    uint32 count = xl6TypedSetCount(values);
    uint32 i;

    result = xl6TypedSetCreate(type);
    for ( i = 0; i < count; ++i ) {
        xvalue* item = xl6_xrt_typed_box_or_null(type, xl6TypedSetItemAt(values, i));
        if ( !xl6TypedSetAddValue(result, item) && item != 0 ) { xrtValueRelease(item); }
        else if ( item != 0 ) { xrtValueRelease(item); }
    }
    return result;
}

static inline xvalue* xl6_xrt_typed_dict_to_xvalue(const xl6_type_desc* type, xtdict values)
{
    xvalue* result = xrtValueObject();
    uint32 count = xl6TypedDictCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        const char* key = 0;
        uint32 keyLen = 0;
        xvalue* item = xl6_xrt_typed_box_or_null(type, xl6TypedDictItemAt(values, i, &key, &keyLen));
        if ( !xrtValueObjectSetNew(result, (xstrview){ key, keyLen }, item) ) {
            xrtValueRelease(result);
            return xrtValueNull();
        }
    }
    return result;
}

static inline xtdict xl6_xrt_typed_dict_copy(const xl6_type_desc* type, xtdict values)
{
    xtdict result;
    uint32 count = xl6TypedDictCount(values);
    uint32 i;

    result = xl6TypedDictCreate(type);
    for ( i = 0; i < count; ++i ) {
        const char* key = 0;
        uint32 keyLen = 0;
        xvalue* item = xl6_xrt_typed_box_or_null(type, xl6TypedDictItemAt(values, i, &key, &keyLen));
        if ( !xl6TypedDictSetValue(result, key, keyLen, item) && item != 0 ) { xrtValueRelease(item); }
        else if ( item != 0 ) { xrtValueRelease(item); }
    }
    return result;
}

/*
    var 只持有静态容器句柄；元素在动态读取边界按需装箱。
    for-in 需要稳定快照时再显式转换，普通读写始终作用于原容器。
*/
static inline xvalue* xl6_xrt_typed_container_box_item(xvalue* values, const void* item)
{
    return xl6_xrt_typed_box_or_null(xl6_xrt_typed_container_item_type(values), item);
}

static inline xvalue* xl6_xrt_typed_container_snapshot(xvalue* values)
{
    uint32 kind = xl6_xrt_typed_container_kind(values);
    void* handle = values != 0 ? xl6ValueHandleData(values) : 0;
    const xl6_type_desc* itemType = xl6_xrt_typed_container_item_type(values);
    xvalue* result;
    if ( kind == XL6_XRT_TYPE_KIND_INVALID ) { return values; }
    if ( kind == XL6_XRT_TYPE_KIND_ARRAY ) {
        result = xl6_xrt_typed_array_to_xvalue(itemType, (xtarray)handle);
    } else if ( kind == XL6_XRT_TYPE_KIND_LIST ) {
        result = xl6_xrt_typed_list_to_xvalue(itemType, (xtlist)handle);
    } else if ( kind == XL6_XRT_TYPE_KIND_SET ) {
        result = xl6_xrt_typed_set_to_xvalue(itemType, (xtset)handle);
    } else {
        result = xl6_xrt_typed_dict_to_xvalue(itemType, (xtdict)handle);
    }
    if ( values != 0 ) { xrtValueRelease(values); }
    return result != 0 ? result : xrtValueNull();
}

static inline uint32 xl6_xrt_typed_container_count(xvalue* values)
{
    uint32 kind = xl6_xrt_typed_container_kind(values);
    void* handle = values != 0 ? xl6ValueHandleData(values) : 0;
    if ( kind == XL6_XRT_TYPE_KIND_ARRAY ) { return xl6TypedArrayCount((xtarray)handle); }
    if ( kind == XL6_XRT_TYPE_KIND_LIST ) { return xl6TypedListCount((xtlist)handle); }
    if ( kind == XL6_XRT_TYPE_KIND_SET ) { return xl6TypedSetCount((xtset)handle); }
    if ( kind == XL6_XRT_TYPE_KIND_DICT ) { return xl6TypedDictCount((xtdict)handle); }
    return 0u;
}

static inline xvalue* xl6_xrt_typed_container_get_i64(xvalue* values, int64 index)
{
    uint32 kind = xl6_xrt_typed_container_kind(values);
    void* handle = values != 0 ? xl6ValueHandleData(values) : 0;
    const void* item = 0;
    if ( kind == XL6_XRT_TYPE_KIND_ARRAY ) {
        item = xl6_xrt_typed_array_get_at((xtarray)handle, index);
    } else if ( kind == XL6_XRT_TYPE_KIND_LIST ) {
        item = xl6TypedListGet((xtlist)handle, index);
    } else if ( kind == XL6_XRT_TYPE_KIND_SET && index >= 0 && (uint64)index <= 0xFFFFFFFFULL ) {
        item = xl6TypedSetItemAt((xtset)handle, (uint32)index);
    }
    return item != 0 ? xl6_xrt_typed_container_box_item(values, item) : xrtValueNull();
}

static inline xvalue* xl6_xrt_typed_container_get_key(xvalue* values, const char* key)
{
    void* item;
    if ( xl6_xrt_typed_container_kind(values) != XL6_XRT_TYPE_KIND_DICT ) {
        return xrtValueNull();
    }
    item = xl6TypedDictGet((xtdict)xl6ValueHandleData(values), key != 0 ? key : "", 0);
    return item != 0 ? xl6_xrt_typed_container_box_item(values, item) : xrtValueNull();
}

static inline bool xl6_xrt_typed_container_set_i64_owned(xvalue* values, int64 index, xvalue* item)
{
    uint32 kind = xl6_xrt_typed_container_kind(values);
    void* handle = values != 0 ? xl6ValueHandleData(values) : 0;
    uint32 resolved = 0;
    bool ok = false;
    if ( kind == XL6_XRT_TYPE_KIND_ARRAY &&
         xl6_xrt_typed_array_resolve_index((xtarray)handle, index, &resolved) ) {
        ok = xl6TypedArraySetValue((xtarray)handle, resolved, item);
    } else if ( kind == XL6_XRT_TYPE_KIND_LIST ) {
        ok = xl6TypedListSetValue((xtlist)handle, index, item);
    }
    if ( ok && item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline bool xl6_xrt_typed_container_set_key_owned(xvalue* values, const char* key, xvalue* item)
{
    bool ok = xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_DICT &&
              xl6TypedDictSetValue((xtdict)xl6ValueHandleData(values), key != 0 ? key : "", 0, item);
    if ( ok && item != 0 ) { xrtValueRelease(item); }
    return ok;
}

static inline bool xl6_xrt_typed_container_set_key_borrow(xvalue* values, const char* key, xvalue* item)
{
    return xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_DICT &&
           xl6TypedDictSetValue((xtdict)xl6ValueHandleData(values), key != 0 ? key : "", 0, item);
}

static inline int xl6_xrt_typed_container_contains(xvalue* values, xvalue* item)
{
    uint32 kind = xl6_xrt_typed_container_kind(values);
    void* handle = values != 0 ? xl6ValueHandleData(values) : 0;
    if ( kind == XL6_XRT_TYPE_KIND_ARRAY ) {
        return xl6TypedArrayContainsValue((xtarray)handle, item) ? 1 : 0;
    }
    if ( kind == XL6_XRT_TYPE_KIND_LIST ) {
        return xl6TypedListContainsValue((xtlist)handle, item) ? 1 : 0;
    }
    if ( kind == XL6_XRT_TYPE_KIND_SET ) {
        return xl6TypedSetExistsValue((xtset)handle, item) ? 1 : 0;
    }
    if ( kind == XL6_XRT_TYPE_KIND_DICT && item != 0 && xl6_xrt_value_kind(item) == XL6_XVO_DT_TEXT ) {
        return xl6TypedDictExists((xtdict)handle, xl6_xrt_value_text(item), 0) ? 1 : 0;
    }
    return 0;
}

static inline int xl6_xrt_typed_container_clear(xvalue* values)
{
    uint32 kind = xl6_xrt_typed_container_kind(values);
    void* handle = values != 0 ? xl6ValueHandleData(values) : 0;
    if ( kind == XL6_XRT_TYPE_KIND_ARRAY ) { return xl6TypedArrayClear((xtarray)handle) ? 1 : 0; }
    if ( kind == XL6_XRT_TYPE_KIND_LIST ) { return xl6TypedListClear((xtlist)handle) ? 1 : 0; }
    if ( kind == XL6_XRT_TYPE_KIND_SET ) { return xl6TypedSetClear((xtset)handle) ? 1 : 0; }
    if ( kind == XL6_XRT_TYPE_KIND_DICT ) { return xl6TypedDictClear((xtdict)handle) ? 1 : 0; }
    return 0;
}
#endif

typedef struct XL6_XrtIterItem {
    int64 index;
    const char* key;
    uint32 keyLen;
    xvalue* value;
} XL6_XrtIterItem;

typedef struct XL6_XrtIterSnapshot {
    XL6_XrtIterItem* items;
    uint32 count;
    uint32 capacity;
    int failed;
} XL6_XrtIterSnapshot;

static inline void xl6_xrt_iter_snapshot_free(XL6_XrtIterSnapshot* snapshot)
{
    uint32 i;
    if ( snapshot == 0 ) { return; }
    for ( i = 0; i < snapshot->count; ++i ) {
        if ( snapshot->items[i].key != 0 ) { xrtFree((void*)snapshot->items[i].key); }
        if ( snapshot->items[i].value != 0 ) { xrtValueRelease(snapshot->items[i].value); }
    }
    xrtFree(snapshot->items);
    xrtFree(snapshot);
}

static inline XL6_XrtIterSnapshot* xl6_xrt_iter_snapshot_create(void)
{
    XL6_XrtIterSnapshot* snapshot = (XL6_XrtIterSnapshot*)xrtMalloc(sizeof(XL6_XrtIterSnapshot));
    if ( snapshot == 0 ) { return 0; }
    snapshot->items = 0;
    snapshot->count = 0;
    snapshot->capacity = 0;
    snapshot->failed = 0;
    return snapshot;
}

static inline int xl6_xrt_iter_snapshot_push(XL6_XrtIterSnapshot* snapshot, int64 index, const void* key, uint32 keyLen, xvalue* value)
{
    XL6_XrtIterItem* item;
    if ( snapshot == 0 ) { return 0; }
    if ( snapshot->count == snapshot->capacity ) {
        uint32 newCap = snapshot->capacity == 0 ? 8u : snapshot->capacity * 2u;
        XL6_XrtIterItem* newItems = (XL6_XrtIterItem*)xrtRealloc(snapshot->items, sizeof(XL6_XrtIterItem) * newCap);
        if ( newItems == 0 ) { return 0; }
        snapshot->items = newItems;
        snapshot->capacity = newCap;
    }
    item = &snapshot->items[snapshot->count++];
    item->index = index;
    item->key = 0;
    item->keyLen = 0;
    item->value = xrtValueRetain(value);
    if ( key != 0 && keyLen > 0 ) {
		char* copy = (char*)xrtMalloc((size_t)keyLen + 1u);
        if ( copy == 0 ) { return 0; }
		memcpy(copy, key, keyLen);
		copy[keyLen] = '\0';
        item->key = copy;
        item->keyLen = keyLen;
    }
    return 1;
}

static inline XL6_XrtIterSnapshot* xl6_xrt_value_snapshot(xvalue* values, int kind)
{
    XL6_XrtIterSnapshot* snapshot = xl6_xrt_iter_snapshot_create();
    xvalueiter* iterator;
    xvaluekey key;
    xvalue* value;

    if ( snapshot == 0 ) { return 0; }
    iterator = xrtValueIterCreate(values);
    if ( iterator == 0 ) {
        xl6_xrt_iter_snapshot_free(snapshot);
        return 0;
    }
    while ( (value = xrtValueIterNext(iterator, &key)) != 0 ) {
        int64 index = (int64)snapshot->count;
        const void* keyData = 0;
        uint32 keyLen = 0;

        if ( kind == XL6_XVO_DT_LIST ) {
            if ( key.Type != XVALUE_KEY_INT ) {
                snapshot->failed = 1;
                break;
            }
            index = key.Integer;
        } else if ( kind == XL6_XVO_DT_TABLE ) {
            if ( (key.Type != XVALUE_KEY_STRING) ||
                 (key.String.Size > 0xFFFFFFFFULL) ) {
                snapshot->failed = 1;
                break;
            }
            keyData = key.String.Data;
            keyLen = (uint32)key.String.Size;
        }
        if ( !xl6_xrt_iter_snapshot_push(
            snapshot,
            index,
            keyData,
            keyLen,
            value
        ) ) {
            snapshot->failed = 1;
            break;
        }
    }
    xrtValueIterDestroy(iterator);
    if ( snapshot->failed ) {
        xl6_xrt_iter_snapshot_free(snapshot);
        return 0;
    }
    return snapshot;
}

static inline XL6_XrtIterSnapshot* xl6_xrt_list_snapshot(xvalue* values)
{
    return xl6_xrt_value_snapshot(values, XL6_XVO_DT_LIST);
}

static inline XL6_XrtIterSnapshot* xl6_xrt_set_snapshot(xvalue* values)
{
    return xl6_xrt_value_snapshot(values, XL6_XVO_DT_COLL);
}

static inline XL6_XrtIterSnapshot* xl6_xrt_dict_snapshot(xvalue* values)
{
    return xl6_xrt_value_snapshot(values, XL6_XVO_DT_TABLE);
}
#endif

#ifdef XL_RUNTIME_NEED_ARGV
static inline xvalue* xl6_argv_to_arg(int argc, char** argv)
{
    int i;
    xvalue* arg = xrtValueArray();
    if ( arg == 0 ) { return 0; }
    for ( i = 0; i < argc; ++i ) {
        const char* text = (argv != 0 && argv[i] != 0) ? argv[i] : "";
        xvalue* item = xrtValueString((xstrview){ text, strlen(text) });
        if ( item == 0 || !xrtValueArrayAppendNew(arg, item) ) {
            xrtValueRelease(arg);
            return 0;
        }
    }
    return arg;
}

#if defined(_WIN32) || defined(_WIN64)
#include <wchar.h>
#ifndef XL6_WINAPI_ARGV_DECL
#define XL6_WINAPI_ARGV_DECL
__declspec(dllimport) wchar_t* __stdcall GetCommandLineW(void);
__declspec(dllimport) wchar_t** __stdcall CommandLineToArgvW(const wchar_t* lpCmdLine, int* pNumArgs);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int CodePage, unsigned long dwFlags, const wchar_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, const char* lpDefaultChar, int* lpUsedDefaultChar);
__declspec(dllimport) void* __stdcall LocalFree(void* hMem);
#define XL6_WIN_UTF8_CODE_PAGE 65001u
#endif

static inline char* xl6_wide_to_utf8(const wchar_t* text)
{
    int size;
    char* utf8;

    if ( text == 0 ) {
        text = L"";
    }
    size = WideCharToMultiByte(XL6_WIN_UTF8_CODE_PAGE, 0, text, -1, 0, 0, 0, 0);
    if ( size <= 0 ) {
        return 0;
    }
    utf8 = (char*)xrtMalloc((size_t)size);
    if ( utf8 == 0 ) {
        return 0;
    }
    if ( WideCharToMultiByte(XL6_WIN_UTF8_CODE_PAGE, 0, text, -1, utf8, size, 0, 0) <= 0 ) {
        xrtFree(utf8);
        return 0;
    }
    return utf8;
}

static inline xvalue* xl6_windows_argv_to_arg(int fallbackArgc, char** fallbackArgv)
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    xvalue* arg;
    int i;

    if ( argv == 0 ) {
        return xl6_argv_to_arg(fallbackArgc, fallbackArgv);
    }
    arg = xrtValueArray();
    if ( arg == 0 ) {
        LocalFree(argv);
        return 0;
    }
    for ( i = 0; i < argc; ++i ) {
        char* text = xl6_wide_to_utf8(argv[i]);
        xvalue* item = text != 0 ?
            xrtValueString((xstrview){ text, strlen(text) }) : 0;
        if ( text != 0 ) {
            xrtFree(text);
        }
        if ( item == 0 || !xrtValueArrayAppendNew(arg, item) ) {
            xrtValueRelease(arg);
            LocalFree(argv);
            return 0;
        }
    }
    LocalFree(argv);
    return arg;
}
#endif

static inline xvalue* xl6_command_argv_to_arg(int argc, char** argv)
{
#if defined(_WIN32) || defined(_WIN64)
    return xl6_windows_argv_to_arg(argc, argv);
#else
    return xl6_argv_to_arg(argc, argv);
#endif
}
#endif

#endif

#endif
