@echo off
rem DEMO6 shared toolchain defaults. Call this file after "cd /d %~dp0".
rem Existing environment variables are kept so CI or local builds can override paths.

if not defined XL6_TCC set "XL6_TCC=..\demo5\lib\tcc\tcc.exe"
if not defined XL6_FLEX set "XL6_FLEX=..\demo5\tools\flex.exe"
if not defined XL6_LEMON set "XL6_LEMON=..\demo5\tools\lemon.exe"

if not defined XL6_TCC_ROOT set "XL6_TCC_ROOT=..\demo5\release\tcc"
if not defined XL6_TCC_SOURCE_ROOT set "XL6_TCC_SOURCE_ROOT=..\demo5\lib\tcc"
if not defined XL6_TCC_INCLUDE set "XL6_TCC_INCLUDE=..\demo5\lib\tcc\include"
if not defined XL6_TCC_WIN_INCLUDE set "XL6_TCC_WIN_INCLUDE=..\demo5\release\tcc\include_win"
if not defined XL6_TCC_WINAPI_INCLUDE set "XL6_TCC_WINAPI_INCLUDE=..\demo5\release\tcc\include_win\winapi"
if not defined XL6_TCC_LIB set "XL6_TCC_LIB=..\demo5\release\tcc\lib"
if not defined XL6_TCC_DLL set "XL6_TCC_DLL=..\demo5\lib\tcc\libtcc.dll"
if not defined XL6_TCC_LIBTCC1 set "XL6_TCC_LIBTCC1=..\demo5\release\tcc\lib\libtcc1-64.a"

if not defined XL6_XRT_OBJ set "XL6_XRT_OBJ=..\..\xrt\release\x64\xrt.o"

set "XL6_TCC_SOURCE_INCLUDE=-I"%XL6_TCC_SOURCE_ROOT%""
set "XL6_TCC_INCLUDES=-I"%XL6_TCC_INCLUDE%" -I"%XL6_TCC_WIN_INCLUDE%""
set "XL6_TCC_INCLUDES_WINAPI=%XL6_TCC_INCLUDES% -I"%XL6_TCC_WINAPI_INCLUDE%""
set "XL6_TCC_LIB_FLAGS=-L"%XL6_TCC_LIB%""
set "XL6_DEFAULT_LINKS="%XL6_XRT_OBJ%" "%XL6_TCC_LIBTCC1%" -lWs2_32 -lIPHLPAPI -lShell32"
set "XL6_XL_DEFAULT_LINK_ARGS=--link "%XL6_XRT_OBJ%" --link "%XL6_TCC_LIBTCC1%" --link Ws2_32 --link IPHLPAPI --link Shell32"

exit /b 0
