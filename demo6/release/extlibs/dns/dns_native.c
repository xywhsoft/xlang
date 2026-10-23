/* DNS 协议辅助与主机地址解析；网络查询统一委托 XRT。 */
#include <string.h>

#define XRT_MODULE_BUFFER
#define XRT_MODULE_CODEC_HEX
#define XRT_MODULE_NET_DNS
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#if defined(_MSC_VER)
#define XDNS_THREAD __declspec(thread)
#elif defined(__GNUC__)
#define XDNS_THREAD __thread
#else
#define XDNS_THREAD
#endif

#define XDNS_RESULT_SLOTS 8u

static XDNS_THREAD xbuffer g_xdns_results[XDNS_RESULT_SLOTS];
static XDNS_THREAD size_t g_xdns_result_index;

static const char* xdns_result_text(xbuffer* pBuffer)
{
    xbuffer* pResult;
    size_t iSlot;

    if (pBuffer == NULL) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.dns", 0,
            "DNS result buffer is missing");
        return "";
    }
    if (!xrtBufferAppendByte(pBuffer, 0u)) {
        xrtBufferUnit(pBuffer);
        xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.dns", 0,
            "could not allocate DNS result text");
        return "";
    }
    iSlot = g_xdns_result_index++ % XDNS_RESULT_SLOTS;
    pResult = &g_xdns_results[iSlot];
    xrtBufferUnit(pResult);
    *pResult = *pBuffer;
    memset(pBuffer, 0, sizeof(*pBuffer));
    xrtClearError();
    return (const char*)pResult->Data;
}

/* DNS record type name */
const char* xdns_type_name(long long type) {
    switch (type) {
        case 1: return "A";
        case 2: return "NS";
        case 5: return "CNAME";
        case 6: return "SOA";
        case 12: return "PTR";
        case 13: return "HINFO";
        case 15: return "MX";
        case 16: return "TXT";
        case 28: return "AAAA";
        case 33: return "SRV";
        case 35: return "NAPTR";
        case 39: return "DNAME";
        case 41: return "OPT";
        case 43: return "DS";
        case 44: return "SSHFP";
        case 46: return "RRSIG";
        case 47: return "NSEC";
        case 48: return "DNSKEY";
        case 50: return "NSEC3";
        case 51: return "NSEC3PARAM";
        case 52: return "TLSA";
        case 53: return "SMIMEA";
        case 55: return "HIP";
        case 59: return "CDS";
        case 60: return "CDNSKEY";
        case 61: return "OPENPGPKEY";
        case 62: return "CSYNC";
        case 64: return "SVCB";
        case 65: return "HTTPS";
        case 99: return "SPF";
        case 249: return "TKEY";
        case 250: return "TSIG";
        case 251: return "IXFR";
        case 252: return "AXFR";
        case 255: return "ANY";
        case 256: return "URI";
        case 257: return "CAA";
        case 32768: return "TA";
        case 32769: return "DLV";
        default: return "UNKNOWN";
    }
}

/* DNS class name */
const char* xdns_class_name(long long cls) {
    switch (cls) {
        case 1: return "IN";
        case 3: return "CH";
        case 4: return "HS";
        case 254: return "NONE";
        case 255: return "ANY";
        default: return "UNKNOWN";
    }
}

/* DNS response code */
const char* xdns_rcode_name(long long rcode) {
    switch (rcode) {
        case 0: return "NOERROR";
        case 1: return "FORMERR";
        case 2: return "SERVFAIL";
        case 3: return "NXDOMAIN";
        case 4: return "NOTIMP";
        case 5: return "REFUSED";
        case 6: return "YXDOMAIN";
        case 7: return "YXRRSET";
        case 8: return "NXRRSET";
        case 9: return "NOTAUTH";
        case 10: return "NOTZONE";
        case 16: return "BADSIG";
        case 17: return "BADKEY";
        case 18: return "BADTIME";
        case 19: return "BADMODE";
        case 20: return "BADNAME";
        case 21: return "BADALG";
        case 22: return "BADTRUNC";
        case 23: return "BADCOOKIE";
        default: return "UNKNOWN";
    }
}

/* DNS opcode */
const char* xdns_opcode_name(long long opcode) {
    switch (opcode) {
        case 0: return "QUERY";
        case 1: return "IQUERY";
        case 2: return "STATUS";
        case 4: return "NOTIFY";
        case 5: return "UPDATE";
        case 6: return "DSO";
        default: return "UNKNOWN";
    }
}

