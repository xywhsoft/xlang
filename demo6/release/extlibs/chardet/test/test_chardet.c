/* XRT 编码探测的 C 侧边界 smoke。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include <stdio.h>
#include <string.h>

const char* xchardet_detect_name_n(const void* pData, uint64 iSize);
int64 xchardet_detect_conf_n(const void* pData, uint64 iSize);
int64 xchardet_detect_bom_size_n(const void* pData, uint64 iSize);

int main(void)
{
    static const unsigned char sAscii[] = "Hello";
    static const unsigned char sUtf8[] = { 0xe4u, 0xbd, 0xa0, 0xe5, 0xa5, 0xbd };
    static const unsigned char sBom[] = { 0xefu, 0xbbu, 0xbfu, 'x' };
    int bOk =
        strcmp(xchardet_detect_name_n(sAscii, sizeof(sAscii) - 1u), "UTF-8") == 0 &&
        xchardet_detect_conf_n(sAscii, sizeof(sAscii) - 1u) == 40 &&
        strcmp(xchardet_detect_name_n(sUtf8, sizeof(sUtf8)), "UTF-8") == 0 &&
        xchardet_detect_conf_n(sUtf8, sizeof(sUtf8)) == 90 &&
        strcmp(xchardet_detect_name_n(sBom, sizeof(sBom)), "UTF-8-BOM") == 0 &&
        xchardet_detect_bom_size_n(sBom, sizeof(sBom)) == 3;

    if ( bOk ) { puts("test_chardet: PASS"); }
    return bOk ? 0 : 1;
}

/* 单文件 C smoke：实现与测试共享同一份 XRT 模块配置。 */
#include "../chardet_native.c"
