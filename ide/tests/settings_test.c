#include "platform/settings.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(test) do { if(!(test)) { fprintf(stderr,"settings_test:%d: %s; %s\n",__LINE__,#test,error.message); failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* content) { return xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)content,strlen(content)}); }
int main(int argc,char** argv)
{
    xide_settings settings={0},loaded={0}; xide_model_error error={0}; int failed=0; char *root=NULL,*path=NULL,*compiler=NULL,*defaultPath=NULL,*font=NULL;
    CHECK(argc==2); root=xidePathResolve(argv[1],"中文 设置"); CHECK(root && xrtDirCreateAll(root)); path=xidePathResolve(root,"settings.json"); compiler=xidePathResolve(root,"编译器 空格.exe"); CHECK(path && compiler && writeText(compiler,"fixture"));
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"future\":7,\"toolchain\":{\"futureToolchain\":true}}"));
    CHECK(xideSettingsLoad(&settings,path,&error) && xideSettingsSetCompiler(&settings,compiler,&error) && xideSettingsLoad(&loaded,path,&error));
    CHECK(loaded.compiler && xidePathEqual(loaded.compiler,compiler) && xrtValueObjectGet(loaded.source,(xstrview){"future",6}));
    CHECK(!xideSettingsSetCompiler(&settings,"missing.exe",&error) && error.code==XIDE_MODEL_INVALID);
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"external\":true}") && !xideSettingsSetCompiler(&settings,compiler,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(writeText(path,"{broken") && !xideSettingsLoad(&settings,path,&error) && !xideSettingsSetCompiler(&loaded,compiler,&error));
    size_t length=0; unsigned char* content=xrtFileReadAllLimit(path,256,&length); CHECK(content && length==7 && !memcmp(content,"{broken",7)); xrtFree(content);
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\"}") && xideSettingsLoad(&settings,path,&error));
    wchar_t locked[32768]; CHECK(MultiByteToWideChar(CP_UTF8,0,path,-1,locked,32768)); HANDLE handle=CreateFileW(locked,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); CHECK(handle!=INVALID_HANDLE_VALUE);
    int okay=xideSettingsSetCompiler(&settings,compiler,&error); CloseHandle(handle); CHECK(!okay && error.code==XIDE_MODEL_IO);
    defaultPath=xideSettingsDefaultPath(); CHECK(defaultPath && strstr(defaultPath,"option"));
    font=xidePathResolve(root,"字体 空格.ttf"); CHECK(font && writeText(font,"font settings fixture"));
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"future\":7,\"editor\":{\"futureEditor\":true,\"font\":{\"futureFont\":9}}}"));
    CHECK(xideSettingsLoad(&settings,path,&error) && xideSettingsSetEditorFont(&settings,font,24,&error) && xideSettingsSetCompiler(&settings,compiler,&error));
    CHECK(xideSettingsLoad(&loaded,path,&error) && loaded.editorFontSize==24 && xidePathEqual(loaded.editorFont,font) && xidePathEqual(loaded.compiler,compiler));
    xvalue* editor=xrtValueObjectGet(loaded.source,(xstrview){"editor",6}),*fontJson=xrtValueObjectGet(editor,(xstrview){"font",4});
    CHECK(xrtValueObjectGet(editor,(xstrview){"futureEditor",12}) && xrtValueObjectGet(fontJson,(xstrview){"futureFont",10}));
    CHECK(!xideSettingsSetEditorFont(&settings,font,41,&error) && error.code==XIDE_MODEL_INVALID);
    CHECK(!xideSettingsSetEditorFont(&settings,"missing.ttf",16,&error) && error.code==XIDE_MODEL_INVALID);
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"external\":true}") && !xideSettingsSetEditorFont(&settings,font,20,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"editor\":{\"font\":{\"size\":41}}}") && !xideSettingsLoad(&settings,path,&error));
    CHECK(loaded.editorFontSize==24);
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"ui\":{\"futureUi\":true,\"font\":{\"futureFont\":11}}}"));
    CHECK(xideSettingsLoad(&settings,path,&error) && xideSettingsSetUiFont(&settings,font,28,&error) && xideSettingsSetEditorFont(&settings,font,20,&error) && xideSettingsSetCompiler(&settings,compiler,&error));
    CHECK(xideSettingsLoad(&loaded,path,&error) && loaded.uiFontSize==28 && loaded.editorFontSize==20 && xidePathEqual(loaded.uiFont,font) && xidePathEqual(loaded.compiler,compiler));
    xvalue* ui=xrtValueObjectGet(loaded.source,(xstrview){"ui",2}),*uiFont=xrtValueObjectGet(ui,(xstrview){"font",4});
    CHECK(xrtValueObjectGet(ui,(xstrview){"futureUi",8}) && xrtValueObjectGet(uiFont,(xstrview){"futureFont",10}));
    CHECK(!xideSettingsSetUiFont(&settings,font,7,&error) && error.code==XIDE_MODEL_INVALID);
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"ui\":{\"font\":{\"size\":41}}}") && !xideSettingsLoad(&settings,path,&error) && loaded.uiFontSize==28);
#if XIDE_ENABLE_C_LSP
    /* C-server preferences share the existing atomic, conflict-aware store. */
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"future\":7,\"toolchain\":{\"cServer\":\"编译器 空格.exe\",\"futureToolchain\":true}}"));
    CHECK(xideSettingsLoad(&settings,path,&error) && settings.cServer && xidePathEqual(settings.cServer,compiler));
    CHECK(xideSettingsSetCServer(&settings,compiler,&error) && xideSettingsSetCompiler(&settings,compiler,&error) && xideSettingsSetEditorFont(&settings,font,21,&error) && xideSettingsSetUiFont(&settings,font,23,&error));
    CHECK(xideSettingsLoad(&loaded,path,&error) && xidePathEqual(loaded.cServer,compiler) && xidePathEqual(loaded.compiler,compiler) && loaded.editorFontSize==21 && loaded.uiFontSize==23);
    xvalue* toolchain=xrtValueObjectGet(loaded.source,(xstrview){"toolchain",9});
    CHECK(xrtValueObjectGet(toolchain,(xstrview){"futureToolchain",15}) && xrtValueObjectGet(loaded.source,(xstrview){"future",6}));
    char* lockPath=malloc(strlen(path)+6); CHECK(lockPath); sprintf(lockPath,"%s.lock",path);
    CHECK(MultiByteToWideChar(CP_UTF8,0,lockPath,-1,locked,32768)); free(lockPath);
    handle=CreateFileW(locked,GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); CHECK(handle!=INVALID_HANDLE_VALUE);
    okay=xideSettingsSetCServer(&settings,compiler,&error); int lockCode=error.code;
    int fontOkay=xideSettingsSetEditorFont(&settings,font,24,&error); CloseHandle(handle);
    CHECK(!okay && lockCode==XIDE_MODEL_CONFLICT && !fontOkay && error.code==XIDE_MODEL_CONFLICT && settings.editorFontSize==21);
    CHECK(!xideSettingsSetCServer(&settings,"missing.exe",&error) && error.code==XIDE_MODEL_INVALID && !xideSettingsSetCServer(&settings,font,&error) && error.code==XIDE_MODEL_INVALID);
    CHECK(MultiByteToWideChar(CP_UTF8,0,path,-1,locked,32768)); handle=CreateFileW(locked,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); CHECK(handle!=INVALID_HANDLE_VALUE);
    okay=xideSettingsSetCServer(&settings,compiler,&error); CloseHandle(handle); CHECK(!okay && error.code==XIDE_MODEL_IO && xidePathEqual(settings.cServer,compiler));
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"external\":true}") && !xideSettingsSetCServer(&settings,compiler,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"toolchain\":{\"cServer\":7}}") && !xideSettingsLoad(&settings,path,&error) && xidePathEqual(loaded.cServer,compiler));
    CHECK(writeText(path,"{broken") && !xideSettingsSetCServer(&loaded,compiler,&error));
    content=xrtFileReadAllLimit(path,256,&length); CHECK(content && length==7 && !memcmp(content,"{broken",7)); xrtFree(content);
