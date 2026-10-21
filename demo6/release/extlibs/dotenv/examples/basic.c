#include "../include/xrt_dotenv.h"
#include "xrt.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	const char* source = "# application\nDB_HOST=localhost\nDB_PORT=5432\nGREETING=\"你好, world\"\n";
	str directory = xrtDirTemp(NULL, "xlang-dotenv-", NULL);
	str path = NULL;
	xrtdotenv* config = NULL;
	xbytesview input = {(cbytes)source, strlen(source)};
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
	result = 0;

cleanup:
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
