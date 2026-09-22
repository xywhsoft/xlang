/* Terraform state read-only query helpers backed by XRT JSON/xvalue. */
#include <limits.h>
#include <string.h>

#define XRT_MODULE_BUFFER
#define XRT_MODULE_JSON
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#if defined(_WIN32) || defined(_WIN64)
#define XTF_THREAD __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define XTF_THREAD __thread
#else
#define XTF_THREAD
#endif

#define XTF_DOMAIN "xlang.extlib.terraform_state"
#define XTF_RESULT_SLOTS 4u

static XTF_THREAD xbuffer g_xtf_results[XTF_RESULT_SLOTS];
static XTF_THREAD size_t g_xtf_result_index;



static void xtf_error(xerrkind Kind, cstr sMessage)
{
    xrtSetErrorInfo(Kind, XTF_DOMAIN, 0, sMessage);
}



static xstrview xtf_view(cstr sText)
{
    return (xstrview){ sText != NULL ? sText : "", sText != NULL ? strlen(sText) : 0u };
}



static bool xtf_view_equal(xstrview Left, xstrview Right)
{
    return Left.Size == Right.Size &&
        (Left.Size == 0u || memcmp(Left.Data, Right.Data, Left.Size) == 0);
}



static cstr xtf_store(xstrview Text)
{
    xbuffer* pOutput = &g_xtf_results[g_xtf_result_index++ % XTF_RESULT_SLOTS];

    xrtBufferClear(pOutput);
    if (!xrtBufferAppend(pOutput, (xbytesview){ (cbytes)Text.Data, Text.Size }) ||
        !xrtBufferAppendByte(pOutput, 0u)) {
        xrtBufferClear(pOutput);
        return "";
    }
    return (cstr)pOutput->Data;
}



static bool xtf_state(const xvalue* pState)
{
    if (pState == NULL || xrtValueType(pState) != XVALUE_OBJECT) {
        xtf_error(XERR_TYPE, "Terraform state must be a JSON object");
        return false;
    }
    return true;
}



static xvalue* xtf_member(const xvalue* pObject, cstr sName)
{
    return xrtValueObjectGet(pObject, xtf_view(sName));
}



static bool xtf_string_member(const xvalue* pObject, cstr sName, xstrview* pText)
{
    xvalue* pValue = xtf_member(pObject, sName);

    if (pValue == NULL || !xrtValueGetString(pValue, pText)) {
        xtf_error(XERR_VALUE, "Terraform resource has a missing or invalid string field");
        return false;
    }
    return true;
}



static xvalue* xtf_resources(const xvalue* pState)
{
    xvalue* pResources;

    if (!xtf_state(pState)) return NULL;
    pResources = xtf_member(pState, "resources");
    if (pResources == NULL) return NULL;
    if (xrtValueType(pResources) != XVALUE_ARRAY) {
        xtf_error(XERR_TYPE, "Terraform state field 'resources' must be an array");
        return NULL;
    }
    return pResources;
}



static bool xtf_resource_address(xbuffer* pOutput, const xvalue* pResource)
{
    xstrview Module;
    xstrview Mode;
    xstrview Type;
    xstrview Name;
    xvalue* pModule;
    xvalue* pMode;

    if (pOutput == NULL || pResource == NULL || xrtValueType(pResource) != XVALUE_OBJECT) {
        xtf_error(XERR_TYPE, "Terraform resources must be objects");
        return false;
    }
    if (!xtf_string_member(pResource, "type", &Type) ||
        !xtf_string_member(pResource, "name", &Name)) {
        return false;
    }
    pModule = xtf_member(pResource, "module");
    if (pModule != NULL && !xrtValueGetString(pModule, &Module)) {
        xtf_error(XERR_TYPE, "Terraform resource field 'module' must be a string");
        return false;
    }
    if (pModule == NULL) Module = XRT_STR_LITERAL("");
    pMode = xtf_member(pResource, "mode");
    if (pMode != NULL && !xrtValueGetString(pMode, &Mode)) {
        xtf_error(XERR_TYPE, "Terraform resource field 'mode' must be a string");
        return false;
    }
    if (pMode == NULL) Mode = XRT_STR_LITERAL("managed");

    xrtBufferClear(pOutput);
    if ((Module.Size != 0u &&
            (!xrtBufferAppend(pOutput, (xbytesview){ (cbytes)Module.Data, Module.Size }) ||
             !xrtBufferAppendByte(pOutput, '.'))) ||
        (xtf_view_equal(Mode, XRT_STR_LITERAL("data")) &&
            !xrtBufferAppend(pOutput, (xbytesview){ (cbytes)"data.", 5u })) ||
        !xrtBufferAppend(pOutput, (xbytesview){ (cbytes)Type.Data, Type.Size }) ||
        !xrtBufferAppendByte(pOutput, '.') ||
        !xrtBufferAppend(pOutput, (xbytesview){ (cbytes)Name.Data, Name.Size })) {
        xtf_error(XERR_MEMORY, "could not build Terraform resource address");
        return false;
    }
    return true;
}



