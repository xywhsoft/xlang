#include "src/common/xl6_common.h"

char* xl6StrDupRange(const char* text, size_t len)
{
    char* out;

    out = (char*)malloc(len + 1);
    if ( out == NULL ) {
        return NULL;
    }
    if ( len > 0 ) {
        memcpy(out, text, len);
    }
    out[len] = '\0';
    return out;
}

char* xl6ReadFileText(const char* path, size_t* outLen)
{
    FILE* fp;
    long fileSize;
    char* data;
    size_t readSize;

    if ( outLen != NULL ) {
        *outLen = 0;
    }

    fp = fopen(path, "rb");
    if ( fp == NULL ) {
        return NULL;
    }

    if ( fseek(fp, 0, SEEK_END) != 0 ) {
        fclose(fp);
        return NULL;
    }
    fileSize = ftell(fp);
    if ( fileSize < 0 ) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    data = (char*)malloc((size_t)fileSize + 1);
    if ( data == NULL ) {
        fclose(fp);
        return NULL;
    }

    readSize = fread(data, 1, (size_t)fileSize, fp);
    fclose(fp);
    data[readSize] = '\0';

    if ( outLen != NULL ) {
        *outLen = readSize;
    }
    return data;
}
