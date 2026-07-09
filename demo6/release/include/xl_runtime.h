#ifndef XL_RUNTIME_H
#define XL_RUNTIME_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;
typedef unsigned char* str;
typedef void* ptr;
typedef int64 xtime;

#ifndef XL6_XVALUE_TYPEDEF
#define XL6_XVALUE_TYPEDEF
typedef struct xvalue_struct* xvalue;
#endif

#ifndef XL6_XRT_TYPED_CONTAINER_TYPEDEF
#define XL6_XRT_TYPED_CONTAINER_TYPEDEF
typedef struct xrt_typed_array_struct* xtarray;
typedef struct xrt_typed_list_struct* xtlist;
typedef struct xrt_typed_set_struct* xtset;
typedef struct xrt_typed_dict_struct* xtdict;
#endif

typedef struct xlist_struct* xlist;
typedef struct xdict_struct* xdict;
typedef struct xavltree_struct* xavltree;

#ifndef XL6_XRT_CALL_TYPEDEF
#define XL6_XRT_CALL_TYPEDEF
typedef struct xrt_type_ops xrt_type_ops;
typedef struct xrt_type_desc xrt_type_desc;
typedef struct xrt_generic_type xrt_generic_type;
typedef struct xrt_method_desc xrt_method_desc;
typedef struct xrt_method_table xrt_method_table;
typedef struct xrt_func_param xrt_func_param;
typedef struct xrt_func_sig xrt_func_sig;
typedef struct xrt_call_frame xrt_call_frame;
typedef struct xrt_call_result xrt_call_result;
typedef struct xrt_callable xrt_callable;
typedef bool (*xrt_xcall_proc)(xrt_call_frame* pFrame, xrt_call_result* pResult);

#define XL6_XRT_TYPE_KIND_INVALID 0u
#define XL6_XRT_TYPE_KIND_ARRAY 9u
#define XL6_XRT_TYPE_KIND_LIST 10u
#define XL6_XRT_TYPE_KIND_SET 11u
#define XL6_XRT_TYPE_KIND_DICT 12u
#define XL6_XRT_TYPE_KIND_RECORD 13u
#define XL6_XRT_TYPE_KIND_HANDLE 14u
#define XL6_XRT_HANDLE_FLAG_BORROWED 0x00000002u
#define XL6_XRT_HANDLE_FLAG_NULLABLE 0x00000004u
#ifndef XRT_OBJMODE_LOCAL
#define XRT_OBJMODE_LOCAL 0u
#endif
#ifndef XRT_OBJMODE_SHARED
#define XRT_OBJMODE_SHARED 1u
#endif

#define XL6_XRT_FUNC_FLAG_VARARGS 0x00000001u
#define XL6_XRT_FUNC_FLAG_KWARGS 0x00000002u
#define XL6_XRT_FUNC_PARAM_OPTIONAL 0x00000001u
#define XRT_CALL_RESULT_INLINE_COUNT 4u
#ifndef XL6_XRT_FUNC_SIG_TYPEDEF
#define XL6_XRT_FUNC_SIG_TYPEDEF
#endif

struct xrt_type_ops {
    void (*init)(void* pObj);
    void (*copy)(void* pDst, const void* pSrc);
    void (*move)(void* pDst, void* pSrc);
    void (*drop)(void* pObj);
    int (*compare)(const void* pA, const void* pB);
    uint64 (*hash)(const void* pObj);
    str (*to_string)(const void* pObj, uint32* pSize);
    xvalue (*box)(const void* pObj, const xrt_type_desc* pType);
    bool (*unbox)(xvalue pVal, void* pOut, const xrt_type_desc* pType);
};

struct xrt_type_desc {
    uint32 TypeId;
    uint32 Kind;
    const char* Name;
    uint32 NameSize;
    const char* AbiName;
    uint32 AbiNameSize;
    size_t Size;
    size_t Align;
    const xrt_type_ops* Ops;
    const xrt_method_table* Methods;
    const void* Extra;
};

struct xrt_generic_type {
    uint32 ArgCount;
    const xrt_type_desc* const* Args;
};

struct xrt_method_desc {
    const char* Name;
    uint32 NameSize;
    const xrt_func_sig* Sig;
    ptr NativeEntry;
    uint32 NativeAbi;
    xrt_xcall_proc XCallEntry;
};

struct xrt_method_table {
    uint32 Count;
    const xrt_method_desc* Methods;
};

struct xrt_func_param {
    const char* Name;
    uint32 NameSize;
    const xrt_type_desc* Type;
    xvalue DefaultValue;
    uint32 Flags;
};

struct xrt_func_sig {
    uint64 TypeId;
    const char* Name;
    uint32 NameSize;
    uint32 ParamCount;
    const xrt_func_param* Params;
    uint32 ReturnCount;
    const xrt_type_desc* const* ReturnTypes;
    uint32 Flags;
};

struct xrt_call_frame {
    xvalue Self;
    ptr Env;
    uint32 Argc;
    xvalue* Argv;
    xvalue Kwargs;
    xvalue Varargs;
    const xrt_func_sig* Sig;
};

struct xrt_call_result {
    uint32 Count;
    xvalue Values[XRT_CALL_RESULT_INLINE_COUNT];
    xvalue Overflow;
};

