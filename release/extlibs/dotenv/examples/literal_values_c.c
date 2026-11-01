/* 仅解析本地文本，不读取、展开或修改任何进程环境变量。 */
#include "../include/xrt_dotenv.h"
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* source = "export SERVICE = demo\nSERVICE=worker\n"
        "PATH='c:\\tools\\bin'\nTOKEN=${USER}\nHASH=a#b\n"
        "EMPTY= # intentional empty\nESCAPED=\"row\\tvalue\"\nexport = ordinary-key\n";
    xrtdotenv* config = xrtDotenvCreate();
    int result = 1;
    if (config == NULL || !xrtDotenvParse(config, source, strlen(source), 4096u)) goto cleanup;
    if (xrtDotenvCount(config) != 7u || strcmp(xrtDotenvGet(config, "EMPTY"), "") != 0 ||
        strcmp(xrtDotenvGet(config, "ESCAPED"), "row\tvalue") != 0) goto cleanup;
    printf("service: %s\n", xrtDotenvGet(config, "SERVICE"));
    printf("path: %s\n", xrtDotenvGet(config, "PATH"));
    printf("literal: %s\n", xrtDotenvGet(config, "TOKEN"));
    printf("hash: %s\n", xrtDotenvGet(config, "HASH"));
    printf("empty: true\n");
    printf("tab bytes: %zu\n", strlen(xrtDotenvGet(config, "ESCAPED")));
    printf("export key: %s\n", xrtDotenvGet(config, "export"));
    result = 0;
cleanup:
    xrtDotenvDestroy(config);
    return result;
}

#include "../xrt_dotenv.c"
