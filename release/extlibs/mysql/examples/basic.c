#define XRT_MODULE_TEMP_MEMORY
#include "xrt.h"
#include "../include/mysql_native.h"
#include <stdio.h>

int main(void)
{
    const char* dsn;
    /* 与 release/examples/extlibs/mysql.xl 保持相同调用和输出。 */
    puts(xmysql_cmd_name(31));
    puts(xmysql_col_type(0xfd));
    puts(xmysql_cap_name(0x200000));
    puts(xmysql_status_str(0x4103));
    puts(xmysql_escape("it's"));
    dsn = xmysql_format_dsn("::1", 3306, "app", "dev");
    if (dsn == NULL) return 1;
    puts(dsn);
    puts(xmysql_dsn_part(dsn, "host"));
    puts(xmysql_dsn_part(dsn, "db"));
    puts(xmysql_error_class(1045));
    puts(xmysql_error_class(2003));
    xrtTempUnit(xrtTempCurrent());
    return 0;
}