struct xrt_callable {
    uint32 RefCount;
    const xrt_func_sig* Sig;
    ptr NativeEntry;
    uint32 NativeAbi;
    xrt_xcall_proc XCallEntry;
    ptr Env;
    void (*DropEnv)(ptr pEnv);
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

typedef struct Dict_Key {
    ptr Key;
    uint32 KeyLen;
    uint32 Hash;
} Dict_Key;
typedef bool (*List_EachProc)(int64 pKey, ptr pVal, ptr pArg);
typedef bool (*Dict_EachProc)(Dict_Key* pKey, ptr pVal, ptr pArg);

void* xrtInit(void);
void xrtUnit(void);
void* xrtMalloc(size_t iSize);
void* xrtRealloc(void* pMem, size_t iSize);
void xrtFree(void* pMem);
void* xrtTempMemory(size_t iSize);
char* xrtCopyStr(const char* sText, size_t iSize);
char* xrtStrConcat(const char* sLeft, const char* sRight);
str xrtStrJoin(uint32 iCount, ...);
int xrtStrComp(const char* s1, const char* s2, size_t iSize, bool bCase);
const char* xrtFindStr(const char* sText, size_t iSize, const char* sSubText, size_t iSubSize, bool bCase);
uint64 xrtHash64(const void* key, size_t len);
int xrtI64ToStr(int64 num, char* buffer);
int xrtNumToStr(double num, char* buffer);
void* xrtTimeToStr(xtime iTime, int iFormat);
void* xrtStringifyJSON(xvalue varVal, int bFormat, void* pRetSize);
void* xrtStringifyXSON(xvalue varVal, int bFormat, uint32 iFlags, void* pRetSize);
bool xrtIntApprox(int64 a, int64 b);
bool xrtNumApprox(double a, double b);
bool xrtTimeApprox(xtime a, xtime b);
bool xrtStrApprox(const char* s1, size_t len1, const char* s2, size_t len2);

const xrt_type_desc* xrtTypeNull(void);
const xrt_type_desc* xrtTypeBool(void);
const xrt_type_desc* xrtTypeInt(void);
const xrt_type_desc* xrtTypeFloat(void);
const xrt_type_desc* xrtTypeString(void);
const xrt_type_desc* xrtTypeTime(void);
const xrt_type_desc* xrtTypePoint(void);
const xrt_type_desc* xrtTypeFunction(void);
const xrt_type_desc* xrtTypeFuture(void);
const xrt_type_desc* xrtTypeArray(void);
const xrt_type_desc* xrtTypeList(void);
const xrt_type_desc* xrtTypeSet(void);
const xrt_type_desc* xrtTypeDict(void);
const xrt_type_desc* xrtTypeType(void);
bool xrtTypeSame(const xrt_type_desc* pA, const xrt_type_desc* pB);
xvalue xrtTypeBoxValue(const xrt_type_desc* pType, const void* pObj);
uint64 xrtFuncSigTypeId(const xrt_func_sig* pSig);

xvalue xvoCreateNull(void);
xvalue xvoCreateBool(bool bVal);
xvalue xvoCreateInt(int64 iVal);
xvalue xvoCreateFloat(double fVal);
xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc);
xvalue xvoCreateTime(xtime tVal);
xvalue xvoCreatePoint(ptr point);
xvalue xvoCreateCallable(xrt_callable* pCallable, bool bColloc);
xvalue xvoCreateRecord(const xrt_type_desc* pType, const void* pData);
xvalue xvoCreateHandle(const xrt_type_desc* pType, ptr pHandle, uint32 iFlags);
xvalue xvoCreateArray(void);
xvalue xvoCreateList(void);
xvalue xvoCreateColl(void);
xvalue xvoCreateSet(void);
xvalue xvoCreateTable(void);
xvalue xvoCreateArrayArgs(uint32 iCount, ...);
xvalue xvoCreateListArgs(uint32 iCount, ...);
xvalue xvoCreateSetArgs(uint32 iCount, ...);
xvalue xvoCreateTableArgs(uint32 iCount, ...);
void xvoAddRef(xvalue pVal);
void xvoUnref(xvalue pVal);
xvalue xvoCopy(xvalue pVal);
xvalue xvoDeepCopy(xvalue pVal);
int xvoType(xvalue pVal);
bool xvoIsNull(xvalue pVal);
bool xvoGetBool(xvalue pVal);
int64 xvoGetInt(xvalue pVal);
double xvoGetFloat(xvalue pVal);
const char* xvoGetText(xvalue pVal);
xtime xvoGetTime(xvalue pVal);
ptr xvoGetPoint(xvalue pVal);
str xvoToString(xvalue pVal, uint32* pSize);
const xrt_type_desc* xvoTypeDesc(xvalue pVal);
bool xvoSetTypeDesc(xvalue pVal, const xrt_type_desc* pType);
ptr xvoGetRecordData(xvalue pVal);
ptr xvoGetHandleData(xvalue pVal);
xrt_callable* xvoGetCallable(xvalue pVal);
const xrt_func_sig* xvoCallableSig(xvalue pVal);
bool xvoInvoke(xvalue pFunc, xrt_call_frame* pFrame, xrt_call_result* pResult);
xvalue xvoArrayGetValue(xvalue pArr, uint32 index);
bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);
bool xvoArrayInsertValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc);
bool xvoArrayRemove(xvalue pArr, uint32 index, uint32 count);
xvalue xvoArrayTakeValue(xvalue pArr, uint32 index);
xvalue xvoArrayPopValue(xvalue pArr);
bool xvoArrayClear(xvalue pArr);
bool xvoArrayAlloc(xvalue pArr, uint32 count);
bool xvoArraySwap(xvalue pArr, uint32 index1, uint32 index2);
bool xvoArraySetValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc);
uint32 xvoArrayItemCount(xvalue pArr);
xvalue xvoListGetValue(xvalue pList, int64 index);
bool xvoListSetValue(xvalue pList, int64 index, xvalue pVal, bool bColloc);
bool xvoListRemove(xvalue pList, int64 index);
xvalue xvoListTakeValue(xvalue pList, int64 index);
bool xvoListClear(xvalue pList);
uint32 xvoListItemCount(xvalue pList);
xvalue xvoSetGetValueAt(xvalue pSet, uint32 iIndex);
uint32 xvoSetItemCount(xvalue pSet);
bool xvoCollSetValue(xvalue pColl, xvalue pVal, bool bColloc);
bool xvoCollExists(xvalue pColl, xvalue pVal);
uint32 xvoCollItemCount(xvalue pColl);
bool xvoSetAddValue(xvalue pSet, xvalue pVal, bool bColloc);
bool xvoSetExistsValue(xvalue pSet, xvalue pVal);
bool xvoSetRemoveValue(xvalue pSet, xvalue pVal);
bool xvoSetClear(xvalue pSet);
xvalue xvoTableGetValue(xvalue pTbl, const void* key, uint32 kl);
bool xvoTableSetValue(xvalue pTbl, const void* key, uint32 kl, xvalue pVal, bool bColloc);
bool xvoTableMerge(xvalue pTbl1, xvalue pTbl2, bool bReWrite);
bool xvoTableExists(xvalue pTbl, str key, uint32 kl);
bool xvoTableRemove(xvalue pTbl, str key, uint32 kl);
xvalue xvoTableTakeValue(xvalue pTbl, str key, uint32 kl);
bool xvoTableClear(xvalue pTbl);
bool xvoTableWalk(xvalue pTbl, Dict_EachProc procEach, void* pArg);
uint32 xvoTableItemCount(xvalue pTbl);
xlist xvoGetList(xvalue pVal);
xdict xvoGetTable(xvalue pVal);
bool xvoBasicEqual(xvalue pLeft, xvalue pRight);
int xvoBasicCompare(xvalue pLeft, xvalue pRight);
bool xvoValueTruthy(xvalue pVal);
xvalue xvoIndexGetI64(xvalue pVal, int64 iIndex);
xvalue xvoIndexGetKey(xvalue pVal, str sKey, uint32 iKeySize);
xvalue xvoIndexGetValue(xvalue pVal, xvalue pKey);
bool xvoValueContains(xvalue pValues, xvalue pItem);
void xvoPrint(xvalue pVal, bool bNewLine);
void xrtListWalk(xlist objList, List_EachProc procEach, ptr pArg);
void xrtDictWalk(xdict objHT, Dict_EachProc procEach, ptr pArg);

xtarray xrtTypedArrayCreate(const xrt_type_desc* pItemType, uint32 iMode);
void xrtTypedArrayDestroy(xtarray pArray);
ptr xrtTypedArrayAppend(xtarray pArray, const void* pItem);
bool xrtTypedArrayAppendValue(xtarray pArray, xvalue pVal);
bool xrtTypedArrayInsertValue(xtarray pArray, uint32 iPos, xvalue pVal);
xvalue xrtTypedArrayTakeValue(xtarray pArray, uint32 iPos);
xvalue xrtTypedArrayPopValue(xtarray pArray);
bool xrtTypedArraySetValue(xtarray pArray, uint32 iPos, xvalue pVal);
ptr xrtTypedArrayGet(xtarray pArray, uint32 iPos);
bool xrtTypedArrayRemove(xtarray pArray, uint32 iPos, uint32 iCount);
bool xrtTypedArrayClear(xtarray pArray);
bool xrtTypedArrayReverse(xtarray pArray);
int64 xrtTypedArrayIndexOfValue(xtarray pArray, xvalue pVal);
bool xrtTypedArrayContainsValue(xtarray pArray, xvalue pVal);
bool xrtTypedArrayExtend(xtarray pArray, xtarray pItems);
xtarray xrtTypedArrayConcat(xtarray pA, xtarray pB);
bool xrtTypedArrayEquals(xtarray pA, xtarray pB);
uint32 xrtTypedArrayCount(xtarray pArray);
xtlist xrtTypedListCreate(const xrt_type_desc* pItemType, uint32 iMode);
void xrtTypedListDestroy(xtlist pList);
bool xrtTypedListSetValue(xtlist pList, int64 iKey, xvalue pVal);
ptr xrtTypedListGet(xtlist pList, int64 iKey);
ptr xrtTypedListItemAt(xtlist pList, uint32 iIndex, int64* pKey);
bool xrtTypedListRemove(xtlist pList, int64 iKey);
xvalue xrtTypedListTakeValue(xtlist pList, int64 iKey);
bool xrtTypedListClear(xtlist pList);
bool xrtTypedListContainsValue(xtlist pList, xvalue pVal);
bool xrtTypedListMerge(xtlist pA, xtlist pB);
xtlist xrtTypedListClone(xtlist pList);
bool xrtTypedListEquals(xtlist pA, xtlist pB);
uint32 xrtTypedListCount(xtlist pList);
xtset xrtTypedSetCreate(const xrt_type_desc* pItemType, uint32 iMode);
void xrtTypedSetDestroy(xtset pSet);
bool xrtTypedSetAddValue(xtset pSet, xvalue pVal);
bool xrtTypedSetExists(xtset pSet, const void* pItem);
bool xrtTypedSetExistsValue(xtset pSet, xvalue pVal);
bool xrtTypedSetRemove(xtset pSet, const void* pItem);
bool xrtTypedSetRemoveValue(xtset pSet, xvalue pVal);
bool xrtTypedSetClear(xtset pSet);
bool xrtTypedSetMerge(xtset pSelf, xtset pSet);
xtset xrtTypedSetUnion(xtset pA, xtset pB);
xtset xrtTypedSetIntersection(xtset pA, xtset pB);
xtset xrtTypedSetDifference(xtset pA, xtset pB);
xtset xrtTypedSetSymmetricDifference(xtset pA, xtset pB);
bool xrtTypedSetIsSubset(xtset pA, xtset pB, bool bProper);
bool xrtTypedSetIsSuperset(xtset pA, xtset pB, bool bProper);
bool xrtTypedSetEquals(xtset pA, xtset pB);
ptr xrtTypedSetItemAt(xtset pSet, uint32 iIndex);
uint32 xrtTypedSetCount(xtset pSet);
const xrt_type_desc* xrtTypedSetItemType(xtset pSet);
xtdict xrtTypedDictCreate(const xrt_type_desc* pItemType, uint32 iMode);
void xrtTypedDictDestroy(xtdict pDict);
bool xrtTypedDictSetValue(xtdict pDict, const void* sKey, uint32 iKeyLen, xvalue pVal);
ptr xrtTypedDictGet(xtdict pDict, const void* sKey, uint32 iKeyLen);
ptr xrtTypedDictItemAt(xtdict pDict, uint32 iIndex, const char** psKey, uint32* piKeyLen);
uint32 xrtTypedDictCount(xtdict pDict);
bool xrtTypedDictExists(xtdict pDict, const void* sKey, uint32 iKeyLen);
bool xrtTypedDictRemove(xtdict pDict, const void* sKey, uint32 iKeyLen);
xvalue xrtTypedDictTakeValue(xtdict pDict, const void* sKey, uint32 iKeyLen);
bool xrtTypedDictClear(xtdict pDict);
bool xrtTypedDictMerge(xtdict pA, xtdict pB);
xtdict xrtTypedDictClone(xtdict pDict);
bool xrtTypedDictEquals(xtdict pA, xtdict pB);
bool xrtTypeUnboxValue(const xrt_type_desc* pType, xvalue pVal, ptr pOut);
void xrtTypeDropValue(const xrt_type_desc* pType, ptr pObj);

