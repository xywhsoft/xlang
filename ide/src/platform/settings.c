#include "platform/settings.h"
#include "platform/path.h"
#include "platform/startup_alias.h"
#include "platform/state_file.h"
#include "platform/file_io.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
static int fail(xide_model_error* error,int code,const char* message)
{
    if(error) { error->code=code; snprintf(error->message,sizeof(error->message),"%s",message); } return 0;
}
static xvalue* get(const xvalue* root,const char* key) { return root ? xrtValueObjectGet(root,sv(key)) : NULL; }
static char* string(const xvalue* root)
{
    xstrview value; if(!root || !xrtValueGetString(root,&value) || memchr(value.Data,0,value.Size)) return NULL;
    char* text=malloc(value.Size+1); if(text) { memcpy(text,value.Data,value.Size); text[value.Size]=0; } return text;
}
static xvalue* read(const char* path)
{
    size_t size; unsigned char* bytes=xideIoReadAllLimit(path,256*1024,&size); if(!bytes) return NULL;
    size_t offset=size>=3 && bytes[0]==0xef && bytes[1]==0xbb && bytes[2]==0xbf ? 3 : 0;
    xvalue* value=size>offset && !memchr(bytes+offset,0,size-offset) && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(char*)bytes+offset,(int)(size-offset),NULL,0) ? xrtJsonParse((xstrview){(char*)bytes+offset,size-offset}) : NULL; xrtFree(bytes);
    if(!value || !xrtValueIs(value,XVALUE_OBJECT)) { xrtValueRelease(value); return NULL; } return value;
}
char* xideSettingsDefaultPath(void)
{
    wchar_t binary[32768]; if(!GetModuleFileNameW(NULL,binary,32768)) return NULL;
    const wchar_t* resource=xideStartupResourceModule(binary);
    int size=WideCharToMultiByte(CP_UTF8,0,resource,-1,NULL,0,NULL,NULL); char* text=size ? malloc((size_t)size) : NULL;
    if(!text) return NULL;
    WideCharToMultiByte(CP_UTF8,0,resource,-1,text,size,NULL,NULL); char* directory=xidePathDirectory(text); free(text);
    char* path=directory ? xidePathResolve(directory,"option/settings.json") : NULL; free(directory); return path;
}
void xideSettingsClear(xide_settings* settings)
{
    if(!settings) return;
    free(settings->path); free(settings->compiler); free(settings->cServer); free(settings->editorFont); free(settings->uiFont); xrtValueRelease(settings->source); memset(settings,0,sizeof(*settings));
}
int xideSettingsLoad(xide_settings* settings,const char* path,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!settings || !path) return fail(error,XIDE_MODEL_INVALID,"Invalid settings path.");
    xide_settings temporary={0}; temporary.editorFontSize=16; temporary.uiFontSize=16; temporary.path=xidePathResolve(NULL,path); if(!temporary.path) return fail(error,XIDE_MODEL_INVALID,"Invalid UTF-8 settings path.");
    temporary.existed=xidePathExists(temporary.path); temporary.source=temporary.existed ? read(temporary.path) : xrtValueObject();
    char* format=get(temporary.source,"format") ? string(get(temporary.source,"format")) : NULL;
    xvalue* toolchain=get(temporary.source,"toolchain"); int okay=temporary.source && (!temporary.existed || (format && !strcmp(format,"xlang-ide-settings-1"))) && (!toolchain || xrtValueIs(toolchain,XVALUE_OBJECT));
    free(format);
    if(okay && get(toolchain,"compiler")) {
        char* compiler=string(get(toolchain,"compiler")); okay=compiler!=NULL;
        if(okay && *compiler) { char* base=xidePathDirectory(temporary.path); temporary.compiler=xidePathResolve(base,compiler); free(base); okay=temporary.compiler!=NULL; }
        free(compiler);
    }
#if XIDE_ENABLE_C_LSP
    if(okay && get(toolchain,"cServer")) {
        char* server=string(get(toolchain,"cServer")); okay=server!=NULL;
        if(okay && *server) { char* base=xidePathDirectory(temporary.path); temporary.cServer=xidePathResolve(base,server); free(base); okay=temporary.cServer!=NULL; }
        free(server);
    }
