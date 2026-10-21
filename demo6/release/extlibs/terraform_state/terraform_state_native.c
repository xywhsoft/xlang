/* Terraform 原始 state 的只读查询；值和文本的所有权由 API 明确区分。 */
#include <limits.h>
#include <string.h>

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_JSON
#define XRT_MODULE_JSON
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "include/terraform_state.h"

#define XTF_DOMAIN "xlang.extlib.terraform_state"
#define XTF_MAX_INPUT (64u * 1024u * 1024u)



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
    if (pResources == NULL) {
        xtf_error(XERR_VALUE, "Terraform state field 'resources' is required");
        return NULL;
    }
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
    if (Type.Size == 0u || Name.Size == 0u ||
        memchr(Type.Data, 0, Type.Size) != NULL ||
        memchr(Name.Data, 0, Name.Size) != NULL) {
        xtf_error(XERR_VALUE, "Terraform resource type and name must be nonempty text");
        return false;
    }
    pModule = xtf_member(pResource, "module");
    if (pModule != NULL && !xrtValueGetString(pModule, &Module)) {
        xtf_error(XERR_TYPE, "Terraform resource field 'module' must be a string");
        return false;
    }
    if (pModule == NULL) Module = XRT_STR_LITERAL("");
    if (memchr(Module.Data, 0, Module.Size) != NULL) {
        xtf_error(XERR_VALUE, "Terraform module address cannot contain NUL");
        return false;
    }
    pMode = xtf_member(pResource, "mode");
    if (pMode != NULL && !xrtValueGetString(pMode, &Mode)) {
        xtf_error(XERR_TYPE, "Terraform resource field 'mode' must be a string");
        return false;
    }
    if (pMode == NULL) Mode = XRT_STR_LITERAL("managed");
    if (!xtf_view_equal(Mode, XRT_STR_LITERAL("managed")) &&
        !xtf_view_equal(Mode, XRT_STR_LITERAL("data"))) {
        xtf_error(XERR_VALUE, "Terraform resource mode must be managed or data");
        return false;
    }

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
    xvalue* pFound = NULL;
    xbuffer Address = { 0 };
    xstrview Target;
    size_t i;

    xrtClearError();
    if (sAddress == NULL || sAddress[0] == 0) {
        xtf_error(XERR_ARGUMENT, "Terraform resource address is required");
        return NULL;
    }
    pResources = xtf_resources(pState);
    if (pResources == NULL) return NULL;
    Target = xtf_view(sAddress);
    for (i = 0u; i < xrtValueCount(pResources); ++i) {
        xvalue* pResource = xrtValueArrayGet(pResources, i);

        if (!xtf_resource_address(&Address, pResource)) {
            xrtBufferUnit(&Address);
            return NULL;
        }
        if (xtf_view_equal((xstrview){ (cstr)Address.Data, Address.Size }, Target)) {
            if (pFound != NULL) {
                xrtBufferUnit(&Address);
                xtf_error(XERR_VALUE, "duplicate Terraform resource address");
                return NULL;
            }
            pFound = pResource;
        }
    }
    xrtBufferUnit(&Address);
    xrtClearError();
    return xrtValueRetain(pFound != NULL ? pFound : xrtValueNull());
}



static xvalue* xtf_parse_view(xstrview Text)
{
    xvalue* pState;
    xvalue* pVersion;
    xvalue* pSerial;
    xvalue* pLineage;
    xvalue* pResources;
    xvalue* pOutputs;
    int64 version;
    int64 serial;
    xstrview lineage;

    xrtClearError();
    if (Text.Data == NULL || Text.Size == 0u || Text.Size > XTF_MAX_INPUT) {
        xtf_error(XERR_RANGE, "Terraform state input is empty or exceeds 64 MiB");
        return NULL;
    }
    pState = xrtJsonParse(Text);
    if (pState == NULL) return NULL;
    if (!xtf_state(pState)) {
        xrtValueRelease(pState);
        return NULL;
    }
    pVersion = xtf_member(pState, "version");
    pSerial = xtf_member(pState, "serial");
    pLineage = xtf_member(pState, "lineage");
    pResources = xtf_member(pState, "resources");
    pOutputs = xtf_member(pState, "outputs");
    if (pVersion == NULL || !xrtValueGetInt(pVersion, &version) || version != 4 ||
        pSerial == NULL || !xrtValueGetInt(pSerial, &serial) || serial < 0 ||
        pLineage == NULL || !xrtValueGetString(pLineage, &lineage) ||
        pResources == NULL || xrtValueType(pResources) != XVALUE_ARRAY ||
        pOutputs == NULL || xrtValueType(pOutputs) != XVALUE_OBJECT) {
        xrtValueRelease(pState);
        xtf_error(XERR_VALUE, "expected raw Terraform state v4 with serial, lineage, resources and outputs");
        return NULL;
    }
    if (lineage.Size != 0u && memchr(lineage.Data, 0, lineage.Size) != NULL) {
        xrtValueRelease(pState);
        xtf_error(XERR_VALUE, "Terraform lineage cannot contain NUL");
        return NULL;
    }
    xrtClearError();
    return pState;
}