void xrtCallResultInit(xrt_call_result* pResult);
void xrtCallResultUnit(xrt_call_result* pResult);
bool xrtCallResultSetValue(xrt_call_result* pResult, uint32 iIndex, xvalue pVal, bool bColloc);
xrt_callable* xrtCallableCreate(const xrt_func_sig* pSig, ptr pNativeEntry, uint32 iNativeAbi, xrt_xcall_proc pXCallEntry, ptr pEnv, void (*pDropEnv)(ptr pEnv));
const xrt_func_sig* xrtCallableSig(const xrt_callable* pCallable);
void xrtCallableAddRef(xrt_callable* pCallable);
void xrtCallableUnref(xrt_callable* pCallable);
bool xrtCallableInvoke(xrt_callable* pCallable, xrt_call_frame* pFrame, xrt_call_result* pResult);

#ifndef XL6_RUNTIME_CORE_IMPL
#define XL6_RUNTIME_CORE_IMPL
static void xl6_runtime_fatal(int code)
{
    exit(code);
}

static xvalue xl6_xrt_retain(xvalue value)
{
    if ( value != 0 ) {
        xvoAddRef(value);
    }
    return value;
}

static xvalue xl6_xrt_value_copy_release(xvalue value, int deep)
{
    xvalue result = deep ? xvoDeepCopy(value) : xvoCopy(value);
    if ( value != 0 ) {
        xvoUnref(value);
    }
    return result;
}
#endif

#ifdef XL_RUNTIME_NEED_LIFECYCLE
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

#ifndef XL6_NO_XRT_LIFECYCLE
#ifndef XL6_USE_XRT_LIFECYCLE
#define XL6_USE_XRT_LIFECYCLE 1
#endif
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
#ifdef XL6_USE_XRT_LIFECYCLE
        if ( xrtInit() == 0 ) { return 0; }
#endif
    }
    xl__runtime_init_count += 1;
    return 1;
}

