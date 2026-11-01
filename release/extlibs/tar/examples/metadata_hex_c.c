/* 只读查看 TAR 元数据；十六进制输出区分空格、换行与 UTF-8 字节。 */
#include "../xrt_tar_common.c"
#ifndef XTAR_TEST_READER
#define XTAR_TEST_READER "../xrt_tar_reader.c"
#endif
#include XTAR_TEST_READER
#include <stdio.h>

static void hex(cstr text)
{
    for (const unsigned char* p = (const unsigned char*)text; *p != 0; ++p)
        printf("%02x", (unsigned)*p);
}

int main(int argc, char** argv)
{
    if (argc < 2) { puts("usage: metadata_hex ARCHIVE..."); return 2; }
    for (int i = 1; i < argc; ++i) {
        xrttarreader* reader = xrtTarReaderOpenFile(argv[i], NULL);
        xrttarentry entry;
        if (reader == NULL) return 1;
        while (xrtTarReaderNext(reader, &entry)) {
            /* C 的条目文本借用 Reader，须在 next/destroy 之前使用。 */
            hex(entry.sName); putchar(';');
            hex(entry.sLinkName); putchar(';');
            hex(entry.sUserName); putchar(';');
            hex(entry.sGroupName); putchar('\n');
        }
        bool ok = xrtTarReaderEOF(reader);
        xrtTarReaderDestroy(reader);
        if (!ok) return 1;
    }
    return 0;
}
