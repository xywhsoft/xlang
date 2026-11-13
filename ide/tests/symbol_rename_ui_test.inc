#include "theme/theme.h"
#include "services/ui_font.h"
static const char* nextSymbolName="next_amount";
static int probeQuit,quitCalls,quitAllowed,workspaceQuitCalls,failSymbolTree,failSymbolRows;
static xui_widget symbolTree;
extern int(*__real___imp_xgeRequestQuit)(void);
static int requestQuitProbe(void)
{if(!probeQuit)return __real___imp_xgeRequestQuit();++quitCalls;int allowed=0;CHECK(xgeQuitRequestDispatch(&allowed)==XGE_OK);quitAllowed=allowed;return allowed;}
int(*__wrap___imp_xgeRequestQuit)(void)=requestQuitProbe;
int __real_xideWorkspaceQuitRequest(void*);
int __wrap_xideWorkspaceQuitRequest(void* raw)
{if(probeQuit){++workspaceQuitCalls;return 1;}return __real_xideWorkspaceQuitRequest(raw);}
extern int(*__real___imp_xuiTreeViewCreate)(xui_context,xui_widget*,const xui_tree_view_desc_t*);
static int treeCreateProbe(xui_context c,xui_widget* out,const xui_tree_view_desc_t* d)
{if(failSymbolTree){failSymbolTree=0;*out=NULL;return XUI_ERROR_OUT_OF_MEMORY;}return __real___imp_xuiTreeViewCreate(c,out,d);}
int(*__wrap___imp_xuiTreeViewCreate)(xui_context,xui_widget*,const xui_tree_view_desc_t*)=treeCreateProbe;
extern int(*__real___imp_xuiTreeViewSetNodes)(xui_widget,const xui_tree_view_node_t*,int);
static int treeRowsProbe(xui_widget w,const xui_tree_view_node_t* rows,int n)
{if(w==symbolTree&&n>0&&failSymbolRows){failSymbolRows=0;return XUI_ERROR_OUT_OF_MEMORY;}return __real___imp_xuiTreeViewSetNodes(w,rows,n);}
int(*__wrap___imp_xuiTreeViewSetNodes)(xui_widget,const xui_tree_view_node_t*,int)=treeRowsProbe;
static unsigned rootChildren(xide_app* a)
{unsigned n=0;for(xui_widget w=xuiWidgetGetFirstChild(a->root);w;w=xuiWidgetGetNextSibling(w))++n;return n;}
static int symbolClick(xide_app* a,xui_widget w)
{CHECK(w&&frame(a));xui_rect_t r=xuiWidgetGetWorldRect(w);float x=r.fX+r.fW/2,y=r.fY+r.fH/2;return r.fW>0&&r.fH>0&&xuiInputPointerMove(a->context,x,y,0)==XUI_OK&&xuiInputPointerDown(a->context,x,y,1,1)==XUI_OK&&xuiInputPointerUp(a->context,x,y,1,0)==XUI_OK&&frame(a);}
static void symbolWait(xide_app* a,int phase)
{ULONGLONG until=GetTickCount64()+30000;while(xideSymbolRenamePhase(a)!=phase){CHECK(frame(a)&&GetTickCount64()<until);Sleep(1);}}
static void symbolScreenshot(xide_app* a,const char* directory,const char* name)
{
 char* path=xidePathResolveLexical(directory,name);CHECK(path&&frame(a)&&a->proxy.surfaceClear(&a->proxy,a->target,ideThemeColor(a->theme,IDE_BG))==XUI_OK);xui_rect_i_t full={0,0,a->viewportWidth,a->viewportHeight};CHECK(xuiRender(a->context,a->target,&full,1)==XUI_OK&&xideAppCapture(a,path)==XUI_OK);free(path);
}
static void symbolUiScenario(xide_app* a,xide_document* xl,xide_document* c,char** paths,const char* directory,const char* mainBody,const char* cBody,int mode)
{
 /* Establish actual acknowledged native input, without creating a candidate. */
 xide_edit_request* ready=capture(a,xl);CHECK(ready);end(a,ready);CHECK(frame(a)&&xideSymbolRenameCanOpen(a));
 const char* original=xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor));char* kept=copy(original);CHECK(kept);const char* name=strstr(original,"amount");CHECK(name&&xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(xl->editor),xuiCodeEditGetDocument(xl->editor),(int)(name-original)+2,0)==XUI_OK&&xuiSetFocusWidget(a->context,xl->editor)==XUI_OK);
 if(mode==27){xide_model_error e={0};CHECK(xideUiFontApply(a,a->uiFontPath,40,&e)&&frame(a));}
 if(mode==21){unsigned before=rootChildren(a);failSymbolTree=1;CHECK(!xideSymbolRenameOpen(a)&&!failSymbolTree&&!a->languageView->renameSymbol&&rootChildren(a)==before&&xideSymbolRenameCanOpen(a));}
 CHECK(xideActionFind(XIDE_ACTION_RENAME_SYMBOL)&&!strcmp(xideActionNamed("file.rename")->shortcut,"F2")&&key(a,XUI_KEY_F2,XUI_MOD_CTRL)&&xideSymbolRenamePhase(a)==XIDE_SYMBOL_INPUT&&xideSymbolRenameIsModal(a));
 xui_widget window=xideSymbolRenameWindow(a);CHECK(window&&xideDialogIsModal(window)&&xideDialogSetModal(window,0)==XUI_ERROR_INVALID_STATE&&!xideActionQuery(a,XIDE_ACTION_SAVE_FILE).enabled);
 prepareCalls=applyCalls=finishCalls=ownerReads=ownerWaits=0;normalApi=1;
 CHECK(xuiEditSetText(xideSymbolRenameControl(a,XIDE_SYMBOL_NAME),mode==25?"not-valid!":nextSymbolName)==XUI_OK);
 if(mode==18){CHECK(symbolClick(a,xideSymbolRenameControl(a,XIDE_SYMBOL_PREVIEW)));CHECK(xideSymbolRenamePhase(a)==XIDE_SYMBOL_PLANNING||xideSymbolRenamePhase(a)==XIDE_SYMBOL_CONFIRM);CHECK(symbolClick(a,xuiWindowGetCloseButtonWidget(window)));symbolWait(a,XIDE_SYMBOL_CLOSED);CHECK(!prepareCalls&&!applyCalls&&!xideSourceJobsHeld(&a->sourceJobs)&&!ownerReads&&!ownerWaits);goto unchanged;}
 if(mode==24){symbolTree=xideSymbolRenameControl(a,XIDE_SYMBOL_FILES);failSymbolRows=1;}
 CHECK(symbolClick(a,xideSymbolRenameControl(a,XIDE_SYMBOL_PREVIEW)));
 if(mode==25){symbolWait(a,XIDE_SYMBOL_RESULT);CHECK(!prepareCalls&&!applyCalls&&!xideSourceJobsHeld(&a->sourceJobs));goto unchanged;}
 if(mode==24){ULONGLONG until=GetTickCount64()+30000;while(failSymbolRows){CHECK(frame(a)&&GetTickCount64()<until);Sleep(1);}CHECK(xideSymbolRenamePhase(a)!=XIDE_SYMBOL_CONFIRM);symbolWait(a,XIDE_SYMBOL_RESULT);CHECK(!failSymbolRows&&!prepareCalls&&!applyCalls&&!xideSourceJobsHeld(&a->sourceJobs));goto unchanged;}
 symbolWait(a,XIDE_SYMBOL_CONFIRM);xide_symbol_rename_view* view=a->languageView->renameSymbol;xide_native_apply_status status={0};CHECK(view->count==(mode==23?1u:5u)&&view->edits==(mode==23?2u:5u)&&view->open==1&&view->closed==(mode==23?0u:4u)&&xideLanguageEditApplyStatus(a,&status)&&status.phase==XIDE_APPLY_CONFIRM&&!status.barrierHeld&&!xidePathExists(status.directory)&&!prepareCalls&&!ownerReads&&!ownerWaits&&xuiWidgetGetEnabled(xideSymbolRenameControl(a,XIDE_SYMBOL_APPLY)));
 if(mode==27){CHECK(frame(a));xui_rect_t client=xuiWidgetGetWorldRect(xuiWindowGetClientWidget(window)),footer=xuiWidgetGetWorldRect(xideDialogGetFooterWidget(window));CHECK(footer.fX>=client.fX-.1f&&footer.fY>=client.fY-.1f&&footer.fX+footer.fW<=client.fX+client.fW+.1f&&footer.fY+footer.fH<=client.fY+client.fH+.1f);xui_rect_t tree=xuiWidgetGetWorldRect(xideSymbolRenameControl(a,XIDE_SYMBOL_FILES)),scroll=xuiWidgetGetWorldRect(xideDialogGetScrollViewWidget(window));float itemHeight=0;CHECK(xuiTreeViewGetMetrics(xideSymbolRenameControl(a,XIDE_SYMBOL_FILES),&itemHeight,NULL,NULL,NULL)==XUI_OK&&itemHeight>=xideUiHeight(a,28));CHECK(tree.fH>=100&&xideDialogSetScroll(window,0,tree.fY-scroll.fY)==XUI_OK&&frame(a));tree=xuiWidgetGetWorldRect(xideSymbolRenameControl(a,XIDE_SYMBOL_FILES));CHECK(tree.fY<scroll.fY+scroll.fH&&tree.fY+tree.fH>scroll.fY+40);symbolScreenshot(a,directory,"symbol-large.png");}
 if(mode==15)symbolScreenshot(a,directory,"symbol-preview.png");
 if(mode==19){CHECK(key(a,XUI_KEY_ESCAPE,0)&&(xideSymbolRenamePhase(a)==XIDE_SYMBOL_CANCELLING||xideSymbolRenamePhase(a)==XIDE_SYMBOL_CLOSED));symbolWait(a,XIDE_SYMBOL_CLOSED);CHECK(!prepareCalls&&!applyCalls);goto unchanged;}
 if(mode==22){CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"// scope changed\n")==XUI_OK&&frame(a));symbolWait(a,XIDE_SYMBOL_RESULT);CHECK(!prepareCalls&&!applyCalls&&!xuiWidgetGetEnabled(xideSymbolRenameControl(a,XIDE_SYMBOL_APPLY)));goto unchanged;}
 if(mode==16||mode==17||mode==28){xfileinfo info={0};CHECK(xideIoReceiptStat(paths[2],&info));sourceVolume=(uint32_t)info.Device;sourceIndex=info.Identity;pauseWrite=1;}
 if(mode==20)failNextCommit=1;
 CHECK(symbolClick(a,xideSymbolRenameControl(a,XIDE_SYMBOL_APPLY))&&!xideSymbolRenameIsModal(a));
 if(mode==16||mode==17||mode==28){awaitSource(a);CHECK(xideLanguageEditApplyStatus(a,&status)&&status.barrierHeld&&!xideActionQuery(a,XIDE_ACTION_SAVE_FILE).enabled&&!xideActionQuery(a,XIDE_ACTION_BUILD).enabled&&xideSymbolRenameAllowsAction(a,XIDE_ACTION_EDITOR_UNDO));
  xide_model_error e={0};CHECK(!xideDocumentSave(xl,NULL,0,&e)&&e.code&&!xideDocumentClose(xl,1,&e)&&xl->open);
  if(mode==16){symbolScreenshot(a,directory,"symbol-progress.png");input(a,c,cBody);}else if(mode==28){CHECK(symbolClick(a,xideSymbolRenameControl(a,XIDE_SYMBOL_CANCEL))&&xideSymbolRenamePhase(a)==XIDE_SYMBOL_CANCELLING);}else {probeQuit=1;int allowed=-1;CHECK(xgeQuitRequestDispatch(&allowed)==XGE_OK&&!allowed&&!workspaceQuitCalls&&view->quitPending&&view->phase==XIDE_SYMBOL_CANCELLING);}
  CHECK(SetEvent(sourceRelease));
 }
 if(mode==28){symbolWait(a,XIDE_SYMBOL_CLOSED);CHECK(xideLanguageEditApplyStatus(a,&status)&&status.transaction.rolledBack&&!status.barrierHeld);goto unchanged;}
 if(mode==17){symbolWait(a,XIDE_SYMBOL_CLOSED);CHECK(quitCalls==1&&quitAllowed&&workspaceQuitCalls==1&&!xideSourceJobsHeld(&a->sourceJobs));probeQuit=0;goto unchanged;}
 symbolWait(a,mode==20?XIDE_SYMBOL_UNCERTAIN:XIDE_SYMBOL_RESULT);CHECK(xideLanguageEditApplyStatus(a,&status)&&!ownerReads&&!ownerWaits);
 if(mode==16){CHECK(status.transaction.rolledBack&&!status.barrierHeld&&!strcmp(xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor)),kept));goto unchanged;}
 CHECK(status.transaction.committed&&strstr(xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor)),nextSymbolName));
 if(mode==20){
  CHECK(status.transaction.needsRecovery&&status.barrierHeld&&view->recovery&&!xideSymbolRenameIsModal(a));symbolScreenshot(a,directory,"symbol-recovery.png");unsigned before=resumeCalls;CHECK(symbolClick(a,xideSymbolRenameControl(a,XIDE_SYMBOL_CANCEL))&&!xideDialogIsOpen(window)&&xideSymbolRenameCanOpen(a)&&xideSymbolRenameOpen(a));
  a->hidden=0;probeQuit=1;CHECK(symbolClick(a,xideSymbolRenameControl(a,XIDE_SYMBOL_APPLY))&&xideMessageCurrent(a)&&!quitAllowed);
  CHECK(symbolClick(a,xuiMsgBoxGetButtonWidget(xideMessageCurrent(a),1)));xideMessagePoll(a);CHECK(!quitAllowed&&!xideMessageBusy(a)&&status.barrierHeld);
  CHECK(symbolClick(a,xideSymbolRenameControl(a,XIDE_SYMBOL_APPLY))&&xideMessageCurrent(a));
  CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"// newer consent\n")==XUI_OK&&symbolClick(a,xuiMsgBoxGetButtonWidget(xideMessageCurrent(a),0)));xideMessagePoll(a);CHECK(!quitAllowed&&xideMessageCurrent(a));
  CHECK(symbolClick(a,xuiMsgBoxGetButtonWidget(xideMessageCurrent(a),0)));xideMessagePoll(a);CHECK(quitAllowed&&!workspaceQuitCalls&&resumeCalls==before&&xideLanguageEditApplyStatus(a,&status)&&status.barrierHeld);probeQuit=0;a->hidden=1;
 }else {CHECK(!status.barrierHeld&&xideDialogIsOpen(window));CHECK(symbolClick(a,xideSymbolRenameControl(a,XIDE_SYMBOL_CANCEL)));input(a,c,cBody);char* after=copy(xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor)));CHECK(after);input(a,xl,after);free(after);}
 normalApi=0;if(mode==23){xide_text_file disk={0};CHECK(xideTextFileRead(paths[0],&disk,NULL)&&!strcmp(disk.text,kept));xideTextFileClear(&disk);}if(mode!=23&&mode!=20)checkDisk(paths,mainBody,"import shared\nfunc main(...) as int\n return shared.amount(2)\nend\n","export func amount(int value) as int\n return value\nend\n",!strcmp(nextSymbolName,"final_amount")?2:1);free(kept);return;
unchanged:
 CHECK(!strcmp(xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor)),kept));normalApi=0;checkDisk(paths,mainBody,"import shared\nfunc main(...) as int\n return shared.amount(2)\nend\n","export func amount(int value) as int\n return value\nend\n",0);free(kept);
}