void xlUnit(void)
{
    if ( xl__runtime_init_count <= 0 ) { return; }
    xl__runtime_init_count -= 1;
    if ( xl__runtime_init_count == 0 ) {
#ifdef XL6_USE_XRT_LIFECYCLE
        xrtUnit();
#endif
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

#ifdef XL_RUNTIME_NEED_STRING
static char* xl6_xrt_str_dup(const char* value)
{
    return (char*)xrtCopyStr((str)value, 0);
}

static char* xl6_xrt_str_concat(const char* left, const char* right)
{
    return (char*)xrtStrConcat((str)left, (str)right);
}

static char* xl6_xrt_fstr_join(int count, ...)
{
    va_list ap;
    int i;
    char* result;
    char** parts;
    if ( count <= 0 ) { return (char*)xrtCopyStr((str)"", 0); }
    parts = (char**)xrtMalloc(sizeof(char*) * (size_t)count);
    if ( parts == 0 ) { return (char*)xrtCopyStr((str)"", 0); }
    va_start(ap, count);
    for ( i = 0; i < count; ++i ) {
        parts[i] = va_arg(ap, char*);
    }
    va_end(ap);
    switch ( count ) {
    case 1: result = (char*)xrtCopyStr((str)parts[0], 0); break;
    case 2: result = (char*)xrtStrJoin(2, parts[0], parts[1]); break;
    case 3: result = (char*)xrtStrJoin(3, parts[0], parts[1], parts[2]); break;
    case 4: result = (char*)xrtStrJoin(4, parts[0], parts[1], parts[2], parts[3]); break;
    case 5: result = (char*)xrtStrJoin(5, parts[0], parts[1], parts[2], parts[3], parts[4]); break;
    case 6: result = (char*)xrtStrJoin(6, parts[0], parts[1], parts[2], parts[3], parts[4], parts[5]); break;
    default:
        {
            size_t total = 0;
            size_t pos = 0;
            for ( i = 0; i < count; ++i ) { total += parts[i] ? strlen(parts[i]) : 0; }
            result = (char*)xrtMalloc(total + 1);
            if ( result != 0 ) {
                for ( i = 0; i < count; ++i ) {
                    size_t len = parts[i] ? strlen(parts[i]) : 0;
                    if ( len > 0 ) { memcpy(result + pos, parts[i], len); pos += len; }
                }
                result[total] = 0;
            }
        }
        break;
    }
    xrtFree(parts);
    return result;
}
#endif

#ifdef XL_RUNTIME_NEED_XVALUE
static uint64 xl6_xrt_type_name_id(const char* name, uint32 size)
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

static const char* xl6_xrt_type_name_from_id(uint64 typeId)
{
    uint32 low = (uint32)typeId;
#define XL6_XRT_MATCH_TYPE_NAME(name, size) if ( low == (uint32)xl6_xrt_type_name_id(name, size) ) { return name; }
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

static xvalue xl6_xrt_bind_type(xvalue value, const xrt_type_desc* desc)
{
    if ( value != 0 && desc != 0 ) {
        (void)xvoSetTypeDesc(value, desc);
    }
    return value;
}

static uint64 xl6_xrt_type_value_id(xvalue value, bool release)
{
    uint64 result;
    const xrt_type_desc* desc;
    const xrt_func_sig* sig;
    int type;
    if ( value == 0 ) { return xl6_xrt_type_name_id("null", 4); }
    type = xvoType(value);
    switch ( type ) {
    case XL6_XVO_DT_NULL: result = xl6_xrt_type_name_id("null", 4); break;
    case XL6_XVO_DT_BOOL: result = xl6_xrt_type_name_id("bool", 4); break;
    case XL6_XVO_DT_INT: result = xl6_xrt_type_name_id("int", 3); break;
    case XL6_XVO_DT_FLOAT: result = xl6_xrt_type_name_id("float", 5); break;
    case XL6_XVO_DT_TEXT: result = xl6_xrt_type_name_id("string", 6); break;
    case XL6_XVO_DT_TIME: result = xl6_xrt_type_name_id("time", 4); break;
    case XL6_XVO_DT_POINT: result = xl6_xrt_type_name_id("point", 5); break;
    case XL6_XVO_DT_FUNC:
        sig = xvoCallableSig(value);
        result = (sig != 0 && xrtFuncSigTypeId(sig) != 0) ? xrtFuncSigTypeId(sig) : xl6_xrt_type_name_id("function", 8);
        break;
    case XL6_XVO_DT_ARRAY:
    case XL6_XVO_DT_LIST:
    case XL6_XVO_DT_COLL:
    case XL6_XVO_DT_TABLE:
    case XL6_XVO_DT_CLASS:
    case XL6_XVO_DT_NATIVE:
        desc = xvoTypeDesc(value);
        result = (desc != 0 && desc->Name != 0 && desc->NameSize > 0) ? xl6_xrt_type_name_id(desc->Name, desc->NameSize) : xl6_xrt_type_name_id("var", 3);
        break;
    default:
        result = xl6_xrt_type_name_id("var", 3);
        break;
    }
    if ( release ) { xvoUnref(value); }
    return result;
}

void xl6_xrt_print(xvalue values, bool newline)
{
    xvoPrint(values, newline);
}

static xvalue xl6_xrt_index_safe_get_i64(xvalue values, int64 index)
{
    return xvoIndexGetI64(values, index);
}

static xvalue xl6_xrt_index_safe_get_key(xvalue values, const char* key)
{
    return xvoIndexGetKey(values, (str)key, 0);
}

static xvalue xl6_xrt_dict_get_var_key(xvalue values, xvalue key)
{
    xvalue result = xvoIndexGetValue(values, key);
    if ( key != 0 ) { xvoUnref(key); }
    return result;
}

static xvalue xl6_xrt_index_safe_get_var_key(xvalue values, xvalue key)
{
    xvalue result = xvoIndexGetValue(values, key);
    if ( key != 0 ) { xvoUnref(key); }
    return result;
}

static void* xl6_xrt_typed_dict_get_var_key(xtdict values, xvalue key)
{
    void* result = 0;
    if ( key != 0 && xvoType(key) == XL6_XVO_DT_TEXT ) {
        result = xrtTypedDictGet(values, xvoGetText(key), 0);
    }
    if ( key != 0 ) { xvoUnref(key); }
    return result;
}

static xvalue xl6_xrt_null_release(xvalue values)
{
    if ( values != 0 ) { xvoUnref(values); }
    return xvoCreateNull();
}

static xvalue xl6_xrt_array_index_retain_release(xvalue values, int64 index)
{
    xvalue result = xl6_xrt_retain(xvoArrayGetValue(values, (uint32)index));
    if ( values != 0 ) { xvoUnref(values); }
    return result;
}

static xvalue xl6_xrt_list_index_retain_release(xvalue values, int64 index)
{
    xvalue result = xl6_xrt_retain(xvoListGetValue(values, index));
    if ( values != 0 ) { xvoUnref(values); }
    return result;
}

static xvalue xl6_xrt_dict_get_key_retain_release(xvalue values, const char* key)
{
    xvalue result = xl6_xrt_retain(xvoTableGetValue(values, key, 0));
    if ( values != 0 ) { xvoUnref(values); }
    return result;
}

static xvalue xl6_xrt_dict_get_var_key_retain_release(xvalue values, xvalue key)
{
    xvalue result = xl6_xrt_retain(xvoIndexGetValue(values, key));
    if ( key != 0 ) { xvoUnref(key); }
    if ( values != 0 ) { xvoUnref(values); }
    return result;
}

static xvalue xl6_xrt_index_safe_get_i64_retain_release(xvalue values, int64 index)
{
    xvalue result = xl6_xrt_retain(xvoIndexGetI64(values, index));
    if ( values != 0 ) { xvoUnref(values); }
    return result;
}

static xvalue xl6_xrt_index_safe_get_key_retain_release(xvalue values, const char* key)
{
    xvalue result = xl6_xrt_retain(xvoIndexGetKey(values, (str)key, 0));
    if ( values != 0 ) { xvoUnref(values); }
    return result;
}

static xvalue xl6_xrt_index_safe_get_var_key_retain_release(xvalue values, xvalue key)
{
    xvalue result = xl6_xrt_retain(xvoIndexGetValue(values, key));
    if ( key != 0 ) { xvoUnref(key); }
    if ( values != 0 ) { xvoUnref(values); }
    return result;
}

static uint32 xl6_xrt_member_length(xvalue values)
{
    int type;
    if ( values == 0 || xvoIsNull(values) ) { return 0; }
    type = xvoType(values);
    if ( type == XL6_XVO_DT_ARRAY ) { return xvoArrayItemCount(values); }
    if ( type == XL6_XVO_DT_LIST ) { return xvoListItemCount(values); }
    if ( type == XL6_XVO_DT_COLL ) { return xvoCollItemCount(values); }
    if ( type == XL6_XVO_DT_TABLE ) { return xvoTableItemCount(values); }
    return 0;
}

static xvalue xl6_xrt_member_safe_length(xvalue values)
{
    if ( values == 0 || xvoIsNull(values) ) { return xvoCreateNull(); }
    return xvoCreateInt((int64)xl6_xrt_member_length(values));
}

static uint32 xl6_xrt_member_length_release(xvalue values)
{
    uint32 result = xl6_xrt_member_length(values);
    if ( values != 0 ) { xvoUnref(values); }
    return result;
}

static xvalue xl6_xrt_member_safe_length_release(xvalue values)
{
    xvalue result = xl6_xrt_member_safe_length(values);
    if ( values != 0 ) { xvoUnref(values); }
    return result;
}

static int xl6_xrt_type_is_numeric(int type)
{
    return type == XL6_XVO_DT_BOOL || type == XL6_XVO_DT_INT || type == XL6_XVO_DT_FLOAT;
}

static double xl6_xrt_value_to_double(xvalue value, int type)
{
    return type == XL6_XVO_DT_FLOAT ? xvoGetFloat(value) : (double)xvoGetInt(value);
}

static int xl6_xrt_basic_compare(xvalue left, xvalue right, int* outResult)
{
    int cmp;
    if ( outResult == 0 ) { return 0; }
    cmp = xvoBasicCompare(left, right);
    if ( cmp == 2 ) {
        *outResult = 0;
        return 0;
    }
    *outResult = cmp;
    return 1;
}

static int xl6_xrt_basic_equal(xvalue left, xvalue right)
{
    return xvoBasicEqual(left, right) ? 1 : (left == right);
}

static int xl6_xrt_basic_approx(xvalue left, xvalue right)
{
    int leftType = xvoType(left);
    int rightType = xvoType(right);
    if ( leftType == XL6_XVO_DT_TEXT || rightType == XL6_XVO_DT_TEXT ) {
        return leftType == XL6_XVO_DT_TEXT && rightType == XL6_XVO_DT_TEXT && xrtStrApprox(xvoGetText(left), 0, xvoGetText(right), 0);
    }
    if ( leftType == XL6_XVO_DT_TIME || rightType == XL6_XVO_DT_TIME ) {
        return leftType == XL6_XVO_DT_TIME && rightType == XL6_XVO_DT_TIME && xrtTimeApprox(xvoGetTime(left), xvoGetTime(right));
    }
    if ( xl6_xrt_type_is_numeric(leftType) && xl6_xrt_type_is_numeric(rightType) ) {
        if ( leftType == XL6_XVO_DT_FLOAT || rightType == XL6_XVO_DT_FLOAT ) {
            return xrtNumApprox(xl6_xrt_value_to_double(left, leftType), xl6_xrt_value_to_double(right, rightType));
        }
        return xrtIntApprox(xvoGetInt(left), xvoGetInt(right));
    }
    return 0;
}

static int xl6_xrt_basic_order(xvalue left, xvalue right, int op)
{
    int cmp = 0;
    if ( !xl6_xrt_basic_compare(left, right, &cmp) ) { return 0; }
    if ( op == 0 ) { return cmp < 0; }
    if ( op == 1 ) { return cmp <= 0; }
    if ( op == 2 ) { return cmp > 0; }
    return cmp >= 0;
}

static xvalue xl6_xrt_value_text_concat(xvalue left, xvalue right)
{
    return xvoCreateText(xrtStrConcat(xvoGetText(left), xvoGetText(right)), 0, true);
}

static xvalue xl6_xrt_array_concat(xvalue left, xvalue right)
{
    xvalue result = xvoCreateArray();
    uint32 leftCount;
    uint32 rightCount;
    uint32 i;

    if ( result == 0 ) {
        return xvoCreateNull();
    }
    leftCount = xvoArrayItemCount(left);
    for ( i = 0; i < leftCount; ++i ) {
        xvalue item = xvoArrayGetValue(left, i);
        if ( item != 0 ) {
            xvoAddRef(item);
        }
        if ( !xvoArrayAppendValue(result, item, true) ) {
            if ( item != 0 ) {
                xvoUnref(item);
            }
            xvoUnref(result);
            return xvoCreateNull();
        }
    }
    rightCount = xvoArrayItemCount(right);
    for ( i = 0; i < rightCount; ++i ) {
        xvalue item = xvoArrayGetValue(right, i);
        if ( item != 0 ) {
            xvoAddRef(item);
        }
        if ( !xvoArrayAppendValue(result, item, true) ) {
            if ( item != 0 ) {
                xvoUnref(item);
            }
            xvoUnref(result);
            return xvoCreateNull();
        }
    }
    return result;
}

static xvalue xl6_xrt_basic_binary(xvalue left, xvalue right, int op)
{
    int leftType = xvoType(left);
    int rightType = xvoType(right);
    if ( op == 0 && leftType == XL6_XVO_DT_TEXT && rightType == XL6_XVO_DT_TEXT ) {
        return xl6_xrt_value_text_concat(left, right);
    }
    if ( op == 0 && leftType == XL6_XVO_DT_ARRAY && rightType == XL6_XVO_DT_ARRAY ) {
        return xl6_xrt_array_concat(left, right);
    }
    if ( !xl6_xrt_type_is_numeric(leftType) || !xl6_xrt_type_is_numeric(rightType) ) { return xvoCreateNull(); }
    if ( op >= 5 ) {
        int64 l;
        int64 r;
        if ( leftType == XL6_XVO_DT_FLOAT || rightType == XL6_XVO_DT_FLOAT ) { return xvoCreateNull(); }
        l = xvoGetInt(left);
        r = xvoGetInt(right);
        if ( op == 5 ) { return xvoCreateInt(l & r); }
        if ( op == 6 ) { return xvoCreateInt(l | r); }
        if ( op == 7 ) { return xvoCreateInt(l ^ r); }
        if ( r < 0 || r >= 64 ) { return xvoCreateNull(); }
        if ( op == 8 ) { return xvoCreateInt((int64)((uint64)l << (uint32)r)); }
        return xvoCreateInt((int64)((uint64)l >> (uint32)r));
    }
    if ( leftType == XL6_XVO_DT_FLOAT || rightType == XL6_XVO_DT_FLOAT ) {
        double l = xl6_xrt_value_to_double(left, leftType);
        double r = xl6_xrt_value_to_double(right, rightType);
        if ( op == 0 ) { return xvoCreateFloat(l + r); }
        if ( op == 1 ) { return xvoCreateFloat(l - r); }
        if ( op == 2 ) { return xvoCreateFloat(l * r); }
        if ( op == 3 ) { return r != 0.0 ? xvoCreateFloat(l / r) : xvoCreateNull(); }
        return xvoCreateNull();
    }
    {
        int64 l = xvoGetInt(left);
        int64 r = xvoGetInt(right);
        if ( op == 0 ) { return xvoCreateInt(l + r); }
        if ( op == 1 ) { return xvoCreateInt(l - r); }
        if ( op == 2 ) { return xvoCreateInt(l * r); }
        if ( op == 3 ) { return r != 0 ? xvoCreateInt(l / r) : xvoCreateNull(); }
        if ( op == 4 ) { return r != 0 ? xvoCreateInt(l % r) : xvoCreateNull(); }
    }
    return xvoCreateNull();
}

static int xl6_xrt_value_truthy(xvalue value)
{
    return xvoValueTruthy(value) ? 1 : 0;
}

static xvalue xl6_xrt_basic_unary(xvalue value, int op)
{
    int type = xvoType(value);
    if ( !xl6_xrt_type_is_numeric(type) ) { return xvoCreateNull(); }
    if ( type == XL6_XVO_DT_FLOAT ) {
        double v = xvoGetFloat(value);
        if ( op == 0 ) { return xvoCreateFloat(v); }
        if ( op == 1 ) { return xvoCreateFloat(-v); }
        return xvoCreateNull();
    }
    {
        int64 v = xvoGetInt(value);
        if ( op == 0 ) { return xvoCreateInt(v); }
        if ( op == 1 ) { return xvoCreateInt(-v); }
        return xvoCreateInt(~v);
    }
}

#ifndef XL6_XRT_VALUE_EXPR_RELEASE_HELPERS
#define XL6_XRT_VALUE_EXPR_RELEASE_HELPERS
static int xl6_xrt_value_truthy_release(xvalue value)
{
    int result = value != 0 ? xl6_xrt_value_truthy(value) : 0;
    if ( value != 0 ) { xvoUnref(value); }
    return result;
}

static xvalue xl6_xrt_basic_unary_release(xvalue value, int op)
{
    xvalue result = value != 0 ? xl6_xrt_basic_unary(value, op) : xvoCreateNull();
    if ( value != 0 ) { xvoUnref(value); }
    return result;
}

static xvalue xl6_xrt_basic_binary_release(xvalue left, xvalue right, int op)
{
    xvalue result = (left != 0 && right != 0) ? xl6_xrt_basic_binary(left, right, op) : xvoCreateNull();
    if ( left != 0 ) { xvoUnref(left); }
    if ( right != 0 ) { xvoUnref(right); }
    return result;
}
#endif

static int xl6_xrt_value_contains(xvalue values, xvalue item)
{
    int found = xvoValueContains(values, item) ? 1 : 0;
    if ( item != 0 ) { xvoUnref(item); }
    return found;
}

static int xl6_xrt_value_contains_release(xvalue values, xvalue item)
{
    int found = xl6_xrt_value_contains(values, item);
    if ( values != 0 ) { xvoUnref(values); }
    return found;
}

static int xl6_xrt_array_contains(xvalue values, xvalue item) { return xl6_xrt_value_contains(values, item); }
static int xl6_xrt_list_contains(xvalue values, xvalue item) { return xl6_xrt_value_contains(values, item); }
static int xl6_xrt_set_contains(xvalue values, xvalue item) { return xl6_xrt_value_contains(values, item); }
static int xl6_xrt_array_contains_release(xvalue values, xvalue item) { return xl6_xrt_value_contains_release(values, item); }
static int xl6_xrt_list_contains_release(xvalue values, xvalue item) { return xl6_xrt_value_contains_release(values, item); }
static int xl6_xrt_set_contains_release(xvalue values, xvalue item) { return xl6_xrt_value_contains_release(values, item); }

static int xl6_xrt_array_remove_value_release(xvalue values, xvalue item)
{
    uint32 count;
    uint32 i;
    int ok;

    count = xvoArrayItemCount(values);
    ok = 0;
    for ( i = 0; i < count; ++i ) {
        if ( xl6_xrt_basic_equal(xvoArrayGetValue(values, i), item) ) {
            ok = xvoArrayRemove(values, i, 1) ? 1 : 0;
            break;
        }
    }
    if ( item != 0 ) { xvoUnref(item); }
    return ok;
}

static int xl6_xrt_value_remove_value_release(xvalue values, xvalue item)
{
    int ok = 0;
    if ( values != 0 && xvoType(values) == XL6_XVO_DT_ARRAY ) {
        ok = xl6_xrt_array_remove_value_release(values, item);
    } else if ( item != 0 ) {
        xvoUnref(item);
    }
    return ok;
}

static int xl6_xrt_table_exists_release(xvalue values, const char* key)
{
    int found = xvoTableExists(values, (str)key, 0) ? 1 : 0;
    if ( values != 0 ) { xvoUnref(values); }
    return found;
}

static int xl6_xrt_array_push_value(xvalue values, xvalue item)
{
    if ( !xvoArrayAppendValue(values, item, true) ) {
        if ( item != 0 ) { xvoUnref(item); }
        return 0;
    }
    return 1;
}

static int xl6_xrt_array_insert_value(xvalue values, uint32 index, xvalue item)
{
    if ( !xvoArrayInsertValue(values, index, item, true) ) {
        if ( item != 0 ) { xvoUnref(item); }
        return 0;
    }
    return 1;
}

static int xl6_xrt_set_add_value(xvalue values, xvalue item)
{
    if ( !xvoSetAddValue(values, item, true) ) {
        if ( item != 0 ) { xvoUnref(item); }
        return 0;
    }
    return 1;
}

static int xl6_xrt_set_remove_value_release(xvalue values, xvalue item)
{
    int ok = xvoSetRemoveValue(values, item) ? 1 : 0;
    if ( item != 0 ) { xvoUnref(item); }
    return ok;
}

static xvalue xl6_xrt_array_take_value_or_null(xvalue values, uint32 index)
{
    xvalue result = xvoArrayTakeValue(values, index);
    return result != 0 ? result : xvoCreateNull();
}

static xvalue xl6_xrt_array_pop_value_or_null(xvalue values)
{
    xvalue result = xvoArrayPopValue(values);
    return result != 0 ? result : xvoCreateNull();
}

static xvalue xl6_xrt_list_take_value_or_null(xvalue values, int64 key)
{
    xvalue result = xvoListTakeValue(values, key);
    return result != 0 ? result : xvoCreateNull();
}

static xvalue xl6_xrt_table_take_value_or_null(xvalue values, const char* key)
{
    xvalue result = xvoTableTakeValue(values, (str)key, 0);
    return result != 0 ? result : xvoCreateNull();
}

static int xl6_xrt_typed_array_append_value_release(xtarray values, xvalue item)
{
    int ok = xrtTypedArrayAppendValue(values, item) ? 1 : 0;
    if ( item != 0 ) { xvoUnref(item); }
    return ok;
}

static int xl6_xrt_typed_array_insert_value_release(xtarray values, uint32 index, xvalue item)
{
    int ok = xrtTypedArrayInsertValue(values, index, item) ? 1 : 0;
    if ( item != 0 ) { xvoUnref(item); }
    return ok;
}

static xvalue xl6_xrt_typed_array_take_value_or_null(xtarray values, uint32 index)
{
    xvalue result = xrtTypedArrayTakeValue(values, index);
    return result != 0 ? result : xvoCreateNull();
}

static xvalue xl6_xrt_typed_array_pop_value_or_null(xtarray values)
{
    xvalue result = xrtTypedArrayPopValue(values);
    return result != 0 ? result : xvoCreateNull();
}

static int64 xl6_xrt_typed_array_index_of_value_release(xtarray values, xvalue item)
{
    int64 result = xrtTypedArrayIndexOfValue(values, item);
    if ( item != 0 ) { xvoUnref(item); }
    return result;
}

static int xl6_xrt_typed_array_remove_value_release(xtarray values, xvalue item)
{
    int64 index = xrtTypedArrayIndexOfValue(values, item);
    int ok = 0;
    if ( index >= 0 ) {
        ok = xrtTypedArrayRemove(values, (uint32)index + 1u, 1u) ? 1 : 0;
    }
    if ( item != 0 ) { xvoUnref(item); }
    return ok;
}

static int xl6_xrt_typed_array_contains_value_release(xtarray values, xvalue item)
{
    int ok = xrtTypedArrayContainsValue(values, item) ? 1 : 0;
    if ( item != 0 ) { xvoUnref(item); }
    return ok;
}

static xvalue xl6_xrt_typed_list_take_value_or_null(xtlist values, int64 key)
{
    xvalue result = xrtTypedListTakeValue(values, key);
    return result != 0 ? result : xvoCreateNull();
}

static int xl6_xrt_typed_list_contains_value_release(xtlist values, xvalue item)
{
    int ok = xrtTypedListContainsValue(values, item) ? 1 : 0;
    if ( item != 0 ) { xvoUnref(item); }
    return ok;
}

static xtlist xl6_xrt_typed_list_concat(xtlist left, xtlist right)
{
    xtlist result = xrtTypedListClone(left);
    if ( result == 0 ) {
        return 0;
    }
    if ( !xrtTypedListMerge(result, right) ) {
        xrtTypedListDestroy(result);
        return 0;
    }
    return result;
}

static xvalue xl6_xrt_typed_dict_take_value_or_null(xtdict values, const char* key, uint32 keyLen)
{
    xvalue result = xrtTypedDictTakeValue(values, key, keyLen);
    return result != 0 ? result : xvoCreateNull();
}

static xtlist xl6_xrt_typed_list_without_key(xtlist values, int64 key)
{
    xtlist result = xrtTypedListClone(values);
    if ( result != 0 ) {
        xrtTypedListRemove(result, key);
    }
    return result;
}

static xtdict xl6_xrt_typed_dict_concat(xtdict left, xtdict right)
{
    xtdict result = xrtTypedDictClone(left);
    if ( result == 0 ) {
        return 0;
    }
    if ( !xrtTypedDictMerge(result, right) ) {
        xrtTypedDictDestroy(result);
        return 0;
    }
    return result;
}

static xtdict xl6_xrt_typed_dict_without_key(xtdict values, const char* key, uint32 keyLen)
{
    xtdict result = xrtTypedDictClone(values);
    if ( result != 0 ) {
        xrtTypedDictRemove(result, key, keyLen);
    }
    return result;
}

static int xl6_xrt_typed_set_relation(xtset left, xtset right, int mode)
{
    if ( mode == 0 ) {
        return xrtTypedSetIsSubset(left, right, false) ? 1 : 0;
    }
    if ( mode == 1 ) {
        return xrtTypedSetIsSubset(left, right, true) ? 1 : 0;
    }
    if ( mode == 2 ) {
        return xrtTypedSetIsSuperset(left, right, false) ? 1 : 0;
    }
    return xrtTypedSetIsSuperset(left, right, true) ? 1 : 0;
}

static int xl6_xrt_typed_set_add_value_release(xtset values, xvalue item)
{
    int ok = xrtTypedSetAddValue(values, item) ? 1 : 0;
    if ( item != 0 ) { xvoUnref(item); }
    return ok;
}

static int xl6_xrt_typed_set_value_op(xtset values, xvalue item, int remove)
{
    const xrt_type_desc* type = values != 0 ? xrtTypedSetItemType(values) : 0;
    ptr raw;
    int ok;
    if ( type == 0 || type->Size == 0u ) {
        if ( item != 0 ) { xvoUnref(item); }
        return 0;
    }
    raw = xrtMalloc(type->Size);
    if ( raw == 0 ) {
        if ( item != 0 ) { xvoUnref(item); }
        return 0;
    }
    if ( !xrtTypeUnboxValue(type, item, raw) ) {
        xrtFree(raw);
        if ( item != 0 ) { xvoUnref(item); }
        return 0;
    }
    ok = remove ? (xrtTypedSetRemove(values, raw) ? 1 : 0) : (xrtTypedSetExists(values, raw) ? 1 : 0);
    xrtTypeDropValue(type, raw);
    xrtFree(raw);
    if ( item != 0 ) { xvoUnref(item); }
    return ok;
}

static int xl6_xrt_typed_set_contains_value_release(xtset values, xvalue item)
{
    return xl6_xrt_typed_set_value_op(values, item, 0);
}

static int xl6_xrt_typed_set_remove_value_release(xtset values, xvalue item)
{
    return xl6_xrt_typed_set_value_op(values, item, 1);
}

static int xl6_xrt_container_push_value(xvalue values, xvalue item)
{
    if ( values != 0 && xvoType(values) == XL6_XVO_DT_ARRAY ) {
        return xl6_xrt_array_push_value(values, item);
    }
    if ( item != 0 ) { xvoUnref(item); }
    return 0;
}

static int xl6_xrt_container_add_value(xvalue values, xvalue item)
{
    if ( values != 0 && xvoType(values) == XL6_XVO_DT_COLL ) {
        return xl6_xrt_set_add_value(values, item);
    }
    if ( item != 0 ) { xvoUnref(item); }
    return 0;
}

static int xl6_xrt_container_insert_value(xvalue values, xvalue index, xvalue item)
{
    int ok = 0;
    if ( values != 0 && index != 0 &&
         xvoType(values) == XL6_XVO_DT_ARRAY &&
         xvoType(index) == XL6_XVO_DT_INT ) {
        ok = xl6_xrt_array_insert_value(values, (uint32)xvoGetInt(index), item);
    } else if ( item != 0 ) {
        xvoUnref(item);
    }
    if ( index != 0 ) { xvoUnref(index); }
    return ok;
}

static int xl6_xrt_container_remove_value(xvalue values, xvalue key, uint32 count, int hasCount)
{
    int ok = 0;
    int valuesType = values != 0 ? xvoType(values) : -1;
    int keyType = key != 0 ? xvoType(key) : -1;
    if ( valuesType == XL6_XVO_DT_ARRAY && keyType == XL6_XVO_DT_INT ) {
        ok = xvoArrayRemove(values, (uint32)xvoGetInt(key), hasCount ? count : 1u) ? 1 : 0;
    } else if ( !hasCount && valuesType == XL6_XVO_DT_LIST && keyType == XL6_XVO_DT_INT ) {
        ok = xvoListRemove(values, xvoGetInt(key)) ? 1 : 0;
    } else if ( !hasCount && valuesType == XL6_XVO_DT_COLL ) {
        ok = xvoSetRemoveValue(values, key) ? 1 : 0;
    } else if ( !hasCount && valuesType == XL6_XVO_DT_TABLE && keyType == XL6_XVO_DT_TEXT ) {
        ok = xvoTableRemove(values, (str)xvoGetText(key), 0) ? 1 : 0;
    }
    if ( key != 0 ) { xvoUnref(key); }
    return ok;
}

bool xl6_xrt_std_table_remove(xvalue table, const char* key, uint32 keyLen)
{
    return xvoTableRemove(table, (str)key, keyLen);
}

static xvalue xl6_xrt_container_take_value(xvalue values, xvalue key)
{
    xvalue result = 0;
    int valuesType = values != 0 ? xvoType(values) : -1;
    int keyType = key != 0 ? xvoType(key) : -1;
    if ( valuesType == XL6_XVO_DT_ARRAY && keyType == XL6_XVO_DT_INT ) {
        result = xvoArrayTakeValue(values, (uint32)xvoGetInt(key));
    } else if ( valuesType == XL6_XVO_DT_LIST && keyType == XL6_XVO_DT_INT ) {
        result = xvoListTakeValue(values, xvoGetInt(key));
    } else if ( valuesType == XL6_XVO_DT_TABLE && keyType == XL6_XVO_DT_TEXT ) {
        result = xvoTableTakeValue(values, (str)xvoGetText(key), 0);
    }
    if ( key != 0 ) { xvoUnref(key); }
    return result != 0 ? result : xvoCreateNull();
}

static xvalue xl6_xrt_container_pop_value(xvalue values)
{
    xvalue result = 0;
    if ( values != 0 && xvoType(values) == XL6_XVO_DT_ARRAY ) {
        result = xvoArrayPopValue(values);
    }
    return result != 0 ? result : xvoCreateNull();
}

static int xl6_xrt_container_clear(xvalue values)
{
    int type = values != 0 ? xvoType(values) : -1;
    if ( type == XL6_XVO_DT_ARRAY ) { return xvoArrayClear(values) ? 1 : 0; }
    if ( type == XL6_XVO_DT_LIST ) { return xvoListClear(values) ? 1 : 0; }
    if ( type == XL6_XVO_DT_COLL ) { return xvoSetClear(values) ? 1 : 0; }
    if ( type == XL6_XVO_DT_TABLE ) { return xvoTableClear(values) ? 1 : 0; }
    return 0;
}

static int xl6_xrt_container_reserve(xvalue values, uint32 count)
{
    return values != 0 && xvoType(values) == XL6_XVO_DT_ARRAY ? (xvoArrayAlloc(values, count) ? 1 : 0) : 0;
}

static int xl6_xrt_container_swap(xvalue values, uint32 index1, uint32 index2)
{
    return values != 0 && xvoType(values) == XL6_XVO_DT_ARRAY ? (xvoArraySwap(values, index1, index2) ? 1 : 0) : 0;
}

#define xl6_xrt_array_varargs xvoCreateArrayArgs
#define xl6_xrt_list_literal xvoCreateListArgs
#define xl6_xrt_set_literal xvoCreateSetArgs
#define xl6_xrt_dict_varargs xvoCreateTableArgs

#ifndef XL6_XRT_TAKE_CSTR_HELPERS
#define XL6_XRT_TAKE_CSTR_HELPERS
static const char* xl6_xrt_temp_cstr(const char* value)
{
    size_t len;
    char* text;

    if ( value == 0 ) {
        return "";
    }
    len = strlen(value);
    text = (char*)xrtTempMemory(len + 1);
    if ( text == 0 ) {
        return "";
    }
    memcpy(text, value, len + 1);
    return text;
}

static const char* xl6_xrt_take_cstr(void* value)
{
    const char* source;
    size_t len;
    char* text;

    if ( value == 0 ) {
        return "";
    }
    source = (const char*)value;
    len = strlen(source);
    text = (char*)xrtTempMemory(len + 1);
    if ( text == 0 ) {
        xrtFree(value);
        return "";
    }
    memcpy(text, source, len + 1);
    xrtFree(value);
    return text;
}
#endif

#ifndef XL6_XRT_VALUE_STRING_HELPERS
#define XL6_XRT_VALUE_STRING_HELPERS
static const char* xl6_xrt_value_to_string(xvalue value)
{
    return xl6_xrt_take_cstr((char*)xvoToString(value, 0));
}

static const char* xl6_xrt_value_to_string_release(xvalue value)
{
    const char* text = xl6_xrt_value_to_string(value);
    if ( value != 0 ) { xvoUnref(value); }
    return text;
}
#endif

#ifndef XL6_XRT_VALUE_RELEASE_CONVERT_HELPERS
#define XL6_XRT_VALUE_RELEASE_CONVERT_HELPERS
static int64 xl6_xrt_value_to_i64_release(xvalue value)
{
    int64 result = value != 0 ? xvoGetInt(value) : 0;
    if ( value != 0 ) { xvoUnref(value); }
    return result;
}

static double xl6_xrt_value_to_f64_release(xvalue value)
{
    double result = value != 0 ? xvoGetFloat(value) : 0.0;
    if ( value != 0 ) { xvoUnref(value); }
    return result;
}

static int xl6_xrt_value_to_bool_release(xvalue value)
{
    int result = value != 0 && xvoGetBool(value) ? 1 : 0;
    if ( value != 0 ) { xvoUnref(value); }
    return result;
}

static int64 xl6_xrt_value_to_time_release(xvalue value)
{
    int64 result = value != 0 ? xvoGetTime(value) : 0;
    if ( value != 0 ) { xvoUnref(value); }
    return result;
}

static void* xl6_xrt_value_to_point_release(xvalue value)
{
    void* result = value != 0 ? xvoGetPoint(value) : 0;
    if ( value != 0 ) { xvoUnref(value); }
    return result;
}
#endif

static xvalue xl6_xrt_typed_box_or_null(const xrt_type_desc* type, const void* item)
{
    if ( item == 0 ) { return xvoCreateNull(); }
    return xrtTypeBoxValue(type, item);
}

typedef xvalue (*xl6_xrt_typed_item_box_fn)(void* item);

static void* xl6_xrt_typed_require_item(void* item)
{
    if ( item == 0 ) { xl6_runtime_fatal(3); }
    return item;
}

static void* xl6_xrt_typed_array_get_at(xtarray values, uint32 index)
{
    return xrtTypedArrayGet(values, index + 1u);
}

static xvalue xl6_xrt_typed_array_to_xvalue_map(xtarray values, xl6_xrt_typed_item_box_fn box)
{
    xvalue result = xvoCreateArray();
    uint32 count = xrtTypedArrayCount(values);
    uint32 i;
    if ( result == 0 ) {
        return xvoCreateNull();
    }
    for ( i = 0; i < count; ++i ) {
        void* rawItem = xl6_xrt_typed_array_get_at(values, i);
        xvalue item = rawItem != 0 && box != 0 ? box(rawItem) : xvoCreateNull();
        if ( !xvoArrayAppendValue(result, item, true) && item != 0 ) {
            xvoUnref(item);
        }
    }
    return result;
}

static bool xl6_xrt_typed_array_set_at(xtarray values, uint32 index, xvalue value)
{
    return xrtTypedArraySetValue(values, index + 1u, value);
}

static xvalue xl6_xrt_typed_array_to_xvalue(const xrt_type_desc* type, xtarray values)
{
    xvalue result = xvoCreateArray();
    uint32 count = xrtTypedArrayCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        xvalue item = xl6_xrt_typed_box_or_null(type, xl6_xrt_typed_array_get_at(values, i));
        if ( !xvoArrayAppendValue(result, item, true) && item != 0 ) { xvoUnref(item); }
    }
    return result;
}

static xtarray xl6_xrt_typed_array_copy_mode(const xrt_type_desc* type, xtarray values, uint32 mode)
{
    xtarray result = xrtTypedArrayCreate(type, mode);
    uint32 count = xrtTypedArrayCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        xvalue item = xl6_xrt_typed_box_or_null(type, xl6_xrt_typed_array_get_at(values, i));
        if ( !xrtTypedArrayAppendValue(result, item) && item != 0 ) { xvoUnref(item); }
        else if ( item != 0 ) { xvoUnref(item); }
    }
    return result;
}

static xtarray xl6_xrt_typed_array_copy(const xrt_type_desc* type, xtarray values)
{
    return xl6_xrt_typed_array_copy_mode(type, values, XRT_OBJMODE_LOCAL);
}

static xvalue xl6_xrt_typed_list_to_xvalue(const xrt_type_desc* type, xtlist values)
{
    xvalue result = xvoCreateList();
    uint32 count = xrtTypedListCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        int64 key = 0;
        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedListItemAt(values, i, &key));
        if ( !xvoListSetValue(result, key, item, true) && item != 0 ) { xvoUnref(item); }
    }
    return result;
}

