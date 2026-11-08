/* Fixed categories; external manifests add definitions, never replace the UI. */
xui_widget xideHomeNewWindow(xide_app* a) {return a && a->homeView ? a->homeView->newWindow:NULL;}
static float templateLineHeight(xide_app* a) {xui_font_metrics_t m={0};a->proxy.fontGetMetrics(&a->proxy,a->uiFont,&m);return fmaxf(20*xideUiFontScale(a),ceilf(m.fLineHeight));}
static int templateDrawText(xide_app* a,xui_draw_context draw,const char* text,xui_rect_t r,uint32_t color,int flags) {
    if(r.fW<=0 || r.fH<=0)return XUI_OK;
    xui_text_item_t item={0};item.iSize=sizeof(item);item.pFont=a->uiFont;item.sText=text;item.iTextSize=-1;item.iFlags=XUI_TEXT_SHAPE_DEFAULT;
    return a->proxy.drawText(&a->proxy,draw,&item,r,color,flags|XUI_TEXT_CLIP);
}
static float templateMeasure(xide_app* a,const char* text,size_t size) {
    xui_text_item_t item={0};item.iSize=sizeof(item);item.pFont=a->uiFont;item.sText=text;item.iTextSize=(int)size;item.iFlags=XUI_TEXT_SHAPE_DEFAULT;xui_vec2_t measured={0};
    return a->proxy.textMeasure(&a->proxy,&item,&measured)==XUI_OK ? measured.fX:0;
}
static size_t templatePrefix(xide_app* a,const char* text,float width) {
    size_t n=strlen(text),at=0,last=0;
    while(at<n){size_t next=at+1;while(next<n&&((unsigned char)text[next]&0xc0)==0x80)++next;if(templateMeasure(a,text,next)>width)break;last=next;at=next;}
    return last;
}
static void templateWrap(xide_app* a,const char* source,float width,char out[2][1024]) {
    out[0][0]=out[1][0]=0;
    for(int line=0;line<2&&*source;++line){size_t n=templatePrefix(a,source,width);if(!n)return;if(n>1000){n=1000;while(n&&((unsigned char)source[n]&0xc0)==0x80)--n;}
        const char* newline=memchr(source,'\n',n);if(newline)n=(size_t)(newline-source);
        if(line==1&&source[n]){float ellipsis=templateMeasure(a,"…",strlen("…"));n=templatePrefix(a,source,fmaxf(0,width-ellipsis));if(n>1000){n=1000;while(n&&((unsigned char)source[n]&0xc0)==0x80)--n;}memcpy(out[line],source,n);strcpy(out[line]+n,"…");return;}
        memcpy(out[line],source,n);out[line][n]=0;source+=n;while(*source==' '||*source=='\n'||*source=='\r')++source;
    }
}
static const xide_template_definition* homeTemplate(xide_app* a,int index) {
    struct xide_home_view* v=a->homeView;return index>=0&&index<v->visibleCount ? &v->templates.items[v->visible[index]]:NULL;
}
static const char* homeTemplateText(xui_widget w,int index,void* user) {(void)w;const xide_template_definition* d=homeTemplate(user,index);return d ? d->title:"";}
static void homeTemplateHeader(xide_app* a,int index,xui_rect_t row,xui_rect_t* title,xui_rect_t* metadata,xui_rect_t* link) {
    struct xide_home_view* v=a->homeView;int id=v->visible[index];float s=xideUiFontScale(a),line=templateLineHeight(a),gap=10*s;
    float x=row.fX+124*s,width=fmaxf(0,row.fW-136*s),top=row.fY+(row.fH-(3*line+6*s))/2;
    float linkWidth=fminf(width,v->headers[id].link),available=fmaxf(0,width-(linkWidth ? linkWidth+gap:0));
    float metaWidth=fminf(v->headers[id].metadata,available*0.45f),titleWidth=fminf(v->headers[id].title,fmaxf(0,available-(metaWidth ? metaWidth+gap:0)));
    *title=(xui_rect_t){x,top,titleWidth,line};*metadata=(xui_rect_t){x+titleWidth+(metaWidth ? gap:0),top,metaWidth,line};*link=(xui_rect_t){x+width-linkWidth,top,linkWidth,line};
}
static int homeTemplateRow(xui_widget w,int index,xui_draw_context draw,xui_rect_t r,int state,void* user) {
    (void)w;xide_app* a=user;struct xide_home_view* v=a->homeView;const xide_template_definition* d=homeTemplate(a,index);if(!d)return XUI_OK;
    float s=xideUiFontScale(a),pad=12*s,iconW=96*s,iconH=72*s,gap=16*s,line=templateLineHeight(a);uint32_t text=ideThemeColor(a->theme,IDE_TEXT);
    if(state&(XUI_LIST_ITEM_SELECTED|XUI_LIST_ITEM_HOVER)){uint32_t color=ideThemeColor(a->theme,(state&XUI_LIST_ITEM_SELECTED) ? IDE_SELECTION:IDE_HOVER);TRY(a->proxy.drawRectFill(&a->proxy,draw,r,color));}
    int id=v->visible[index];xui_surface image=v->icons[id];
    if(image){xui_rect_t src=v->iconSources[id];float factor=fminf(iconW/src.fW,iconH/src.fH),width=src.fW*factor,height=src.fH*factor;TRY(a->proxy.drawSurface(&a->proxy,draw,image,src,(xui_rect_t){r.fX+pad+(iconW-width)/2,r.fY+(r.fH-height)/2,width,height},XUI_COLOR_WHITE,0));}
    float x=r.fX+pad+iconW+gap,width=fmaxf(0,r.fW-2*pad-iconW-gap);xui_rect_t title,meta,link;homeTemplateHeader(a,index,r,&title,&meta,&link);
    char metadata[256];snprintf(metadata,sizeof(metadata),"%s%s%s",d->author,*d->author&&*d->version ? " · ":"",d->version);
    TRY(templateDrawText(a,draw,d->title,title,text,XUI_TEXT_ALIGN_LEFT));
    TRY(templateDrawText(a,draw,metadata,meta,ideThemeColor(a->theme,IDE_MUTED),XUI_TEXT_ALIGN_LEFT));
    if(link.fW>0){uint32_t color=ideThemeColor(a->theme,IDE_ACCENT);TRY(templateDrawText(a,draw,"点击查看说明",link,color,XUI_TEXT_ALIGN_RIGHT));TRY(a->proxy.drawRectFill(&a->proxy,draw,(xui_rect_t){link.fX,link.fY+link.fH-2,link.fW,1},color));}
    for(int i=0;i<2;++i)TRY(templateDrawText(a,draw,v->descriptions[id][i],(xui_rect_t){x,title.fY+line+6*s+i*line,width,line},text,XUI_TEXT_ALIGN_LEFT));
    if(state&XUI_LIST_ITEM_FOCUS)TRY(a->proxy.drawRectStroke(&a->proxy,draw,(xui_rect_t){r.fX+1,r.fY+1,r.fW-2,r.fH-2},1,ideThemeColor(a->theme,IDE_BORDER)));
    return 1;
}
static int homeCategoryIcon(xui_widget w,int id,xui_draw_context draw,xui_rect_t r,int state,void* user) {
    (void)w;(void)state;xide_app* a=user;struct xide_home_view* v=a->homeView;int kind=id==1 ? 0:id==3 ? 2:id>=100&&id-100<v->categoryCount&&!v->templates.items[v->categoryDefinition[id-100]].project ? 2:1;
    xui_surface icon=v->categoryIcons[kind];if(!icon)return 0;
    xui_surface_desc_t desc={0};TRY(a->proxy.surfaceGetDesc(&a->proxy,icon,&desc));
    TRY(a->proxy.drawSurface(&a->proxy,draw,icon,(xui_rect_t){0,0,desc.iWidth,desc.iHeight},(xui_rect_t){r.fX,r.fY+(r.fH-16)/2,16,16},XUI_COLOR_WHITE,0));return 1;
}
static int homeTemplateLinkAt(xide_app* a,int x,int y) {
    xui_widget list=xideHomeControl(a,XIDE_HOME_TEMPLATES);xui_rect_t world=xuiWidgetGetWorldRect(list);x-=world.fX;y-=world.fY;
    int index=xuiListViewGetItemAt(list,x,y);if(index<0)return -1;
    xui_rect_t title,metadata,link;homeTemplateHeader(a,index,xuiListViewGetItemRect(list,index),&title,&metadata,&link);
    return link.fW>0&&x>=link.fX&&x<link.fX+link.fW&&y>=link.fY&&y<link.fY+link.fH ? index:-1;
}
static int homeTemplateCursor(xui_widget w,int x,int y,void* user) {(void)w;return homeTemplateLinkAt(user,x,y)>=0 ? XUI_CURSOR_HAND:XUI_CURSOR_INHERIT;}
static void homeTemplateCancelLink(xide_app* a) {
    struct xide_home_view* v=a->homeView;if(!v->linkPressed)return;v->linkPressed=0;
    if(xuiGetPointerCaptureEx(a->context,v->linkPointer,v->linkPointerType)==v->picker)xuiReleasePointerCaptureEx(a->context,v->linkPointer,v->linkPointerType,v->picker);
}
static int homeTemplatePointer(xui_widget w,const xui_event_t* e,void* user) {
    xide_app* a=user;struct xide_home_view* v=a->homeView;if(e->iPhase==XUI_EVENT_PHASE_BUBBLE)return XUI_OK;
    if(e->iType==XUI_EVENT_POINTER_CAPTURE_LOST){v->linkPressed=0;return XUI_OK;}
    if(e->iButton!=XUI_POINTER_BUTTON_LEFT)return XUI_OK;
    int hit=homeTemplateLinkAt(a,e->fX,e->fY);
    if(e->iType==XUI_EVENT_POINTER_DOWN&&hit>=0){v->linkPressed=hit+1;v->linkPointer=e->iPointerId;v->linkPointerType=e->iPointerType;TRY(xuiSetPointerCaptureEx(a->context,e->iPointerId,e->iPointerType,w));return XUI_EVENT_DISPATCH_STOP;}
    if(e->iType==XUI_EVENT_POINTER_UP&&v->linkPressed&&v->linkPointer==e->iPointerId&&v->linkPointerType==e->iPointerType){int pressed=v->linkPressed-1;v->linkPressed=0;TRY(xuiReleasePointerCaptureEx(a->context,e->iPointerId,e->iPointerType,w));if(hit==pressed){const xide_template_definition* d=homeTemplate(a,hit);wchar_t url[2049];if(d&&MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,d->website,-1,url,2049)){if((INT_PTR)ShellExecuteW(NULL,L"open",url,NULL,NULL,SW_SHOWNORMAL)<=32)xideMessageShow(a,"模板说明","无法打开浏览器，请检查默认浏览器设置。",XUI_MSGBOX_ICON_ERROR);}}return XUI_EVENT_DISPATCH_STOP;}
    return hit>=0&&(e->iType==XUI_EVENT_POINTER_DOUBLE_CLICK||e->iType==XUI_EVENT_POINTER_CLICK) ? XUI_EVENT_DISPATCH_STOP:XUI_OK;
}
static void homeNewSelected(xui_widget w,int index,void* user) {
    (void)w;xide_app* a=user;const xide_template_definition* d=homeTemplate(a,index);
    xuiLabelSetText(xideHomeControl(a,XIDE_HOME_TEMPLATE_DESCRIPTION),d ? d->description:"请选择模板。");
    xuiButtonSetText(xideHomeControl(a,XIDE_HOME_CREATE),d&&d->project ? "创建工程…":"创建文件");xuiWidgetSetEnabled(xideHomeControl(a,XIDE_HOME_CREATE),d!=NULL);
}
static void homeCategorySelected(xui_widget w,int category,void* user) {
    (void)w;xide_app* a=user;struct xide_home_view* v=a->homeView;homeTemplateCancelLink(a);v->category=category;v->visibleCount=0;const xide_template_definition* sub=category>=100&&category-100<v->categoryCount ? &v->templates.items[v->categoryDefinition[category-100]]:NULL;
    /* Console first; stable catalog IDs do not depend on visual ordering. */
    if(category==1||category==2){for(size_t i=0;i<v->templates.count;++i)if(v->templates.items[i].project&&!strcmp(v->templates.items[i].id,"console"))v->visible[v->visibleCount++]=(int)i;}
    for(size_t i=0;i<v->templates.count;++i){const xide_template_definition* d=&v->templates.items[i];if((category==1)||(category==2&&d->project)||(category==3&&!d->project)||(sub&&d->project==sub->project&&!strcmp(d->category,sub->category))){if((category==1||category==2)&&d->project&&!strcmp(d->id,"console"))continue;v->visible[v->visibleCount++]=(int)i;}}
    xuiListViewSetVirtualItems(v->controls[XIDE_HOME_TEMPLATES],v->visibleCount,homeTemplateText,a);xuiListViewSetSelected(v->controls[XIDE_HOME_TEMPLATES],v->visibleCount ? 0:-1);xuiListViewSetScroll(v->controls[XIDE_HOME_TEMPLATES],0);homeNewSelected(NULL,v->visibleCount ? 0:-1,a);xgeRenderRequest();
}
static void homeNewClose(xui_widget w,void* user) {(void)w;xide_app* a=user;homeTemplateCancelLink(a);xideDialogSetOpen(xideHomeNewWindow(a),0);xideActionsRefresh(a);xgeRenderRequest();}
static void homeNewCreate(xui_widget w,void* user) {
    (void)w;xide_app* a=user;if(!xideDialogIsOpen(xideHomeNewWindow(a)))return;
    const xide_template_definition* d=homeTemplate(a,xuiListViewGetSelected(xideHomeControl(a,XIDE_HOME_TEMPLATES)));if(!d)return;
    if(d->project||!d->builtin){if(xideTemplateViewOpenDefinition(a,d))xideDialogSetOpen(xideHomeNewWindow(a),0);else xuiLabelSetText(xideHomeControl(a,XIDE_HOME_TEMPLATE_DESCRIPTION),"无法打开创建表单；请先应用／取消工程属性草稿，或重新打开新建窗口。");return;}
    xide_model_error error={0};xide_document* doc=xideDocumentNew(&a->documents,d->language,&error);
    if(!doc){xideMessageShow(a,"新建文件",error.message,XUI_MSGBOX_ICON_ERROR);return;}
    xideDialogSetOpen(xideHomeNewWindow(a),0);if(xideEditorShow(a,doc)!=XUI_OK){xideDocumentClose(doc,1,NULL);xideDialogSetOpen(xideHomeNewWindow(a),1);}
    xideActionsRefresh(a);xgeRenderRequest();
}
static void homeNewActivate(xui_widget w,int index,int button,void* user) {(void)index;(void)button;homeNewCreate(w,user);}
static int homeNewPickerLayout(xui_widget picker,xui_rect_t area,void* user) {
    xide_app* a=user;struct xide_home_view* v=a->homeView;float s=xideUiFontScale(a),gap=12*s,left=fminf(184*s,area.fW*0.3f),right=fmaxf(0,area.fW-left-gap),text=fmaxf(0,right-24*s-96*s-16*s-16*s);
    if(v->templateScale!=s||v->templateFont!=a->uiFont||fabsf(v->descriptionWidth-text)>0.5f){for(size_t i=0;i<v->templates.count;++i){const xide_template_definition* d=&v->templates.items[i];char metadata[256];snprintf(metadata,sizeof(metadata),"%s%s%s",d->author,*d->author&&*d->version ? " · ":"",d->version);v->headers[i].title=ceilf(templateMeasure(a,d->title,strlen(d->title)));v->headers[i].metadata=ceilf(templateMeasure(a,metadata,strlen(metadata)));v->headers[i].link=*d->website ? ceilf(templateMeasure(a,"点击查看说明",strlen("点击查看说明"))):0;templateWrap(a,d->description,text,v->descriptions[i]);}v->descriptionWidth=text;v->templateScale=s;v->templateFont=a->uiFont;xuiListViewSetItemHeight(v->controls[XIDE_HOME_TEMPLATES],24*s+fmaxf(72*s,3*templateLineHeight(a)+6*s));xuiTreeViewSetMetrics(v->controls[XIDE_HOME_CATEGORY],28*s,18*s,4*s,1);}
    TRY(xuiLayoutArrangeChild(picker,v->controls[XIDE_HOME_CATEGORY],(xui_rect_t){area.fX,area.fY,left,area.fH}));
    return xuiLayoutArrangeChild(picker,v->controls[XIDE_HOME_TEMPLATES],(xui_rect_t){area.fX+left+gap,area.fY,right,area.fH});
}
static int homeTemplatesLoad(xide_app* a) {
    struct xide_home_view* v=a->homeView;if(v->templatesLoaded)return XUI_OK;
    char* settings=a->preferences.path ? xidePathDirectory(a->preferences.path):NULL;char* root=settings ? xidePathResolve(settings,"templates"):NULL;xide_model_error error={0};
    int okay=xideTemplateRegistryLoad(&v->templates,root,&error);free(root);free(settings);if(!okay)return XUI_ERROR;
    v->descriptions=calloc(v->templates.count,sizeof(*v->descriptions));if(!v->descriptions)return XUI_ERROR_OUT_OF_MEMORY;
    wchar_t binary[32768];char utf8[32768*3];DWORD got=GetModuleFileNameW(NULL,binary,32768);char* exe=NULL;if(got&&WideCharToMultiByte(CP_UTF8,0,binary,-1,utf8,sizeof(utf8),NULL,NULL))exe=xidePathDirectory(utf8);
    for(size_t i=0;i<v->templates.count;++i){xide_template_definition* d=&v->templates.items[i];char name[256];snprintf(name,sizeof(name),"data/template-icons/%s.png",d->builtin ? d->icon:d->project ? "blank":"script");char* path=d->builtin||!*d->icon ? (exe ? xidePathResolve(exe,name):NULL):xidePathResolve(d->root,d->icon);
        if(path){int result=xidePathHasExtension(path,".svg") ? a->proxy.surfaceLoadSvgFile(&a->proxy,&v->icons[i],path,384,288,0):a->proxy.surfaceLoadFile(&a->proxy,&v->icons[i],path,0);if(result!=XUI_OK)v->icons[i]=NULL;if(v->icons[i]){xui_surface_desc_t desc={0};if(a->proxy.surfaceGetDesc(&a->proxy,v->icons[i],&desc)==XUI_OK&&desc.iWidth>0&&desc.iHeight>0)v->iconSources[i]=(xui_rect_t){0,0,(float)desc.iWidth,(float)desc.iHeight};else{a->proxy.surfaceDestroy(&a->proxy,v->icons[i]);v->icons[i]=NULL;}}free(path);}
    }
    const char* categoryNames[]={"tree-local","tree-project","tree-file"};for(int i=0;i<3;++i){char name[128];snprintf(name,sizeof(name),"data/template-icons/%s%s.png",categoryNames[i],a->dpiScale>1 ? "@2x":"");char* path=exe ? xidePathResolve(exe,name):NULL;if(path){int result=a->proxy.surfaceLoadFile(&a->proxy,&v->categoryIcons[i],path,0);if(result!=XUI_OK)v->categoryIcons[i]=NULL;free(path);}}
    free(exe);v->templatesLoaded=1;return XUI_OK;
}
static int homeNewBuild(xide_app* a) {
    struct xide_home_view* v=a->homeView;TRY(homeTemplatesLoad(a));
    TRY(xideDialogCreate(a,&v->newWindow,"新建",900,600,14));TRY(xuiWidgetAddChild(a->root,v->newWindow));TRY(xideDialogSetCancel(v->newWindow,homeNewClose,a));TRY(xideDialogFormBegin(v->newWindow,760,480));
    TRY(xuiWidgetCreate(a->context,&v->picker));TRY(xuiWidgetSetLayoutType(v->picker,XUI_LAYOUT_MANUAL));
    TRY(xuiWidgetSetEventCallback(v->picker,homeTemplatePointer,a));TRY(xuiWidgetSetCursorQueryCallback(v->picker,homeTemplateCursor,a));
    v->categoryCount=0;xui_tree_view_node_t nodes[259]={0};nodes[0]=(xui_tree_view_node_t){.iId=1,.iParent=-1,.sText="本地模板",.bExpanded=1,.bEnabled=1,.bHasChildren=1,.bIconReserved=1};nodes[1]=(xui_tree_view_node_t){.iId=2,.iParent=1,.sText="工程",.bExpanded=1,.bEnabled=1,.bIconReserved=1};nodes[2]=(xui_tree_view_node_t){.iId=3,.iParent=1,.sText="独立文件",.bExpanded=1,.bEnabled=1,.bIconReserved=1};
    for(size_t i=6;i<v->templates.count;++i){const xide_template_definition* d=&v->templates.items[i];if(!*d->category)continue;int duplicate=0;for(int j=0;j<v->categoryCount;++j){const xide_template_definition* old=&v->templates.items[v->categoryDefinition[j]];if(old->project==d->project&&!strcmp(old->category,d->category))duplicate=1;}if(duplicate)continue;int n=v->categoryCount++;v->categoryDefinition[n]=(int)i;nodes[n+3]=(xui_tree_view_node_t){.iId=100+n,.iParent=d->project ? 2:3,.sText=d->category,.bEnabled=1,.bIconReserved=1};nodes[d->project ? 1:2].bHasChildren=1;}
    xui_tree_view_desc_t tree={0};tree.iSize=sizeof(tree);tree.pFont=a->uiFont;tree.arrNodes=nodes;tree.iNodeCount=3+v->categoryCount;tree.iSelectedId=2;tree.fItemHeight=28*xideUiFontScale(a);
    TRY(xuiTreeViewCreate(a->context,&v->controls[XIDE_HOME_CATEGORY],&tree));TRY(xuiWidgetAddChild(v->picker,v->controls[XIDE_HOME_CATEGORY]));TRY(xuiTreeViewSetSelect(v->controls[XIDE_HOME_CATEGORY],homeCategorySelected,a));TRY(xuiTreeViewSetIconRenderer(v->controls[XIDE_HOME_CATEGORY],homeCategoryIcon,a));
    xui_list_view_desc_t list={0};list.iSize=sizeof(list);list.pFont=a->uiFont;list.fItemHeight=96*xideUiFontScale(a);
    TRY(xuiListViewCreate(a->context,&v->controls[XIDE_HOME_TEMPLATES],&list));TRY(xuiWidgetAddChild(v->picker,v->controls[XIDE_HOME_TEMPLATES]));TRY(xuiListViewSetSelect(v->controls[XIDE_HOME_TEMPLATES],homeNewSelected,a));TRY(xuiListViewSetActivate(v->controls[XIDE_HOME_TEMPLATES],homeNewActivate,a));TRY(xuiListViewSetItemRenderer(v->controls[XIDE_HOME_TEMPLATES],homeTemplateRow,a));TRY(xuiWidgetSetLayoutChildrenCallback(v->picker,homeNewPickerLayout,a));TRY(xuiWidgetSetCursorQueryCallback(xuiListViewGetViewportWidget(v->controls[XIDE_HOME_TEMPLATES]),homeTemplateCursor,a));
    xui_label_desc_t label={0};label.iSize=sizeof(label);label.pFont=a->uiFont;label.iWrapMode=XUI_TEXT_WRAP_WORD;TRY(xuiLabelCreate(a->context,&v->controls[XIDE_HOME_TEMPLATE_DESCRIPTION],&label));
    const char* titles[]={"关闭","创建工程…"};int ids[]={XIDE_HOME_CANCEL,XIDE_HOME_CREATE};for(int i=0;i<2;++i){xui_button_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;d.sText=titles[i];TRY(xuiButtonCreate(a->context,&v->controls[ids[i]],&d));TRY(xuiButtonSetClick(v->controls[ids[i]],i==0 ? homeNewClose:homeNewCreate,a));}
    TRY(xideDialogFormBlock(v->newWindow,v->picker,300,1));TRY(xideDialogFormBlock(v->newWindow,v->controls[XIDE_HOME_TEMPLATE_DESCRIPTION],0,0));
    if(*v->templates.issues)TRY(xideDialogFormText(v->newWindow,v->templates.issues));
    xui_widget buttons[]={v->controls[XIDE_HOME_CANCEL],v->controls[XIDE_HOME_CREATE]};TRY(xideDialogFormActions(v->newWindow,buttons,2));TRY(xideDialogFormEnd(v->newWindow));TRY(xuiWidgetSetDefaultAction(v->newWindow,homeNewCreate,a));homeCategorySelected(NULL,2,a);return XUI_OK;
}
static void homeNewLayout(xide_app* a) {xui_widget w=xideHomeNewWindow(a);if(w)xideDialogSetMaxSize(w,a->viewportWidth-32,a->viewportHeight-32);}
static int homeNewOpen(xide_app* a) {
    struct xide_home_view* v=a->homeView;if(!v)return 0;
    if(!v->newWindow&&homeNewBuild(a)!=XUI_OK){xideDialogDestroy(v->newWindow);v->newWindow=NULL;v->picker=NULL;for(int i=XIDE_HOME_CATEGORY;i<XIDE_HOME_CONTROL_COUNT;++i)v->controls[i]=NULL;xideMessageShow(a,"新建","无法加载模板选择窗口，请重试。",XUI_MSGBOX_ICON_ERROR);return 0;}
    xuiTreeViewSetSelected(v->controls[XIDE_HOME_CATEGORY],2);homeCategorySelected(NULL,2,a);homeNewLayout(a);xideDialogSetScroll(v->newWindow,0,0);xideDialogSetOpen(v->newWindow,1);xuiSetFocusWidget(a->context,v->controls[XIDE_HOME_TEMPLATES]);xideActionsRefresh(a);xgeRenderRequest();return 1;
}
