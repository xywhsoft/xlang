gcc type.c smm.c pmm.c -g -o test.exe

drmemory -check_leaks test.exe

pause