static xtlist xl6_xrt_typed_list_copy_mode(const xrt_type_desc* type, xtlist values, uint32 mode)
{
    xtlist result = xrtTypedListCreate(type, mode);
    uint32 count = xrtTypedListCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        int64 key = 0;
        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedListItemAt(values, i, &key));
        if ( !xrtTypedListSetValue(result, key, item) && item != 0 ) { xvoUnref(item); }
        else if ( item != 0 ) { xvoUnref(item); }
    }
    return result;
}

static xtlist xl6_xrt_typed_list_copy(const xrt_type_desc* type, xtlist values)
{
    return xl6_xrt_typed_list_copy_mode(type, values, XRT_OBJMODE_LOCAL);
}

static xvalue xl6_xrt_typed_set_to_xvalue(const xrt_type_desc* type, xtset values)
{
    xvalue result = xvoCreateSet();
    uint32 count = xrtTypedSetCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedSetItemAt(values, i));
        if ( !xvoCollSetValue(result, item, true) && item != 0 ) { xvoUnref(item); }
    }
    return result;
}

static xtset xl6_xrt_typed_set_copy_mode(const xrt_type_desc* type, xtset values, uint32 mode)
{
    xtset result = xrtTypedSetCreate(type, mode);
    uint32 count = xrtTypedSetCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedSetItemAt(values, i));
        if ( !xrtTypedSetAddValue(result, item) && item != 0 ) { xvoUnref(item); }
        else if ( item != 0 ) { xvoUnref(item); }
    }
    return result;
}

