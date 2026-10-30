#include <stdbool.h>

/* 真实网络字节由 C 提供；XLang 源字面量本身必须是有效 UTF-8，不能内嵌断开的字符。 */
const char* xi_folded_fixture(bool invalid)
{
    if (invalid) return "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:test\r\n"
        "X-NOTE:\xed\r\n \xa0\x80\r\nEND:VCALENDAR\r\n";
    return "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:test\r\n"
        "BEGIN:VEVENT\r\nUID:one\r\nDTSTAMP:20261030T000000Z\r\n"
        "DTSTART:20261030T120000Z\r\nSUMMARY:\xe4\r\n \xbd\r\n\t\xa0\r\n"
        "END:VEVENT\r\nEND:VCALENDAR\r\n";
}