#endif
    xvalue* editor=get(temporary.source,"editor"),*font=get(editor,"font");
    if(okay && editor && !xrtValueIs(editor,XVALUE_OBJECT)) okay=0;
    if(okay && font && !xrtValueIs(font,XVALUE_OBJECT)) okay=0;
    if(okay && get(font,"path")) {
        char* value=string(get(font,"path")); okay=value!=NULL;
        if(okay && *value) { char* base=xidePathDirectory(temporary.path); temporary.editorFont=xidePathResolve(base,value); free(base); okay=temporary.editorFont!=NULL; }
        free(value);
    }
    if(okay && get(font,"size")) {
        int64_t size=0; okay=xrtValueGetInt(get(font,"size"),&size) && size>=8 && size<=40;
        if(okay) temporary.editorFontSize=(int)size;
    }
    {
    xvalue* ui=get(temporary.source,"ui"),*font=get(ui,"font");
    if(okay && ui && !xrtValueIs(ui,XVALUE_OBJECT)) okay=0;
    if(okay && font && !xrtValueIs(font,XVALUE_OBJECT)) okay=0;
    if(okay && get(font,"path")) {
        char* value=string(get(font,"path")); okay=value!=NULL;
        if(okay && *value) { char* base=xidePathDirectory(temporary.path); temporary.uiFont=xidePathResolve(base,value); free(base); okay=temporary.uiFont!=NULL; }
        free(value);
    }
    if(okay && get(font,"size")) {
        int64_t size=0; okay=xrtValueGetInt(get(font,"size"),&size) && size>=8 && size<=40;
        if(okay) temporary.uiFontSize=(int)size;
    }
    }
    if(!okay) { xideSettingsClear(&temporary); return fail(error,XIDE_MODEL_INVALID,"Settings are unreadable or unsupported; the original file is preserved."); }
    xideSettingsClear(settings); *settings=temporary; return 1;
}
/* Keep bundled tools relocatable in the program's own portable settings.
   External preferences and external executables retain absolute bindings. */
static char* portablePath(const xide_settings* settings,const char* resource,const char* prefix)
{
    char* defaults=xideSettingsDefaultPath();
    if(!defaults || !xidePathLexicalEqual(settings->path,defaults)) { free(defaults); return NULL; }
    char* options=xidePathDirectory(defaults); char* program=options ? xidePathDirectory(options) : NULL;
    char* relative=program ? xidePathRelative(program,resource) : NULL;
    size_t length=strlen(prefix);
    int bundled=relative && (!strcmp(relative,prefix) || (length && prefix[length-1]=='/' && !strncmp(relative,prefix,length)));
    char* stored=bundled ? xidePathRelative(options,resource) : NULL;
    free(defaults); free(options); free(program); free(relative); return stored;
}
static char* portableExecutablePath(const xide_settings* settings,const char* executable)
{
    char* path=portablePath(settings,executable,"toolchains/");
    return path ? path : portablePath(settings,executable,"xl.exe");
}
/* Legacy server preferences remain part of the loaded disk identity, but
 * disabled builds drop the retired key only in the independent write copy. */
