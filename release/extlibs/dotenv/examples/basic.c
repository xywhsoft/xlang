#include "../include/xrt_dotenv.h"
#include "xrt.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	const char* source = "# application\nDB_HOST=localhost\nDB_PORT=5432\nGREETING=\"你好, world\"\n";
	const char* envKey = "XL_DEMO6_DOTENV_EXAMPLE";
	const char* replacement = "XL_DEMO6_DOTENV_EXAMPLE=new\n";
	str directory = xrtDirTemp(NULL, "xlang-dotenv-", NULL);
	str path = NULL;
	str previous = NULL;
	str current = NULL;
	xrtdotenv* config = NULL;
	xbytesview input = {(cbytes)source, strlen(source)};
	bool envChanged = false;
	int result = 2;
	if (directory == NULL) goto cleanup;
	path = xrtPathJoin(directory, "配置.env");
	if (path == NULL || !xrtFileWriteAll(path, input)) goto cleanup;
	config = xrtDotenvCreate();
	if (config == NULL || !xrtDotenvLoad(config, path, 4194304u) ||
		xrtDotenvCount(config) != 3u ||
		xrtDotenvGet(config, "GREETING") == NULL ||
		strcmp(xrtDotenvGet(config, "GREETING"), "你好, world") != 0 ||
		!xrtDotenvSet(config, "DB_PORT", "6432")) {
		fprintf(stderr, "dotenv C example: %s\n", xrtDotenvErrorText(config));
		result = 3;
		goto cleanup;
	}
	printf("%s:%s\n", xrtDotenvGet(config, "DB_HOST"), xrtDotenvGet(config, "DB_PORT"));
	puts(xrtDotenvGet(config, "GREETING"));

	/* 替换配置后再显式导入进程环境，验证默认保留与主动覆盖。 */
	if (!xrtDotenvParse(config, replacement, strlen(replacement), 4194304u) ||
		xrtDotenvCount(config) != 1u || !xrtDotenvHas(config, envKey) ||
		xrtDotenvHas(config, "DB_HOST") || !xrtEnvLookup(envKey, &previous) ||
		!xrtEnvSet(envKey, "original")) goto cleanup;
	envChanged = true;
	printf("replace: %zu\n", xrtDotenvCount(config));
	if (!xrtDotenvApply(config, false)) goto cleanup;
	current = xrtEnvGet(envKey);
	if (current == NULL || strcmp(current, "original") != 0) goto cleanup;
	printf("apply default: %s\n", current);
	xrtFree(current);
	current = NULL;
	if (!xrtDotenvApply(config, true)) goto cleanup;
	current = xrtEnvGet(envKey);
	if (current == NULL || strcmp(current, "new") != 0) goto cleanup;
	printf("apply overwrite: %s\n", current);
	xrtFree(current);
	current = NULL;
	xrtDotenvClear(config);
	if (xrtDotenvCount(config) != 0u) goto cleanup;
	printf("clear: %zu\n", xrtDotenvCount(config));
	result = 0;

cleanup:
	xrtFree(current);
	if (envChanged && !(previous != NULL ? xrtEnvSet(envKey, previous) :
		xrtEnvRemove(envKey))) result = 4;
	xrtFree(previous);
	xrtDotenvDestroy(config);
	if (path != NULL && xrtFileExists(path) && !xrtFileDelete(path)) result = 4;
	if (directory != NULL && !xrtDirRemove(directory)) result = 4;
	xrtFree(path);
	xrtFree(directory);
	if (result == 0) puts("dotenv example: PASS");
	return result;
}

/* 教学范例可单文件运行；正式扩展包仍按清单分别编译。 */
#include "../xrt_dotenv.c"
