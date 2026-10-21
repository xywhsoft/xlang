#include "ui/layout_state.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
static const char* ids[]={"commands","workspace","properties","xlang_demo","c_demo","command_help","problems","build_output","terminal","tasks","outline"};
_Static_assert(sizeof(ids)/sizeof(ids[0])==XIDE_WINDOW_COUNT,"Every IDE panel needs a stable layout ID.");
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* key) { return xrtValueObjectGet(v,sv(key)); }
static const char* text(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) ? s.Data : NULL; }
static int take(xvalue* object,const char* name,xvalue** value) { xvalue* owned=*value; *value=NULL; return xrtValueObjectSetNew(object,sv(name),owned); }
static int append(xvalue* array,xvalue** value) { xvalue* owned=*value; *value=NULL; return xrtValueArrayAppendNew(array,owned); }
static int numeric(const xvalue* value,double* result)
{
    int64 i=0; uint64 u=0;
    if(xrtValueGetFloat(value,result)) return 1;
    if(xrtValueGetInt(value,&i)) { *result=(double)i; return 1; }
    if(xrtValueGetUInt(value,&u)) { *result=(double)u; return 1; }
    return 0;
}
static int number(const xvalue* v,int* out) { int64 i=0; uint64 u=0; if(xrtValueGetInt(v,&i) && i>=0 && i<=1000000) { *out=(int)i; return 1; } if(xrtValueGetUInt(v,&u) && u<=1000000) { *out=(int)u; return 1; } return 0; }
static int fail(xide_model_error* e,const char* message) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",message); } return 0; }
static const char* key(xide_app* app,int window)
{
    for(int i=0;i<XIDE_WINDOW_COUNT;++i) if(app->windows[i]==window) return ids[i];
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open && app->documents.items[i]->window==window) return app->documents.items[i]->sessionId;
    return NULL;
}
static int window(xide_app* app,const char* id)
{
    if(!id) return -1;
    for(int i=0;i<XIDE_WINDOW_COUNT;++i) if(!strcmp(ids[i],id)) return app->windows[i];
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open && app->documents.items[i]->sessionId && !strcmp(app->documents.items[i]->sessionId,id)) return app->documents.items[i]->window;
    return -1;
}
static xvalue* mapped(xide_app* app,const xvalue* id,int saving,int* valid)
{
    if(saving) { int n=0; if(!number(id,&n)) { *valid=0; return NULL; } const char* s=key(app,n); return s ? xrtValueString(sv(s)) : NULL; }
    const char* s=text(id); if(!s || !*s || strlen(s)>160) { *valid=0; return NULL; }
    int n=window(app,s); return n>=0 ? xrtValueInt(n) : NULL;
}
static int duplicate(xvalue* seen,const xvalue* id)
{
    for(size_t i=0;i<xrtValueCount(seen);++i) if(xrtValueEqual(id,xrtValueArrayGet(seen,i))) return 1;
    return !xrtValueArrayAppendNew(seen,xrtValueDeepClone(id));
}
static xvalue* node(xide_app* app,const xvalue* source,int saving,int depth,int* valid,xvalue* seen)
{
    if(xrtValueIs(source,XVALUE_NULL)) return xrtValueNull();
    if(depth>64 || !xrtValueIs(source,XVALUE_OBJECT)) { *valid=0; return NULL; }
    const char* type=text(get(source,"type")); if(!type) { *valid=0; return NULL; }
    xvalue* result=xrtValueDeepClone(source); if(!result) { *valid=0; return NULL; }
    if(!strcmp(type,"split")) {
        const char* axis=text(get(source,"axis")); double ratio=0;
        if(!axis || (strcmp(axis,"horizontal") && strcmp(axis,"vertical")) || !numeric(get(source,"ratio"),&ratio) || !isfinite(ratio) || ratio<=0 || ratio>=1) { *valid=0; goto bad; }
        xvalue* first=node(app,get(source,"first"),saving,depth+1,valid,seen),*second=node(app,get(source,"second"),saving,depth+1,valid,seen);
        if(!*valid || !first || !second) { xrtValueRelease(first); xrtValueRelease(second); goto bad; }
        if(xrtValueIs(first,XVALUE_NULL) || xrtValueIs(second,XVALUE_NULL)) {
            xvalue* kept=xrtValueIs(first,XVALUE_NULL) ? second : first; xvalue* removed=kept==first ? second : first; xrtValueRelease(removed); xrtValueRelease(result); return kept;
        }
        if(!xrtValueObjectSetNew(result,sv("axis"),xrtValueString(sv(!strcmp(axis,"horizontal") ? "vertical" : "horizontal"))) || !take(result,"first",&first) || !take(result,"second",&second)) { xrtValueRelease(first); xrtValueRelease(second); *valid=0; goto bad; }
        return result;
    }
    if(strcmp(type,"pane") || !xrtValueIs(get(source,"tabs"),XVALUE_ARRAY) || xrtValueCount(get(source,"tabs"))>10000) { *valid=0; goto bad; }
    xvalue* tabs=xrtValueArray(); if(!tabs) { *valid=0; goto bad; } int active=0,originalActive=0; const char* activeId=NULL;
    if(saving) { if(!number(get(source,"active"),&originalActive)) { xrtValueRelease(tabs); *valid=0; goto bad; } }
    else { activeId=text(get(source,"active")); if(!activeId) { xrtValueRelease(tabs); *valid=0; goto bad; } }
    const char* savedActive=NULL;
    for(size_t i=0;i<xrtValueCount(get(source,"tabs"));++i) {
        const xvalue* original=xrtValueArrayGet(get(source,"tabs"),i); xvalue* value=mapped(app,original,saving,valid);
        if(!*valid) { xrtValueRelease(tabs); goto bad; } if(!value) continue;
        if(duplicate(seen,value)) { xrtValueRelease(value); xrtValueRelease(tabs); *valid=0; goto bad; }
        if(saving && (int)i==originalActive) savedActive=text(value);
        if(!saving && !strcmp(text(original),activeId)) active=(int)xrtValueCount(tabs);
        if(!xrtValueArrayAppendNew(tabs,value)) { xrtValueRelease(tabs); *valid=0; goto bad; }
    }
    if(!xrtValueCount(tabs)) { xrtValueRelease(tabs); xrtValueRelease(result); return xrtValueNull(); }
    if(saving && !savedActive) savedActive=text(xrtValueArrayGet(tabs,0));
    if(!xrtValueObjectSetNew(result,sv("active"),saving ? xrtValueString(sv(savedActive)) : xrtValueInt(active)) || !take(result,"tabs",&tabs)) { xrtValueRelease(tabs); *valid=0; goto bad; }
    return result;
bad:
    xrtValueRelease(result); return NULL;
}
static int rect(xvalue* object,const char* field,xide_app* app,int clamp)
{
    xvalue* value=get(object,field); if(!xrtValueIs(value,XVALUE_ARRAY) || xrtValueCount(value)!=4) return 0;
    double values[4];
    for(int i=0;i<4;++i) if(!numeric(xrtValueArrayGet(value,i),&values[i]) || !isfinite(values[i]) || fabs(values[i])>100000) return 0;
    if(clamp) {
        double w=app->viewportWidth>320 ? app->viewportWidth : 1280,h=app->viewportHeight>240 ? app->viewportHeight-92 : 708;
        if(values[2]<160) values[2]=160;
        if(values[3]<120) values[3]=120;
        if(values[2]>w) values[2]=w;
        if(values[3]>h) values[3]=h;
        if(values[0]<0) values[0]=0;
        if(values[1]<0) values[1]=0;
        if(values[0]>w-80) values[0]=w-80;
        if(values[1]>h-40) values[1]=h-40;
    }
    xvalue* fresh=xrtValueArray(); if(!fresh) return 0;
    for(int i=0;i<4;++i) if(!xrtValueArrayAppendNew(fresh,xrtValueFloat(values[i]))) { xrtValueRelease(fresh); return 0; }
    return xrtValueObjectSetNew(object,sv(field),fresh);
}