static int retiredServerPreference(xvalue* root)
{
#if !XIDE_ENABLE_C_LSP
    xvalue* old=get(root,"toolchain");
    if(get(old,"cServer")) {
        xvalue* tools=xrtValueClone(old); if(!tools) return 0;
        if(!xrtValueObjectRemove(tools,sv("cServer"))) {xrtValueRelease(tools);return 0;}
        return xrtValueObjectSetNew(root,sv("toolchain"),tools);
    }
#else
    (void)root;
#endif
    return 1;
}
static int writeSettings(xide_settings* settings,xvalue* root,xide_model_error* error)
{
    if(!retiredServerPreference(root)) return fail(error,XIDE_MODEL_MEMORY,"Cannot prepare preferences without retired C LSP settings.");
    size_t size=0; char* text=xrtJsonStringify(root,true,&size); int okay=text && size<=256u*1024u; xrtFree(text);
    if(!okay) return fail(error,XIDE_MODEL_INVALID,"Preferences exceed their 256 KiB limit.");
    xide_state_file state={settings->path,settings->source,settings->existed};
    okay=xideStateWrite(&state,root,error); settings->source=state.source; settings->existed=state.existed; return okay;
}
static int setExecutable(xide_settings* settings,const char* compiler,const char* key,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!settings || !settings->path || !settings->source) return fail(error,XIDE_MODEL_INVALID,"Reload or repair settings before saving a toolchain preference.");
    char* resolved=xidePathResolve(NULL,compiler); if(!resolved || !xidePathExists(resolved) || !xidePathHasExtension(resolved,".exe")) { free(resolved); return fail(error,XIDE_MODEL_INVALID,"Choose an existing toolchain executable."); }
    int exists=xidePathExists(settings->path); xvalue* disk=exists ? read(settings->path) : NULL;
    int unchanged=exists==settings->existed && (!exists || (disk && xrtValueEqual(disk,settings->source))); xrtValueRelease(disk);
    if(!unchanged) { free(resolved); return fail(error,XIDE_MODEL_CONFLICT,"Settings changed on disk; reload before saving."); }
    xvalue *root=xrtValueClone(settings->source),*toolchain=get(root,"toolchain") ? xrtValueClone(get(root,"toolchain")) : xrtValueObject();
    char* portable=portableExecutablePath(settings,resolved);
    int okay=root && toolchain && xrtValueObjectSetNew(root,sv("format"),xrtValueString(sv("xlang-ide-settings-1"))) && xrtValueObjectSetNew(toolchain,sv(key),xrtValueString(sv(portable ? portable : resolved)));
    free(portable);
    if(okay) okay=xrtValueObjectSetNew(root,sv("toolchain"),toolchain); else xrtValueRelease(toolchain);
    if(okay) okay=writeSettings(settings,root,error);
    xrtValueRelease(root);
    if(!okay) { free(resolved); if(error && !error->message[0]) fail(error,XIDE_MODEL_IO,"Cannot atomically save toolchain preference."); return 0; }
    char** slot=!strcmp(key,"compiler") ? &settings->compiler : &settings->cServer; free(*slot); *slot=resolved; return 1;
}
int xideSettingsSetCompiler(xide_settings* settings,const char* compiler,xide_model_error* error)
{ return setExecutable(settings,compiler,"compiler",error); }
#if XIDE_ENABLE_C_LSP
int xideSettingsSetCServer(xide_settings* settings,const char* server,xide_model_error* error)
{ return setExecutable(settings,server,"cServer",error); }
#endif
int xideSettingsRememberOpen(xide_settings* settings,const char* path,xide_model_error* error)
{
    if(error)memset(error,0,sizeof(*error));
    if(!settings || !settings->path || !settings->source)return fail(error,XIDE_MODEL_INVALID,"Preferences are unavailable.");
    char* resolved=xidePathResolve(NULL,path);if(!resolved || strlen(resolved)>8192) { free(resolved);return fail(error,XIDE_MODEL_INVALID,"Invalid recent path."); }
    xvalue *root=xrtValueDeepClone(settings->source),*recent=xrtValueArray();int okay=root && recent;
    if(okay)okay=xrtValueArrayAppendNew(recent,xrtValueString(sv(resolved)));
    const xvalue* old=get(settings->source,"recent");
    for(size_t i=0;okay && xrtValueIs(old,XVALUE_ARRAY) && i<xrtValueCount(old) && i<12 && xrtValueCount(recent)<12;++i) {
        char* previous=string(xrtValueArrayGet(old,i));
        if(previous && strlen(previous)<=8192 && !xidePathEqual(previous,resolved))okay=xrtValueArrayAppendNew(recent,xrtValueString(sv(previous)));
        free(previous);
    }
    if(okay)okay=xrtValueObjectSetNew(root,sv("recent"),recent);else xrtValueRelease(recent);
    if(okay)okay=xrtValueObjectSetNew(root,sv("format"),xrtValueString(sv("xlang-ide-settings-1"))) && writeSettings(settings,root,error);
    xrtValueRelease(root);free(resolved);
    if(!okay && error && !error->code)fail(error,XIDE_MODEL_MEMORY,"Cannot save recent paths; the previous list is preserved.");
    return okay;
}
/* Shared settings object retains compiler and unknown JSON members. */
static int setFont(xide_settings* settings,const char* path,int size,const char* section,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!settings || !settings->path || !settings->source) return fail(error,XIDE_MODEL_INVALID,"Reload or repair settings before saving a font preference.");
    char* resolved=xidePathResolve(NULL,path);
    if(!resolved || !xidePathExists(resolved) || (!xidePathHasExtension(resolved,".ttf") && !xidePathHasExtension(resolved,".ttc") && !xidePathHasExtension(resolved,".otf")) || size<8 || size>40) {
        free(resolved); return fail(error,XIDE_MODEL_INVALID,"Choose an existing TTF/TTC/OTF font and an integer size from 8 to 40.");
    }
    int exists=xidePathExists(settings->path); xvalue* disk=exists ? read(settings->path) : NULL;
    int unchanged=exists==settings->existed && (!exists || (disk && xrtValueEqual(disk,settings->source))); xrtValueRelease(disk);
    if(!unchanged) { free(resolved); return fail(error,XIDE_MODEL_CONFLICT,"Settings changed on disk; reload before saving."); }
    xvalue *root=xrtValueClone(settings->source),*editor=get(root,section) ? xrtValueClone(get(root,section)) : xrtValueObject();
    xvalue* font=get(editor,"font") ? xrtValueClone(get(editor,"font")) : xrtValueObject();
    char* relative=portablePath(settings,resolved,"data/fonts/");
    int okay=root && editor && font && xrtValueObjectSetNew(root,sv("format"),xrtValueString(sv("xlang-ide-settings-1"))) &&
        xrtValueObjectSetNew(font,sv("path"),xrtValueString(sv(relative ? relative : resolved))) && xrtValueObjectSetNew(font,sv("size"),xrtValueInt(size));
    free(relative);
    if(okay) okay=xrtValueObjectSetNew(editor,sv("font"),font); else xrtValueRelease(font);
    if(okay) okay=xrtValueObjectSetNew(root,sv(section),editor); else xrtValueRelease(editor);
    if(okay) okay=writeSettings(settings,root,error);
    xrtValueRelease(root);
    if(!okay) { free(resolved); if(error && !error->message[0]) fail(error,XIDE_MODEL_IO,"Cannot atomically save font preference."); return 0; }
    if(!strcmp(section,"ui")) { free(settings->uiFont); settings->uiFont=resolved; settings->uiFontSize=size; }
    else { free(settings->editorFont); settings->editorFont=resolved; settings->editorFontSize=size; }
    return 1;
}

