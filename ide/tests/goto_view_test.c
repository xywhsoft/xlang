#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/goto_view.h"
#include "ui/editor_view.h"
#include "ui/font_view.h"
#include "ui/catalog_view.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char* directory; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(e) do { ++f->checks; if(!(e)) { fprintf(stderr,"goto_view_test:%d: %s; %s\n",__LINE__,#e,error.message); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int key(xide_app* a,int k,uint32_t mods) { return xuiInputKeyDownEx(a->context,k,mods,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,0,NULL)==XUI_OK && settle(a); }
static int pointer(xide_app* a,xui_rect_t r) { int x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,int id) { xui_widget w=xideGotoViewControl(a,id); return xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideGotoViewWindow(a)),w)==XUI_OK && settle(a) && pointer(a,xuiWidgetGetWorldRect(w)); }
static int position(xide_app* a,const char* s) { return xuiEditSetText(xideGotoViewControl(a,XIDE_GOTO_POSITION),s)==XUI_OK && settle(a); }
static int caret(xide_document* d) { xui_code_selection_t s={0}; return xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&s)==XUI_OK ? s.iCaretOffset : -1; }
static int gotoAt(xide_app* a,const char* value,int offset) {
 int ok=key(a,'G',XUI_MOD_CTRL) && xideGotoViewIsOpen(a) && position(a,value) && key(a,XUI_KEY_ENTER,0) && !xideGotoViewIsOpen(a) && xuiGetFocusWidget(a->context)==a->activeDocument->editor && caret(a->activeDocument)==offset;
 if(!ok) fprintf(stderr,"goto %s expected=%d caret=%d popup=%d message=%s\n",value,offset,caret(a->activeDocument),xideGotoViewIsOpen(a),xideGotoViewMessage(a));
 return ok;
}
static int capture(xide_app* a,const char* p) { xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return xuiUpdate(a->context,0)==XUI_OK && a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,p)==XGE_OK; }
static int menu(xide_app* a,xui_widget m,int id) {
 int at=-1;for(int i=0;i<xuiMenuGetItemCount(m);++i) if(xuiMenuGetItem(m,i)->iValue==id) at=i;
 if(at<0 || !(xuiMenuGetItemState(m,at)&XUI_MENU_ITEM_ENABLED) || xuiMenuOpenAt(m,a->root,100,60)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiMenuGetItemRect(m,at),world=xuiWidgetGetWorldRect(m);r.fX+=world.fX;r.fY+=world.fY;return pointer(a,r);
}
static int context(xide_app* a,xide_document* d) {
 if(xuiCodeEditOpenMenu(d->editor,380,220)!=XUI_OK || !settle(a)) return 0;
 xui_widget m=xuiCodeEditGetMenuWidget(d->editor);int at=-1;
 for(int i=0;i<xuiMenuGetItemCount(m);++i) if(!strcmp(xuiMenuGetItem(m,i)->sText ? xuiMenuGetItem(m,i)->sText : "",xideActionFind(XIDE_ACTION_EDITOR_GOTO)->title)) at=i;
 if(at<0 || !(xuiMenuGetItemState(m,at)&XUI_MENU_ITEM_ENABLED)) return 0;
 xui_rect_t r=xuiMenuGetItemRect(m,at),world=xuiWidgetGetWorldRect(m);r.fX+=world.fX;r.fY+=world.fY;return pointer(a,r);
}
static int ime(xide_app* a,const char* s,int active) { xui_ime_composition_t c={0};c.iSize=sizeof(c);c.sText=s;c.iTextSize=(int)strlen(s);c.bActive=active;c.iCursor=c.iSelectionStart=c.iSelectionEnd=c.iTextSize;return xuiInputImeCompositionEx(a->context,&c)==XUI_OK && settle(a); }
static int verify(void* user) {
 fixture* f=user;if(f->started) return XGE_OK;f->started=1;
 xide_app app={0};xide_model_error error={0};int began=0;char *root=NULL,*source=NULL,*settings=NULL,*saved=NULL,*longText=NULL,*images[4]={0};
 char* id=xideCreateId("goto");root=id ? xidePathResolve(f->directory,id) : NULL;free(id);CHECK(root && xrtDirCreateAll(root));
 source=xidePathResolve(root,"定位 中文.c");settings=xidePathResolve(root,"settings.json");CHECK(source && settings);
 const char* original="A中😀é\tZ\nline2\n";CHECK(writeText(source,original));
 const char* names[]={"native-goto-light.png","native-goto-invalid-dark.png","native-goto-folded-target.png","native-goto-large-font.png"};
 for(int i=0;i<4;++i) { images[i]=xidePathResolve(root,names[i]);CHECK(images[i]); }
 app.hidden=1;app.noSession=1;app.highDpi=1;app.dpiOverride=1.25f;app.settingsPath=settings;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(settle(&app));
 CHECK(xideActionCount()==73 && !xideActionQuery(&app,XIDE_ACTION_EDITOR_GOTO).enabled && xideActionExecute(&app,XIDE_ACTION_EDITOR_GOTO)==XIDE_ACTION_DISABLED);
 const xide_action_info* info=xideActionNamed("editor.gotoLine");CHECK(info && info->id==XIDE_ACTION_EDITOR_GOTO && info->codeCommand==XUI_CODE_COMMAND_GOTO_LINE && info->shortcutScope==XIDE_SHORTCUT_EDITOR);
 CHECK(xideEditorOpen(&app,source,&error) && settle(&app));xide_document* c=app.activeDocument;xui_code_document doc=xuiCodeEditGetDocument(c->editor);
 CHECK(!strcmp(c->language,"c") && !c->dirty && xuiCodeDocumentGetLineCount(doc)==3 && xideActionQuery(&app,XIDE_ACTION_EDITOR_GOTO).enabled);
 CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(c->editor),doc,1,8)==XUI_OK && xuiCodeEditSetScroll(c->editor,0,0)==XUI_OK);
 xui_code_selection_t extra={0};extra.iSize=sizeof(extra);extra.iAnchorOffset=14;extra.iCaretOffset=16;
 CHECK(xuiCodeSelectionAdd(xuiCodeEditGetSelection(c->editor),&extra)==XUI_OK && xuiCodeSelectionGetCount(xuiCodeEditGetSelection(c->editor))==2);
 uint32_t version=xuiCodeDocumentGetVersion(doc);CHECK(key(&app,'G',XUI_MOD_CTRL) && xideGotoViewIsOpen(&app));
 CHECK(!strcmp(xuiEditGetText(xideGotoViewControl(&app,XIDE_GOTO_POSITION)),"1:4"));int first=0,last=0;
 CHECK(xuiEditGetSelection(xideGotoViewControl(&app,XIDE_GOTO_POSITION),&first,&last)==XUI_OK && first==0 && last==3);
 CHECK(xuiInputGetFont(xideGotoViewControl(&app,XIDE_GOTO_POSITION))==app.uiFont && capture(&app,images[0]));
 CHECK(xideActionsHasModal(&app) && xideActionExecute(&app,XIDE_ACTION_NEW_FILE)==XIDE_ACTION_DISABLED && xideActionExecute(&app,XIDE_ACTION_THEME_GRAPHITE)==XIDE_ACTION_DISABLED && !xideFontViewOpen(&app));
 CHECK(position(&app,"2") && key(&app,XUI_KEY_ESCAPE,0) && !xideGotoViewIsOpen(&app) && caret(c)==8);
 CHECK(xuiCodeSelectionGetRange(xuiCodeEditGetSelection(c->editor),&first,&last)==XUI_OK && first==1 && last==8 && xuiCodeSelectionGetCount(xuiCodeEditGetSelection(c->editor))==2 && xuiGetFocusWidget(app.context)==c->editor);
 CHECK(key(&app,'G',XUI_MOD_CTRL) && position(&app,"3") && click(&app,XIDE_GOTO_CANCEL) && caret(c)==8 && !xideGotoViewIsOpen(&app));
 CHECK(menu(&app,app.editMenu,XIDE_ACTION_EDITOR_GOTO) && xideGotoViewIsOpen(&app) && position(&app," 2 : 3 \t") && click(&app,XIDE_GOTO_APPLY) && caret(c)==16);
 CHECK(context(&app,c) && xideGotoViewIsOpen(&app) && position(&app,"1:3") && key(&app,XUI_KEY_ENTER,0) && caret(c)==4);
 CHECK(gotoAt(&app,"1:4",8) && gotoAt(&app,"1:5",11) && gotoAt(&app,"1:6",12) && gotoAt(&app,"1:7",13) && gotoAt(&app,"3",20));
 CHECK(xuiCodeSelectionGetCount(xuiCodeEditGetSelection(c->editor))==1 && !c->dirty && xuiCodeDocumentGetVersion(doc)==version && !strcmp(xuiCodeEditGetText(c->editor),original));
 CHECK(xideActionExecute(&app,XIDE_ACTION_THEME_GRAPHITE)==XIDE_ACTION_DISPATCHED && key(&app,'G',XUI_MOD_CTRL));
 const char* bad[]={"","0","-1","+2","1:0","4","1:8","3:2","1:","1:2:3","1x","2147483648","1:2147483648","一","1\n2"};
 for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i) { CHECK(position(&app,bad[i]) && key(&app,XUI_KEY_ENTER,0) && xideGotoViewIsOpen(&app) && *xideGotoViewMessage(&app) && caret(c)==20 && xuiCodeDocumentGetVersion(doc)==version && !c->dirty); }
 CHECK(capture(&app,images[1]) && position(&app,"1:1"));
 CHECK(ime(&app,"",1) && key(&app,XUI_KEY_ENTER,0) && xideGotoViewIsOpen(&app) && caret(c)==20);
 CHECK(ime(&app,"中",1) && key(&app,XUI_KEY_ENTER,0) && xideGotoViewIsOpen(&app) && caret(c)==20);
 CHECK(ime(&app,"",0) && key(&app,XUI_KEY_ENTER,0) && !xideGotoViewIsOpen(&app) && caret(c)==0);
 CHECK(xuiCodeEditSetReadonly(c->editor,1)==XUI_OK && gotoAt(&app,"2",14) && xuiCodeEditSetReadonly(c->editor,0)==XUI_OK);
 CHECK(xuiInputText(app.context,'!')==XUI_OK && settle(&app) && c->dirty && xuiCodeDocumentCanUndo(doc));
 saved=_strdup(xuiCodeEditGetText(c->editor));CHECK(saved);version=xuiCodeDocumentGetVersion(doc);
 CHECK(gotoAt(&app,"1",0) && xuiCodeDocumentGetVersion(doc)==version && !strcmp(saved,xuiCodeEditGetText(c->editor)) && xuiCodeDocumentCanUndo(doc));
 CHECK(key(&app,'Z',XUI_MOD_CTRL) && !strcmp(original,xuiCodeEditGetText(c->editor)) && !c->dirty && xuiCodeDocumentCanRedo(doc));
 CHECK(gotoAt(&app,"3",20) && xuiCodeDocumentCanRedo(doc) && key(&app,'Y',XUI_MOD_CTRL) && !strcmp(saved,xuiCodeEditGetText(c->editor)));
 CHECK(key(&app,'G',XUI_MOD_CTRL) && xuiCodeDocumentInsert(doc,0,"changed") == XUI_OK && !xideGotoViewApply(&app,&error) && xideGotoViewIsOpen(&app) && key(&app,XUI_KEY_ESCAPE,0));
 xide_document* xl=xideDocumentNew(&app.documents,"xlang",&error);CHECK(xl && xideEditorShow(&app,xl)==XUI_OK && settle(&app) && gotoAt(&app,"1:1",0));
 CHECK(xuiCodeEditSetText(xl->editor,"func main() {\n    println(\"中文\")\n}\n")==XUI_OK && settle(&app));
 CHECK(gotoAt(&app,"2:5",18));
 CHECK(key(&app,'G',XUI_MOD_CTRL));app.activeDocument=c;
 CHECK(!xideGotoViewApply(&app,&error));app.activeDocument=xl;CHECK(key(&app,XUI_KEY_ESCAPE,0));
 CHECK(xideEditorShow(&app,xl)==XUI_OK && settle(&app) && key(&app,'G',XUI_MOD_CTRL));uint64_t oldId=xl->id;
 CHECK(xideDocumentClose(xl,1,&error));xide_document* plain=xideDocumentNew(&app.documents,"text",&error);app.activeDocument=plain;
 CHECK(plain==xl && plain->id!=oldId && !xideGotoViewApply(&app,&error) && caret(plain)==0 && key(&app,XUI_KEY_ESCAPE,0) && xideEditorShow(&app,plain)==XUI_OK && settle(&app));
 CHECK(xuiCodeEditSetText(plain->editor,"plain\n中文😀\n")==XUI_OK && settle(&app) && gotoAt(&app,"2:3",12));
 CHECK(xideEditorShow(&app,c)==XUI_OK && settle(&app));
 longText=calloc(1,16384);CHECK(longText);size_t length=0;
 for(int i=0;i<210;++i) length+=(size_t)snprintf(longText+length,16384-length,"// line %d 中文😀\n",i+1);
 CHECK(xuiCodeEditSetText(c->editor,longText)==XUI_OK && settle(&app));doc=xuiCodeEditGetDocument(c->editor);
 xui_code_fold_range_t ranges[]={{sizeof(*ranges),0,150,0,XUI_CODE_FOLD_COLLAPSED},{sizeof(*ranges),2,60,1,XUI_CODE_FOLD_COLLAPSED},{sizeof(*ranges),160,190,0,XUI_CODE_FOLD_COLLAPSED}};
 xui_code_fold_state folds=xuiCodeEditGetFoldState(c->editor);CHECK(xuiCodeFoldStateSetRanges(folds,ranges,3)==XUI_OK && settle(&app));
 int target=0;CHECK(xuiCodeDocumentLineColumnToOffset(doc,49,3,&target)==XUI_OK && gotoAt(&app,"50:4",target));
 for(int i=0;i<3;++i) { xui_code_fold_range_t r={0};CHECK(xuiCodeFoldStateGetRange(folds,i,&r)==XUI_OK && !!(r.iFlags&XUI_CODE_FOLD_COLLAPSED)==(i==2)); }
 float sx=0,sy=0;CHECK(xuiCodeEditGetScroll(c->editor,&sx,&sy)==XUI_OK && sy>0);
 xui_rect_t targetRect={0};CHECK(xuiCodeEditGetTextOffsetRect(c->editor,target,&targetRect)==XUI_OK);
 xui_rect_t viewport=xuiScrollModelGetViewport(xuiCodeEditGetScrollModel(c->editor)),world=xuiWidgetGetWorldRect(c->editor);
 CHECK(targetRect.fY>=world.fY+viewport.fY && targetRect.fY+2*targetRect.fH<=world.fY+viewport.fY+viewport.fH+1);
 CHECK(capture(&app,images[2]));
 xui_code_selection_t selection={0};CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(c->editor),&selection)==XUI_OK);
 CHECK(key(&app,'G',XUI_MOD_CTRL) && position(&app,"999") && key(&app,XUI_KEY_ENTER,0) && key(&app,XUI_KEY_ESCAPE,0));
 float afterX=0,afterY=0;CHECK(xuiCodeEditGetScroll(c->editor,&afterX,&afterY)==XUI_OK && afterX==sx && afterY==sy && caret(c)==selection.iCaretOffset);
 CHECK(xuiSetFocusWidget(app.context,xideCatalogSearchWidget(&app))==XUI_OK && key(&app,'G',XUI_MOD_CTRL) && !xideGotoViewIsOpen(&app));
 CHECK(xideUiFontApply(&app,app.uiFontPath,40,&error) && settle(&app));
 CHECK(xgeEnd()==XGE_OK);began=0;app.dpiOverride=2;CHECK(xideAppFrame(&app)==XGE_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(settle(&app) && app.dpiScale==2 && app.viewportWidth==800);
 CHECK(xideEditorShow(&app,c)==XUI_OK && settle(&app) && key(&app,'G',XUI_MOD_CTRL) && xuiInputGetFont(xideGotoViewControl(&app,XIDE_GOTO_POSITION))==app.uiFont);
 float cw=0,ch=0;CHECK(xideDialogGetContentSize(xideGotoViewWindow(&app),&cw,&ch)==XUI_OK && cw>app.viewportWidth && ch>app.viewportHeight);
 CHECK(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideGotoViewWindow(&app)),xideGotoViewControl(&app,XIDE_GOTO_APPLY))==XUI_OK && settle(&app) && capture(&app,images[3]));
 CHECK(position(&app,"1:1") && click(&app,XIDE_GOTO_APPLY) && !xideGotoViewIsOpen(&app) && caret(c)==0);
 CHECK(xideUiFontApply(&app,app.uiFontPath,16,&error) && settle(&app) && key(&app,'G',XUI_MOD_CTRL));
 wchar_t lockedPath[32768];CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,settings,-1,lockedPath,32768));
 HANDLE lock=CreateFileW(lockedPath,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(lock!=INVALID_HANDLE_VALUE);
 xui_font font=app.uiFont;int blocked=!xideUiFontApply(&app,app.uiFontPath,24,&error);CloseHandle(lock);
 CHECK(blocked && app.uiFont==font && app.uiFontSize==16 && xuiInputGetFont(xideGotoViewControl(&app,XIDE_GOTO_POSITION))==font && settle(&app) && key(&app,XUI_KEY_ESCAPE,0));
 size_t fileSize=0;unsigned char* bytes=xrtFileReadAllLimit(source,1024,&fileSize);int unchanged=bytes && fileSize==strlen(original) && !memcmp(bytes,original,fileSize);xrtFree(bytes);CHECK(unchanged);
 printf("goto_view_test passed: %d assertions; real menu/context/Ctrl+G/Enter/Escape/button routing, native Unicode grapheme columns, trailing empty line and invalid bounds, native empty/nonempty IME, modal and ordinary-field isolation, C/xlang/plain/untitled/read-only navigation, stable document identity/version, undo/redo and dirty/disk preservation, enclosing fold reveal and unchanged sibling fold, viewport scrolling, UIFont rollback, 125/200%% DPI and four native screenshots\n",f->checks);
cleanup:
 if(began) xgeEnd();
 xideAppDestroy(&app);free(root);free(source);free(settings);free(saved);free(longText);for(int i=0;i<4;++i) free(images[i]);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="xlang IDE native goto verification";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int ret=xgeInit(&d);if(ret==XGE_OK) ret=xgeRun(verify,f);xgeUnit();return ret==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;fixture f={.directory=argv[1]};wchar_t name[100];swprintf(name,100,L"XlangGotoVerification-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t) { if(WaitForSingleObject(t,90000)!=WAIT_OBJECT_0) { fputs("goto_view_test timed out\n",stderr);ExitProcess(1); }GetExitCodeThread(t,&code);CloseHandle(t); }CloseDesktop(f.desktop);return (int)code; }
