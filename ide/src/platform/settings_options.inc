/* All pages share one validated draft and one conflict-checked atomic write. */
static const xide_settings_options optionDefaults={4,4,0,1,1,1,0,4,1,-1,1,75,1,1,1};
static const char* optionThemes[]={"classic-light","graphite-dark","warm-white","ocean"};
const xide_settings_options* xideSettingsOptions(const xide_settings* s)
{return s && s->optionsReady ? &s->options : &optionDefaults;}
static int optionInt(const xvalue* root,const char* key,int* target,int low,int high)
{
    const xvalue* value=get(root,key);int64 n=0;if(!value)return 1;
    if(!xideMetaInt(value,&n) || n<low || n>high)return 0;
    *target=(int)n;return 1;
}
static int optionBool(const xvalue* root,const char* key,int* target)
{const xvalue* value=get(root,key);bool n=false;if(!value)return 1;if(!xrtValueGetBool(value,&n))return 0;*target=n;return 1;}
static int loadOptions(xide_settings* s)
{
    s->options=optionDefaults;s->documentFontSize=16;s->optionsReady=1;
    const xvalue *editor=get(s->source,"editor"),*doc=get(s->source,"documents"),*ui=get(s->source,"ui"),*ls=get(s->source,"languageService");
    if((doc && !xrtValueIs(doc,XVALUE_OBJECT)) || (ls && !xrtValueIs(ls,XVALUE_OBJECT)))return 0;
    xide_settings_options* o=&s->options;
    if(!optionInt(editor,"tabColumns",&o->tabColumns,1,16) || !optionInt(editor,"indentColumns",&o->indentColumns,1,16) ||
       !optionBool(editor,"expandTabs",&o->expandTabs) || !optionBool(editor,"lineNumbers",&o->lineNumbers) ||
       !optionBool(editor,"foldMargin",&o->foldMargin) || !optionBool(editor,"minimap",&o->minimap) || !optionBool(editor,"wordWrap",&o->wordWrap) ||
       !optionInt(doc,"lineGap",&o->documentLineGap,0,24) || !optionBool(doc,"wordWrap",&o->documentWrap) || !optionBool(doc,"followSelection",&o->helpFollow) ||
       !optionBool(ls,"automaticDiagnostics",&o->automaticDiagnostics) || !optionInt(ls,"diagnosticDelay",&o->diagnosticDelay,0,2000) ||
       !optionBool(ls,"completion",&o->completion) || !optionBool(ls,"signatureHelp",&o->signatureHelp))return 0;
    if(get(ui,"theme")) {
        char* theme=string(get(ui,"theme"));int found=-1;
        if(theme)for(int i=0;i<4;++i)if(!strcmp(theme,optionThemes[i]))found=i;
        free(theme);if(found<0)return 0;o->theme=found;
    }
    const xvalue* font=get(doc,"font");if(font && !xrtValueIs(font,XVALUE_OBJECT))return 0;
    if(!optionInt(font,"size",&s->documentFontSize,8,40))return 0;
    if(get(font,"path")) {
        char* value=string(get(font,"path"));if(!value)return 0;
        if(*value){char* base=xidePathDirectory(s->path);s->documentFont=xidePathResolve(base,value);free(base);}
        int okay=!*value || s->documentFont;free(value);if(!okay)return 0;
    }
    return 1;
}
static xvalue* optionObject(xvalue* root,const char* key)
{
    xvalue* value=get(root,key);if(value)return xrtValueIs(value,XVALUE_OBJECT) ? value : NULL;
    value=xrtValueObject();return value && xrtValueObjectSetNew(root,sv(key),value) ? value : NULL;
}
static int optionSet(xvalue* root,const char* key,int value,int boolean)
{return root && xrtValueObjectSetNew(root,sv(key),boolean ? xrtValueBool(value!=0) : xrtValueInt(value));}
static int draftFont(xvalue* root,const xide_settings* s,const char* key,const char* path,int size)
{
    xvalue* section=optionObject(root,key),*font=section ? optionObject(section,"font") : NULL;
    char* relative=path ? portablePath(s,path,"data/fonts/") : NULL;
    int okay=font && xrtValueObjectSetNew(font,sv("path"),xrtValueString(sv(relative ? relative : path ? path : ""))) && optionSet(font,"size",size,0);
    free(relative);return okay;
}
int xideSettingsSaveDraft(xide_settings* s,const xide_settings_draft* d,xide_model_error* error)
{
    if(error)memset(error,0,sizeof(*error));
    if(!s || !s->path || !s->source || !d)return fail(error,XIDE_MODEL_INVALID,"设置不可用，请先重新加载设置文件。");
    const xide_settings_options* o=&d->options;
    if(d->editorSize<8 || d->editorSize>40 || d->uiSize<8 || d->uiSize>40 || d->documentSize<8 || d->documentSize>40 ||
       o->tabColumns<1 || o->tabColumns>16 || o->indentColumns<1 || o->indentColumns>16 || o->documentLineGap<0 || o->documentLineGap>24 ||
       o->theme<0 || o->theme>3 || o->diagnosticDelay<0 || o->diagnosticDelay>2000)
        return fail(error,XIDE_MODEL_INVALID,"字号、缩进、行距、主题或诊断延迟超出允许范围。");
    const int flags[]={o->expandTabs,o->lineNumbers,o->foldMargin,o->minimap,o->wordWrap,o->documentWrap,o->automaticDiagnostics,o->completion,o->signatureHelp,o->helpFollow};
    for(size_t i=0;i<sizeof(flags)/sizeof(*flags);++i)if(flags[i]!=0 && flags[i]!=1)return fail(error,XIDE_MODEL_INVALID,"设置开关必须为布尔值。");
    char *paths[4]={0};const char* inputs[]={d->compiler,d->editorFont,d->uiFont,d->documentFont};
    for(int i=0;i<4;++i)if(inputs[i] && *inputs[i]) {
        paths[i]=xidePathResolve(NULL,inputs[i]);
        if(!paths[i] || !xidePathExists(paths[i]) || (i==0 && !xidePathHasExtension(paths[i],".exe"))) {
            for(int j=0;j<4;++j)free(paths[j]);
            return fail(error,XIDE_MODEL_INVALID,"请选择有效的编译器和字体文件。");
        }
    }
    xvalue* root=xrtValueDeepClone(s->source);
    xvalue *editor=root ? optionObject(root,"editor") : NULL,*doc=root ? optionObject(root,"documents") : NULL;
    xvalue *ui=root ? optionObject(root,"ui") : NULL,*ls=root ? optionObject(root,"languageService") : NULL,*tools=root ? optionObject(root,"toolchain") : NULL;
    char* compiler=paths[0] ? portableExecutablePath(s,paths[0]) : NULL;
    int okay=root && editor && doc && ui && ls && tools && xrtValueObjectSetNew(root,sv("format"),xrtValueString(sv("xlang-ide-settings-1"))) &&
        xrtValueObjectSetNew(tools,sv("compiler"),xrtValueString(sv(compiler ? compiler : paths[0] ? paths[0] : ""))) &&
        draftFont(root,s,"editor",paths[1],d->editorSize) && draftFont(root,s,"ui",paths[2],d->uiSize) && draftFont(root,s,"documents",paths[3],d->documentSize) &&
        optionSet(editor,"tabColumns",o->tabColumns,0) && optionSet(editor,"indentColumns",o->indentColumns,0) && optionSet(editor,"expandTabs",o->expandTabs,1) &&
        optionSet(editor,"lineNumbers",o->lineNumbers,1) && optionSet(editor,"foldMargin",o->foldMargin,1) && optionSet(editor,"minimap",o->minimap,1) && optionSet(editor,"wordWrap",o->wordWrap,1) &&
        optionSet(doc,"lineGap",o->documentLineGap,0) && optionSet(doc,"wordWrap",o->documentWrap,1) && optionSet(doc,"followSelection",o->helpFollow,1) &&
        xrtValueObjectSetNew(ui,sv("theme"),xrtValueString(sv(optionThemes[o->theme]))) &&
        optionSet(ls,"automaticDiagnostics",o->automaticDiagnostics,1) && optionSet(ls,"diagnosticDelay",o->diagnosticDelay,0) && optionSet(ls,"completion",o->completion,1) && optionSet(ls,"signatureHelp",o->signatureHelp,1);
    free(compiler);
    if(okay)okay=writeSettings(s,root,error);
    xrtValueRelease(root);
    if(!okay){for(int i=0;i<4;++i)free(paths[i]);if(error && !error->code)fail(error,XIDE_MODEL_MEMORY,"无法保存设置；原配置保持不变。");return 0;}
    free(s->compiler);free(s->editorFont);free(s->uiFont);free(s->documentFont);
    s->compiler=paths[0];s->editorFont=paths[1];s->uiFont=paths[2];s->documentFont=paths[3];
    s->editorFontSize=d->editorSize;s->uiFontSize=d->uiSize;s->documentFontSize=d->documentSize;s->options=*o;s->optionsReady=1;return 1;
}
