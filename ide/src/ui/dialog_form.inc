/* Shared responsive forms. Geometry lives in XUI rows/columns, not scaled
 * design-time X/Y coordinates. Every widget keeps its original native owner. */
#define FORM_TRY(call) do {int result_=(call);if(result_!=XUI_OK)return result_;}while(0)
static int adoptFormWidget(xui_widget parent,xui_widget* child)
{
    int result=xuiWidgetAddChild(parent,*child);
    if(result!=XUI_OK){xuiWidgetDestroy(*child);*child=NULL;}
    return result;
}
static int moveFormWidget(xui_widget parent,xui_widget child)
{
    if(!parent || !child)return XUI_ERROR_INVALID_ARGUMENT;
    xui_widget old=xuiWidgetGetParent(child);if(old==parent)return XUI_OK;
    if(old)FORM_TRY(xuiWidgetRemoveFromParent(child));
    int result=xuiWidgetAddChild(parent,child);
    if(result!=XUI_OK && old)xuiWidgetAddChild(old,child);
    return result;
}
int xideDialogSetMinSize(xui_widget w,float width,float height)
{xide_dialog* d=dialog(w);if(!d || !isfinite(width) || !isfinite(height) || width<=0 || height<=0)return XUI_ERROR_INVALID_ARGUMENT;d->minWidth=width;d->minHeight=height;return XUI_OK;}
int xideDialogFormBegin(xui_widget w,float minWidth,float minHeight)
{
    xide_dialog* d=dialog(w);if(!d || d->form)return XUI_ERROR_INVALID_ARGUMENT;
    FORM_TRY(xideDialogSetMinSize(w,minWidth,minHeight));
    xui_widget client=xuiWindowGetClientWidget(w);
    FORM_TRY(xuiWidgetSetLayoutType(client,XUI_LAYOUT_COLUMN));
    FORM_TRY(xuiWidgetSetGap(client,0));
    FORM_TRY(xuiWidgetSetSizeMode(d->scroll,XUI_SIZE_FILL,XUI_SIZE_FILL));
    FORM_TRY(xuiWidgetSetFlex(d->scroll,1,1));
    FORM_TRY(xuiScrollViewSetScrollbarPolicy(d->scroll,XUI_SCROLLBAR_POLICY_HIDDEN,XUI_SCROLLBAR_POLICY_AUTO));
    FORM_TRY(xuiWidgetSetLayoutType(d->content,XUI_LAYOUT_COLUMN));
    FORM_TRY(xuiWidgetCreate(d->app->context,&d->footer));
    FORM_TRY(adoptFormWidget(client,&d->footer));
    FORM_TRY(xuiWidgetSetLayoutType(d->footer,XUI_LAYOUT_COLUMN));
    FORM_TRY(xuiWidgetSetSizeMode(d->footer,XUI_SIZE_FILL,XUI_SIZE_FIXED));
    FORM_TRY(xuiWidgetSetFlex(d->footer,0,0));
    xui_separator_desc_t line={0};line.iSize=sizeof(line);line.fThickness=1;line.iOrientation=XUI_ORIENTATION_HORIZONTAL;
    FORM_TRY(xuiSeparatorCreate(d->app->context,&d->divider,&line));
    FORM_TRY(adoptFormWidget(d->footer,&d->divider));
    FORM_TRY(xuiWidgetSetSizeMode(d->divider,XUI_SIZE_FILL,XUI_SIZE_FIXED));
    FORM_TRY(xuiWidgetSetVisible(d->footer,0));
    d->form=1;return XUI_OK;
}
static dialog_row* formRow(xide_dialog* d,int footer)
{
    dialog_row* r=calloc(1,sizeof(*r));if(!r)return NULL;
    if(xuiWidgetCreate(d->app->context,&r->row)!=XUI_OK){free(r);return NULL;}
    if(xuiWidgetAddChild(footer ? d->footer:d->content,r->row)!=XUI_OK){xuiWidgetDestroy(r->row);free(r);return NULL;}
    r->footer=footer;
    if(d->tail)d->tail->next=r;else d->rows=r;d->tail=r;return r;
}
static int rowWidgets(dialog_row* r,const xui_widget* children,size_t count)
{
    if(!children || !count || count>12)return XUI_ERROR_INVALID_ARGUMENT;
    for(size_t i=0;i<count;++i) {
        if(!children[i])return XUI_ERROR_INVALID_ARGUMENT;
        r->children[i]=children[i];
        FORM_TRY(moveFormWidget(r->line ? r->line:r->row,children[i]));
    }
    r->count=count;return XUI_OK;
}
static int namedFormField(xui_widget w,const char* title,xui_widget label,xui_widget control,xui_widget first,xui_widget second)
{
    xide_dialog* d=dialog(w);if(!d || !d->form || !control)return XUI_ERROR_INVALID_ARGUMENT;
    dialog_row* r=formRow(d,0);if(!r)return XUI_ERROR_OUT_OF_MEMORY;r->field=1;r->height=32;
    int created=!label;
    if(created) {
        xui_label_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=d->app->uiFont;desc.sText=title;desc.iWrapMode=XUI_TEXT_WRAP_WORD;
        FORM_TRY(xuiLabelCreate(d->app->context,&label,&desc));
    }
    int result=moveFormWidget(r->row,label);
    if(result!=XUI_OK){if(created)xuiWidgetDestroy(label);return result;}
    r->label=label;
    FORM_TRY(xuiWidgetCreate(d->app->context,&r->line));FORM_TRY(adoptFormWidget(r->row,&r->line));
    xui_widget values[]={control,first,second};return rowWidgets(r,values,second ? 3:first ? 2:1);
}
int xideDialogFormField(xui_widget w,const char* title,xui_widget control,xui_widget first,xui_widget second)
{return title ? namedFormField(w,title,NULL,control,first,second):XUI_ERROR_INVALID_ARGUMENT;}
int xideDialogFormNamedField(xui_widget w,xui_widget label,xui_widget control,xui_widget first,xui_widget second)
{return label ? namedFormField(w,NULL,label,control,first,second):XUI_ERROR_INVALID_ARGUMENT;}
int xideDialogFormBlock(xui_widget w,xui_widget child,float height,int grow)
{
    xide_dialog* d=dialog(w);if(!d || !d->form || !child || height<0)return XUI_ERROR_INVALID_ARGUMENT;
    dialog_row* r=formRow(d,0);if(!r)return XUI_ERROR_OUT_OF_MEMORY;r->block=1;r->height=height;r->grow=grow;
    return rowWidgets(r,&child,1);
}
int xideDialogFormText(xui_widget w,const char* text)
{
    xide_dialog* d=dialog(w);if(!d || !d->form || !text)return XUI_ERROR_INVALID_ARGUMENT;
    xui_label_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=d->app->uiFont;desc.sText=text;desc.iWrapMode=XUI_TEXT_WRAP_WORD;
    xui_widget label=NULL;FORM_TRY(xuiLabelCreate(d->app->context,&label,&desc));
    int result=xideDialogFormBlock(w,label,0,0);if(result!=XUI_OK && !xuiWidgetGetParent(label))xuiWidgetDestroy(label);return result;
}
int xideDialogFormRow(xui_widget w,const xui_widget* children,size_t count,float height,int grow)
{
    xide_dialog* d=dialog(w);if(!d || !d->form)return XUI_ERROR_INVALID_ARGUMENT;
    dialog_row* r=formRow(d,0);if(!r)return XUI_ERROR_OUT_OF_MEMORY;r->height=height;r->grow=grow;return rowWidgets(r,children,count);
}
int xideDialogFormActions(xui_widget w,const xui_widget* children,size_t count)
{
    xide_dialog* d=dialog(w);if(!d || !d->form)return XUI_ERROR_INVALID_ARGUMENT;
    /* One wrapping action strip uses spare width across action groups. A
     * separate forced row per declaration wastes scarce vertical space. */
    for(dialog_row* old=d->rows;old;old=old->next)if(old->footer) {
        if(!children || !count || old->count+count>12)return XUI_ERROR_INVALID_ARGUMENT;
        for(size_t i=0;i<count;++i){if(!children[i])return XUI_ERROR_INVALID_ARGUMENT;FORM_TRY(moveFormWidget(old->row,children[i]));old->children[old->count++]=children[i];}
        return XUI_OK;
    }
    dialog_row* r=formRow(d,1);if(!r)return XUI_ERROR_OUT_OF_MEMORY;r->height=32;
    FORM_TRY(xuiWidgetCreate(d->app->context,&r->spacer));
    FORM_TRY(adoptFormWidget(r->row,&r->spacer));
    FORM_TRY(xuiWidgetSetSizeMode(r->spacer,XUI_SIZE_FILL,XUI_SIZE_FIXED));
    FORM_TRY(xuiWidgetSetFlex(r->spacer,1,1));
    return rowWidgets(r,children,count);
}
int xideDialogFormEnd(xui_widget w)
{
    xide_dialog* d=dialog(w);if(!d || !d->form)return XUI_ERROR_INVALID_ARGUMENT;
    /* Unnamed labels from the old form canvas are replaced by field labels.
     * Controls may never be silently discarded by an incomplete migration. */
    for(xui_widget c=xuiWidgetGetFirstChild(d->content);c;) {
        xui_widget next=xuiWidgetGetNextSibling(c);int row=0;
        for(dialog_row* r=d->rows;r;r=r->next)if(r->row==c)row=1;
        if(!row) {
            if(xuiWidgetGetType(c)!=xuiLabelGetType(d->app->context)) {
                fprintf(stderr,"Unplaced dialog control: %s\n",xuiWindowGetTitle(w));return XUI_ERROR_INVALID_STATE;
            }
            FORM_TRY(xuiWidgetDestroyChecked(c));
        }
        c=next;
    }
    return XUI_OK;
}
int xideDialogSetFixedBody(xui_widget w,int fixed)
{
    xide_dialog* d=dialog(w);if(!d || !d->form)return XUI_ERROR_INVALID_ARGUMENT;
    d->fixedBody=!!fixed;return xuiScrollViewSetScrollbarPolicy(d->scroll,XUI_SCROLLBAR_POLICY_HIDDEN,fixed ? XUI_SCROLLBAR_POLICY_HIDDEN:XUI_SCROLLBAR_POLICY_AUTO);
}
xui_widget xideDialogGetFooterWidget(xui_widget w){xide_dialog* d=dialog(w);return d ? d->footer:NULL;}
xui_widget xideDialogGetDividerWidget(xui_widget w){xide_dialog* d=dialog(w);return d ? d->divider:NULL;}
int xideDialogGetBodySize(xui_widget w,float* width,float* height)
{xide_dialog* d=dialog(w);if(!d)return XUI_ERROR_INVALID_ARGUMENT;if(width)*width=d->bodyWidth;if(height)*height=d->bodyHeight;return XUI_OK;}
static int rowLayout(xide_dialog* d,dialog_row* r,float available,float scale,int* footerLines)
{
    int visible=0;float fixed=0;size_t shown=0;
    for(size_t i=0;i<r->count;++i)if(xuiWidgetGetVisible(r->children[i])) {visible=1;++shown;}
    if(r->field)visible=xuiWidgetGetVisible(r->children[0]);
    FORM_TRY(xuiWidgetSetVisible(r->row,visible));if(!visible)return XUI_OK;
    float gap=(r->footer ? 6:8)*scale,height=r->height*scale;
    int compact=r->field && available<500*scale;
    for(size_t i=0;i<r->count;++i)if(xuiWidgetGetType(r->children[i])==xuiButtonGetType(d->app->context)) {
        const char* text=xuiButtonGetText(r->children[i]);uint64_t hash=1;
        for(const unsigned char* p=(const unsigned char*)text;*p;++p)hash=hash*131+*p;
        if(r->measureScale!=scale || r->measureFont!=d->app->uiFont || r->textHash[i]!=hash) {
            xui_vec2_t size={0};FORM_TRY(xideDpiTextMeasure(&d->app->proxy,d->app->uiFont,text,&size));
            r->measured[i]=fmaxf((r->footer ? 48:80)*scale,ceilf(size.fX)+(r->footer ? 16:32)*scale);r->textHash[i]=hash;
        }
    }
    r->measureScale=scale;r->measureFont=d->app->uiFont;
    FORM_TRY(xuiWidgetSetLayoutType(r->row,r->block || compact ? XUI_LAYOUT_COLUMN:XUI_LAYOUT_ROW));
    FORM_TRY(xuiWidgetSetGap(r->row,gap));
    FORM_TRY(xuiWidgetSetSizeMode(r->row,XUI_SIZE_FILL,r->block && r->grow ? XUI_SIZE_FILL:XUI_SIZE_CONTENT));
    FORM_TRY(xuiWidgetSetFlex(r->row,r->grow ? 1:0,0));
    FORM_TRY(xuiWidgetSetMinSize(r->row,(xui_vec2_t){0,height}));
    if(r->field) {
        FORM_TRY(xuiWidgetSetPreferredSize(r->label,(xui_vec2_t){compact ? 0:128*scale,0}));
        FORM_TRY(xuiWidgetSetSizeMode(r->label,compact ? XUI_SIZE_FILL:XUI_SIZE_FIXED,XUI_SIZE_CONTENT));
        FORM_TRY(xuiWidgetSetLayoutType(r->line,XUI_LAYOUT_ROW));
        FORM_TRY(xuiWidgetSetGap(r->line,gap));
        FORM_TRY(xuiWidgetSetSizeMode(r->line,XUI_SIZE_FILL,XUI_SIZE_CONTENT));
        FORM_TRY(xuiWidgetSetFlex(r->line,1,1));
        if(!compact)available-=128*scale+gap;
    }
    for(size_t i=0;i<r->count;++i)if(xuiWidgetGetVisible(r->children[i])) {
        int button=xuiWidgetGetType(r->children[i])==xuiButtonGetType(d->app->context);
        fixed+=button ? r->measured[i]:80*scale;
    }
    int stack=!r->block && fixed+gap*(shown ? shown-1:0)>available;
    if(r->spacer)FORM_TRY(xuiWidgetSetVisible(r->spacer,!stack));
    xui_widget line=r->line ? r->line:r->row;
    if(stack)FORM_TRY(xuiWidgetSetLayoutType(line,r->footer ? XUI_LAYOUT_FLOW:XUI_LAYOUT_COLUMN));
    if(r->footer) {FORM_TRY(xuiWidgetSetAlign(line,XUI_ALIGN_END,XUI_ALIGN_START));FORM_TRY(xuiWidgetSetGap(line,gap));}
    float used=0;int lines=1;
    for(size_t i=0;i<r->count;++i) {
        xui_widget c=r->children[i];int button=xuiWidgetGetType(c)==xuiButtonGetType(d->app->context);
        if(xuiWidgetGetType(c)==xuiComboBoxGetType(d->app->context)) {
            xui_widget menu=xuiComboBoxGetMenuWidget(c),popup=menu ? xuiMenuGetPopupWidget(menu):NULL;
            if(popup)FORM_TRY(xuiPopupSetMaxSize(popup,fmaxf(1,d->app->viewportWidth-32),fmaxf(1,d->app->viewportHeight-64*scale)));
        }
        float width=button ? r->measured[i]:0;
        if(width>available)width=available;
        FORM_TRY(xuiWidgetSetFlowMode(c,r->footer && stack ? XUI_FLOW_INLINE_BLOCK:XUI_FLOW_BLOCK));
        FORM_TRY(xuiWidgetSetSizeMode(c,button && (!stack || r->footer) ? XUI_SIZE_FIXED:XUI_SIZE_FILL,
            r->block ? (r->grow ? XUI_SIZE_FILL:XUI_SIZE_CONTENT):r->grow ? XUI_SIZE_FILL:XUI_SIZE_FIXED));
        FORM_TRY(xuiWidgetSetPreferredSize(c,(xui_vec2_t){width,height}));
        FORM_TRY(xuiWidgetSetMinSize(c,(xui_vec2_t){0,height}));
        FORM_TRY(xuiWidgetSetFlex(c,button ? 0:1,button ? 0:1));
        if(r->footer && xuiWidgetGetVisible(c)) {if(used && used+gap+width>available){++lines;used=0;}used+=(used ? gap:0)+width;}
    }
    if(r->footer) {
        float pad=d->padding*scale;
        FORM_TRY(xuiWidgetSetMargin(line,(xui_thickness_t){pad,0,pad,0}));
        FORM_TRY(xuiWidgetSetSizeMode(line,XUI_SIZE_FILL,XUI_SIZE_FIXED));
        FORM_TRY(xuiWidgetSetPreferredSize(line,(xui_vec2_t){0,lines*height+(lines-1)*gap}));
        *footerLines+=lines;
    }
    return XUI_OK;
}
static int layoutForm(xide_dialog* d,float width,float height,float scale)
{
    float pad=d->padding*scale,bodyWidth=fmaxf(1,width-(d->fixedBody ? 0:16*scale));int lines=0,groups=0;float actionsHeight=0;
    FORM_TRY(xuiWidgetSetPadding(d->content,(xui_thickness_t){pad,pad,pad,pad}));
    FORM_TRY(xuiWidgetSetGap(d->content,10*scale));
    for(dialog_row* r=d->rows;r;r=r->next) {
        FORM_TRY(rowLayout(d,r,fmaxf(1,(r->footer ? width:bodyWidth)-2*pad),scale,&lines));
        if(r->footer && xuiWidgetGetVisible(r->row)){++groups;actionsHeight+=xuiWidgetGetPreferredSize(r->row).fY;}
    }
    float footerPad=fminf(d->padding,8)*scale;
    float footer=lines ? scale+footerPad+groups*footerPad+actionsHeight:0;
    FORM_TRY(xuiWidgetSetVisible(d->footer,lines>0));
    FORM_TRY(xuiWidgetSetPadding(d->footer,(xui_thickness_t){0,0,0,footerPad}));
    FORM_TRY(xuiWidgetSetGap(d->footer,footerPad));
    FORM_TRY(xuiWidgetSetPreferredSize(d->divider,(xui_vec2_t){0,scale}));
    FORM_TRY(xuiSeparatorSetColor(d->divider,ideThemeColor(d->app->theme,IDE_BORDER)));
    FORM_TRY(xuiWidgetSetPreferredSize(d->footer,(xui_vec2_t){0,footer}));
    float viewport=fmaxf(1,height-footer);d->bodyWidth=fmaxf(1,bodyWidth-2*pad);d->bodyHeight=fmaxf(1,viewport-2*pad);
    float natural=2*pad;int bodyRows=0;
    /* ScrollView owns its content's fixed extent. Measuring that same widget
     * would return yesterday's extent instead of the natural form height. */
    if(!d->fixedBody)for(dialog_row* r=d->rows;r;r=r->next)if(!r->footer && xuiWidgetGetVisible(r->row)) {
        xui_vec2_t measured={0};
        FORM_TRY(xuiWidgetMeasureContent(r->row,(xui_vec2_t){fmaxf(1,bodyWidth-2*pad),XUI_LAYOUT_UNBOUNDED},&measured));
        natural+=fmaxf(measured.fY,r->height*scale)+(bodyRows++ ? 10*scale:0);
    }
    FORM_TRY(xuiScrollViewSetContentSize(d->scroll,bodyWidth,d->fixedBody ? viewport:fmaxf(viewport,natural)));
    return XUI_OK;
}
#undef FORM_TRY
