#include <windows.h>

int *tls_address(int number);

typedef struct {
    HANDLE ready;
    HANDLE release;
    int *address;
    int number;
} worker;

static DWORD WINAPI run_worker(void *argument)
{
    worker *item = (worker *)argument;
    item->address = tls_address(item->number);
    SetEvent(item->ready);
    WaitForSingleObject(item->release, INFINITE);
    return 0;
}

int main(void)
{
    DWORD written;
    worker items[2] = {{0}, {0}};
    HANDLE threads[2] = {NULL, NULL};
    HANDLE ready[2];
    HANDLE release = CreateEventA(NULL, TRUE, FALSE, NULL);
    int same;
    int i;

    if (!release) return 2;
    for (i = 0; i < 2; ++i) {
        items[i].ready = CreateEventA(NULL, TRUE, FALSE, NULL);
        items[i].release = release;
        items[i].number = i + 1;
        if (!items[i].ready) return 2;
        threads[i] = CreateThread(NULL, 0, run_worker, &items[i], 0, NULL);
        if (!threads[i]) return 2;
        ready[i] = items[i].ready;
    }
    if (WaitForMultipleObjects(2, ready, TRUE, 5000) != WAIT_OBJECT_0)
        return 2;
    same = items[0].address == items[1].address;
    if (same)
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), "same TLS address: 1\n", 20, &written, NULL);
    else
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), "same TLS address: 0\n", 20, &written, NULL);
    SetEvent(release);
    WaitForMultipleObjects(2, threads, TRUE, 5000);
    for (i = 0; i < 2; ++i) {
        CloseHandle(threads[i]);
        CloseHandle(ready[i]);
    }
    CloseHandle(release);
    return same ? 1 : 0;
}