static xtset xl6_xrt_typed_set_copy(const xrt_type_desc* type, xtset values)
{
    return xl6_xrt_typed_set_copy_mode(type, values, XRT_OBJMODE_LOCAL);
}

static xvalue xl6_xrt_typed_dict_to_xvalue(const xrt_type_desc* type, xtdict values)
{
    xvalue result = xvoCreateTable();
    uint32 count = xrtTypedDictCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        const char* key = 0;
        uint32 keyLen = 0;
        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedDictItemAt(values, i, &key, &keyLen));
        if ( !xvoTableSetValue(result, key, keyLen, item, true) && item != 0 ) { xvoUnref(item); }
    }
    return result;
}

static xtdict xl6_xrt_typed_dict_copy_mode(const xrt_type_desc* type, xtdict values, uint32 mode)
{
    xtdict result = xrtTypedDictCreate(type, mode);
    uint32 count = xrtTypedDictCount(values);
    uint32 i;
    for ( i = 0; i < count; ++i ) {
        const char* key = 0;
        uint32 keyLen = 0;
        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedDictItemAt(values, i, &key, &keyLen));
        if ( !xrtTypedDictSetValue(result, key, keyLen, item) && item != 0 ) { xvoUnref(item); }
        else if ( item != 0 ) { xvoUnref(item); }
    }
    return result;
}

