#include "ui/help_document.h"
#include "platform/dpi_proxy.h"
#include "ui/reading_state.h"
#include "xui_document.h"
#include "lang/languages.h"
#include "theme/theme.h"
#include "services/editor_font.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#define TRY(call) do { int r_=(call); if(r_!=XUI_OK) return r_; } while(0)
#define MAX_BLOCKS 4096
typedef struct help_block {
    xui_widget widget,editor; char* text; int code,heading,height;
    xui_cache_policy_t renderCache; xide_help_view* view;
    xui_widget_event_proc keyHandler,moveHandler; void *keyUser,*moveUser;
    int viewport;
    const xide_help_document* source;
} help_block;
typedef struct help_chapter { char* title; size_t block; } help_chapter;
struct xide_help_view {
    xide_app* app; xui_widget content,navigator,pages;
    help_block** blocks; size_t blockCount;
    help_chapter* chapters; size_t chapterCount;
    char** pageTitles; size_t pageCount;
    int width,height,page,updating; size_t pendingChapter;
    const xide_help_document** documents; size_t documentCount;
};
typedef struct text_buffer { char* data; size_t size,capacity; } text_buffer;
static int append(text_buffer* text,const char* data,size_t bytes)
{
    if(bytes>4u*1024u*1024u || text->size>4u*1024u*1024u-bytes) return XUI_DOC_ERROR_LIMIT;
    if(text->size+bytes+1>text->capacity) {
        size_t capacity=(text->size+bytes+1)*2;
        char* fresh=realloc(text->data,capacity); if(!fresh) return XUI_ERROR_OUT_OF_MEMORY;
        text->data=fresh; text->capacity=capacity;
    }
    memcpy(text->data+text->size,data,bytes); text->size+=bytes; text->data[text->size]=0; return XUI_OK;
}
static int nodeText(xui_document_snapshot snapshot,xui_doc_node_id id,text_buffer* text,int depth)
{
    if(depth>128) return XUI_DOC_ERROR_LIMIT;
    xui_doc_node_info_t node={0}; node.iSize=sizeof(node); TRY(xuiDocumentSnapshotGetNode(snapshot,id,&node));
    if(node.iTextBytes) {
        if(node.iTextBytes>4u*1024u*1024u) return XUI_DOC_ERROR_LIMIT;
        char* data=malloc((size_t)node.iTextBytes+1); if(!data) return XUI_ERROR_OUT_OF_MEMORY;
        uint64_t bytes=0; int result=xuiDocumentSnapshotCopyText(snapshot,id,data,node.iTextBytes+1,&bytes);
        if(result==XUI_OK) result=append(text,data,(size_t)bytes);
        free(data); if(result!=XUI_OK) return result;
    }
    if(node.iKind==XUI_DOC_SOFT_BREAK) TRY(append(text," ",1));
    if(node.iKind==XUI_DOC_HARD_BREAK) TRY(append(text,"\n",1));
    if(node.iKind==XUI_DOC_IMAGE && node.sResource) {
        TRY(append(text,"[图片：",strlen("[图片："))); TRY(append(text,node.sResource,strlen(node.sResource))); TRY(append(text,"]",1));
    }
    for(uint64_t i=0;i<node.iChildCount;++i) { xui_doc_node_id child=0; TRY(xuiDocumentSnapshotGetChild(snapshot,id,i,&child)); TRY(nodeText(snapshot,child,text,depth+1)); }
    return XUI_OK;
}
static void clear(xide_help_view* view,int destroyWidgets)
{
    if(destroyWidgets) { xuiComboBoxSetItems(view->navigator,NULL,0); xuiComboBoxSetItems(view->pages,NULL,0); }
    for(size_t i=0;i<view->blockCount;++i) { help_block* block=view->blocks[i]; if(destroyWidgets) xuiWidgetDestroy(block->widget); free(block->text); free(block); }
    for(size_t i=0;i<view->chapterCount;++i) free(view->chapters[i].title);
    if(view->pageTitles) for(size_t i=0;i<view->pageCount;++i) free(view->pageTitles[i]);
    free(view->blocks); free(view->chapters); free(view->pageTitles); free(view->documents); view->documents=NULL; view->documentCount=0;
    view->blocks=NULL; view->chapters=NULL; view->pageTitles=NULL; view->blockCount=0; view->chapterCount=0; view->pageCount=0;
    view->width=0; view->height=0; view->page=0; view->pendingChapter=SIZE_MAX;
}
static int codeWheel(xui_widget widget,const xui_event_t* event,void* user)
{
    help_block* block=user;
    if(event->iPhase!=XUI_EVENT_PHASE_CAPTURE) return XUI_OK;
    float x=0,y=0; TRY(xuiCodeEditGetScroll(block->editor,&x,&y));
    if(event->fWheelX) TRY(xuiCodeEditSetScroll(block->editor,x+event->fWheelX*32,y));
    if(event->fWheelY) TRY(xuiScrollViewScrollBy(block->view->app->help,0,-event->fWheelY*xuiScrollViewGetWheelStep(block->view->app->help)));
    xgeRenderRequest(); return XUI_EVENT_DISPATCH_STOP;
}
static int codeInput(xui_widget widget,const xui_event_t* event,void* user)
{
    help_block* block=user; xui_code_selection_t before={0},after={0};
    xuiCodeSelectionGetState(xuiCodeEditGetSelection(widget),&before);
    xui_widget_event_proc handler=event->iType==XUI_EVENT_KEY_DOWN ? block->keyHandler : block->moveHandler;
    void* handlerUser=event->iType==XUI_EVENT_KEY_DOWN ? block->keyUser : block->moveUser;
    int result=handler ? handler(widget,event,handlerUser) : XUI_OK;
    if(result<0) return result;
    /* Readonly keyboard navigation still uses the full document. Move the
       outer reading viewport when the native editor reveals a distant caret. */
    if(block->viewport && event->iPhase==XUI_EVENT_PHASE_TARGET) {
        TRY(xuiCodeSelectionGetState(xuiCodeEditGetSelection(widget),&after));
        if(after.iCaretOffset!=before.iCaretOffset) {
            xide_app* app=block->view->app; int line=0;
            TRY(xuiCodeDocumentOffsetToLineColumn(xuiCodeEditGetDocument(widget),after.iCaretOffset,&line,NULL));
            float y=0; TRY(xuiScrollViewGetOffset(app->help,NULL,&y));
            float caret=xuiWidgetGetWorldRect(block->widget).fY-xuiWidgetGetWorldRect(app->helpContent).fY+4+line*xideEditorLineHeight(app);
            float height=xuiWidgetGetWorldRect(xuiScrollViewGetViewportWidget(app->help)).fH;
            float delta=caret<y ? caret-y : caret+xideEditorLineHeight(app)>y+height ? caret+xideEditorLineHeight(app)-y-height : 0;
            if(delta) TRY(xuiScrollViewScrollBy(app->help,0,delta));
        }
    }
    return result;
}
static int addBlock(xide_help_view* view,const char* text,int heading,const char* language)
{
    if(view->blockCount>=MAX_BLOCKS) return XUI_DOC_ERROR_LIMIT;
    help_block** fresh=realloc(view->blocks,(view->blockCount+1)*sizeof(*fresh)); if(!fresh) return XUI_ERROR_OUT_OF_MEMORY; view->blocks=fresh;
    help_block* block=calloc(1,sizeof(*block)); if(!block) return XUI_ERROR_OUT_OF_MEMORY;
    view->blocks[view->blockCount++]=block; block->view=view; block->heading=heading; block->text=malloc(strlen(text)+1); if(!block->text) return XUI_ERROR_OUT_OF_MEMORY; strcpy(block->text,text);
    if(language) {
        TRY(xuiWidgetCreate(view->app->context,&block->widget));
        TRY(xuiWidgetSetLayoutType(block->widget,XUI_LAYOUT_MANUAL));
        TRY(xuiWidgetSetOverflow(block->widget,XUI_OVERFLOW_HIDDEN));
        xui_code_edit_desc_t code={0}; code.iSize=sizeof(code); code.pFont=view->app->codeFont; code.sText=text; code.bReadonly=1; code.bShowLineNumbers=1; code.iTabColumns=4; code.iIndentColumns=4;
        TRY(xideDpiCodeEditCreate(view->app->context,&block->editor,&code)); block->code=1;
        TRY(xuiWidgetAddChild(block->widget,block->editor));
        TRY(xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(block->editor))); TRY(xideConfigureEditorLanguage(block->editor,language));
        TRY(xuiWidgetSetEventHandler(block->widget,XUI_EVENT_POINTER_WHEEL,codeWheel,block));
        TRY(xuiWidgetGetEventHandler(block->editor,XUI_EVENT_KEY_DOWN,&block->keyHandler,&block->keyUser));
        TRY(xuiWidgetSetEventHandler(block->editor,XUI_EVENT_KEY_DOWN,codeInput,block));
        TRY(xuiWidgetGetEventHandler(block->editor,XUI_EVENT_POINTER_MOVE,&block->moveHandler,&block->moveUser));
        TRY(xuiWidgetSetEventHandler(block->editor,XUI_EVENT_POINTER_MOVE,codeInput,block));
    } else {
        xui_label_desc_t label={0}; label.iSize=sizeof(label); label.pFont=view->app->uiFont; label.sText=text; label.iWrapMode=XUI_TEXT_WRAP_WORD;
        label.iTextFlags=XUI_TEXT_ALIGN_LEFT|XUI_TEXT_ALIGN_TOP|XUI_TEXT_CLIP; label.fLineGap=4;
        TRY(xuiLabelCreate(view->app->context,&block->widget,&label)); TRY(ideThemeSetTextRole(block->widget,heading ? IDE_ACCENT : IDE_TEXT));
    }
    /* Both Label and CodeEdit paint text through the SDK cache callback.
       Retain their native policy for visible blocks; PrepareRender releases
       offscreen surfaces. Only code containers have no drawing of their own. */
    block->renderCache=xuiWidgetGetCachePolicy(block->code ? block->editor : block->widget);
    {
        xui_cache_policy_t direct={0}; direct.iSize=sizeof(direct); direct.iPolicy=XUI_CACHE_POLICY_NONE;
        TRY(xuiWidgetSetCachePolicy(block->widget,&direct));
    }
    TRY(xuiWidgetSetSizeMode(block->widget,XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetAddChild(view->content,block->widget));
    return XUI_OK;
}
static int chapter(xide_help_view* view,const char* title,size_t block)
{
    help_chapter* fresh=realloc(view->chapters,(view->chapterCount+1)*sizeof(*fresh)); if(!fresh) return XUI_ERROR_OUT_OF_MEMORY; view->chapters=fresh;
    char* owned=malloc(strlen(title)+1); if(!owned) return XUI_ERROR_OUT_OF_MEMORY; strcpy(owned,title);
    view->chapters[view->chapterCount++]=(help_chapter){owned,block}; return XUI_OK;
}
static int renderNode(xide_help_view* view,xui_document_snapshot snapshot,xui_doc_node_id id,const char* prefix,int depth)
{
    if(depth>128) return XUI_DOC_ERROR_LIMIT;
    xui_doc_node_info_t node={0}; node.iSize=sizeof(node); TRY(xuiDocumentSnapshotGetNode(snapshot,id,&node));
    if(node.iKind==XUI_DOC_PARAGRAPH || node.iKind==XUI_DOC_HEADING || node.iKind==XUI_DOC_CODE_BLOCK || node.iKind==XUI_DOC_HTML || node.iKind==XUI_DOC_MATH || node.iKind==XUI_DOC_DIAGRAM) {
        text_buffer text={0}; int result=append(&text,prefix,strlen(prefix));
        if(result==XUI_OK) result=nodeText(snapshot,id,&text,0);
        if(result==XUI_OK && text.size) {
            int heading=node.iKind==XUI_DOC_HEADING;
            if(heading) result=chapter(view,text.data,view->blockCount);
            const char* language=NULL;
            if(node.iKind==XUI_DOC_CODE_BLOCK) language=node.sInfo && (!strcmp(node.sInfo,"xl") || !strcmp(node.sInfo,"xlang")) ? "xlang" : node.sInfo && (!strcmp(node.sInfo,"c") || !strcmp(node.sInfo,"C")) ? "c" : "text";
            if(result==XUI_OK) result=addBlock(view,text.data,heading,language);
        }
        free(text.data); return result;
    }
    if(node.iKind==XUI_DOC_TABLE) {
        if(!node.iChildCount) return XUI_OK;
        xui_doc_node_id header=0; TRY(xuiDocumentSnapshotGetChild(snapshot,id,0,&header));
        xui_doc_node_info_t headerInfo={0}; headerInfo.iSize=sizeof(headerInfo); TRY(xuiDocumentSnapshotGetNode(snapshot,header,&headerInfo));
        for(uint64_t row=1;row<node.iChildCount;++row) {
            xui_doc_node_id rowId=0; TRY(xuiDocumentSnapshotGetChild(snapshot,id,row,&rowId));
            xui_doc_node_info_t rowInfo={0}; rowInfo.iSize=sizeof(rowInfo); TRY(xuiDocumentSnapshotGetNode(snapshot,rowId,&rowInfo));
            text_buffer text={0}; int result=XUI_OK;
            for(uint64_t col=0;col<rowInfo.iChildCount && result==XUI_OK;++col) {
                if(col) result=append(&text,"\n",1);
                if(result==XUI_OK && col<headerInfo.iChildCount) { xui_doc_node_id cell=0; result=xuiDocumentSnapshotGetChild(snapshot,header,col,&cell); if(result==XUI_OK) result=nodeText(snapshot,cell,&text,0); if(result==XUI_OK) result=append(&text,"：",strlen("：")); }
                xui_doc_node_id cell=0; if(result==XUI_OK) result=xuiDocumentSnapshotGetChild(snapshot,rowId,col,&cell); if(result==XUI_OK) result=nodeText(snapshot,cell,&text,0);
            }
            if(result==XUI_OK && text.size) result=addBlock(view,text.data,0,NULL);
            free(text.data); if(result!=XUI_OK) return result;
        }
        return XUI_OK;
    }
    if(node.iKind==XUI_DOC_RULE) return addBlock(view,"────────────────",0,NULL);
    for(uint64_t i=0;i<node.iChildCount;++i) {
        xui_doc_node_id child=0; TRY(xuiDocumentSnapshotGetChild(snapshot,id,i,&child));
        char itemPrefix[160]; const char* childPrefix=prefix;
        if(node.iKind==XUI_DOC_LIST) {
            if(node.tAttributes.iFlags&XUI_DOC_ORDERED) snprintf(itemPrefix,sizeof(itemPrefix),"%s%llu. ",prefix,(unsigned long long)(node.tAttributes.iListStart+i));
            else snprintf(itemPrefix,sizeof(itemPrefix),"%s• ",prefix);
            childPrefix=itemPrefix;
        } else if(node.iKind==XUI_DOC_QUOTE) { snprintf(itemPrefix,sizeof(itemPrefix),"%s│ ",prefix); childPrefix=itemPrefix; }
        TRY(renderNode(view,snapshot,child,childPrefix,depth+1));
    }
    return XUI_OK;
}
static int pageItems(xide_help_view* view,int page)
{
    if(page<0 || (size_t)page>=view->pageCount) return XUI_ERROR_INVALID_ARGUMENT;
    const char* titles[XUI_COMBOBOX_ITEM_CAPACITY]; size_t start=(size_t)page*XUI_COMBOBOX_ITEM_CAPACITY,count=view->chapterCount-start;
    if(count>XUI_COMBOBOX_ITEM_CAPACITY) count=XUI_COMBOBOX_ITEM_CAPACITY;
    for(size_t i=0;i<count;++i) titles[i]=view->chapters[start+i].title;
    view->updating=1; int result=xuiComboBoxSetItems(view->navigator,titles,(int)count); xuiComboBoxSetSelected(view->navigator,count ? 0 : -1); view->page=page; view->updating=0;
    return result;
}
static void selected(xui_widget widget,int index,int value,void* user)
{
    (void)widget; (void)value; xide_help_view* view=user;
    if(!view->updating && index>=0) xideHelpJump(view,(size_t)view->page*XUI_COMBOBOX_ITEM_CAPACITY+(size_t)index);
}
static void pageSelected(xui_widget widget,int index,int value,void* user)
{
    (void)widget; (void)value; xide_help_view* view=user; if(!view->updating && index>=0) pageItems(view,index);
}
int xideHelpCreate(xide_app* app,xide_help_view** out)
{
    xide_help_view* view=calloc(1,sizeof(*view)); if(!view) return XUI_ERROR_OUT_OF_MEMORY;
    *out=view; view->app=app; view->pendingChapter=SIZE_MAX;
    TRY(xuiWidgetCreate(app->context,&view->content)); TRY(xuiWidgetSetLayoutType(view->content,XUI_LAYOUT_COLUMN)); TRY(xuiWidgetSetGap(view->content,10));
    xui_combobox_desc_t combo={0}; combo.iSize=sizeof(combo); combo.pFont=app->uiFont; combo.iSelected=-1; combo.sPlaceholder="跳转到章节"; combo.fItemHeight=28; combo.fPopupMaxHeight=300;
    TRY(xuiComboBoxCreate(app->context,&view->navigator,&combo)); TRY(xuiComboBoxSetSelect(view->navigator,selected,view));
    combo.sPlaceholder="章节分组"; TRY(xuiComboBoxCreate(app->context,&view->pages,&combo)); TRY(xuiComboBoxSetSelect(view->pages,pageSelected,view));
    return XUI_OK;
}
int xideHelpSet(xide_help_view* view,const xide_help_document* const* documents,size_t count)
{
    if(!view || (count && !documents)) return XUI_ERROR_INVALID_ARGUMENT;
    if(count && count==view->documentCount) { size_t i=0; for(;i<count && view->documents[i]==documents[i];++i) {} if(i==count) return XUI_OK; }
    view->updating=1; clear(view,1); int result=XUI_OK;
    for(size_t i=0;i<count && result==XUI_OK;++i) {
        const xide_help_document* source=documents[i]; size_t first=view->blockCount;
        char navigation[1024]; snprintf(navigation,sizeof(navigation),"章节导航 · %s",source->title); result=chapter(view,navigation,first);
        if(result==XUI_OK) result=addBlock(view,source->title,1,NULL);
        if(result==XUI_OK) { char origin[1024]; snprintf(origin,sizeof(origin),"文档来源：%s:%d",source->source,source->sourceLine); result=addBlock(view,origin,0,NULL); }
        xui_document document=NULL; xui_document_snapshot snapshot=NULL;
        xui_doc_desc_t desc={0}; desc.iSize=sizeof(desc); desc.iProfile=XUI_DOCUMENT_MARKDOWN; desc.bDisableHistory=1; desc.iMaxTextBytes=4u*1024u*1024u; desc.iMaxNodes=100000;
        if(result==XUI_OK) result=xuiDocumentCreate(&desc,&document);
        if(result==XUI_OK) result=xuiDocumentLoadMarkdown(document,source->markdown,strlen(source->markdown));
        if(result==XUI_OK) result=xuiDocumentAcquireSnapshot(document,&snapshot);
        if(result==XUI_OK) result=renderNode(view,snapshot,XUI_DOCUMENT_ROOT,"",0);
        xuiDocumentSnapshotRelease(snapshot); xuiDocumentRelease(document);
        if(result==XUI_OK) for(size_t block=first;block<view->blockCount;++block) view->blocks[block]->source=source;
    }
    if(result!=XUI_OK) { clear(view,1); addBlock(view,"长文档无法完整显示；请检查随附文档格式和大小，并根据条目来源查阅原始文件。",0,NULL); }
    if(view->chapterCount) {
        view->pageCount=(view->chapterCount+XUI_COMBOBOX_ITEM_CAPACITY-1)/XUI_COMBOBOX_ITEM_CAPACITY; view->pageTitles=calloc(view->pageCount,sizeof(*view->pageTitles));
        if(!view->pageTitles) { view->updating=0; return XUI_ERROR_OUT_OF_MEMORY; }
        for(size_t i=0;i<view->pageCount;++i) {
            char title[100]; snprintf(title,sizeof(title),"章节 %zu–%zu",i*XUI_COMBOBOX_ITEM_CAPACITY+1,(i+1)*XUI_COMBOBOX_ITEM_CAPACITY<view->chapterCount ? (i+1)*XUI_COMBOBOX_ITEM_CAPACITY : view->chapterCount);
            view->pageTitles[i]=malloc(strlen(title)+1); if(!view->pageTitles[i]) { view->updating=0; return XUI_ERROR_OUT_OF_MEMORY; } strcpy(view->pageTitles[i],title);
        }
        TRY(xuiComboBoxSetItems(view->pages,(const char**)view->pageTitles,(int)view->pageCount)); TRY(pageItems(view,0));
    }
    xuiWidgetSetVisible(view->navigator,view->chapterCount>0); xuiWidgetSetVisible(view->pages,view->pageCount>1); xuiWidgetSetVisible(view->content,view->blockCount>0);
    if(result==XUI_OK && count) { view->documents=malloc(count*sizeof(*view->documents)); if(!view->documents) return XUI_ERROR_OUT_OF_MEMORY; memcpy(view->documents,documents,count*sizeof(*view->documents)); view->documentCount=count; }
    view->updating=0; TRY(xideHelpTheme(view)); return result;
}
int xideHelpLayout(xide_help_view* view,int width)
{
    if(!view || view->width==width) return view ? view->height : 0;
    view->width=width; view->height=0;
    for(size_t i=0;i<view->blockCount;++i) {
        help_block* block=view->blocks[i]; int height=28;
        if(block->code) { int lines=1; for(const char* p=block->text;*p;++p) if(*p=='\n') ++lines; height=(int)ceilf(lines*xideEditorLineHeight(view->app))+24; if(height<75) height=75; }
        else { xui_vec2_t measured={0}; xuiWidgetMeasureContent(block->widget,(xui_vec2_t){width,XUI_LAYOUT_UNBOUNDED},&measured); height=measured.fY+10; if(height<28) height=28; if(block->heading) height+=10; }
        block->height=height; xuiWidgetSetPreferredSize(block->widget,(xui_vec2_t){0,height}); view->height+=height+10;
    }
    if(view->height) view->height-=10;
    xuiWidgetSetPreferredSize(view->content,(xui_vec2_t){0,view->height}); return view->height;
}
int xideHelpCodeFont(xide_help_view* view)
{
    if(!view) return XUI_OK;
    view->width=-1;
    for(size_t i=0;i<view->blockCount;++i) if(view->blocks[i]->code) { int result=xideCodeWidgetSetFont(view->app,view->blocks[i]->editor,view->app->codeFont); if(result!=XUI_OK) return result; }
    return XUI_OK;
}
static int intersects(xui_rect_t a,xui_rect_t b)
{
    return a.fW>0 && a.fH>0 && b.fW>0 && b.fH>0 && (int64_t)a.fX<(int64_t)b.fX+b.fW && (int64_t)a.fX+a.fW>b.fX && (int64_t)a.fY<(int64_t)b.fY+b.fH && (int64_t)a.fY+a.fH>b.fY;
}
static int prepareCache(help_block* block,int visible)
{
    xui_widget target=block->code ? block->editor : block->widget;
    xui_cache_policy_t policy=block->renderCache;
    if(!visible) policy.iPolicy=XUI_CACHE_POLICY_NONE;
    TRY(xuiWidgetSetCachePolicy(target,&policy));
    /* Hidden parents are skipped during cache preparation. Release their
       surfaces explicitly while retaining the text and reading anchors. */
    if(!visible) TRY(xuiWidgetSetCacheStateCount(target,0));
    return XUI_OK;
}
int xideHelpPrepareRender(xide_help_view* view)
{
    if(!view) return XUI_OK;
    xide_app* app=view->app;
    xui_rect_t clip=xuiWidgetGetWorldRect(xuiScrollViewGetViewportWidget(app->help));
    xui_rect_t screen={0,0,app->viewportWidth,app->viewportHeight};
    int shown=xuiWidgetGetEffectiveVisible(view->content) && intersects(clip,screen);
    for(size_t i=0;i<view->blockCount;++i) {
        help_block* block=view->blocks[i]; xui_rect_t rect=xuiWidgetGetWorldRect(block->widget);
        int visible=shown && intersects(rect,clip) && intersects(rect,screen);
        if(!block->code) { TRY(prepareCache(block,visible)); continue; }
        /* Keep full-height blocks for document layout and reading anchors, but
           give CodeEdit only a viewport-sized bitmap and the original text.
           Overscan preserves line placement at both edges while scrolling. */
        int height=rect.fH;
        if(height>clip.fH+128) height=clip.fH+128;
        if(height<1) height=1;
        block->viewport=rect.fH>height;
        int top=block->viewport ? clip.fY-rect.fY-64 : 0;
        int maximum=rect.fH-height-16;
        if(maximum<0) maximum=0;
        if(top>maximum) top=maximum;
        if(top<0) top=0;
        /* CodeEdit has no public scrollbar policy. Clip its native vertical
           bar beyond this container; the outer document owns vertical scroll. */
        xui_rect_t area={0,top,rect.fW+(block->viewport ? 8 : 0),height};
        xui_rect_t old=xuiWidgetGetRect(block->editor);
        if(old.fX!=area.fX || old.fY!=area.fY || old.fW!=area.fW || old.fH!=area.fH) {
            TRY(xuiWidgetSetRect(block->editor,area));
            TRY(xuiWidgetArrangeChild(block->widget,block->editor,area));
        }
        float x=0,y=0; TRY(xuiCodeEditGetScroll(block->editor,&x,&y));
        if(y!=top) TRY(xuiCodeEditSetScroll(block->editor,x,top));
        TRY(prepareCache(block,visible));
    }
    return XUI_OK;
}
int xideHelpTheme(xide_help_view* view)
{
    if(!view) return XUI_OK;
    int styles[]={XUI_CODE_STYLE_DEFAULT,XUI_CODE_STYLE_KEYWORD,XUI_CODE_STYLE_TYPE,XUI_CODE_STYLE_NUMBER,XUI_CODE_STYLE_STRING,XUI_CODE_STYLE_CHAR,XUI_CODE_STYLE_COMMENT};
    ide_color_role roles[]={IDE_TEXT,IDE_KEYWORD,IDE_TYPE,IDE_NUMBER,IDE_STRING,IDE_STRING,IDE_COMMENT};
    for(size_t i=0;i<view->blockCount;++i) if(view->blocks[i]->code) for(size_t j=0;j<sizeof(styles)/sizeof(*styles);++j) {
        xui_code_style_t style={sizeof(style),ideThemeColor(view->app->theme,roles[j]),ideThemeColor(view->app->theme,IDE_FIELD),0}; TRY(xuiCodeEditSetStyle(view->blocks[i]->editor,styles[j],&style));
    }
    return XUI_OK;
}
void xideHelpDestroy(xide_help_view* view) { if(view) { clear(view,0); free(view); } }
xui_widget xideHelpNavigator(xide_help_view* view) { return view ? view->navigator : NULL; }
xui_widget xideHelpPages(xide_help_view* view) { return view ? view->pages : NULL; }
xui_widget xideHelpContent(xide_help_view* view) { return view ? view->content : NULL; }
const xide_help_document* xideHelpSourceDocument(xide_help_view* view)
{
    if(!view || !view->documentCount) return NULL;
    int selected=xuiComboBoxGetSelected(view->navigator);
    size_t chapter=selected>=0 ? (size_t)view->page*XUI_COMBOBOX_ITEM_CAPACITY+(size_t)selected : SIZE_MAX;
    if(chapter<view->chapterCount && view->chapters[chapter].block<view->blockCount) return view->blocks[view->chapters[chapter].block]->source;
    return view->documents[0];
}
size_t xideHelpCodeCount(xide_help_view* view) { size_t count=0; if(view) for(size_t i=0;i<view->blockCount;++i) count+=view->blocks[i]->code!=0; return count; }
xui_widget xideHelpCode(xide_help_view* view,size_t index)
{ if(view) for(size_t i=0;i<view->blockCount;++i) if(view->blocks[i]->code && index--==0) return view->blocks[i]->editor; return NULL; }
size_t xideHelpChapterCount(xide_help_view* view) { return view ? view->chapterCount : 0; }
const char* xideHelpChapterTitle(xide_help_view* view,size_t chapter) { return view && chapter<view->chapterCount ? view->chapters[chapter].title : NULL; }
int xideHelpJump(xide_help_view* view,size_t chapter)
{
    if(!view || chapter>=view->chapterCount) return 0;
    int page=(int)(chapter/XUI_COMBOBOX_ITEM_CAPACITY); view->updating=1; xuiComboBoxSetSelected(view->pages,page);
    if(view->page!=page) pageItems(view,page);
    view->updating=1; xuiComboBoxSetSelected(view->navigator,(int)(chapter%XUI_COMBOBOX_ITEM_CAPACITY)); view->updating=0;
    view->pendingChapter=chapter; xgeRenderRequest(); return 1;
}
void xideHelpApplyJump(xide_help_view* view)
{
    if(!view || view->pendingChapter==SIZE_MAX) return;
    size_t chapter=view->pendingChapter; view->pendingChapter=SIZE_MAX;
    if(chapter>=view->chapterCount) return;
    xui_widget widget=view->blocks[view->chapters[chapter].block]->widget;
    xui_rect_t target=xuiWidgetGetWorldRect(widget),content=xuiWidgetGetWorldRect(view->app->helpContent);
    xuiScrollViewSetOffset(view->app->help,0,target.fY-content.fY-12);
}
int xideHelpUiFont(xide_help_view* view) { if(view) view->width=-1; return XUI_OK; }

