#define _WIN32_WINNT 0x0601
#include "ui/template_view.h"
#include "ui/home_view.h"
#include "ui/dialog.h"
#include "ui/message_box.h"
#include "ui/file_dialog.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/reading_state.h"
#include "services/template_generation.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include "app/actions.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define TRY(call) do {int result_=(call);if(result_!=XUI_OK){fprintf(stderr,"template form line %d: %s = %d\n",__LINE__,#call,result_);return result_;}}while(0)
typedef struct template_field {
    xide_app* app;xvalue* description;
    const char *id,*type;xui_widget label,widget,browse,error;int multiline;
} template_field;
struct xide_template_view {
    xui_widget window,controls[XIDE_TEMPLATE_CONTROL_COUNT],message,title;
    xide_template_definition definition;template_field fields[80];size_t fieldCount;
    xvalue *descriptions,*baseline;xide_template_job* job;
    int syncing,workspaceNameEdited,completed,jobMode,jobStartup;
    char messageText[512];
};
static xstrview sv(const char* s){return (xstrview){s,strlen(s)};}
static xvalue* get(const xvalue* v,const char* key){return v&&xrtValueIs(v,XVALUE_OBJECT) ? xrtValueObjectGet(v,sv(key)):NULL;}
static int put(xvalue* v,const char* key,const char* value){return xrtValueObjectSetNew(v,sv(key),xrtValueString(sv(value ? value:"")));}
static int fail(xide_model_error* e,int code,const char* text){if(e){e->code=code;snprintf(e->message,sizeof(e->message),"%s",text);}return 0;}
static void message(xide_template_view* v,const char* text){snprintf(v->messageText,sizeof(v->messageText),"%s",text);if(v->message)xuiLabelSetText(v->message,v->messageText);xgeRenderRequest();}
static xvalue* baseline(xide_app* a){
    xvalue *root=xrtValueObject(),*projects=xrtValueArray(),*workspace=a->workspaceModel.source ? xideWorkspaceExport(&a->workspaceModel,NULL):NULL;
    int okay=root&&projects&&put(root,"manifest",a->workspaceModel.manifest)&&put(root,"name",a->workspaceModel.name)&&xrtValueObjectSetNew(root,sv("dirty"),xrtValueBool(a->workspaceModel.dirty!=0));
    if(workspace)okay=okay&&xrtValueObjectSet(root,sv("workspace"),workspace);
    for(size_t i=0;okay&&i<a->workspaceModel.projectCount;++i){xide_project* p=&a->workspaceModel.projects[i];xvalue* item=xrtValueObject(),*json=p->available ? xideProjectExport(p,NULL):p->source ? xrtValueDeepClone(p->source):xrtValueObject();okay=item&&json&&xrtValueObjectSet(item,sv("data"),json)&&xrtValueObjectSetNew(item,sv("dirty"),xrtValueBool(p->dirty!=0))&&xrtValueArrayAppendNew(projects,item);xrtValueRelease(json);if(!okay)xrtValueRelease(item);}
    if(okay){okay=xrtValueObjectSet(root,sv("projects"),projects);}xrtValueRelease(projects);xrtValueRelease(workspace);if(!okay){xrtValueRelease(root);return NULL;}return root;
}
static template_field* field(xide_template_view* v,const char* id){for(size_t i=0;i<v->fieldCount;++i)if(!strcmp(v->fields[i].id,id))return &v->fields[i];return NULL;}
xui_widget xideTemplateViewField(xide_app* a,const char* id){template_field* f=a&&a->templateView ? field(a->templateView,id):NULL;return f ? f->widget:NULL;}
static const char* fieldText(template_field* f){return !f ? "":f->multiline ? xuiTextEditGetText(f->widget):xuiEditGetText(f->widget);}
static void setFieldText(template_field* f,const char* value){if(f){if(f->multiline)xuiTextEditSetText(f->widget,value);else xuiEditSetText(f->widget,value);}}
static xvalue* readFields(xide_template_view* v,int showErrors){
    xvalue* values=xrtValueObject();if(!values)return NULL;
    for(size_t i=0;i<v->fieldCount;++i){template_field* f=&v->fields[i];xvalue* value=NULL;const char* error="";
        if(!strcmp(f->type,"boolean"))value=xrtValueBool(xuiCheckBoxGetChecked(f->widget)!=0);
        else if(!strcmp(f->type,"choice")){int n=xuiComboBoxGetSelected(f->widget);xvalue* item=n>=0 ? xrtValueArrayGet(get(f->description,"options"),(size_t)n):NULL;value=xrtValueString(sv(xideTemplateString(item,"value","")));}
        else if(!strcmp(f->type,"integer")){const char* text=fieldText(f);char* end=NULL;errno=0;long long n=strtoll(text,&end,10);if(!*text||errno||!end||*end)error="请输入有效整数。";else value=xrtValueInt(n);}
        else value=xrtValueString(sv(fieldText(f)));
        if(value&&showErrors){bool required=false;if(xrtValueIs(get(f->description,"required"),XVALUE_BOOL))xrtValueGetBool(get(f->description,"required"),&required);xstrview text={0};if(required&&xrtValueIs(value,XVALUE_STRING)&&xrtValueGetString(value,&text)&&!text.Size)error="此项不能为空。";if(xrtValueIs(value,XVALUE_INT)){int64 n=0,min=INT64_MIN,max=INT64_MAX;xrtValueGetInt(value,&n);if(xrtValueIs(get(f->description,"min"),XVALUE_INT))xrtValueGetInt(get(f->description,"min"),&min);if(xrtValueIs(get(f->description,"max"),XVALUE_INT))xrtValueGetInt(get(f->description,"max"),&max);if(n<min||n>max)error="数值超出允许范围。";}}
        if(value){if(!xrtValueObjectSetNew(values,sv(f->id),value)){xrtValueRelease(values);return NULL;}}
        if(*error&&showErrors){xuiLabelSetText(f->error,error);xuiWidgetSetVisible(f->error,1);}
        if(showErrors&&!*error)xuiWidgetSetVisible(f->error,0);
    }return values;
}
static int truth(const xvalue* value){bool result=false;if(xrtValueIs(value,XVALUE_BOOL))xrtValueGetBool(value,&result);return result;}
static void choosePath(xide_app* a,const char* path,void* user){template_field* f=user;if(path&&f->app==a)setFieldText(f,path);xideTemplateViewRefresh(a);}
static void browseField(xui_widget w,void* user){(void)w;template_field* f=user;int mode=!strcmp(f->type,"directory") ? XUI_FILE_DIALOG_MODE_SELECT_FOLDER:XUI_FILE_DIALOG_MODE_OPEN_FILE;xideFileDialogChoose(f->app,f->app->templateView->window,mode,xideTemplateString(f->description,"label","选择"),NULL,fieldText(f),NULL,choosePath,f);}
static int changed(xui_widget w,const xui_edit_event_t* event,void* user){xide_app* a=user;xide_template_view* v=a->templateView;if(event->iType==XUI_EDIT_EVENT_TEXT_CHANGED&&!v->syncing){if(w==v->controls[XIDE_TEMPLATE_WORKSPACE_NAME])v->workspaceNameEdited=1;if(w==v->controls[XIDE_TEMPLATE_NAME]&&!v->workspaceNameEdited){v->syncing=1;setFieldText(field(v,"workspace.name"),xuiEditGetText(w));v->syncing=0;}xideTemplateViewRefresh(a);}return XUI_OK;}
static void textChanged(xui_widget w,const char* text,void* user){(void)w;(void)text;xideTemplateViewRefresh(user);}
static void checked(xui_widget w,int state,void* user){(void)w;(void)state;xideTemplateViewRefresh(user);}
static void selected(xui_widget w,int index,int value,void* user){(void)w;(void)index;(void)value;xideTemplateViewRefresh(user);}
static int addField(xide_app* a,xvalue* description){
    xide_template_view* v=a->templateView;if(v->fieldCount>=80)return XUI_ERROR_INVALID_ARGUMENT;
    template_field* f=&v->fields[v->fieldCount++];memset(f,0,sizeof(*f));f->app=a;f->description=description;f->id=xideTemplateString(description,"id","");f->type=xideTemplateString(description,"type","");f->multiline=!strcmp(f->type,"multiline");
    xui_label_desc_t label={0};label.iSize=sizeof(label);label.pFont=a->uiFont;label.sText=xideTemplateString(description,"label",f->id);label.iWrapMode=XUI_TEXT_WRAP_WORD;if(strcmp(f->type,"boolean"))TRY(xuiLabelCreate(a->context,&f->label,&label));
    if(!strcmp(f->type,"boolean")){xui_checkbox_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=a->uiFont;desc.fIndicatorSize=18;desc.bUseBuiltinAtlas=1;desc.sText=xideTemplateString(description,"label",f->id);desc.iTextFlags=XUI_TEXT_ALIGN_LEFT|XUI_TEXT_ALIGN_MIDDLE;TRY(xuiCheckBoxCreate(a->context,&f->widget,&desc));TRY(xuiCheckBoxSetChange(f->widget,checked,a));xuiCheckBoxSetChecked(f->widget,truth(get(description,"default")));}
    else if(!strcmp(f->type,"choice")){const char* titles[64];xvalue* options=get(description,"options");size_t count=xrtValueCount(options);if(count>64)return XUI_ERROR_INVALID_ARGUMENT;for(size_t i=0;i<count;++i)titles[i]=xideTemplateString(xrtValueArrayGet(options,i),"label","");xui_combobox_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=a->uiFont;desc.arrItems=titles;desc.iItemCount=(int)count;TRY(xuiComboBoxCreate(a->context,&f->widget,&desc));TRY(xuiComboBoxSetSelect(f->widget,selected,a));int pick=0;const char* def=xideTemplateString(description,"default","");for(size_t i=0;i<count;++i)if(!strcmp(def,xideTemplateString(xrtValueArrayGet(options,i),"value","")))pick=(int)i;xuiComboBoxSetSelected(f->widget,pick);}
    else if(f->multiline){xui_text_edit_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=a->uiFont;desc.bWordWrap=1;desc.iMaxLength=65536;const char* initial=xideTemplateString(description,"default","");desc.sText=*initial ? initial:NULL;desc.sPlaceholder=xideTemplateString(description,"label","");TRY(xuiTextEditCreate(a->context,&f->widget,&desc));TRY(xuiTextEditSetChange(f->widget,textChanged,a));}
    else {xui_input_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=a->uiFont;TRY(xuiInputCreate(a->context,&f->widget,&desc));char number[64];const char* def=xideTemplateString(description,"default","");if(!strcmp(f->type,"integer")){int64 n=0;if(xrtValueIs(get(description,"default"),XVALUE_INT))xrtValueGetInt(get(description,"default"),&n);snprintf(number,sizeof(number),"%lld",(long long)n);def=number;}xuiEditSetText(f->widget,def);TRY(xuiEditSetEvent(f->widget,changed,a));}
    if(!strcmp(f->type,"directory")||!strcmp(f->type,"file")){xui_button_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=a->uiFont;desc.sText="浏览…";TRY(xuiButtonCreate(a->context,&f->browse,&desc));TRY(xuiButtonSetClick(f->browse,browseField,f));}
    TRY(xuiWidgetSetAccessibleName(f->widget,xideTemplateString(description,"label",f->id)));if(f->multiline){TRY(xideDialogFormBlock(v->window,f->label,0,0));TRY(xideDialogFormBlock(v->window,f->widget,80,0));}else if(!strcmp(f->type,"boolean")){const char* after=xideTemplateString(description,"inlineAfter","");template_field* host=*after ? field(v,after):NULL;if(*after){if(!host)return XUI_ERROR_INVALID_ARGUMENT;TRY(xideDialogFormFieldOption(v->window,host->widget,f->widget));}else{TRY(xideDialogFormRow(v->window,&f->widget,1,32,0));}}else{TRY(xideDialogFormNamedField(v->window,f->label,f->widget,f->browse,NULL));}
    if(f->multiline){TRY(xuiWidgetSetPreferredSize(f->widget,(xui_vec2_t){0,80}));}
    label.sText="";TRY(xuiLabelCreate(a->context,&f->error,&label));TRY(xideDialogFormBlock(v->window,f->error,0,0));TRY(xuiWidgetSetVisible(f->error,0));
    const char* help=xideTemplateString(description,"description","");if(*help)TRY(xideDialogFormText(v->window,help));return XUI_OK;
}
static const char* coreFields="["
 "{\"id\":\"project.name\",\"type\":\"text\",\"label\":\"工程名称\",\"default\":\"MyProject\"},"
 "{\"id\":\"project.location\",\"type\":\"directory\",\"label\":\"存放位置\"},"
 "{\"id\":\"project.subdirectory\",\"type\":\"boolean\",\"label\":\"为工程创建子目录\",\"inlineAfter\":\"project.name\",\"default\":true},"
 "{\"id\":\"workspace.mode\",\"type\":\"choice\",\"label\":\"工作空间\",\"options\":[{\"value\":\"none\",\"label\":\"不关联工作空间\"},{\"value\":\"create\",\"label\":\"创建新的工作空间\"},{\"value\":\"current\",\"label\":\"加入当前工作空间\"}]},"
 "{\"id\":\"workspace.name\",\"type\":\"text\",\"label\":\"工作空间名称\",\"default\":\"MyProject\",\"visibleWhen\":{\"field\":\"workspace.mode\",\"equals\":\"create\"}},"
 "{\"id\":\"workspace.subdirectory\",\"type\":\"boolean\",\"label\":\"为工作空间创建子目录\",\"inlineAfter\":\"workspace.name\",\"default\":true,\"visibleWhen\":{\"field\":\"workspace.mode\",\"equals\":\"create\"}},"
 "{\"id\":\"project.startup\",\"type\":\"boolean\",\"label\":\"设为启动工程\",\"default\":true}]";
