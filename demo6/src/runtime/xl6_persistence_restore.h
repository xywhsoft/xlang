#ifndef XL6_PERSISTENCE_RESTORE_H
#define XL6_PERSISTENCE_RESTORE_H
#include "xl6_text_value.h"
#include "xl6_value_domain.h"

/* Wire restoration is NOT erased-value admission. JSON loses numeric widths
 * (and encodes char as an integer); restore must normalize to the declared
 * domain before publishing a field. No strings-to-numbers, float-to-integer
 * truncation, integer wrapping, or bool/numeric coercion is permitted.
 * Borrowed input, independent owned output, NULL on a rejected wire value or
 * allocation failure. The nullable reader finish distinguishes those cases.
 * Pure metadata from verified IR selects the rule; this code never examines
 * source type spellings, class layouts, or a generated-code registry. */
/* Rejection is a fact established by tag/range checks, NOT inferred from an
 * error kind. Getter, allocation, clone and callback failures never set it.
 * The old strict callers may omit this status without changing their ABI. */
static inline xvalue* xl6PersistenceRejectScalarV1(bool* rejected)
{ if (rejected) *rejected=true; return NULL; }
static inline xvalue* xl6PersistenceRestoreScalarCheckedV1(
    const xvalue* source, unsigned int rule, bool json, bool* rejected)
{
    xvaluetype tag;
    int64_t signedNumber;
    uint64_t unsignedNumber;
    double number;
    uint32_t scalar;
    if (rejected) *rejected=false;
    if (source == NULL) return NULL;
    tag = xrtValueType(source);
    if (rule == XL6_VALUE_ADMIT_ANY) return xrtValueClone(source);
    if (rule >= XL6_VALUE_ADMIT_I8 && rule <= XL6_VALUE_ADMIT_U32) {
        if (tag == XVALUE_INT) {
            if (!xrtValueGetInt(source, &signedNumber)) return NULL;
        } else if (tag == XVALUE_UINT) {
            if (!xrtValueGetUInt(source, &unsignedNumber)) return NULL;
            if (unsignedNumber > INT64_MAX) return xl6PersistenceRejectScalarV1(rejected);
            signedNumber = (int64_t)unsignedNumber;
        } else return xl6PersistenceRejectScalarV1(rejected);
        return xl6IntegerDomainV1(signedNumber, rule) ? xrtValueInt(signedNumber) :
            xl6PersistenceRejectScalarV1(rejected);
    }
    switch (rule) {
    case XL6_VALUE_ADMIT_U64:
        if (tag == XVALUE_UINT) {
            if (!xrtValueGetUInt(source, &unsignedNumber)) return NULL;
        } else if (tag == XVALUE_INT) {
            if (!xrtValueGetInt(source, &signedNumber)) return NULL;
            if (signedNumber < 0) return xl6PersistenceRejectScalarV1(rejected);
            unsignedNumber = (uint64_t)signedNumber;
        } else return xl6PersistenceRejectScalarV1(rejected);
        return xrtValueUInt(unsignedNumber);
    case XL6_VALUE_ADMIT_F32:
    case XL6_VALUE_ADMIT_F64:
        if (tag == XVALUE_FLOAT) {
            if (!xrtValueGetFloat(source, &number)) return NULL;
        } else if (tag == XVALUE_INT) {
            if (!xrtValueGetInt(source, &signedNumber)) return NULL;
            number = (double)signedNumber;
        } else if (tag == XVALUE_UINT) {
            if (!xrtValueGetUInt(source, &unsignedNumber)) return NULL;
            number = (double)unsignedNumber;
        } else return xl6PersistenceRejectScalarV1(rejected);
        if (rule == XL6_VALUE_ADMIT_F32) {
            /* XLON admits NaN/Inf. A finite overflow is not an infinity, and
             * the range check must precede the potentially undefined cast. */
            if (isfinite(number) && (number < -FLT_MAX || number > FLT_MAX))
                return xl6PersistenceRejectScalarV1(rejected);
            number = (double)(float)number;
        }
        return xrtValueFloat(number);
    case XL6_VALUE_ADMIT_CHAR:
        if (tag == XVALUE_CHAR) {
            if (!xrtValueGetChar(source, &scalar)) return NULL;
        } else if (json && tag == XVALUE_INT) {
            if (!xrtValueGetInt(source, &signedNumber)) return NULL;
            if (signedNumber < 0 || signedNumber > UINT32_MAX) return xl6PersistenceRejectScalarV1(rejected);
            scalar = (uint32_t)signedNumber;
        } else if (json && tag == XVALUE_UINT) {
            if (!xrtValueGetUInt(source, &unsignedNumber)) return NULL;
            if (unsignedNumber > UINT32_MAX) return xl6PersistenceRejectScalarV1(rejected);
            scalar = (uint32_t)unsignedNumber;
        } else return xl6PersistenceRejectScalarV1(rejected);
        return xl6CharDomainV1(scalar) ? xrtValueChar(scalar) : xl6PersistenceRejectScalarV1(rejected);
    case XL6_VALUE_ADMIT_BOOL:
        if (tag != XVALUE_BOOL) return xl6PersistenceRejectScalarV1(rejected);
        break;
    case XL6_VALUE_ADMIT_STRING:
        if (tag != XVALUE_STRING) return xl6PersistenceRejectScalarV1(rejected);
        break;
    case XL6_VALUE_ADMIT_BYTES:
        if (json || tag != XVALUE_BYTES) return xl6PersistenceRejectScalarV1(rejected);
        break;
    case XL6_VALUE_ADMIT_TIME:
        if (json || tag != XVALUE_TIME) return xl6PersistenceRejectScalarV1(rejected);
        break;
    default: return NULL;
    }
    return xrtValueClone(source);
}
static inline xvalue* xl6PersistenceRestoreScalarV1(const xvalue* source, unsigned int rule, bool json)
{ return xl6PersistenceRestoreScalarCheckedV1(source,rule,json,NULL); }