static xvalue* xtf_find_resource(const xvalue* pState, cstr sAddress)
{
    xvalue* pResources;
    xbuffer Address = { 0 };
    xstrview Target;
    size_t i;

    if (sAddress == NULL || sAddress[0] == 0) {
        xtf_error(XERR_ARGUMENT, "Terraform resource address is required");
        return NULL;
    }
    pResources = xtf_resources(pState);
    if (pResources == NULL) {
        return xrtGetError() == NULL ? xrtValueRetain(xrtValueNull()) : NULL;
    }
    Target = xtf_view(sAddress);
    for (i = 0u; i < xrtValueCount(pResources); ++i) {
        xvalue* pResource = xrtValueArrayGet(pResources, i);

        if (!xtf_resource_address(&Address, pResource)) {
            xrtBufferUnit(&Address);
            return NULL;
        }
        if (xtf_view_equal((xstrview){ (cstr)Address.Data, Address.Size }, Target)) {
            xrtBufferUnit(&Address);
            return xrtValueRetain(pResource);
        }
    }
    xrtBufferUnit(&Address);
    xrtClearError();
    return xrtValueRetain(xrtValueNull());
}



xvalue* xtf_parse(cstr sText)
{
    xvalue* pState = xrtJsonParse(xtf_view(sText));

    if (pState == NULL) return NULL;
    if (!xtf_state(pState)) {
        xrtValueRelease(pState);
        return NULL;
    }
    xrtClearError();
    return pState;
}



bool xtf_valid(cstr sText)
{
    xvalue* pState = xtf_parse(sText);

    if (pState == NULL) {
        xrtClearError();
        return false;
    }
    xrtValueRelease(pState);
    xrtClearError();
    return true;
}



long long xtf_version(const xvalue* pState)
{
    int64 iValue;
    xvalue* pValue;

    if (!xtf_state(pState)) return -1;
    pValue = xtf_member(pState, "version");
    if (pValue == NULL || !xrtValueGetInt(pValue, &iValue)) {
        xtf_error(XERR_VALUE, "Terraform state field 'version' must be an integer");
        return -1;
    }
    xrtClearError();
    return (long long)iValue;
}



long long xtf_serial(const xvalue* pState)
{
    int64 iValue;
    xvalue* pValue;

    if (!xtf_state(pState)) return -1;
    pValue = xtf_member(pState, "serial");
    if (pValue == NULL || !xrtValueGetInt(pValue, &iValue)) {
        xtf_error(XERR_VALUE, "Terraform state field 'serial' must be an integer");
        return -1;
    }
    xrtClearError();
    return (long long)iValue;
}



cstr xtf_lineage(const xvalue* pState)
{
    xstrview Text;

    if (!xtf_state(pState) || !xtf_string_member(pState, "lineage", &Text)) return "";
    xrtClearError();
    return xtf_store(Text);
}



xvalue* xtf_addresses(const xvalue* pState)
{
    xvalue* pResources = xtf_resources(pState);
    xvalue* pResult;
    xbuffer Address = { 0 };
    size_t i;

    if (pResources == NULL && xrtGetError() != NULL) return NULL;
    pResult = xrtValueArray();
    if (pResult == NULL) return NULL;
    if (pResources == NULL) return pResult;
    for (i = 0u; i < xrtValueCount(pResources); ++i) {
        xvalue* pAddress;

        if (!xtf_resource_address(&Address, xrtValueArrayGet(pResources, i))) {
            xrtBufferUnit(&Address);
            xrtValueRelease(pResult);
            return NULL;
        }
        pAddress = xrtValueString((xstrview){ (cstr)Address.Data, Address.Size });
        if (pAddress == NULL || !xrtValueArrayAppendNew(pResult, pAddress)) {
            xrtValueRelease(pAddress);
            xrtBufferUnit(&Address);
            xrtValueRelease(pResult);
            return NULL;
        }
    }
    xrtBufferUnit(&Address);
    xrtClearError();
    return pResult;
}



xvalue* xtf_find(const xvalue* pState, cstr sAddress)
{
    return xtf_find_resource(pState, sAddress);
}



xvalue* xtf_instances(const xvalue* pState, cstr sAddress)
{
    xvalue* pResource = xtf_find_resource(pState, sAddress);
    xvalue* pInstances;

    if (pResource == NULL || xrtValueType(pResource) == XVALUE_NULL) return pResource;
    pInstances = xtf_member(pResource, "instances");
    if (pInstances == NULL || xrtValueType(pInstances) != XVALUE_ARRAY) {
        xrtValueRelease(pResource);
        xtf_error(XERR_VALUE, "Terraform resource field 'instances' must be an array");
        return NULL;
    }
    xrtValueRelease(pResource);
    xrtClearError();
    return xrtValueRetain(pInstances);
}



