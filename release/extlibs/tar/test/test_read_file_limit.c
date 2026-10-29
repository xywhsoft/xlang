#include "xrt.h"

#include <stdio.h>
#include <string.h>

extern void *xtar_read_file(const char *path, unsigned long long limit);

int main(void)
{
	str path = NULL;
	xfile temporary = xrtFileTemp(NULL, "xlang-tar-limit-", ".bin", &path);
	xbuffer *data = NULL;
	xrtresourcelimits limits;
	int result = 1;

	if (temporary == NULL || path == NULL) goto done;
	/* 关闭临时句柄后，按真实文件读取路径验证默认输入限额。 */
	{
		bool closed = xrtClose(temporary);
		temporary = NULL;
		if (!closed) goto done;
	}
	if (!xrtFileWriteAll(path, XRT_BYTES_LITERAL("tiny"))) goto done;
	data = xtar_read_file(path, 0);
	if (data == NULL || data->Size != 4u ||
		memcmp(data->Data, "tiny", 4u) != 0) goto done;
	xrtBufferDestroy(data);
	data = NULL;

	xrtResourceLimitsInit(&limits);
	if (!xrtFileSetSize(path, limits.iMaxInputBytes + 1u)) goto done;
	data = xtar_read_file(path, 0);
	if (data != NULL || xrtGetError() == NULL ||
		xrtErrorKind(xrtGetError()) != XERR_RANGE) goto done;

	result = 0;
	puts("tar default read limit: PASS");
done:
	if (result != 0) {
		fprintf(stderr, "tar default read limit failed: data=%p, error=%d\n",
			(void *)data, xrtGetError() != NULL ?
				(int)xrtErrorKind(xrtGetError()) : -1);
	}
	xrtBufferDestroy(data);
	if (temporary != NULL) (void)xrtClose(temporary);
	if (path != NULL) {
		(void)xrtFileDelete(path);
		xrtFree(path);
	}
	return result;
}
