/* 与 chardet_basic.xl 相同输入和输出，C 直接复用同一 XRT 探测基础。 */
#include "../include/chardet.h"
#include <stdio.h>
#include <string.h>

static const char* boolean(bool value) { return value ? "true" : "false"; }
int main(void)
{
    const char* text="\xe4\xbd\xa0\xe5\xa5\xbd";
    unsigned char wide[]={255,254,65,0};
    unsigned char bom[]={239,187,191,255};
    unsigned char binary[]={'A',0,255};
    size_t size=strlen(text);
    printf("text=%s; confidence=%d; bom=%d\n",xchardet_detect_name_str(text,size),
        xchardet_detect_conf_str(text,size),xchardet_detect_bom_size_str(text,size));
    printf("ascii=%s; utf8=%s\n",boolean(xchardet_is_ascii_str("hello",5)),boolean(xchardet_is_utf8_str("hello",5)));
    printf("empty=%s; ascii=%s; utf8=%s\n",xchardet_detect_name_n(NULL,0),
        boolean(xchardet_is_ascii_n(NULL,0)),boolean(xchardet_is_utf8_n(NULL,0)));
    printf("utf16=%s; confidence=%d; bom=%d\n",xchardet_detect_name_n(wide,sizeof(wide)),
        xchardet_detect_conf_n(wide,sizeof(wide)),xchardet_detect_bom_size_n(wide,sizeof(wide)));
    printf("bom-only=%s; confidence=%d\n",xchardet_detect_name_n(bom,3),xchardet_detect_conf_n(bom,3));
    printf("bom-invalid=%s; valid=%s\n",xchardet_detect_name_n(bom,sizeof(bom)),boolean(xchardet_is_utf8_n(bom,sizeof(bom))));
    printf("binary=%s; valid=%s\n",xchardet_detect_name_n(binary,sizeof(binary)),boolean(xchardet_is_utf8_n(binary,sizeof(binary))));
    return 0;
}