xvalue* xtf_attributes(const xvalue* pState, cstr sAddress, long long iIndex)
{
    xvalue* pInstances = xtf_instances(pState, sAddress);
    xvalue* pInstance;
    xvalue* pAttributes;

    if (pInstances == NULL || xrtValueType(pInstances) == XVALUE_NULL) return pInstances;
    if (iIndex < 0 || (uint64)iIndex >= xrtValueCount(pInstances)) {
        xrtValueRelease(pInstances);
        xtf_error(XERR_RANGE, "Terraform instance index is out of range");
        return NULL;
    }
    pInstance = xrtValueArrayGet(pInstances, (size_t)iIndex);
    if (pInstance == NULL || xrtValueType(pInstance) != XVALUE_OBJECT) {
        xrtValueRelease(pInstances);
        xtf_error(XERR_TYPE, "Terraform resource instances must be objects");
        return NULL;
    }
    pAttributes = xtf_member(pInstance, "attributes");
    if (pAttributes == NULL || xrtValueType(pAttributes) != XVALUE_OBJECT) {
        xrtValueRelease(pInstances);
        xtf_error(XERR_VALUE, "Terraform instance field 'attributes' must be an object");
        return NULL;
    }
    pAttributes = xrtValueRetain(pAttributes);
    xrtValueRelease(pInstances);
    xrtClearError();
    return pAttributes;
}



xvalue* xtf_outputs(const xvalue* pState)
{
    xvalue* pOutputs;

    if (!xtf_state(pState)) return NULL;
    pOutputs = xtf_member(pState, "outputs");
    if (pOutputs == NULL) return xrtValueRetain(xrtValueNull());
    if (xrtValueType(pOutputs) != XVALUE_OBJECT) {
        xtf_error(XERR_TYPE, "Terraform state field 'outputs' must be an object");
        return NULL;
    }
    xrtClearError();
    return xrtValueRetain(pOutputs);
}



xvalue* xtf_output(const xvalue* pState, cstr sName)
{
    xvalue* pOutputs = xtf_outputs(pState);
    xvalue* pValue;

    if (pOutputs == NULL || xrtValueType(pOutputs) == XVALUE_NULL) return pOutputs;
    if (sName == NULL || sName[0] == 0) {
        xrtValueRelease(pOutputs);
        xtf_error(XERR_ARGUMENT, "Terraform output name is required");
        return NULL;
    }
    pValue = xtf_member(pOutputs, sName);
    if (pValue == NULL) {
        xrtValueRelease(pOutputs);
        xrtClearError();
        return xrtValueRetain(xrtValueNull());
    }
    pValue = xrtValueRetain(pValue);
    xrtValueRelease(pOutputs);
    xrtClearError();
    return pValue;
}



cstr xtf_last_error(void)
{
    return xrtErrorMessage(xrtGetError());
}



/*
    xlang extern 的 byref var ABI 把变量槽位地址按 xvalue* 传入。
    普通 C 调用继续使用上面的 xtf_*，不需要了解这个桥接细节。
*/
static const xvalue* xtf_borrowed_state(xvalue* pStateSlot)
{
    return pStateSlot != NULL ? *(xvalue**)pStateSlot : NULL;
}

long long xtf_version_ref(xvalue* pStateSlot)
{
    return xtf_version(xtf_borrowed_state(pStateSlot));
}

long long xtf_serial_ref(xvalue* pStateSlot)
{
    return xtf_serial(xtf_borrowed_state(pStateSlot));
}

cstr xtf_lineage_ref(xvalue* pStateSlot)
{
    return xtf_lineage(xtf_borrowed_state(pStateSlot));
}

xvalue* xtf_addresses_ref(xvalue* pStateSlot)
{
    return xtf_addresses(xtf_borrowed_state(pStateSlot));
}

xvalue* xtf_find_ref(xvalue* pStateSlot, cstr sAddress)
{
    return xtf_find(xtf_borrowed_state(pStateSlot), sAddress);
}

xvalue* xtf_instances_ref(xvalue* pStateSlot, cstr sAddress)
{
    return xtf_instances(xtf_borrowed_state(pStateSlot), sAddress);
}

xvalue* xtf_attributes_ref(xvalue* pStateSlot, cstr sAddress, long long iIndex)
{
    return xtf_attributes(xtf_borrowed_state(pStateSlot), sAddress, iIndex);
}

xvalue* xtf_outputs_ref(xvalue* pStateSlot)
{
    return xtf_outputs(xtf_borrowed_state(pStateSlot));
}

xvalue* xtf_output_ref(xvalue* pStateSlot, cstr sName)
{
    return xtf_output(xtf_borrowed_state(pStateSlot), sName);
}
