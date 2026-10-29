#ifndef XRT_DOTENV_H
#define XRT_DOTENV_H

#include <stdbool.h>
#include <stddef.h>

typedef struct xrtdotenv xrtdotenv;

enum {
	XRT_DOTENV_OK = 0,
	XRT_DOTENV_ARGUMENT = 1,
	XRT_DOTENV_FORMAT = 2,
	XRT_DOTENV_LIMIT = 3,
	XRT_DOTENV_MEMORY = 4,
	XRT_DOTENV_IO = 5,
	XRT_DOTENV_ENVIRONMENT = 6
};

/* 配置对象互不共享；读取到的字符串借用自对象，下次修改前有效。 */
xrtdotenv* xrtDotenvCreate(void);
void xrtDotenvDestroy(xrtdotenv* config);
bool xrtDotenvParse(xrtdotenv* config, const char* text, size_t size, size_t maxBytes);
bool xrtDotenvLoad(xrtdotenv* config, const char* path, size_t maxBytes);
bool xrtDotenvSet(xrtdotenv* config, const char* key, const char* value);
const char* xrtDotenvGet(const xrtdotenv* config, const char* key);
bool xrtDotenvHas(const xrtdotenv* config, const char* key);
size_t xrtDotenvCount(const xrtdotenv* config);
void xrtDotenvClear(xrtdotenv* config);

/* 显式写入进程环境；进程环境是全局资源，多键操作不承诺原子性。 */
bool xrtDotenvApply(xrtdotenv* config, bool overwrite);
int xrtDotenvErrorCode(const xrtdotenv* config);
size_t xrtDotenvErrorLine(const xrtdotenv* config);
const char* xrtDotenvErrorText(const xrtdotenv* config);

#endif