/* XL 的 var 保留字符串长度，避免 C 字符串在内嵌 NUL 处被截断。 */
xvalue* xtf_parse_value(xvalue* pText)
{
    xstrview Text;
    xrtClearError();
    if (pText == NULL || !xrtValueGetString(pText, &Text)) {
        xtf_error(XERR_TYPE, "Terraform state input must be a string");
        return NULL;
    }
    return xtf_parse_view(Text);
}



xvalue* xtf_parse_len(cstr sText, size_t iLength)
{
    return xtf_parse_view((xstrview){ sText, iLength });
}



xvalue* xtf_parse(cstr sText)
{
    size_t iLength = 0u;
    if (sText != NULL) {
        while (iLength <= XTF_MAX_INPUT && sText[iLength] != 0) ++iLength;
    }
    return xtf_parse_len(sText, iLength);
}



bool xtf_valid(cstr sText)
{
    xvalue* pState = xtf_parse(sText);

    if (pState == NULL) {
        if (xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
            xrtClearError();
        return false;
    }
    xrtValueRelease(pState);
    xrtClearError();
    return true;
}



bool xtf_valid_value(xvalue* pText)
{
    xvalue* pState = xtf_parse_value(pText);
    if (pState == NULL) {
        if (xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
            xrtClearError();
        return false;
    }
    xrtValueRelease(pState);
    xrtClearError();
    return true;
}



int64_t xtf_version(xvalue* pState)
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
    return iValue;
}



int64_t xtf_serial(xvalue* pState)
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
    return iValue;
}



char* xtf_lineage(xvalue* pState)
{
    xstrview Text;

    xrtClearError();
    if (!xtf_state(pState) || !xtf_string_member(pState, "lineage", &Text)) return NULL;
    if (memchr(Text.Data, 0, Text.Size) != NULL) {
        xtf_error(XERR_VALUE, "Terraform lineage cannot contain NUL");
        return NULL;
    }
    xrtClearError();
    return xrtStrDupView(Text);
}



const char* xtf_lineage_borrowed(xvalue* pState)
{
    xstrview Text;
    xrtClearError();
    if (!xtf_state(pState) || !xtf_string_member(pState, "lineage", &Text)) return NULL;
    if (memchr(Text.Data, 0, Text.Size) != NULL) {
        xtf_error(XERR_VALUE, "Terraform lineage cannot contain NUL");
        return NULL;
    }
    return Text.Data;
}



xvalue* xtf_addresses(xvalue* pState)
{
    xvalue* pResources;
    xvalue* pResult;
    xvalue* pSeen;
    xbuffer Address = { 0 };
    size_t i;

    xrtClearError();
    pResources = xtf_resources(pState);
    if (pResources == NULL) return NULL;
    pResult = xrtValueArray();
    if (pResult == NULL) return NULL;
    pSeen = xrtValueObject();
    if (pSeen == NULL) {
        xrtValueRelease(pResult);
        xtf_error(XERR_MEMORY, "could not track Terraform resource addresses");
        return NULL;
    }
    for (i = 0u; i < xrtValueCount(pResources); ++i) {
        xvalue* pAddress;
        xstrview Key;

        if (!xtf_resource_address(&Address, xrtValueArrayGet(pResources, i))) {
            goto fail;
        }
        Key = (xstrview){ (cstr)Address.Data, Address.Size };
        if (xrtValueObjectGet(pSeen, Key) != NULL) {
            xtf_error(XERR_VALUE, "duplicate Terraform resource address");
            goto fail;
        }
        if (!xrtValueObjectSetNew(pSeen, Key, xrtValueBool(true))) {
            xtf_error(XERR_MEMORY, "could not track Terraform resource addresses");
            goto fail;
        }
        pAddress = xrtValueString(Key);
        if (pAddress == NULL || !xrtValueArrayAppendNew(pResult, pAddress)) {
            xtf_error(XERR_MEMORY, "could not build Terraform address list");
            goto fail;
        }
    }
    xrtBufferUnit(&Address);
    xrtValueRelease(pSeen);
    xrtClearError();
    return pResult;
fail:
    xrtBufferUnit(&Address);
    xrtValueRelease(pSeen);
    xrtValueRelease(pResult);
    return NULL;
}



xvalue* xtf_find(xvalue* pState, cstr sAddress)
{
    return xtf_find_resource(pState, sAddress);
}



xvalue* xtf_instances(xvalue* pState, cstr sAddress)
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
    pInstances = xrtValueRetain(pInstances);
    xrtValueRelease(pResource);
    xrtClearError();
    return pInstances;
}



xvalue* xtf_attributes(xvalue* pState, cstr sAddress, int64_t iIndex)
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



xvalue* xtf_outputs(xvalue* pState)
{
    xvalue* pOutputs;

    xrtClearError();
    if (!xtf_state(pState)) return NULL;
    pOutputs = xtf_member(pState, "outputs");
    if (pOutputs == NULL) {
        xtf_error(XERR_VALUE, "Terraform state field 'outputs' is required");
        return NULL;
    }
    if (xrtValueType(pOutputs) != XVALUE_OBJECT) {
        xtf_error(XERR_TYPE, "Terraform state field 'outputs' must be an object");
        return NULL;
    }
    xrtClearError();
    return xrtValueRetain(pOutputs);
}



xvalue* xtf_output(xvalue* pState, cstr sName)
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
