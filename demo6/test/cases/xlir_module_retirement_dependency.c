#include <windows.h>
static unsigned *unloaded, *deletes, *deletes_at_unload;
__declspec(dllexport) void retirement_dependency_bind(unsigned* gone, unsigned* count, unsigned* observed)
{ unloaded = gone; deletes = count; deletes_at_unload = observed; }
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance; (void)reserved;
    if (reason == DLL_PROCESS_DETACH && unloaded != NULL) {
        *deletes_at_unload = *deletes; ++*unloaded;
    }
    return TRUE;
}
