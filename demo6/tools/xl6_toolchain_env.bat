@echo off
rem DEMO6 shared toolchain defaults. Call this file after "cd /d %~dp0".
rem Existing environment variables are kept so CI or local builds can override paths.

if not defined XL6_BOOTSTRAP_CC set "XL6_BOOTSTRAP_CC=gcc"
if not defined XL6_TCC set "XL6_TCC=build\tmp\tcc_bootstrap.exe"
if not defined XL6_FLEX set "XL6_FLEX=flex"
if not defined XL6_LEMON set "XL6_LEMON=lemon"

if not defined XL6_EMBED_TCC_SOURCE_ROOT set "XL6_EMBED_TCC_SOURCE_ROOT=lib\tcc"
if not defined XL6_EMBED_TCC_DIST_X64 set "XL6_EMBED_TCC_DIST_X64=lib\tcc\win32\dist\x86_64"
if not defined XL6_TCC_ROOT set "XL6_TCC_ROOT=lib\tcc\win32\dist\x86_64"
if not defined XL6_TCC_SOURCE_ROOT set "XL6_TCC_SOURCE_ROOT=lib\tcc"
if not defined XL6_TCC_INCLUDE set "XL6_TCC_INCLUDE=lib\tcc\include"
if not defined XL6_TCC_WIN_INCLUDE set "XL6_TCC_WIN_INCLUDE=lib\tcc\win32\dist\x86_64\include"
if not defined XL6_TCC_WINAPI_INCLUDE set "XL6_TCC_WINAPI_INCLUDE=lib\tcc\win32\dist\x86_64\include\winapi"
if not defined XL6_TCC_LIB set "XL6_TCC_LIB=lib\tcc\win32\dist\x86_64\lib"
if not defined XL6_TCC_DLL set "XL6_TCC_DLL=lib\tcc\win32\dist\x86_64\libtcc.dll"
if not defined XL6_TCC_LIBTCC1 set "XL6_TCC_LIBTCC1=lib\tcc\win32\dist\x86_64\lib\libtcc1.a"

if not defined XL6_XRT_OBJ set "XL6_XRT_OBJ=build\tmp\xrt.o"

set "XL6_TCC_SOURCE_INCLUDE=-I"%XL6_TCC_SOURCE_ROOT%""
set "XL6_TCC_INCLUDES=-I"%XL6_TCC_INCLUDE%" -I"%XL6_TCC_WIN_INCLUDE%""
set "XL6_TCC_INCLUDES_WINAPI=%XL6_TCC_INCLUDES% -I"%XL6_TCC_WINAPI_INCLUDE%""
set "XL6_TCC_LIB_FLAGS=-L"%XL6_TCC_LIB%""
set "XL6_DEFAULT_LINKS="%XL6_XRT_OBJ%" "%XL6_TCC_LIBTCC1%" -lWs2_32 -lIPHLPAPI -lShell32"
set "XL6_XL_DEFAULT_LINK_ARGS=--link "%XL6_XRT_OBJ%" --link "%XL6_TCC_LIBTCC1%" --link Ws2_32 --link IPHLPAPI --link Shell32"

exit /b 0
