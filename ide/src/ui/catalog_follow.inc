/* Polling only observes a versioned bounded selection. No analysis requests or
 * command-tree rebuilds are initiated by this feature. */
struct follow_binding {follow_binding* next;xui_widget editor;xide_app* app;xui_widget_event_proc original[2];void* user[2];};
static const int followEvents[]={XUI_EVENT_IME_COMPOSITION,XUI_EVENT_BLUR};
static int helpVisible(xide_app* a) {
 if(!a->dock || !a->catalogView || !xuiWidgetGetEffectiveVisible(a->panels[XIDE_HELP]))return 0;
 xui_dock_window_info_t info={0};info.iSize=sizeof(info);
 if(xuiDockPanelGetWindowInfo(a->dock,a->windows[XIDE_HELP],&info)!=XUI_OK || info.iState==XUI_DOCK_PANEL_WINDOW_HIDDEN)return 0;
 return info.iState!=XUI_DOCK_PANEL_WINDOW_DOCKED || xuiDockPanelGetPaneActiveWindow(a->dock,info.iPane)==a->windows[XIDE_HELP];
}
static void followReset(xide_app* a) {
 xide_catalog_view* v=a->catalogView;v->observedEditor=NULL;v->followPending=0;v->helpRetry=0;v->choiceCount=v->choiceTotal=0;++v->followRevision;
 if(v->followChoices){v->followUpdating=1;xuiComboBoxSetItems(v->followChoices,NULL,0);xuiWidgetSetVisible(v->followChoices,0);v->followUpdating=0;}
}
static int followEvent(xui_widget w,const xui_event_t* e,void* user) {
 follow_binding* b=user;xide_catalog_view* v=b->app->catalogView;int i=e->iType==XUI_EVENT_IME_COMPOSITION ? 0:1;
 if(v) {if(!i)v->composingEditor=e->bCompositionActive ? w:NULL;else if(v->composingEditor==w)v->composingEditor=NULL;}
 return b->original[i] ? b->original[i](w,e,b->user[i]):XUI_OK;
}
static int followBind(xide_app* a,xui_widget editor) {
 xide_catalog_view* v=a->catalogView;if(!v || !editor)return XUI_ERROR_INVALID_ARGUMENT;
 for(follow_binding* b=v->followBindings;b;b=b->next)if(b->editor==editor)return XUI_OK;
 follow_binding* b=calloc(1,sizeof(*b));if(!b)return XUI_ERROR_OUT_OF_MEMORY;b->app=a;b->editor=editor;
 for(int i=0;i<2;++i){int r=xuiWidgetGetEventHandler(editor,followEvents[i],&b->original[i],&b->user[i]);if(r==XUI_OK)r=xuiWidgetSetEventHandler(editor,followEvents[i],followEvent,b);if(r!=XUI_OK){for(int j=0;j<i;++j)xuiWidgetSetEventHandler(editor,followEvents[j],b->original[j],b->user[j]);free(b);return r;}}
 b->next=v->followBindings;v->followBindings=b;return XUI_OK;
}
static void followDestroy(xide_catalog_view* v) {while(v->followBindings){follow_binding* n=v->followBindings->next;free(v->followBindings);v->followBindings=n;}}
static void pinChanged(xui_widget w,int checked,void* user) {(void)w;xide_catalog_view* v=((xide_app*)user)->catalogView;v->pinned=checked;v->followPending=0;v->observedEditor=NULL;}
static void candidateSelected(xui_widget w,int index,int value,void* user) {
 (void)w;(void)value;xide_app* a=user;xide_catalog_view* v=a->catalogView;
 if(!v->followUpdating && index>=0 && (size_t)index<v->choiceCount)selectCommand(a,v->choices[index],1);
}
static int followCreate(xide_app* a) {
 xide_catalog_view* v=a->catalogView;
 TRY(xuiWidgetCreate(a->context,&v->followBar));TRY(xuiWidgetSetLayoutType(v->followBar,XUI_LAYOUT_ROW));TRY(xuiWidgetSetGap(v->followBar,8));
 xui_label_desc_t label={0};label.iSize=sizeof(label);label.pFont=a->uiFont;label.sText="选择代码单词以查看名称匹配帮助";
 TRY(xuiLabelCreate(a->context,&v->followNotice,&label));TRY(xuiWidgetSetSizeMode(v->followNotice,XUI_SIZE_FILL,XUI_SIZE_FILL));TRY(xuiWidgetSetFlex(v->followNotice,1,1));TRY(xuiWidgetAddChild(v->followBar,v->followNotice));
 xui_checkbox_desc_t pin={0};pin.iSize=sizeof(pin);pin.pFont=a->uiFont;pin.sText="固定当前条目";
 TRY(xuiCheckBoxCreate(a->context,&v->followPin,&pin));TRY(xuiCheckBoxSetChange(v->followPin,pinChanged,a));TRY(xuiWidgetAddChild(v->followBar,v->followPin));
 xui_combobox_desc_t choices={0};choices.iSize=sizeof(choices);choices.pFont=a->uiFont;choices.iSelected=-1;choices.sPlaceholder="同名候选 / 重载（名称匹配）";
 TRY(xuiComboBoxCreate(a->context,&v->followChoices,&choices));TRY(xuiComboBoxSetSelect(v->followChoices,candidateSelected,a));return xuiWidgetSetVisible(v->followChoices,0);
}
static int wordByte(unsigned char c) {return c>=128 || (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_';}
static int nameByte(unsigned char c) {return wordByte(c) || c=='.';}
static int selectedName(xui_widget editor,int start,int end,int caret,char name[513]) {
 xui_code_document core=xuiCodeEditGetDocument(editor);int length=xuiCodeDocumentGetLength(core);
 if(start<0 || end<start || end>length || end-start>512 || (!caret && start==end))return 0;
 int first=start>513 ? start-513:0,last=length-end>513 ? end+513:length,n=0;char text[1540];
 if(xuiCodeDocumentCopyRange(core,first,last,text,sizeof(text),&n)!=XUI_OK || n<last-first)return 0;
 int left=start-first,right=end-first;
 if(start==end) {while(left>0 && nameByte((unsigned char)text[left-1]))--left;while(right<n && nameByte((unsigned char)text[right]))++right;}
 else {
  for(int i=left;i<right;++i)if(!nameByte((unsigned char)text[i]))return 0;
  if((left>0 && wordByte((unsigned char)text[left-1])) || (right<n && wordByte((unsigned char)text[right])))return 0;
  while(left>0 && nameByte((unsigned char)text[left-1]))--left;
  while(right<n && nameByte((unsigned char)text[right]))++right;
 }
 if(right<=left || right-left>512 || (!left && first) || (right==n && last<length))return 0;
 if(!wordByte((unsigned char)text[left]) || isdigit((unsigned char)text[left]) || text[right-1]=='.' || ((unsigned char)text[left]&0xc0)==0x80)return 0;
 for(int i=left+1;i<right;++i)if(text[i]=='.' && (text[i-1]=='.' || i+1==right || isdigit((unsigned char)text[i+1])))return 0;
 xui_code_token_t tokens[8];int count=0;
 if(xuiCodeTokenBufferGetTokensInRange(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(core),first+left,first+right,tokens,8,&count)!=XUI_OK)return 0;
 for(int i=0;i<count && i<8;++i)if(tokens[i].iKind==XUI_CODE_TOKEN_STRING || tokens[i].iKind==XUI_CODE_TOKEN_CHAR || tokens[i].iKind==XUI_CODE_TOKEN_COMMENT || tokens[i].iKind==XUI_CODE_TOKEN_ERROR)return 0;
 memcpy(name,text+left,(size_t)(right-left));name[right-left]=0;return 1;
}
static xui_widget selectionEditor(xide_app* a) {
 xui_widget focus=xuiGetFocusWidget(a->context);
 if(a->activeDocument && a->activeDocument->open && focus==a->activeDocument->editor)return focus;
 return NULL;
}
static int matchSelection(xide_app* a,xui_widget editor,int start,int end,int caret) {
 xide_catalog_view* v=a->catalogView;char name[513];if(!selectedName(editor,start,end,caret,name))return 0;
 uint32_t found[64];size_t total=0;const char* language=xuiCodeEditGetLanguage(editor);
 size_t count=xideCatalogLookupName(v->lookup,name,language,0,found,64,&total);
 if(!count) {const char* p=strrchr(name,'.');count=xideCatalogLookupName(v->lookup,p ? p+1:name,language,1,found,64,&total);}
 if(!count)return 0;
 int same=count==v->choiceCount && total==v->choiceTotal && !memcmp(found,v->choices,count*sizeof(*found));
 if(!same) {
  memcpy(v->choices,found,count*sizeof(*found));v->choiceCount=count;v->choiceTotal=total;v->followUpdating=1;
  const char* titles[64];for(size_t i=0;i<count;++i)titles[i]=a->catalog.entries[found[i]].signature;
  xuiComboBoxSetItems(v->followChoices,titles,(int)count);xuiComboBoxSetSelected(v->followChoices,0);xuiWidgetSetVisible(v->followChoices,count>1);v->followUpdating=0;
 }
 char notice[650];snprintf(notice,sizeof(notice),"名称匹配：%s · %zu 个候选%s（未作语义确认）",name,total,total>count ? "，显示前 64 项":"");
 if(strcmp(xuiLabelGetText(v->followNotice),notice))xuiLabelSetText(v->followNotice,notice);
 size_t index=found[0];if(same && v->selected)for(size_t i=0;i<count;++i)if(v->selected==a->catalog.entries+found[i])index=found[i];
 return selectCommand(a,index,1);
}
static int followPoll(xide_app* a,uint64_t now,int force) {
 xide_catalog_view* v=a->catalogView;if(!v || !v->lookup)return 0;
 if(!force && (!xideSettingsOptions(&a->preferences)->helpFollow || xuiCheckBoxGetChecked(v->followPin) || !helpVisible(a) || xideActionsHasModal(a))){v->followPending=0;v->observedEditor=NULL;return 0;}
 xui_widget editor=selectionEditor(a);if(!editor || v->composingEditor==editor || xuiGetPointerCapture(a->context)){v->followPending=0;v->observedEditor=NULL;return 0;}
 if(followBind(a,editor)!=XUI_OK)return 0;
 int start=0,end=0;if(xuiCodeSelectionGetCount(xuiCodeEditGetSelection(editor))!=1 || xuiCodeSelectionGetRange(xuiCodeEditGetSelection(editor),&start,&end)!=XUI_OK)return 0;
 xui_code_document core=xuiCodeEditGetDocument(editor);uint32_t version=xuiCodeDocumentGetVersion(core);
 if(force)return matchSelection(a,editor,start,end,1);
 uint64_t document=a->activeDocument && a->activeDocument->editor==editor ? a->activeDocument->id:0;const char* language=xuiCodeEditGetLanguage(editor);
 if(editor!=v->observedEditor || core!=v->observedCore || document!=v->observedDocument || strcmp(language,v->observedLanguage) || version!=v->observedVersion || start!=v->observedStart || end!=v->observedEnd) {
  v->observedDocument=document;snprintf(v->observedLanguage,sizeof(v->observedLanguage),"%s",language);v->observedEditor=editor;v->observedCore=core;v->observedVersion=version;v->observedStart=start;v->observedEnd=end;v->followAt=now+80;v->followPending=start!=end;return 0;
 }
 if(!v->followPending || now<v->followAt)return 0;
 v->followPending=0;return matchSelection(a,editor,start,end,0);
}
void xideCatalogFollowPollAt(xide_app* a,uint64_t now){if(a && a->catalogView)followPoll(a,now,0);}
int xideCatalogFollowNow(xide_app* a,int open){if(!a || !a->catalogView)return 0;int result=followPoll(a,GetTickCount64(),1);if(open && result)xideWorkbenchRevealHelp(a);return result;}
xui_widget xideCatalogFollowPin(xide_app* a){return a && a->catalogView ? a->catalogView->followPin:NULL;}
xui_widget xideCatalogFollowChoices(xide_app* a){return a && a->catalogView ? a->catalogView->followChoices:NULL;}
size_t xideCatalogFollowCandidateCount(xide_app* a){return a && a->catalogView ? a->catalogView->choiceTotal:0;}
