#include <stdio.h>
#include <string.h>

static const char* arg_at(int argc, char** argv, int index)
{
    return index < argc ? argv[index] : "";
}

static int is_protocol(const char* text)
{
    return text != NULL && (
        strcmp(text, "http") == 0 ||
        strcmp(text, "tcp") == 0 ||
        strcmp(text, "udp") == 0 ||
        strcmp(text, "custom") == 0);
}

static void write_response(const char* path, const char* text)
{
    FILE* fp;
    if ( path == NULL || path[0] == '\0' ) {
        puts(text);
        return;
    }
    fp = fopen(path, "wb");
    if ( fp == NULL ) {
        return;
    }
    fputs(text, fp);
    fclose(fp);
}

int main(int argc, char** argv)
{
    int offset = is_protocol(arg_at(argc, argv, 0)) ? 0 : 1;
    const char* protocol = arg_at(argc, argv, offset);
    const char* method = arg_at(argc, argv, offset + 1);
    const char* path = arg_at(argc, argv, offset + 2);
    const char* query = arg_at(argc, argv, offset + 3);
    const char* body = arg_at(argc, argv, offset + 4);
    const char* out = arg_at(argc, argv, offset + 5);
    char response[4096];

    snprintf(response, sizeof(response), "c-http:%s:%s:%s:%s:%s", protocol, method, path, query, body);
    write_response(out, response);
    return 0;
}
