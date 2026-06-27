/*
 * TCC VFS LZMA packer.
 *
 * 这个程序只在构建阶段使用，用于把清单中的资源压缩为 LZMA 数据。
 * 发布产物不链接本文件，也不链接 LZMA encoder，运行时只保留 decoder。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LzmaEnc.h"

static void *xl6_lzma_alloc(ISzAllocPtr alloc, size_t size)
{
    (void)alloc;
    if (size == 0)
        return NULL;
    return malloc(size);
}

static void xl6_lzma_free(ISzAllocPtr alloc, void *address)
{
    (void)alloc;
    free(address);
}

static const ISzAlloc xl6_lzma_allocator = {
    xl6_lzma_alloc,
    xl6_lzma_free
};

static int read_all(const char *path, unsigned char **out_data, size_t *out_size)
{
    FILE *file;
    long size;
    unsigned char *data;

    *out_data = NULL;
    *out_size = 0;

    file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "open input failed: %s\n", path);
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "seek input failed: %s\n", path);
        return 1;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        fprintf(stderr, "tell input failed: %s\n", path);
        return 1;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "rewind input failed: %s\n", path);
        return 1;
    }

    data = (unsigned char *)malloc((size_t)size ? (size_t)size : 1u);
    if (!data) {
        fclose(file);
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    if (size > 0 && fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        fprintf(stderr, "read input failed: %s\n", path);
        return 1;
    }

    fclose(file);
    *out_data = data;
    *out_size = (size_t)size;
    return 0;
}

static int write_all(const char *path, const unsigned char *data, size_t size)
{
    FILE *file;

    file = fopen(path, "wb");
    if (!file) {
        fprintf(stderr, "open output failed: %s\n", path);
        return 1;
    }
    if (size > 0 && fwrite(data, 1, size, file) != size) {
        fclose(file);
        fprintf(stderr, "write output failed: %s\n", path);
        return 1;
    }
    fclose(file);
    return 0;
}

int main(int argc, char **argv)
{
    const char *input_path;
    const char *output_path;
    unsigned char *input;
    unsigned char *output;
    size_t input_size;
    SizeT props_size;
    SizeT packed_size;
    size_t output_capacity;
    int level;
    CLzmaEncProps props;
    SRes result;

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: tcc_vfs_lzma_pack input output [level]\n");
        return 2;
    }

    input_path = argv[1];
    output_path = argv[2];
    level = argc >= 4 ? atoi(argv[3]) : 9;
    if (level < 0)
        level = 0;
    if (level > 9)
        level = 9;

    if (read_all(input_path, &input, &input_size) != 0)
        return 1;

    /*
     * LZMA 最坏情况会略大于原始输入；这里预留一个宽松上界。
     * 前 5 字节保存 LZMA properties，后面是压缩数据。
     */
    output_capacity = input_size + input_size / 3u + 65536u;
    output = (unsigned char *)malloc(output_capacity ? output_capacity : 1u);
    if (!output) {
        free(input);
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    LzmaEncProps_Init(&props);
    props.level = level;
    props.writeEndMark = 0;
    props.numThreads = 1;
    props.reduceSize = (UInt64)input_size;

    props_size = LZMA_PROPS_SIZE;
    packed_size = (SizeT)(output_capacity - LZMA_PROPS_SIZE);
    result = LzmaEncode(
        output + LZMA_PROPS_SIZE,
        &packed_size,
        input,
        (SizeT)input_size,
        &props,
        output,
        &props_size,
        0,
        NULL,
        &xl6_lzma_allocator,
        &xl6_lzma_allocator);

    free(input);

    if (result != SZ_OK || props_size != LZMA_PROPS_SIZE) {
        free(output);
        fprintf(stderr, "lzma encode failed: %d\n", (int)result);
        return 1;
    }

    if (write_all(output_path, output, (size_t)packed_size + LZMA_PROPS_SIZE) != 0) {
        free(output);
        return 1;
    }

    free(output);
    return 0;
}