static xtdict xl6_xrt_typed_dict_copy(const xrt_type_desc* type, xtdict values)
{
    return xl6_xrt_typed_dict_copy_mode(type, values, XRT_OBJMODE_LOCAL);
}

typedef struct XL6_XrtIterItem {
    int64 index;
    const char* key;
    uint32 keyLen;
    xvalue value;
} XL6_XrtIterItem;

typedef struct XL6_XrtIterSnapshot {
    XL6_XrtIterItem* items;
    uint32 count;
    uint32 capacity;
    int failed;
} XL6_XrtIterSnapshot;

static void xl6_xrt_iter_snapshot_free(XL6_XrtIterSnapshot* snapshot)
{
    uint32 i;
    if ( snapshot == 0 ) { return; }
    for ( i = 0; i < snapshot->count; ++i ) {
        if ( snapshot->items[i].key != 0 ) { xrtFree((void*)snapshot->items[i].key); }
        if ( snapshot->items[i].value != 0 ) { xvoUnref(snapshot->items[i].value); }
    }
    xrtFree(snapshot->items);
    xrtFree(snapshot);
}

static XL6_XrtIterSnapshot* xl6_xrt_iter_snapshot_create(void)
{
    XL6_XrtIterSnapshot* snapshot = (XL6_XrtIterSnapshot*)xrtMalloc(sizeof(XL6_XrtIterSnapshot));
    if ( snapshot == 0 ) { return 0; }
    snapshot->items = 0;
    snapshot->count = 0;
    snapshot->capacity = 0;
    snapshot->failed = 0;
    return snapshot;
}

