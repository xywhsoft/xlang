#include "ui/reading_state.h"
#include "ui/workspace_view.h"
#include "ui/catalog_view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static const xvalue* get(const xvalue* v,const char* key) { return v ? xrtValueObjectGet(v,sv(key)) : NULL; }
static const char* text(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) ? s.Data : NULL; }
static int put(xvalue* v,const char* key,xvalue* item) { return item && xrtValueObjectSetNew(v,sv(key),item); }
static int putText(xvalue* v,const char* key,const char* s) { return put(v,key,xrtValueString(sv(s ? s : ""))); }
static xvalue* object(const xvalue* old) { return xrtValueIs(old,XVALUE_OBJECT) ? xrtValueDeepClone(old) : xrtValueObject(); }
double xideReadingNumber(const xvalue* value,double fallback)
{
    double n=0; int64 i=0; uint64 u=0;
    if(xrtValueGetFloat(value,&n)) return isfinite(n) ? n : fallback;
    if(xrtValueGetInt(value,&i)) return (double)i;
    if(xrtValueGetUInt(value,&u)) return (double)u;
    return fallback;
}
char* xideReadingKey(const char* kind,const char* owner,const char* value)
{
    xvalue* array=xrtValueArray(); if(!array) return NULL;
    const char* parts[]={kind,owner ? owner : "",value ? value : ""}; int okay=1;
    for(int i=0;i<3 && okay;++i) okay=xrtValueArrayAppendNew(array,xrtValueString(sv(parts[i])));
    size_t length=0; char* result=okay ? xrtJsonStringify(array,false,&length) : NULL; xrtValueRelease(array); return result;
}
static uint64_t hash(uint64_t h,const void* data,size_t size) { const unsigned char* p=data; for(size_t i=0;i<size;++i) { h^=p[i]; h*=UINT64_C(1099511628211); } return h; }
static uint64_t keyHash(const char* key) { return hash(UINT64_C(1469598103934665603),key,strlen(key)); }
typedef struct state_map { const char** keys; const xvalue** values; size_t capacity; } state_map;
static void clearMap(state_map* map) { free(map->keys); free(map->values); memset(map,0,sizeof(*map)); }
static int makeMap(state_map* map,const xvalue* nodes)
{
    size_t count=xrtValueCount(nodes); if(count>20000) return 0;
    map->capacity=8; while(map->capacity<count*2) map->capacity*=2;
    map->keys=calloc(map->capacity,sizeof(*map->keys)); map->values=calloc(map->capacity,sizeof(*map->values));
    if(!map->keys || !map->values) { clearMap(map); return 0; }
    for(size_t i=0;i<count;++i) {
        const xvalue* node=xrtValueArrayGet(nodes,i); const char* key=text(get(node,"id"));
        if(!key || !*key || strlen(key)>8192) { clearMap(map); return 0; }
        size_t index=(size_t)keyHash(key)&(map->capacity-1);
        while(map->keys[index] && strcmp(map->keys[index],key)) index=(index+1)&(map->capacity-1);
        if(map->keys[index]) { clearMap(map); return 0; }
        map->keys[index]=key; map->values[index]=node;
    }
    return 1;
}
static const xvalue* lookup(const state_map* map,const char* key)
{
    if(!map->capacity || !key) return NULL;
    size_t index=(size_t)keyHash(key)&(map->capacity-1);
    while(map->keys[index]) { if(!strcmp(map->keys[index],key)) return map->values[index]; index=(index+1)&(map->capacity-1); }
    return NULL;
}
static int numberField(const xvalue* v,const char* key,double limit)
{ const xvalue* item=get(v,key); double n=xideReadingNumber(item,-1); return !item || (n>=0 && n<=limit); }
static int textField(const xvalue* v,const char* key,size_t limit)
{ const xvalue* item=get(v,key); const char* s=text(item); return !item || (s && strlen(s)<=limit); }
static int treeValid(const xvalue* v)
{
    if(!v) return 1;
    const xvalue* nodes=get(v,"nodes");
    if(!xrtValueIs(v,XVALUE_OBJECT) || !xrtValueIs(nodes,XVALUE_ARRAY) || !textField(v,"selected",8192) || !textField(v,"anchor",8192) ||
       !numberField(v,"scrollY",10000000) || !numberField(v,"fraction",1)) return 0;
    state_map map={0}; if(!makeMap(&map,nodes)) return 0; clearMap(&map);
    for(size_t i=0;i<xrtValueCount(nodes);++i) { const xvalue* node=xrtValueArrayGet(nodes,i); bool expanded=false; if(!xrtValueIs(node,XVALUE_OBJECT) || !xrtValueGetBool(get(node,"expanded"),&expanded)) return 0; }
    return 1;
}
int xideReadingValid(const xvalue* v)
{
    if(!v) return 1;
    if(!xrtValueIs(v,XVALUE_OBJECT) || xideReadingNumber(get(v,"version"),0)!=1 || !treeValid(get(v,"workspace")) || !treeValid(get(v,"commands"))) return 0;
    const xvalue* properties=get(v,"properties"),*help=get(v,"help"),*position=get(help,"position");
    if(properties && (!xrtValueIs(properties,XVALUE_OBJECT) || !numberField(properties,"scrollY",10000000))) return 0;
    if(help && (!xrtValueIs(help,XVALUE_OBJECT) || !textField(help,"command",8192))) return 0;
    bool atEnd=false; if(get(position,"end") && !xrtValueGetBool(get(position,"end"),&atEnd)) return 0;
    if(position && (!xrtValueIs(position,XVALUE_OBJECT) || !numberField(position,"scrollX",10000000) || !numberField(position,"scrollY",10000000))) return 0;
    const xvalue* chapter=get(position,"chapter"),*block=get(position,"block");
    if(chapter && (!xrtValueIs(chapter,XVALUE_OBJECT) || !text(get(chapter,"title")) || !textField(chapter,"title",8192) ||
       !numberField(chapter,"occurrence",4096) || floor(xideReadingNumber(get(chapter,"occurrence"),0))!=xideReadingNumber(get(chapter,"occurrence"),0))) return 0;
    if(block && (!xrtValueIs(block,XVALUE_OBJECT) || !text(get(block,"id")) || !textField(block,"id",100) || !numberField(block,"occurrence",4096) ||
       !numberField(block,"fraction",1) || floor(xideReadingNumber(get(block,"occurrence"),0))!=xideReadingNumber(get(block,"occurrence"),0))) return 0;
    return 1;
}
typedef char* (*node_key)(xide_app*,int);
static xvalue* captureTree(xide_app* app,xui_widget tree,node_key key,const xvalue* old)
{
    xvalue* result=object(old),*nodes=xrtValueArray(); state_map map={0};
    if(!result || !nodes || !makeMap(&map,get(old,"nodes"))) goto failed;
    for(int i=0;i<xuiTreeViewGetNodeCount(tree);++i) {
        const xui_tree_view_node_t* node=xuiTreeViewGetNode(tree,i); if(!node || !node->bHasChildren) continue;
        char* id=key(app,node->iId); xvalue* saved=id ? object(lookup(&map,id)) : NULL;
        int okay=saved && putText(saved,"id",id) && put(saved,"expanded",xrtValueBool(xuiTreeViewGetNodeExpanded(tree,node->iId)!=0)); xrtFree(id);
        if(!okay) { xrtValueRelease(saved); goto failed; }
        if(!xrtValueArrayAppendNew(nodes,saved)) goto failed;
    }
    float row=28; xuiTreeViewGetMetrics(tree,&row,NULL,NULL,NULL); if(row<=0) row=28;
    float scroll=xuiTreeViewGetScroll(tree); int first=(int)floorf(scroll/row),count=xuiTreeViewGetVisibleCount(tree); if(first>=count) first=count-1;
    char* selected=key(app,xuiTreeViewGetSelected(tree)),*anchor=key(app,xuiTreeViewGetVisibleNodeId(tree,first));
    double fraction=first>=0 ? (scroll-first*row)/row : 0; if(fraction<0) fraction=0; if(fraction>1) fraction=1;
    int okay=putText(result,"selected",selected) && putText(result,"anchor",anchor) && put(result,"scrollY",xrtValueFloat(scroll)) && put(result,"fraction",xrtValueFloat(fraction));
    xrtFree(selected); xrtFree(anchor); clearMap(&map);
    if(!okay) goto failed;
    { xvalue* owned=nodes; nodes=NULL; if(!put(result,"nodes",owned)) goto failed; }
    return result;
failed:
    clearMap(&map); xrtValueRelease(nodes); xrtValueRelease(result); return NULL;
}
xvalue* xideReadingCapture(xide_app* app,const xvalue* old)
{
    xvalue *root=object(old),*workspace=captureTree(app,app->workspace,xideWorkspaceTreeKey,get(old,"workspace")),*commands=captureTree(app,app->commands,xideCatalogTreeKey,get(old,"commands"));
    if(!root || !workspace || !commands) { xrtValueRelease(root); xrtValueRelease(workspace); xrtValueRelease(commands); return NULL; }
    int okay=put(root,"version",xrtValueInt(1));
    okay=put(root,"workspace",workspace) && okay; okay=put(root,"commands",commands) && okay;
    xvalue* properties=object(get(old,"properties")); float y=0; xuiScrollViewGetOffset(app->properties,NULL,&y);
    if(properties && put(properties,"scrollY",xrtValueFloat(y))) okay=put(root,"properties",properties) && okay;
    else { xrtValueRelease(properties); okay=0; }
    const char* command=app->selectedCommand>=0 && (size_t)app->selectedCommand<app->catalog.count ? app->catalog.entries[app->selectedCommand].id : "";
    const xvalue* before=get(old,"help"); const char* previous=text(get(before,"command")); if(!previous || strcmp(command,previous)) before=NULL;
    xvalue* help=object(before),*position=xideHelpCaptureReading(xideCatalogHelpDocument(app),get(before,"position"));
    if(help && position && putText(help,"command",command)) {
        xvalue* owned=position; position=NULL; okay=put(help,"position",owned) && okay;
        owned=help; help=NULL; okay=put(root,"help",owned) && okay;
    } else okay=0;
    xrtValueRelease(help); xrtValueRelease(position);
    if(!okay || !xideReadingValid(root)) { xrtValueRelease(root); return NULL; } return root;
}
static void restoreTree(xide_app* app,xui_widget tree,node_key key,const xvalue* state,int afterLayout,int workspace)
{
    if(!state) return;
    state_map map={0}; if(!afterLayout && !makeMap(&map,get(state,"nodes"))) return;
    const char* wanted=text(get(state,afterLayout ? "anchor" : "selected")); int target=-1;
    for(int i=0;i<xuiTreeViewGetNodeCount(tree);++i) {
        const xui_tree_view_node_t* node=xuiTreeViewGetNode(tree,i); if(!node) continue;
        char* id=key(app,node->iId); if(!id) continue;
        if(wanted && *wanted && !strcmp(wanted,id)) target=node->iId;
        if(!afterLayout && node->bHasChildren) { const xvalue* entry=lookup(&map,id); bool expanded=false; if(entry && xrtValueGetBool(get(entry,"expanded"),&expanded) && expanded!=(xuiTreeViewGetNodeExpanded(tree,node->iId)!=0)) xuiTreeViewSetNodeExpanded(tree,node->iId,expanded); }
        xrtFree(id);
    }
    clearMap(&map);
    if(!afterLayout) {
        if(target>=0 && xuiTreeViewGetVisibleIndexOfId(tree,target)>=0) { if(workspace) xideWorkspaceTreeSelect(app,target); else xuiTreeViewSetSelected(tree,target); }
    } else {
        float row=28; xuiTreeViewGetMetrics(tree,&row,NULL,NULL,NULL); int index=target>=0 ? xuiTreeViewGetVisibleIndexOfId(tree,target) : -1;
        double offset=index>=0 ? (index+xideReadingNumber(get(state,"fraction"),0))*row : xideReadingNumber(get(state,"scrollY"),0);
        xuiTreeViewSetScroll(tree,(float)offset);
    }
}
int xideReadingRestore(xide_app* app,const xvalue* value,int afterLayout)
{
    if(!value) return 1;
    if(!xideReadingValid(value)) return 0;
    int explicitProject=app->frames==0 && app->initialSelectedProject;
    int explicitCommand=app->frames==0 && (app->initialCommand || app->initialCommandSearch);
    if(!afterLayout || !explicitProject) restoreTree(app,app->workspace,xideWorkspaceTreeKey,get(value,"workspace"),afterLayout,1);
    if(!afterLayout || !explicitCommand) restoreTree(app,app->commands,xideCatalogTreeKey,get(value,"commands"),afterLayout,0);
    if(afterLayout) {
        if(!explicitProject) xuiScrollViewSetOffset(app->properties,0,(float)xideReadingNumber(get(get(value,"properties"),"scrollY"),0));
        const xvalue* help=get(value,"help"); const char* command=text(get(help,"command"));
        if(!(app->frames==0 && (app->initialChapterRequested || app->initialCommand)) && command && app->selectedCommand>=0 && (size_t)app->selectedCommand<app->catalog.count && !strcmp(command,app->catalog.entries[app->selectedCommand].id))
            xideHelpRestoreReading(xideCatalogHelpDocument(app),get(help,"position"));
    }
    return 1;
}
uint64_t xideReadingFingerprint(xide_app* app,uint64_t h)
{
    xui_widget trees[]={app->commands,app->workspace};
    for(int i=0;i<2;++i) { int changes=xuiTreeViewGetChangeCount(trees[i]),selected=xuiTreeViewGetSelected(trees[i]); float y=xuiTreeViewGetScroll(trees[i]); h=hash(h,&changes,sizeof(changes)); h=hash(h,&selected,sizeof(selected)); h=hash(h,&y,sizeof(y)); }
    xui_widget scrolls[]={app->help,app->properties};
    for(int i=0;i<2;++i) { float x=0,y=0; xuiScrollViewGetOffset(scrolls[i],&x,&y); h=hash(h,&x,sizeof(x)); h=hash(h,&y,sizeof(y)); }
    int page=xuiComboBoxGetSelected(xideHelpPages(xideCatalogHelpDocument(app))),chapter=xuiComboBoxGetSelected(xideHelpNavigator(xideCatalogHelpDocument(app)));
    h=hash(h,&page,sizeof(page)); return hash(h,&chapter,sizeof(chapter));
}

void xideReadingReset(xide_app* app)
{
    for(int i=0;i<xuiTreeViewGetNodeCount(app->workspace);++i) { const xui_tree_view_node_t* node=xuiTreeViewGetNode(app->workspace,i); if(node && node->bHasChildren) xuiTreeViewSetNodeExpanded(app->workspace,node->iId,1); }
    xuiTreeViewSetScroll(app->workspace,0); xuiScrollViewSetOffset(app->properties,0,0); xideCatalogResetReading(app);
}
