#include <stdbool.h>

/* 由 C 提供实际网络字节；XLang 源字面量必须是完整 UTF-8。返回值仅借用。 */
const char* xv_folded_fixture(bool invalid, int version)
{
    if (invalid) return "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:keep\r\n"
        "X-NOTE:\xed\r\n \xa0\x80\r\nEND:VCARD\r\n";
    if (version == 3) return "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:\xe4\r\n \xbd\r\n\t\xa0\r\n"
        "NOTE:\xc2\r\n \xa2\r\nNICKNAME:\xf0\r\n \x9f\r\n \x98\r\n \x80\r\nEND:VCARD\r\n";
    return "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:\xe4\r\n \xbd\r\n\t\xa0\r\n"
        "NOTE:\xc2\r\n \xa2\r\nNICKNAME:\xf0\r\n \x9f\r\n \x98\r\n \x80\r\nEND:VCARD\r\n";
}
