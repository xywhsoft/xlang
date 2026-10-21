#include "xrt.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* TOML 数字格式固定为 ASCII 小数点，不依赖宿主进程的 LC_NUMERIC。 */
static double __tomlc17ParseFloat(const char* sText, char** pEnd)
{
	double fValue = 0.0;
	if ( !xrtNumParse(xrtStrView(sText), XNUMBER_PARSE_SPECIAL, &fValue) ) {
		*pEnd = (char*)sText;
		errno = EINVAL;
		return 0.0;
	}
	*pEnd = (char*)sText + strlen(sText);
	return fValue;
}

/* 仅改此编译单元的回调；私有解析器的 1.0 语法补丁见 VENDOR.md。 */
#define realloc xrtRealloc
#define free xrtFree
#define strtod __tomlc17ParseFloat
#include "src/tomlc17.c"
#undef strtod
#undef free
#undef realloc