xvalue* xideSettingsCapture(const xide_settings* settings,const char* path,const char* editorFont,int editorSize,const char* uiFont,int uiSize)
{
    if(!settings || !path || !editorFont || !uiFont || editorSize<8 || editorSize>40 || uiSize<8 || uiSize>40) return NULL;
    xvalue* root=settings->source ? xrtValueDeepClone(settings->source) : xrtValueObject();
    int okay=root && xrtValueObjectSetNew(root,sv("format"),xrtValueString(sv("xlang-ide-settings-1")));
    xvalue* tools=get(root,"toolchain"); if(!tools && okay) { tools=xrtValueObject(); okay=tools && xrtValueObjectSetNew(root,sv("toolchain"),tools); }
#if XIDE_ENABLE_C_LSP
    const char* keys[]={"compiler","cServer"}; const char* values[]={settings->compiler,settings->cServer};
#else
    const char* keys[]={"compiler"}; const char* values[]={settings->compiler};
#endif
    xide_settings binding={0}; binding.path=(char*)path;
    for(size_t i=0;i<sizeof(values)/sizeof(*values) && okay;++i) if(values[i]) {
        char* relative=portableExecutablePath(&binding,values[i]); okay=xrtValueObjectSetNew(tools,sv(keys[i]),xrtValueString(sv(relative ? relative : values[i]))); free(relative);
    }
    const char* sections[]={"editor","ui"}; const char* fonts[]={editorFont,uiFont}; int sizes[]={editorSize,uiSize};
    for(int i=0;i<2 && okay;++i) {
        xvalue* section=get(root,sections[i]); if(!section) { section=xrtValueObject(); okay=section && xrtValueObjectSetNew(root,sv(sections[i]),section); }
        xvalue* font=okay ? get(section,"font") : NULL; if(!font && okay) { font=xrtValueObject(); okay=font && xrtValueObjectSetNew(section,sv("font"),font); }
        char* relative=portablePath(&binding,fonts[i],"data/fonts/");
        if(okay) okay=xrtValueObjectSetNew(font,sv("path"),xrtValueString(sv(relative ? relative : fonts[i]))) && xrtValueObjectSetNew(font,sv("size"),xrtValueInt(sizes[i]));
        free(relative);
    }
    if(okay) okay=retiredServerPreference(root);
    size_t n=0; char* text=okay ? xrtJsonStringify(root,true,&n) : NULL; okay=text && n<=256u*1024u; xrtFree(text);
    if(!okay) { xrtValueRelease(root); return NULL; } return root;
}

int xideSettingsSetEditorFont(xide_settings* settings,const char* path,int size,xide_model_error* error)
{ return setFont(settings,path,size,"editor",error); }
int xideSettingsSetUiFont(xide_settings* settings,const char* path,int size,xide_model_error* error)
{ return setFont(settings,path,size,"ui",error); }
