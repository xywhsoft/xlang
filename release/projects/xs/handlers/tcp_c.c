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

static int ends_with(const char* text, const char* suffix)
{
    size_t textLen;
    size_t suffixLen;
    if ( text == NULL || suffix == NULL ) {
        return 0;
    }
    textLen = strlen(text);
    suffixLen = strlen(suffix);
    return textLen >= suffixLen && strcmp(text + textLen - suffixLen, suffix) == 0;
}

int main(int argc, char** argv)
{
    int offset = is_protocol(arg_at(argc, argv, 0)) ? 0 : 1;
    const char* last = arg_at(argc, argv, argc - 1);
    const char* out = ends_with(last, ".out") ? last : "";
    char response[4096];
    snprintf(response, sizeof(response), "c-tcp:%s:%s", arg_at(argc, argv, offset + 1), arg_at(argc, argv, offset + 2));
    if ( out != NULL && out[0] != '\0' ) {
        FILE* fp = fopen(out, "wb");
        if ( fp != NULL ) {
            fputs(response, fp);
            fclose(fp);
        }
    } else {
        puts(response);
    }
    return 0;
}
