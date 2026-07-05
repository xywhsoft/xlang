@echo off
setlocal

set ROOT=%~dp0
set OUT_DIR=%ROOT%release
set XGE_ROOT=D:\GIT\xge
set XGE_STABLE_DIR=D:\GIT\auto_test_ide\build
set XGE_DLL=%XGE_STABLE_DIR%\xge.dll
set OUT=%OUT_DIR%\xlang_ide.exe

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

where gcc >nul 2>nul
if errorlevel 1 (
    echo [ERROR] gcc not found in PATH
    exit /b 1
)

if not exist "%XGE_ROOT%\xui.h" (
    echo [ERROR] xui.h not found: %XGE_ROOT%\xui.h
    exit /b 1
)

if not exist "%XGE_DLL%" (
    echo [ERROR] stable xge.dll not found: %XGE_DLL%
    exit /b 1
)

set INC=-I"%ROOT%src" -I"%XGE_ROOT%"
set SRC=
set SRC=%SRC% "%ROOT%src\main.c"
set SRC=%SRC% "%ROOT%src\app\xide_app.c"
set SRC=%SRC% "%ROOT%src\app\xide_platform.c"
set SRC=%SRC% "%ROOT%src\i18n\xide_i18n.c"
set SRC=%SRC% "%ROOT%src\workspace\xide_workspace.c"
set SRC=%SRC% "%ROOT%src\ui\xide_main_window.c"
set SRC=%SRC% "%ROOT%src\ui\xide_dock_layout.c"
set SRC=%SRC% "%ROOT%src\views\xide_view_common.c"
set SRC=%SRC% "%ROOT%src\views\xide_home_view.c"
set SRC=%SRC% "%ROOT%src\views\xide_project_view.c"
set SRC=%SRC% "%ROOT%src\views\xide_command_view.c"
set SRC=%SRC% "%ROOT%src\views\xide_help_view.c"
set SRC=%SRC% "%ROOT%src\views\xide_editor_view.c"
set SRC=%SRC% "%ROOT%src\views\xide_output_view.c"
set SRC=%SRC% "%ROOT%src\views\xide_task_view.c"
set SRC=%SRC% "%ROOT%src\views\xide_debug_view.c"
set SRC=%SRC% "%ROOT%src\views\xide_tools_view.c"
set SRC=%SRC% "%ROOT%src\views\xide_property_view.c"
set SRC=%SRC% "%ROOT%src\lang\xide_xlang_language.c"

set FLAGS=-O2 -Wall -Wextra -Wno-unused-parameter -finput-charset=UTF-8 -fexec-charset=UTF-8 -DXGE_DLL -DXUI_DLL -DXGE_DEBUGMODE=0
set LIBS="%XGE_DLL%" -lm -lws2_32 -liphlpapi -lgdi32 -luser32 -lshell32 -lopengl32 -lole32 -lwinmm -lavrt

echo [BUILD] %OUT%
gcc %FLAGS% %INC% -o "%OUT%" %SRC% %LIBS%
if errorlevel 1 exit /b 1

copy /Y "%XGE_DLL%" "%OUT_DIR%\xge.dll" >nul
if errorlevel 1 exit /b 1

echo [OK] %OUT%
exit /b 0
