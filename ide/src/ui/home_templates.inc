/* Initial local-template chooser. Projects/workspaces keep their existing
 * commands; this chooser creates independent, unsaved source documents only. */
xui_widget xideHomeNewWindow(xide_app* a) {return a && a->homeView ? a->homeView->newWindow:NULL;}
static void homeNewLayout(xide_app* a) {
    xui_widget w=xideHomeNewWindow(a);if(w)xideDialogSetMaxSize(w,a->viewportWidth-32,a->viewportHeight-32);
}
static void homeNewClose(xui_widget w,void* user) {
    (void)w;xide_app* a=user;xideDialogSetOpen(xideHomeNewWindow(a),0);xideActionsRefresh(a);xgeRenderRequest();
}
static void homeNewSelected(xui_widget w,int index,void* user) {
    (void)w;xide_app* a=user;
    const char* text=index==0 ? "创建独立的 xlang 文件（.xl），保存时选择文件名和位置。":
        index==1 ? "创建独立的 C 文件（.c），保存时选择文件名和位置。":"请选择一个文件模板。";
    xuiLabelSetText(xideHomeControl(a,XIDE_HOME_TEMPLATE_DESCRIPTION),text);
    xuiWidgetSetEnabled(xideHomeControl(a,XIDE_HOME_CREATE),index>=0 && index<2);
}
static void homeNewCreate(xui_widget w,void* user) {
    (void)w;xide_app* a=user;if(!xideDialogIsOpen(xideHomeNewWindow(a)))return;
    int index=xuiListViewGetSelected(xideHomeControl(a,XIDE_HOME_TEMPLATES));if(index<0 || index>1)return;
    xide_model_error error={0};xide_document* doc=xideDocumentNew(&a->documents,index==0 ? "xlang":"c",&error);
    if(!doc) {xideMessageShow(a,"新建文件",error.message,XUI_MSGBOX_ICON_ERROR);return;}
    xideDialogSetOpen(xideHomeNewWindow(a),0);
    if(xideEditorShow(a,doc)!=XUI_OK) {
        xideDocumentClose(doc,1,NULL);if(doc->window>=0)xuiDockPanelHideWindow(a->dock,doc->window);
        xideDialogSetOpen(xideHomeNewWindow(a),1);
        xideMessageShow(a,"新建文件","无法打开新的编辑器，请重试。",XUI_MSGBOX_ICON_ERROR);
    }
    xideActionsRefresh(a);xgeRenderRequest();
}
static void homeNewActivate(xui_widget w,int index,int button,void* user) {(void)index;(void)button;homeNewCreate(w,user);}
static int homeNewPickerLayout(xui_widget picker,xui_rect_t area,void* user) {
    xide_app* a=user;float s=xideUiFontScale(a),gap=10*s,left=fminf(152*s,area.fW*0.34f);
    TRY(xuiLayoutArrangeChild(picker,xideHomeControl(a,XIDE_HOME_CATEGORY),(xui_rect_t){area.fX,area.fY,left,area.fH}));
    return xuiLayoutArrangeChild(picker,xideHomeControl(a,XIDE_HOME_TEMPLATES),(xui_rect_t){area.fX+left+gap,area.fY,fmaxf(0,area.fW-left-gap),area.fH});
}
static int homeNewBuild(xide_app* a) {
    struct xide_home_view* v=a->homeView;
    TRY(xideDialogCreate(a,&v->newWindow,"新建",640,420,14));TRY(xuiWidgetAddChild(a->root,v->newWindow));
    TRY(xideDialogSetCancel(v->newWindow,homeNewClose,a));TRY(xideDialogFormBegin(v->newWindow,520,340));
    TRY(xuiWidgetCreate(a->context,&v->picker));TRY(xuiWidgetSetLayoutType(v->picker,XUI_LAYOUT_MANUAL));
    static const xui_tree_view_node_t categories[]={
        {.iId=1,.iParent=-1,.sText="本地模板",.bExpanded=1,.bEnabled=1,.bHasChildren=1},
        {.iId=2,.iParent=1,.sText="独立文件",.bEnabled=1}
    };
    xui_tree_view_desc_t tree={0};tree.iSize=sizeof(tree);tree.pFont=a->uiFont;tree.arrNodes=categories;tree.iNodeCount=2;tree.iSelectedId=2;
    TRY(xuiTreeViewCreate(a->context,&v->controls[XIDE_HOME_CATEGORY],&tree));TRY(xuiWidgetAddChild(v->picker,v->controls[XIDE_HOME_CATEGORY]));
    static const char* items[]={"xlang 文件（.xl）","C 文件（.c）"};
    xui_list_view_desc_t list={0};list.iSize=sizeof(list);list.pFont=a->uiFont;list.arrItems=items;list.iItemCount=2;list.iSelected=0;list.fItemHeight=40*xideUiFontScale(a);
    TRY(xuiListViewCreate(a->context,&v->controls[XIDE_HOME_TEMPLATES],&list));TRY(xuiWidgetAddChild(v->picker,v->controls[XIDE_HOME_TEMPLATES]));
    TRY(xuiListViewSetSelect(v->controls[XIDE_HOME_TEMPLATES],homeNewSelected,a));TRY(xuiListViewSetActivate(v->controls[XIDE_HOME_TEMPLATES],homeNewActivate,a));
    TRY(xuiWidgetSetLayoutChildrenCallback(v->picker,homeNewPickerLayout,a));
    xui_label_desc_t label={0};label.iSize=sizeof(label);label.pFont=a->uiFont;label.iWrapMode=XUI_TEXT_WRAP_WORD;
    TRY(xuiLabelCreate(a->context,&v->controls[XIDE_HOME_TEMPLATE_DESCRIPTION],&label));
    const char* titles[]={"关闭","创建"};int ids[]={XIDE_HOME_CANCEL,XIDE_HOME_CREATE};
    for(int i=0;i<2;++i) {
        xui_button_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;d.sText=titles[i];
        TRY(xuiButtonCreate(a->context,&v->controls[ids[i]],&d));TRY(xuiButtonSetClick(v->controls[ids[i]],i==0 ? homeNewClose:homeNewCreate,a));
    }
    TRY(xideDialogFormBlock(v->newWindow,v->picker,220,1));
    TRY(xideDialogFormBlock(v->newWindow,v->controls[XIDE_HOME_TEMPLATE_DESCRIPTION],0,0));
    xui_widget buttons[]={v->controls[XIDE_HOME_CANCEL],v->controls[XIDE_HOME_CREATE]};
    TRY(xideDialogFormActions(v->newWindow,buttons,2));TRY(xideDialogFormEnd(v->newWindow));
    TRY(xuiWidgetSetDefaultAction(v->newWindow,homeNewCreate,a));homeNewSelected(NULL,0,a);return XUI_OK;
}
static int homeNewOpen(xide_app* a) {
    struct xide_home_view* v=a->homeView;if(!v)return 0;
    if(!v->newWindow && homeNewBuild(a)!=XUI_OK) {
        xideDialogDestroy(v->newWindow);v->newWindow=NULL;v->picker=NULL;
        for(int i=XIDE_HOME_CATEGORY;i<XIDE_HOME_CONTROL_COUNT;++i)v->controls[i]=NULL;
        xideMessageShow(a,"新建","无法创建模板选择窗口，请重试。",XUI_MSGBOX_ICON_ERROR);return 0;
    }
    homeNewSelected(NULL,xuiListViewGetSelected(v->controls[XIDE_HOME_TEMPLATES]),a);
    homeNewLayout(a);xideDialogSetScroll(v->newWindow,0,0);xideDialogSetOpen(v->newWindow,1);
    xuiSetFocusWidget(a->context,v->controls[XIDE_HOME_TEMPLATES]);xideActionsRefresh(a);xgeRenderRequest();return 1;
}