typedef enum XL6_PersistenceRestoreCollectionV1 {
    XL6_PERSIST_RESTORE_ARRAY_V1 = 1,
    XL6_PERSIST_RESTORE_LIST_V1,
    XL6_PERSIST_RESTORE_SET_V1,
    XL6_PERSIST_RESTORE_DICT_V1
} XL6_PersistenceRestoreCollectionV1;

/* Synchronous element callback, borrowed for this invocation only. Traversal
 * and rollback live here, not in a generated loop for every collection type.
 * Array/List/Set wire inputs are arrays; Dict wire inputs are objects. Shape
 * must be checked before Count/Get, even for an empty mismatched container.
 * Kind is a closed resident contract, independent of compiler enum numbers. */
static inline xvalue* xl6PersistenceRestoreCollectionV1(const xvalue* source,
    unsigned int kind, uint64_t identity, xvalue* (*restore)(const xvalue*))
{
    xvalue* result;
    size_t count;
    if (source == NULL || restore == NULL || identity == 0 ||
        kind < XL6_PERSIST_RESTORE_ARRAY_V1 || kind > XL6_PERSIST_RESTORE_DICT_V1 ||
        xrtValueType(source) != (kind == XL6_PERSIST_RESTORE_DICT_V1 ? XVALUE_OBJECT : XVALUE_ARRAY)) return NULL;
    result = kind == XL6_PERSIST_RESTORE_ARRAY_V1 ? xrtValueArray() : kind == XL6_PERSIST_RESTORE_LIST_V1 ? xrtValueIntMap() :
        kind == XL6_PERSIST_RESTORE_SET_V1 ? xrtValueSet() : xrtValueObject();
    if (result == NULL) return NULL;
    count = xrtValueCount(source);
    if (count > INT64_MAX) goto failed;
    for (size_t index = 0; index < count; ++index) {
        xstrview key = {0};
        const xvalue* borrowed = kind == XL6_PERSIST_RESTORE_DICT_V1 ? xrtValueObjectAt(source, index, &key) :
            xrtValueArrayGet(source, index);
        xvalue* item = borrowed != NULL ? restore(borrowed) : NULL;
        if (item == NULL) goto failed;
        /* Each New operation consumes item on BOTH success and failure. */
        bool ok = kind == XL6_PERSIST_RESTORE_ARRAY_V1 ? xrtValueArrayAppendNew(result, item) :
            kind == XL6_PERSIST_RESTORE_LIST_V1 ? xrtValueIntMapSetNew(result, (int64_t)index, item) :
            kind == XL6_PERSIST_RESTORE_SET_V1 ? xrtValueSetAddNew(result, item) : xrtValueObjectSetNew(result, key, item);
        if (!ok) goto failed;
    }
    if (xrtValueTypeIdBind(result, identity)) return result;
failed:
    {
        xerror* failure = xrtTakeError();
        xrtValueRelease(result);
        xrtClearError(); xrtSetErrorTake(failure);
    }
    return NULL;
}
#endif
