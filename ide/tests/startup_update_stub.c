/* Offline child process used only by startup_services_test. */
#include <windows.h>
#include <stdio.h>
int main(void)
{
    FILE* log=fopen("startup-update-stub.log","ab");
    if(!log)return 1;
    fprintf(log,"offline updater child %lu\n",(unsigned long)GetCurrentProcessId());fclose(log);Sleep(60);return 0;
}
