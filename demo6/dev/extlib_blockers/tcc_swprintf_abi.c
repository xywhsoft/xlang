#include <wchar.h>

int main(void)
{
    wchar_t text[32] = {0};
    int length = swprintf(text, 32, L"value=%d", 7);
    return length == 7 && wcscmp(text, L"value=7") == 0 ? 0 : 1;
}
