#include "../semver_native.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char line[2048];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *tab = strchr(line, '\t');
        char *end = strchr(line, '\n');
        if (tab == NULL || end == NULL || tab > end) return 2;
        *tab++ = '\0';
        *end = '\0';
        puts(xsemver_satisfies(line, tab) ? "1" : "0");
    }
    return ferror(stdin) ? 2 : 0;
}
