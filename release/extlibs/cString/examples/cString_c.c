#include <stdio.h>
#include <string.h>

int main(void) {
    const char text[] = {'a', '\0', 'b', '\0'};
    printf("UTF-8: %zu\n", strlen("你好"));
    printf("XLang bytes: %zu\n", sizeof(text) - 1);
    printf("C string: %zu\n", strlen(text));
    return 0;
}
