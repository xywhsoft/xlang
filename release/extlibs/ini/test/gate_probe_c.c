/* 只验证门禁的进程状态、参数和输出，不承担 INI 功能验收。 */
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

int main(int argc, char** argv)
{
    if (argc == 1) return puts("empty arguments") < 0 ? 39 : 0;
    if (argc != 2) return 38;
    if (strcmp(argv[1], "exit37") == 0) return 37;
    if (strcmp(argv[1], "streams") == 0) {
        fputs("stdout\n", stdout);
        fputs("stderr\n", stderr);
        return 0;
    }
    if (strcmp(argv[1], "bare-cr") == 0) return fputs("a\rb\n", stdout) < 0 ? 39 : 0;
    if (strcmp(argv[1], "relative") == 0) {
        char text[32];
        FILE* file = fopen("probe.txt", "rb");
        if (file == NULL) return 39;
        if (fgets(text, sizeof(text), file) == NULL) { fclose(file); return 39; }
        if (fclose(file) != 0) return 39;
        return fputs(text, stdout) < 0 ? 39 : 0;
    }
    if (strcmp(argv[1], "wait") == 0) {
#ifdef _WIN32
        Sleep(30000);
#else
        sleep(30);
#endif
        return 0;
    }
    return 38;
}
