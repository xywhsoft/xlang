/* 独立 C 对照范例，不作为 reload.xl 的同名 companion。 */
#include "../include/xrt_dotenv.h"
#include "xrt.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	xrtdotenv* config = xrtDotenvCreate();
	const char* initial = "HOST=app.local\nPORT=8080\n";
	const char* invalid = "HOST=wrong.local\nbad-key=1\n";
	const char* updated = "HOST=api.local\nPORT=9090\n";
	str saved = NULL;
	int result = 1;
	if (config == NULL || !xrtDotenvParse(config, initial, strlen(initial), 4096u)) goto cleanup;
	/* C 查询返回借用；要跨修改保留内容，调用方显式取得自己的副本。 */
	saved = xrtStrDup(xrtDotenvGet(config, "HOST"));
	if (saved == NULL) goto cleanup;
	printf("initial: %s:%s\n", xrtDotenvGet(config, "HOST"), xrtDotenvGet(config, "PORT"));
	if (xrtDotenvParse(config, invalid, strlen(invalid), 4096u) ||
		xrtDotenvErrorCode(config) != XRT_DOTENV_FORMAT || xrtDotenvErrorLine(config) != 2u ||
		strcmp(xrtDotenvGet(config, "HOST"), "app.local") != 0 ||
		strcmp(xrtDotenvGet(config, "PORT"), "8080") != 0) goto cleanup;
	printf("bad reload kept: %s:%s\n", xrtDotenvGet(config, "HOST"), xrtDotenvGet(config, "PORT"));
	if (!xrtDotenvParse(config, updated, strlen(updated), 4096u)) goto cleanup;
	printf("reloaded: %s:%s\n", xrtDotenvGet(config, "HOST"), xrtDotenvGet(config, "PORT"));
	xrtDotenvClear(config);
	if (xrtDotenvCount(config) != 0u || strcmp(saved, "app.local") != 0) goto cleanup;
	printf("saved host: %s\n", saved);
	printf("clear count: %zu\n", xrtDotenvCount(config));
	result = 0;
cleanup:
	xrtFree(saved);
	xrtDotenvDestroy(config);
	return result;
}

/* 单文件可由 xl run 执行；正式扩展包仍按清单分别编译。 */
#include "../xrt_dotenv.c"