#else
    /* A retired key must not enable a server or block other preferences. */
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"future\":7,\"toolchain\":{\"cServer\":7,\"futureToolchain\":true}}"));
    CHECK(xideSettingsLoad(&settings,path,&error) && !settings.cServer);
    CHECK(xideSettingsSetCompiler(&settings,compiler,&error) && xideSettingsSetEditorFont(&settings,font,21,&error) && xideSettingsSetUiFont(&settings,font,23,&error));
    CHECK(xideSettingsLoad(&loaded,path,&error) && !loaded.cServer && xidePathEqual(loaded.compiler,compiler));
    xvalue* toolchain=xrtValueObjectGet(loaded.source,(xstrview){"toolchain",9});
    CHECK(!xrtValueObjectGet(toolchain,(xstrview){"cServer",7}) && xrtValueObjectGet(toolchain,(xstrview){"futureToolchain",15}) && xrtValueObjectGet(loaded.source,(xstrview){"future",6}));
    CHECK(writeText(path,"{\"format\":\"xlang-ide-settings-1\",\"external\":true}") && !xideSettingsSetCompiler(&settings,compiler,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(writeText(path,"{broken") && !xideSettingsSetCompiler(&loaded,compiler,&error));
    content=xrtFileReadAllLimit(path,256,&length); CHECK(content && length==7 && !memcmp(content,"{broken",7)); xrtFree(content);
#endif
    puts("settings_test passed: Unicode compiler/font preferences, nested unknown fields, sequential updates, restart, range/paths, external conflict and malformed preservation; feature-dependent C-server preferences.");
cleanup:
    xideSettingsClear(&settings); xideSettingsClear(&loaded); free(root); free(path); free(compiler); free(defaultPath); free(font); return failed;
}