/* DNS 标签只接受 ASCII 主机名形式；末尾根点允许但不会重复编码。 */
long long xdns_valid_name(const char* sName)
{
    size_t iLength;
    size_t i;
    size_t iLabel = 0u;

    if (sName == NULL) { return 0; }
    iLength = strlen(sName);
    if (iLength == 0u || iLength > 253u) { return 0; }
    if (sName[iLength - 1u] == '.') { --iLength; }
    if (iLength == 0u) { return 0; }
    for (i = 0u; i < iLength; ++i) {
        unsigned char iChar = (unsigned char)sName[i];
        if (iChar == '.') {
            if (iLabel == 0u || sName[i - 1u] == '-') { return 0; }
            iLabel = 0u;
            continue;
        }
        if (!((iChar >= 'a' && iChar <= 'z') ||
              (iChar >= 'A' && iChar <= 'Z') ||
              (iChar >= '0' && iChar <= '9') || iChar == '-')) {
            return 0;
        }
        if (++iLabel > 63u) { return 0; }
        if (iLabel == 1u && iChar == '-') { return 0; }
    }
    return sName[iLength - 1u] != '-' ? 1 : 0;
}

/* 把完整域名写为 DNS wire name，再由 XRT 统一编码为小写十六进制。 */
const char* xdns_encode_name(const char* sDomain)
{
    xbuffer tWire;
    const char* sLabel;
    size_t iLength;
    str sHex;

    if (!xdns_valid_name(sDomain)) {
        xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.dns", 0,
            "invalid DNS domain name");
        return "";
    }
    if (!xrtBufferInit(&tWire)) {
        return "";
    }
    iLength = strlen(sDomain);
    if (iLength != 0u && sDomain[iLength - 1u] == '.') { --iLength; }
    sLabel = sDomain;
    while ((size_t)(sLabel - sDomain) < iLength) {
        const char* sDot = strchr(sLabel, '.');
        size_t iLabel = sDot != NULL ? (size_t)(sDot - sLabel) :
            iLength - (size_t)(sLabel - sDomain);
        if (!xrtBufferAppendByte(&tWire, (uint8)iLabel) ||
            !xrtBufferAppend(&tWire, (xbytesview){ (cbytes)sLabel, iLabel })) {
            xrtBufferUnit(&tWire);
            xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.dns", 0,
                "could not encode DNS domain name");
            return "";
        }
        if (sDot == NULL || (size_t)(sDot - sDomain) >= iLength) { break; }
        sLabel = sDot + 1;
    }
    if (!xrtBufferAppendByte(&tWire, 0u)) {
        xrtBufferUnit(&tWire);
        xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.dns", 0,
            "could not encode DNS domain name");
        return "";
    }
    sHex = xrtHexEncodeNew(tWire.Data, tWire.Size, 0u);
    xrtBufferUnit(&tWire);
    if (sHex == NULL) { return ""; }
    if (!xrtBufferInit(&tWire) ||
        !xrtBufferAppend(&tWire, (xbytesview){ (cbytes)sHex, strlen(sHex) })) {
        xrtFree(sHex);
        xrtBufferUnit(&tWire);
        xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.dns", 0,
            "could not store DNS encoded name");
        return "";
    }
    xrtFree(sHex);
    return xdns_result_text(&tWire);
}

long long xdns_is_ipv4(const char* sAddress)
{
    xnetaddr tAddress;
    return xrtNetAddrParse(&tAddress, sAddress, 0u) &&
        tAddress.Family == XNET_FAMILY_IPV4 ? 1 : 0;
}

static xnetfamily xdns_family(long long iFamily)
{
    if (iFamily == 4) { return XNET_FAMILY_IPV4; }
    if (iFamily == 6) { return XNET_FAMILY_IPV6; }
    return XNET_FAMILY_UNSPEC;
}

/* 返回稳定的 xvalue 字符串数组，列表顺序与 XRT 系统解析顺序一致。 */
xvalue* xdns_resolve(const char* sHost, long long iFamily)
{
    xnetaddrlist* pAddresses;
    xvalue* pResult;
    size_t i;

    if (sHost == NULL || sHost[0] == '\0') {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.dns", 0,
            "DNS host must not be empty");
        return xrtValueNull();
    }
    if (iFamily != 0 && iFamily != 4 && iFamily != 6) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.dns", 0,
            "DNS family must be 0, 4, or 6");
        return xrtValueNull();
    }
    pAddresses = xrtNetLookup(sHost, xdns_family(iFamily));
    if (pAddresses == NULL) { return xrtValueNull(); }
    pResult = xrtValueArray();
    if (pResult == NULL) {
        xrtNetAddrListDestroy(pAddresses);
        return xrtValueNull();
    }
    for (i = 0u; i < xrtNetAddrListCount(pAddresses); ++i) {
        const xnetaddr* pAddress = xrtNetAddrListGet(pAddresses, i);
        str sText = pAddress != NULL ? xrtNetAddrString(pAddress) : NULL;
        xvalue* pItem = sText != NULL ? xrtValueStringTake(&sText, strlen(sText)) : NULL;
        xrtFree(sText);
        if (pItem == NULL || !xrtValueArrayAppendTake(pResult, &pItem)) {
            xrtValueRelease(pItem);
            xrtValueRelease(pResult);
            xrtNetAddrListDestroy(pAddresses);
            return xrtValueNull();
        }
    }
    xrtNetAddrListDestroy(pAddresses);
    xrtClearError();
    return pResult;
}
