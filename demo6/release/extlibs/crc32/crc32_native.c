#include <stdint.h>
#include <stdio.h>
#include <string.h>

static char g_xcrc32_error[512];
static uint32_t g_xcrc32_table[256];
static int g_xcrc32_table_ready = 0;

#define XCRC32_HEX_SLOTS 8u

static char g_xcrc32_hex[XCRC32_HEX_SLOTS][9];
static size_t g_xcrc32_hex_index;

static void xcrc32_set_error(const char* text)
{
    size_t i = 0u;

    if ( text == NULL ) {
        text = "";
    }

    while ( i + 1u < sizeof(g_xcrc32_error) && text[i] != '\0' ) {
        g_xcrc32_error[i] = text[i];
        ++i;
    }
    g_xcrc32_error[i] = '\0';
}

static void xcrc32_init_table(void)
{
    uint32_t i;

    if ( g_xcrc32_table_ready ) {
        return;
    }

    for ( i = 0u; i < 256u; ++i ) {
        uint32_t crc = i;
        int bit;

        for ( bit = 0; bit < 8; ++bit ) {
            if ( (crc & 1u) != 0u ) {
                crc = (crc >> 1) ^ 0xedb88320u;
            } else {
                crc >>= 1;
            }
        }
        g_xcrc32_table[i] = crc;
    }
    g_xcrc32_table_ready = 1;
}

static uint32_t xcrc32_update_value(uint32_t crc, const unsigned char* data, size_t size)
{
    size_t i;

    xcrc32_init_table();
    for ( i = 0u; i < size; ++i ) {
        crc = g_xcrc32_table[(crc ^ (uint32_t)data[i]) & 0xffu] ^ (crc >> 8);
    }
    return crc;
}

static uint32_t xcrc32_string_value(const char* text)
{
    if ( text == NULL ) {
        text = "";
    }
    return xcrc32_update_value(0xffffffffu, (const unsigned char*)text, strlen(text)) ^ 0xffffffffu;
}

static const char* xcrc32_format(uint32_t value)
{
    char* out = g_xcrc32_hex[g_xcrc32_hex_index % XCRC32_HEX_SLOTS];

    g_xcrc32_hex_index = (g_xcrc32_hex_index + 1u) % XCRC32_HEX_SLOTS;
    snprintf(out, sizeof(g_xcrc32_hex[0]), "%08lx", (unsigned long)value);
    xcrc32_set_error("");
    return out;
}

int xcrc32_available(void)
{
    return 1;
}

const char* xcrc32_version(void)
{
    return "crc32 0.1.0";
}

const char* xcrc32_last_error(void)
{
    return g_xcrc32_error;
}

long long xcrc32_init(void)
{
    xcrc32_set_error("");
    return (long long)0xffffffffu;
}

long long xcrc32_update(long long crc, const char* text)
{
    uint32_t value = (uint32_t)crc;

    if ( text == NULL ) {
        text = "";
    }
    xcrc32_set_error("");
    return (long long)xcrc32_update_value(value, (const unsigned char*)text, strlen(text));
}

long long xcrc32_finish(long long crc)
{
    xcrc32_set_error("");
    return (long long)((uint32_t)crc ^ 0xffffffffu);
}

long long xcrc32_string(const char* text)
{
    xcrc32_set_error("");
    return (long long)xcrc32_string_value(text);
}

const char* xcrc32_string_hex(const char* text)
{
    return xcrc32_format(xcrc32_string_value(text));
}

long long xcrc32_file(const char* path)
{
    unsigned char buffer[32768];
    uint32_t crc = 0xffffffffu;
    FILE* fp;
    size_t n;

    if ( path == NULL || path[0] == '\0' ) {
        xcrc32_set_error("empty file path");
        return 0;
    }

    fp = fopen(path, "rb");
    if ( fp == NULL ) {
        xcrc32_set_error("failed to open file");
        return 0;
    }

    while ( (n = fread(buffer, 1u, sizeof(buffer), fp)) > 0u ) {
        crc = xcrc32_update_value(crc, buffer, n);
    }

    if ( ferror(fp) ) {
        fclose(fp);
        xcrc32_set_error("failed to read file");
        return 0;
    }

    fclose(fp);
    xcrc32_set_error("");
    return (long long)(crc ^ 0xffffffffu);
}

const char* xcrc32_file_hex(const char* path)
{
    long long value = xcrc32_file(path);

    if ( g_xcrc32_error[0] != '\0' ) {
        return "";
    }
    return xcrc32_format((uint32_t)value);
}

const char* xcrc32_hex(long long crc)
{
    return xcrc32_format((uint32_t)crc);
}