static int xl6_xrt_iter_snapshot_push(XL6_XrtIterSnapshot* snapshot, int64 index, const void* key, uint32 keyLen, xvalue value)
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
    item->value = value;
    if ( value != 0 ) { xvoAddRef(value); }
    if ( key != 0 && keyLen > 0 ) {
        char* copy = (char*)xrtCopyStr((str)key, keyLen);
        if ( copy == 0 ) { return 0; }
        item->key = copy;
        item->keyLen = keyLen;
    }
    return 1;
}

static bool xl6_xrt_list_snapshot_each(int64 key, void* value, void* arg)
{
    XL6_XrtIterSnapshot* snapshot = (XL6_XrtIterSnapshot*)arg;
    xvalue* slot = (xvalue*)value;
    if ( !xl6_xrt_iter_snapshot_push(snapshot, key, 0, 0, slot != 0 ? *slot : 0) ) {
        if ( snapshot != 0 ) { snapshot->failed = 1; }
        return true;
    }
    return false;
}

static bool xl6_xrt_dict_snapshot_each(Dict_Key* key, void* value, void* arg)
{
    XL6_XrtIterSnapshot* snapshot = (XL6_XrtIterSnapshot*)arg;
    xvalue* slot = (xvalue*)value;
    const void* keyData = key != 0 ? key->Key : 0;
    uint32 keyLen = key != 0 ? key->KeyLen : 0;
    if ( !xl6_xrt_iter_snapshot_push(snapshot, 0, keyData, keyLen, slot != 0 ? *slot : 0) ) {
        if ( snapshot != 0 ) { snapshot->failed = 1; }
        return true;
    }
    return false;
}

static XL6_XrtIterSnapshot* xl6_xrt_list_snapshot(xvalue values)
{
    XL6_XrtIterSnapshot* snapshot = xl6_xrt_iter_snapshot_create();
    if ( snapshot == 0 ) { return 0; }
    xrtListWalk(xvoGetList(values), xl6_xrt_list_snapshot_each, snapshot);
    if ( snapshot->failed ) {
        xl6_xrt_iter_snapshot_free(snapshot);
        return 0;
    }
    return snapshot;
}

static XL6_XrtIterSnapshot* xl6_xrt_dict_snapshot(xvalue values)
{
    XL6_XrtIterSnapshot* snapshot = xl6_xrt_iter_snapshot_create();
    if ( snapshot == 0 ) { return 0; }
    xvoTableWalk(values, xl6_xrt_dict_snapshot_each, snapshot);
    if ( snapshot->failed ) {
        xl6_xrt_iter_snapshot_free(snapshot);
        return 0;
    }
    return snapshot;
}
#endif

#ifdef XL_RUNTIME_NEED_ARGV
static xvalue xl6_argv_to_arg(int argc, char** argv)
{
    int i;
    xvalue arg = xvoCreateArray();
    if ( arg == 0 ) { return 0; }
    for ( i = 0; i < argc; ++i ) {
        const char* text = (argv != 0 && argv[i] != 0) ? argv[i] : "";
        xvalue item = xvoCreateText((void*)text, 0, false);
        if ( item == 0 || !xvoArrayAppendValue(arg, item, true) ) {
            if ( item != 0 ) { xvoUnref(item); }
            xvoUnref(arg);
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
#define XL6_CP_UTF8 65001u
#endif

static char* xl6_wide_to_utf8(const wchar_t* text)
{
    int size;
    char* utf8;

    if ( text == 0 ) {
        text = L"";
    }
    size = WideCharToMultiByte(XL6_CP_UTF8, 0, text, -1, 0, 0, 0, 0);
    if ( size <= 0 ) {
        return 0;
    }
    utf8 = (char*)xrtMalloc((size_t)size);
    if ( utf8 == 0 ) {
        return 0;
    }
    if ( WideCharToMultiByte(XL6_CP_UTF8, 0, text, -1, utf8, size, 0, 0) <= 0 ) {
        xrtFree(utf8);
        return 0;
    }
    return utf8;
}

static xvalue xl6_windows_argv_to_arg(int fallbackArgc, char** fallbackArgv)
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    xvalue arg;
    int i;

    if ( argv == 0 ) {
        return xl6_argv_to_arg(fallbackArgc, fallbackArgv);
    }
    arg = xvoCreateArray();
    if ( arg == 0 ) {
        LocalFree(argv);
        return 0;
    }
    for ( i = 0; i < argc; ++i ) {
        char* text = xl6_wide_to_utf8(argv[i]);
        xvalue item = text != 0 ? xvoCreateText((void*)text, 0, false) : 0;
        if ( text != 0 ) {
            xrtFree(text);
        }
        if ( item == 0 || !xvoArrayAppendValue(arg, item, true) ) {
            if ( item != 0 ) { xvoUnref(item); }
            xvoUnref(arg);
            LocalFree(argv);
            return 0;
        }
    }
    LocalFree(argv);
    return arg;
}
#endif

static xvalue xl6_command_argv_to_arg(int argc, char** argv)
{
#if defined(_WIN32) || defined(_WIN64)
    return xl6_windows_argv_to_arg(argc, argv);
#else
    return xl6_argv_to_arg(argc, argv);
#endif
}
#endif

#endif