static int placed(const xvalue* seen,const xvalue* id)
{ for(size_t i=0;i<xrtValueCount(seen);++i) if(xrtValueEqual(id,xrtValueArrayGet(seen,i))) return 1; return 0; }
static int repairPlacement(xvalue* regions,const char* name,const xvalue* id)
{
    xvalue* region=NULL;
    for(size_t i=0;i<xrtValueCount(regions);++i) { xvalue* candidate=xrtValueArrayGet(regions,i); if(!strcmp(name,text(get(candidate,"kind")))) { region=candidate; break; } }
    if(!region) return 0;
    xvalue *pane=xrtValueObject(),*tabs=xrtValueArray(),*split=NULL;
    int okay=pane && tabs && xrtValueArrayAppendNew(tabs,xrtValueDeepClone(id)) &&
        xrtValueObjectSetNew(pane,sv("type"),xrtValueString(sv("pane"))) &&
        xrtValueObjectSetNew(pane,sv("active"),xrtValueInt(0)) && take(pane,"tabs",&tabs);
    const xvalue* previous=get(region,"root");
    if(okay && !xrtValueIs(previous,XVALUE_NULL)) {
        split=xrtValueObject();
        okay=split && xrtValueObjectSetNew(split,sv("type"),xrtValueString(sv("split"))) &&
            xrtValueObjectSetNew(split,sv("axis"),xrtValueString(sv("horizontal"))) &&
            xrtValueObjectSetNew(split,sv("ratio"),xrtValueFloat(0.6)) &&
            xrtValueObjectSetNew(split,sv("first"),xrtValueDeepClone(previous)) && take(split,"second",&pane) && take(region,"root",&split);
    } else if(okay) okay=take(region,"root",&pane);
    xrtValueRelease(pane); xrtValueRelease(tabs); xrtValueRelease(split); return okay;
}
static xvalue* convert(xide_app* app,const xvalue* source,int saving)
{
    int version=0,valid=1; const char* kind=text(get(source,"kind"));
    if(!kind || strcmp(kind,saving ? "xui.dockpanel" : "xlang.dock") || !number(get(source,"version"),&version) || version!=1) return NULL;
    const char* arrays[]={"regions","windows","floating"};
    for(int i=0;i<3;++i) if(!xrtValueIs(get(source,arrays[i]),XVALUE_ARRAY) || xrtValueCount(get(source,arrays[i]))>10000) return NULL;
    xvalue* result=xrtValueDeepClone(source),*regions=xrtValueArray(),*windows=xrtValueArray(),*floating=xrtValueArray(),*seen=xrtValueArray();
    if(!result || !regions || !windows || !floating || !seen) goto bad;
    int regionSeen[5]={0}; const char* names[]={"document","left","right","top","bottom"};
    for(size_t i=0;i<xrtValueCount(get(source,"regions"));++i) {
        const xvalue* original=xrtValueArrayGet(get(source,"regions"),i); const char* name=text(get(original,"kind")); int index=-1;
        for(int k=0;name && k<5;++k) if(!strcmp(name,names[k])) index=k;
        double value=0; const char* mode=text(get(original,"sizeMode")); bool visible=false;
        if(index<0 || regionSeen[index]++ || !mode || (strcmp(mode,"pixel") && strcmp(mode,"portion")) || !numeric(get(original,"value"),&value) || !isfinite(value) || value<0 || value>100000 || !xrtValueGetBool(get(original,"visible"),&visible)) goto bad;
        xvalue* child=node(app,get(original,"root"),saving,0,&valid,seen),*region=xrtValueDeepClone(original);
        if(!valid || !child || !region) { xrtValueRelease(child); xrtValueRelease(region); goto bad; }
        if(!xrtValueObjectSetNew(region,sv("value"),xrtValueFloat(value)) || !xrtValueObjectSetNew(region,sv(!strcmp(mode,"pixel") ? "pixelSize" : "portion"),xrtValueFloat(value)) || !take(region,"root",&child) || !append(regions,&region)) { xrtValueRelease(child); xrtValueRelease(region); goto bad; }
    }
    if(xrtValueCount(regions)!=5) goto bad;
    xvalue* windowSeen=xrtValueArray(); if(!windowSeen) goto bad;
    for(size_t i=0;i<xrtValueCount(get(source,"windows"));++i) {
        const xvalue* original=xrtValueArrayGet(get(source,"windows"),i); xvalue* id=mapped(app,get(original,"id"),saving,&valid);
        if(!valid) { xrtValueRelease(windowSeen); goto bad; } if(!id) continue;
        if(duplicate(windowSeen,id)) { xrtValueRelease(id); xrtValueRelease(windowSeen); goto bad; }
        const char* state=text(get(original,"state"));
        if(!state || (strcmp(state,"docked") && strcmp(state,"hidden") && strcmp(state,"floating") && strcmp(state,"autoHide"))) { xrtValueRelease(id); xrtValueRelease(windowSeen); goto bad; }
        if(!saving && !strcmp(state,"docked") && !placed(seen,id)) {
            const char* region=text(get(original,"lastRegion"));
            if(!region || !repairPlacement(regions,region,id) || duplicate(seen,id)) { xrtValueRelease(id); xrtValueRelease(windowSeen); goto bad; }
        }
        xvalue* item=xrtValueDeepClone(original);
        if(!item || !rect(item,"floatRect",app,!saving) || !rect(item,"lastDockRect",app,0) || !take(item,"id",&id) || !append(windows,&item)) { xrtValueRelease(id); xrtValueRelease(item); xrtValueRelease(windowSeen); goto bad; }
    }
    xrtValueRelease(windowSeen);
    for(size_t i=0;i<xrtValueCount(get(source,"floating"));++i) {
        const xvalue* original=xrtValueArrayGet(get(source,"floating"),i); xvalue* id=mapped(app,get(original,"id"),saving,&valid);
        if(!valid) goto bad;
        if(!id) continue;
        if(duplicate(seen,id)) { xrtValueRelease(id); goto bad; }
        xvalue* item=xrtValueDeepClone(original);
        if(!item || !rect(item,"rect",app,!saving) || !take(item,"id",&id) || !append(floating,&item)) { xrtValueRelease(id); xrtValueRelease(item); goto bad; }
    }
    if(!xrtValueObjectSetNew(result,sv("kind"),xrtValueString(sv(saving ? "xlang.dock" : "xui.dockpanel"))) ||
        !take(result,"regions",&regions) || !take(result,"windows",&windows) || !take(result,"floating",&floating)) goto bad;
    xrtValueRelease(seen); return result;
bad:
    xrtValueRelease(result); xrtValueRelease(regions); xrtValueRelease(windows); xrtValueRelease(floating); xrtValueRelease(seen); return NULL;
}
xvalue* xideLayoutCapture(xide_app* app,xide_model_error* error)
{
    xvalue* native=NULL; if(xuiDockPanelSaveState(app->dock,&native)!=XUI_OK) { fail(error,"Cannot capture native Dock state."); return NULL; }
    xvalue* state=convert(app,native,1); xuiDockPanelStateFree(native); if(!state) fail(error,"Unsupported native Dock schema."); return state;
}
int xideLayoutRestore(xide_app* app,const xvalue* state,xide_model_error* error)
{
    xvalue* native=convert(app,state,0); if(!native) return fail(error,"Invalid logical Dock state; default layout is preserved.");
    xvalue* before=NULL; xuiDockPanelSaveState(app->dock,&before);
    int result=xuiDockPanelLoadState(app->dock,native); xrtValueRelease(native);
    if(result!=XUI_OK) { if(before) xuiDockPanelLoadState(app->dock,before); xuiDockPanelStateFree(before); return fail(error,"Native Dock restore failed; previous layout restored."); }
    xuiDockPanelStateFree(before);
    int logical[]={XIDE_XLANG_DEMO,XIDE_HELP,XIDE_WORKSPACE,XIDE_PROPERTIES}; int* panes[]={&app->documentPane,&app->helpPane,&app->workspacePane,&app->propertyPane};
    for(int i=0;i<4;++i) { xui_dock_window_info_t info={0}; info.iSize=sizeof(info); xuiDockPanelGetWindowInfo(app->dock,app->windows[logical[i]],&info); *panes[i]=info.iPane; }
    if(app->activeDocument && app->activeDocument->open) {
        xui_dock_window_info_t info={0}; info.iSize=sizeof(info); xuiDockPanelGetWindowInfo(app->dock,app->activeDocument->window,&info); if(info.iPane>=0) app->documentPane=info.iPane;
    }
    return 1;
}