#include <stddef.h>
#include <string.h>

int main(void) {
    static const char embedded[] = {'a', '\0', 'b', '\0'};
    return strlen("") == 0 && strlen("hello") == 5 &&
        strlen("你好") == 6 && sizeof(embedded) - 1 == 3 &&
        strlen(embedded) == 1 ? 0 : 1;
}