static const char* createTitle(const xide_template_view* v){return v->definition.kind==XIDE_TEMPLATE_WORKSPACE ? "创建工作空间":v->definition.kind==XIDE_TEMPLATE_PROJECT ? "创建工程":"创建文件";}
static const char* workspaceFields="["
 "{\"id\":\"workspace.name\",\"type\":\"text\",\"label\":\"工作空间名称\",\"default\":\"MyWorkspace\"},"
 "{\"id\":\"workspace.location\",\"type\":\"directory\",\"label\":\"存放位置\"},"
 "{\"id\":\"workspace.subdirectory\",\"type\":\"boolean\",\"label\":\"为工作空间创建子目录\",\"inlineAfter\":\"workspace.name\",\"default\":true}]";
static void cancelForm(xui_widget w,void* user){(void)w;xide_app* a=user;xide_template_view* v=a->templateView;if(v->job){xideTemplateJobCancel(v->job);message(v,"正在取消创建，请稍候…");return;}xideDialogSetOpen(v->window,0);xideActionsRefresh(a);xgeRenderRequest();}
static void createClick(xui_widget w,void* user){(void)w;xideActionExecute(user,XIDE_ACTION_TEMPLATE_CREATE);}
static int buildForm(xide_app* a){
    xide_template_view* v=a->templateView;v->syncing=1;TRY(xideDialogCreate(a,&v->window,createTitle(v),720,560,14));TRY(xuiWidgetAddChild(a->root,v->window));TRY(xideDialogSetCancel(v->window,cancelForm,a));TRY(xideDialogFormBegin(v->window,600,420));TRY(xideDialogFormText(v->window,v->definition.title));
    v->descriptions=v->definition.kind==XIDE_TEMPLATE_WORKSPACE ? xrtJsonParse(sv(workspaceFields)):(v->definition.kind==XIDE_TEMPLATE_PROJECT) ? xrtJsonParse(sv(coreFields)):xrtValueArray();if(!v->descriptions)return XUI_ERROR_OUT_OF_MEMORY;
    for(size_t i=0;i<xrtValueCount(v->definition.fields);++i)if(!xrtValueArrayAppend(v->descriptions,xrtValueArrayGet(v->definition.fields,i)))return XUI_ERROR_OUT_OF_MEMORY;
    for(size_t i=0;i<xrtValueCount(v->descriptions);++i)TRY(addField(a,xrtValueArrayGet(v->descriptions,i)));
    const char* ids[]={"project.name","project.location","project.startup","project.subdirectory","workspace.mode","workspace.name","workspace.subdirectory"};const int controls[]={XIDE_TEMPLATE_NAME,XIDE_TEMPLATE_PARENT,XIDE_TEMPLATE_STARTUP,XIDE_TEMPLATE_PROJECT_DIRECTORY,XIDE_TEMPLATE_WORKSPACE_MODE,XIDE_TEMPLATE_WORKSPACE_NAME,XIDE_TEMPLATE_WORKSPACE_DIRECTORY};for(size_t i=0;i<7;++i){template_field* f=field(v,ids[i]);if(f)v->controls[controls[i]]=f->widget;}
    template_field* parent=field(v,v->definition.kind==XIDE_TEMPLATE_WORKSPACE ? "workspace.location":"project.location");if(parent){v->controls[XIDE_TEMPLATE_BROWSE]=parent->browse;char* path=a->workspaceModel.manifest ? xidePathDirectory(a->workspaceModel.manifest):xidePathResolve(NULL,".");setFieldText(parent,path ? path:"");free(path);}
    if((v->definition.kind==XIDE_TEMPLATE_PROJECT)){int enabled[]={1,1,(a->workspaceModel.source!=NULL && !a->projectSessionPath)};xuiComboBoxSetEnabledItems(v->controls[XIDE_TEMPLATE_WORKSPACE_MODE],enabled,3);xuiComboBoxSetSelected(v->controls[XIDE_TEMPLATE_WORKSPACE_MODE],(a->workspaceModel.source && !a->projectSessionPath) ? 2:0);xuiCheckBoxSetChecked(v->controls[XIDE_TEMPLATE_STARTUP],!a->workspaceModel.startupProject||!*a->workspaceModel.startupProject);}
    xui_label_desc_t label={0};label.iSize=sizeof(label);label.pFont=a->uiFont;label.iWrapMode=XUI_TEXT_WRAP_WORD;TRY(xuiLabelCreate(a->context,&v->controls[XIDE_TEMPLATE_DESTINATION],&label));TRY(xideDialogFormBlock(v->window,v->controls[XIDE_TEMPLATE_DESTINATION],0,0));TRY(xuiLabelCreate(a->context,&v->message,&label));TRY(xideDialogFormBlock(v->window,v->message,0,0));
    const char* titles[]={"关闭",createTitle(v)};int idsButtons[]={XIDE_TEMPLATE_CANCEL,XIDE_TEMPLATE_CREATE};for(int i=0;i<2;++i){xui_button_desc_t b={0};b.iSize=sizeof(b);b.pFont=a->uiFont;b.sText=titles[i];TRY(xuiButtonCreate(a->context,&v->controls[idsButtons[i]],&b));TRY(xuiButtonSetClick(v->controls[idsButtons[i]],i ? createClick:cancelForm,a));}
    xui_widget buttons[]={v->controls[XIDE_TEMPLATE_CANCEL],v->controls[XIDE_TEMPLATE_CREATE]};TRY(xideDialogFormActions(v->window,buttons,2));TRY(xideDialogFormEnd(v->window));TRY(xuiWidgetSetDefaultAction(v->window,createClick,a));TRY(xideUiFontBindWidget(a,v->window));v->syncing=0;return XUI_OK;
}
int xideTemplateViewRefresh(xide_app* a){
    xide_template_view* v=a ? a->templateView:NULL;if(!v||!v->window||v->syncing)return v!=NULL;v->syncing=1;xvalue* values=readFields(v,0);int okay=values!=NULL;
    for(size_t i=0;i<v->fieldCount;++i){template_field* f=&v->fields[i];int visible=xideTemplateFieldActive(f->description,"visibleWhen",values);if(!strcmp(f->id,"project.startup")&&!strcmp(xideTemplateString(values,"workspace.mode","none"),"none"))visible=0;if(f->label)xuiWidgetSetVisible(f->label,visible);xuiWidgetSetVisible(f->widget,visible);if(f->browse)xuiWidgetSetVisible(f->browse,visible);xuiWidgetSetEnabled(f->widget,!v->job&&xideTemplateFieldActive(f->description,"enabledWhen",values));if(f->browse)xuiWidgetSetEnabled(f->browse,!v->job);if(!visible)xuiWidgetSetVisible(f->error,0);}
    if((v->definition.kind==XIDE_TEMPLATE_PROJECT)){const char* name=xideTemplateString(values,"project.name",""),*parent=xideTemplateString(values,"project.location",""),*workspaceName=xideTemplateString(values,"workspace.name",""),*mode=xideTemplateString(values,"workspace.mode","");char* base=xidePathResolve(NULL,parent);if(!strcmp(mode,"create")&&truth(get(values,"workspace.subdirectory"))){char* next=base ? xidePathResolve(base,workspaceName):NULL;free(base);base=next;}char* project=base ? (truth(get(values,"project.subdirectory")) ? xidePathResolve(base,name):xidePathResolve(NULL,base)):NULL;char preview[1536];snprintf(preview,sizeof(preview),"工程位置：%s%s%s%s",project ? project:"",!strcmp(mode,"create") ? "\n工作空间位置：":"",!strcmp(mode,"create")&&base ? base:"",!strcmp(mode,"current") ? "\n将加入当前工作空间；工作空间修改按正常保存流程处理。":"");xuiLabelSetText(v->controls[XIDE_TEMPLATE_DESTINATION],preview);free(project);free(base);okay=okay&&xideTemplateNameValid(name)&&*parent&&(!strcmp(mode,"none")||!strcmp(mode,"current")||xideTemplateNameValid(workspaceName));}
    if(v->definition.kind==XIDE_TEMPLATE_WORKSPACE){const char* name=xideTemplateString(values,"workspace.name",""),*location=xideTemplateString(values,"workspace.location","");char* dir=truth(get(values,"workspace.subdirectory")) ? xidePathResolve(location,name):xidePathResolve(NULL,location);char text[1536];snprintf(text,sizeof(text),"工作空间位置：%s/%s.xlw",dir ? dir:"",name);xuiLabelSetText(v->controls[XIDE_TEMPLATE_DESTINATION],text);free(dir);okay=okay&&xideTemplateNameValid(name)&&*location;}
    xuiWidgetSetEnabled(v->controls[XIDE_TEMPLATE_CREATE],okay&&!v->job&&!v->completed);xuiButtonSetText(v->controls[XIDE_TEMPLATE_CANCEL],v->job ? "取消创建":"关闭");xrtValueRelease(values);v->syncing=0;xgeRenderRequest();return okay;
}
int xideTemplateViewApply(xide_app* a,xide_model_error* e){
    xide_template_view* v=a->templateView;if(!v||!xideTemplateViewIsOpen(a)||v->job||v->completed)return fail(e,XIDE_MODEL_INVALID,"创建窗口不可用或已有创建任务。");
    xvalue* values=readFields(v,1);if(!values)return fail(e,XIDE_MODEL_MEMORY,"无法读取表单。");
    const char* mode=xideTemplateString(values,"workspace.mode","none");int selectedMode=v->definition.kind==XIDE_TEMPLATE_WORKSPACE ? 1:!strcmp(mode,"current") ? 2:!strcmp(mode,"create") ? 1:0;
    if(v->definition.kind!=XIDE_TEMPLATE_FILE&&selectedMode!=2&&!xideWorkspaceQuitRequest(a)){xrtValueRelease(values);return 0;}
    xvalue* state=baseline(a);if(!state){xrtValueRelease(values);return fail(e,XIDE_MODEL_MEMORY,"无法保存创建基线。");}xrtValueRelease(v->baseline);v->baseline=state;
    if(!xideTemplateViewRefresh(a)){xrtValueRelease(values);return fail(e,XIDE_MODEL_INVALID,"请检查工程名称、已有存放位置和工作空间名称。");}
    char* settings=a->preferences.path ? xidePathDirectory(a->preferences.path):NULL;char* scratch=settings ? xidePathResolve(settings,"template-jobs"):NULL;free(settings);
    xide_template_request q={0};q.definition=&v->definition;q.values=values;q.name=xideTemplateString(values,"project.name","");q.location=xideTemplateString(values,v->definition.kind==XIDE_TEMPLATE_WORKSPACE ? "workspace.location":"project.location",".");q.workspaceName=selectedMode==2 ? a->workspaceModel.name:selectedMode==1 ? xideTemplateString(values,"workspace.name",""):"";q.compiler=a->compilerPath;q.scratch=scratch;q.projectDirectory=truth(get(values,"project.subdirectory"));q.workspaceMode=selectedMode;q.workspaceDirectory=truth(get(values,"workspace.subdirectory"));
    v->job=xideTemplateJobStart(&q,e);v->jobMode=selectedMode;v->jobStartup=selectedMode!=0&&truth(get(values,"project.startup"));free(scratch);xrtValueRelease(values);
    if(!v->job){return 0;}message(v,"正在后台生成，请稍候…");xideTemplateViewRefresh(a);xgeRenderRequestAfter(30);return 1;
}
int xideTemplateViewAction(xide_app* a,int id){(void)id;xide_model_error error={0};if(!xideTemplateViewApply(a,&error)&&*error.message)message(a->templateView,error.message);return 1;}
static void poll(xide_app* a){
    xide_template_view* v=a->templateView;if(!v||!v->job)return;if(!xideTemplateJobDone(v->job)){xgeRenderRequestAfter(30);return;}
    xide_model_error error={0};int okay=xideTemplateJobSucceeded(v->job);v->completed=*xideTemplateJobProject(v->job)!=0||*xideTemplateJobWorkspace(v->job)!=0;
    if(okay&&v->definition.kind!=XIDE_TEMPLATE_FILE){xvalue* now=baseline(a);okay=now&&v->baseline&&xrtValueEqual(now,v->baseline);xrtValueRelease(now);
        if(!okay)snprintf(error.message,sizeof(error.message),"工程已生成，但当前工作空间已变化，未自动关联：%s",xideTemplateJobProject(v->job));
        else if(v->jobMode==2){okay=xideWorkspaceAddProject(&a->workspaceModel,xideTemplateJobProject(v->job),NULL,&error);if(okay){xide_project* p=&a->workspaceModel.projects[a->workspaceModel.projectCount-1];if(v->jobStartup)okay=xideWorkspaceSetStartup(&a->workspaceModel,p->id,&error);xideWorkspaceViewRefresh(a);xideWorkspaceViewSelectProject(a,p->id);}}
        else {a->sessionSwitchConfirmed=1;const char* workspace=xideTemplateJobWorkspace(v->job);okay=xideWorkspaceOpen(a,*workspace ? workspace:xideTemplateJobProject(v->job),!*workspace,&error);a->sessionSwitchConfirmed=0;}
        if(okay){xide_project* project=xideWorkspaceFindProject(&a->workspaceModel,a->selectedProject);if(project)xideEditorOpen(a,project->entry,&error);const char* artifact=xideTemplateJobWorkspace(v->job);xideHomeRemember(a,*artifact ? artifact:xideTemplateJobProject(v->job));}
    }else if(okay){xide_document* doc=xideDocumentNew(&a->documents,v->definition.language,&error);okay=doc&&xideEditorShow(a,doc)==XUI_OK;if(okay)okay=xuiCodeEditSetText(doc->editor,xideTemplateJobText(v->job))==XUI_OK;if(!okay&&doc)xideEditorClose(a,doc,1,NULL);}
    if(okay){message(v,"创建完成。");xideDialogSetOpen(v->window,0);}else message(v,*error.message ? error.message:xideTemplateJobError(v->job));
    const xvalue* errors=xideTemplateJobFieldErrors(v->job);if(errors){for(size_t i=0;i<v->fieldCount;++i){const char* text=xideTemplateString(errors,v->fields[i].id,"");if(*text){xuiLabelSetText(v->fields[i].error,text);xuiWidgetSetVisible(v->fields[i].error,1);}}}
    xideTemplateJobDestroy(v->job);v->job=NULL;xideTemplateViewRefresh(a);xideActionsRefresh(a);xgeRenderRequest();
}
int xideTemplateViewOpenDefinition(xide_app* a,const xide_template_definition* definition){
    xide_template_view* v=a->templateView;if(!v||v->job||xideWorkspaceViewHasDraft(a)){fprintf(stderr,"template open blocked: view=%d job=%d draft=%d\n",v!=NULL,v&&v->job,xideWorkspaceViewHasDraft(a));return 0;}
    if(!v->window||!v->definition.id||strcmp(v->definition.id,definition->id)){if(v->window)xideDialogDestroy(v->window);v->window=NULL;v->message=NULL;v->fieldCount=0;memset(v->controls,0,sizeof(v->controls));xrtValueRelease(v->descriptions);v->descriptions=NULL;xideTemplateDefinitionClear(&v->definition);int copied=xideTemplateDefinitionCopy(&v->definition,definition);int built=copied ? buildForm(a):XUI_ERROR_OUT_OF_MEMORY;if(built!=XUI_OK){fprintf(stderr,"template form build failed: %d fields=%zu\n",built,v->fieldCount);if(v->window)xideDialogDestroy(v->window);v->window=NULL;return 0;}}
    if(v->completed){message(v,"这个工程已生成；请更换工程名称后创建另一个工程。");v->completed=0;}
    if((v->definition.kind==XIDE_TEMPLATE_PROJECT)){int enabled[]={1,1,(a->workspaceModel.source!=NULL && !a->projectSessionPath)};v->syncing=1;xuiComboBoxSetEnabledItems(v->controls[XIDE_TEMPLATE_WORKSPACE_MODE],enabled,3);xuiComboBoxSetSelected(v->controls[XIDE_TEMPLATE_WORKSPACE_MODE],(a->workspaceModel.source && !a->projectSessionPath) ? 2:0);v->syncing=0;}
    xideTemplateViewRefresh(a);xideTemplateViewLayout(a);xideDialogSetScroll(v->window,0,0);xideDialogSetOpen(v->window,1);xuiSetFocusWidget(a->context,v->fieldCount ? v->fields[0].widget:v->controls[XIDE_TEMPLATE_CREATE]);xideActionsRefresh(a);return 1;
}
int xideTemplateViewCreate(xide_app* a){a->templateView=calloc(1,sizeof(*a->templateView));return a->templateView ? XUI_OK:XUI_ERROR_OUT_OF_MEMORY;}
int xideTemplateViewOpen(xide_app* a){return xideActionExecute(a,XIDE_ACTION_NEW)==XIDE_ACTION_DISPATCHED;}
int xideTemplateViewIsOpen(xide_app* a){return a&&a->templateView&&xideDialogIsOpen(a->templateView->window);}
void xideTemplateViewLayout(xide_app* a){poll(a);if(a->templateView&&a->templateView->window)xideDialogSetMaxSize(a->templateView->window,a->viewportWidth-32,a->viewportHeight-32);}
int xideTemplateViewCodeFont(xide_app* a){(void)a;return XUI_OK;}
void xideTemplateViewDestroy(xide_app* a){xide_template_view* v=a->templateView;if(v){xideTemplateJobDestroy(v->job);xideTemplateDefinitionClear(&v->definition);xrtValueRelease(v->descriptions);xrtValueRelease(v->baseline);free(v);a->templateView=NULL;}}
xui_widget xideTemplateViewWindow(xide_app* a){return a&&a->templateView ? a->templateView->window:NULL;}
xui_widget xideTemplateViewControl(xide_app* a,int id){return a&&a->templateView&&id>=0&&id<XIDE_TEMPLATE_CONTROL_COUNT ? a->templateView->controls[id]:NULL;}
const char* xideTemplateViewMessage(xide_app* a){return a&&a->templateView ? a->templateView->messageText:"";}