static xstrview readingString(const char* text) { return (xstrview){text,strlen(text)}; }
static const xvalue* readingGet(const xvalue* value,const char* key) { return value ? xrtValueObjectGet(value,readingString(key)) : NULL; }
static const char* readingText(const xvalue* value) { xstrview text={0}; return xrtValueGetString(value,&text) ? text.Data : ""; }
static int readingPut(xvalue* value,const char* key,xvalue* item) { return item && xrtValueObjectSetNew(value,readingString(key),item); }
static xvalue* readingObject(const xvalue* old) { return xrtValueIs(old,XVALUE_OBJECT) ? xrtValueDeepClone(old) : xrtValueObject(); }
static void blockIdentity(const help_block* block,char identity[100])
{
    uint64_t hash=UINT64_C(1469598103934665603); const unsigned char* text=(const unsigned char*)block->text;
    for(;*text;++text) { hash^=*text; hash*=UINT64_C(1099511628211); }
    snprintf(identity,100,"%d-%d-%zu-%016llx",block->code,block->heading,strlen(block->text),(unsigned long long)hash);
}
static float blockTop(xide_help_view* view,size_t block)
{ return (float)(xuiWidgetGetWorldRect(view->blocks[block]->widget).fY-xuiWidgetGetWorldRect(view->app->helpContent).fY); }
xvalue* xideHelpCaptureReading(xide_help_view* view,const xvalue* previous)
{
    xvalue* root=readingObject(previous); if(!root) return NULL;
    float x=0,y=0; if(view) xuiScrollViewGetOffset(view->app->help,&x,&y);
    xui_scroll_model_t* model=view ? xuiScrollViewGetModel(view->app->help) : NULL;
    float maximum=model ? fmaxf(0,model->fContentHeight-model->tViewportRect.fH) : 0;
    int okay=readingPut(root,"scrollX",xrtValueFloat(x)) && readingPut(root,"scrollY",xrtValueFloat(y)) && readingPut(root,"end",xrtValueBool(maximum>0 && y>=maximum-0.01f));
    xrtValueObjectRemove(root,readingString("block")); xrtValueObjectRemove(root,readingString("chapter"));
    int choice=view ? xuiComboBoxGetSelected(view->navigator) : -1; size_t chapterIndex=view && choice>=0 ? (size_t)view->page*XUI_COMBOBOX_ITEM_CAPACITY+(size_t)choice : SIZE_MAX;
    if(okay && view && chapterIndex<view->chapterCount) {
        const char* title=view->chapters[chapterIndex].title; int occurrence=0;
        for(size_t i=0;i<chapterIndex;++i) if(!strcmp(title,view->chapters[i].title)) ++occurrence;
        xvalue* chapter=readingObject(readingGet(previous,"chapter"));
        okay=chapter && readingPut(chapter,"title",xrtValueString(readingString(title))) && readingPut(chapter,"occurrence",xrtValueInt(occurrence));
        if(okay) okay=readingPut(root,"chapter",chapter); else xrtValueRelease(chapter);
    }
    size_t anchor=SIZE_MAX;
    if(view) for(size_t i=0;i<view->blockCount;++i) { if(blockTop(view,i)>y+0.01f) break; anchor=i; }
    if(okay && anchor!=SIZE_MAX) {
        char identity[100]; blockIdentity(view->blocks[anchor],identity); int occurrence=0;
        for(size_t i=0;i<anchor;++i) { char candidate[100]; blockIdentity(view->blocks[i],candidate); if(!strcmp(identity,candidate)) ++occurrence; }
        double fraction=view->blocks[anchor]->height>0 ? (y-blockTop(view,anchor))/(view->blocks[anchor]->height+10) : 0;
        if(fraction<0) fraction=0;
        if(fraction>1) fraction=1;
        xvalue* block=readingObject(readingGet(previous,"block"));
        okay=block && readingPut(block,"id",xrtValueString(readingString(identity))) && readingPut(block,"occurrence",xrtValueInt(occurrence)) && readingPut(block,"fraction",xrtValueFloat(fraction));
        if(okay) okay=readingPut(root,"block",block); else xrtValueRelease(block);
    }
    if(!okay) { xrtValueRelease(root); return NULL; } return root;
}
void xideHelpRestoreReading(xide_help_view* view,const xvalue* position)
{
    if(!view || !position) return;
    const xvalue* chapter=readingGet(position,"chapter"); const char* title=readingText(readingGet(chapter,"title"));
    int occurrence=(int)xideReadingNumber(readingGet(chapter,"occurrence"),0),seen=0; size_t foundChapter=SIZE_MAX;
    for(size_t i=0;chapter && i<view->chapterCount;++i) if(!strcmp(title,view->chapters[i].title) && seen++==occurrence) { foundChapter=i; xideHelpJump(view,i); view->pendingChapter=SIZE_MAX; break; }
    const xvalue* block=readingGet(position,"block"); const char* identity=readingText(readingGet(block,"id"));
    occurrence=(int)xideReadingNumber(readingGet(block,"occurrence"),0); seen=0; size_t anchor=SIZE_MAX;
    for(size_t i=0;block && i<view->blockCount;++i) { char candidate[100]; blockIdentity(view->blocks[i],candidate); if(!strcmp(identity,candidate) && seen++==occurrence) { anchor=i; break; } }
    double y=xideReadingNumber(readingGet(position,"scrollY"),0);
    if(anchor!=SIZE_MAX) y=blockTop(view,anchor)+xideReadingNumber(readingGet(block,"fraction"),0)*(view->blocks[anchor]->height+10);
    else if(block && foundChapter!=SIZE_MAX) y=blockTop(view,view->chapters[foundChapter].block);
    bool atEnd=false; if(xrtValueGetBool(readingGet(position,"end"),&atEnd) && atEnd) y=10000000;
    xuiScrollViewSetOffset(view->app->help,(float)xideReadingNumber(readingGet(position,"scrollX"),0),(float)y);
}

void xideHelpResetReading(xide_help_view* view)
{
    if(!view) return;
    view->updating=1; xuiComboBoxSetSelected(view->pages,0); if(view->page!=0 && view->chapterCount) pageItems(view,0);
    view->updating=1; xuiComboBoxSetSelected(view->navigator,-1); view->updating=0; view->pendingChapter=SIZE_MAX;
}
