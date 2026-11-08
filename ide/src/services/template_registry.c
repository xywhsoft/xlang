#define _WIN32_WINNT 0x0601
#include "services/template_registry.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static xstrview sv(const char* s) {return (xstrview){s,strlen(s)};}
static xvalue* get(const xvalue* v,const char* k) {return v && xrtValueIs(v,XVALUE_OBJECT) ? xrtValueObjectGet(v,sv(k)):NULL;}
const char* xideTemplateString(const xvalue* v,const char* k,const char* fallback) {
    xvalue* value=get(v,k);xstrview s={0};return value&&xrtValueIs(value,XVALUE_STRING)&&xrtValueGetString(value,&s) && !memchr(s.Data,0,s.Size) ? s.Data:fallback;
}
static char* duplicate(const char* s) {size_t n=strlen(s)+1;char* p=malloc(n);if(p)memcpy(p,s,n);return p;}
static int fail(xide_model_error* e,const char* s) {if(e){e->code=XIDE_MODEL_INVALID;snprintf(e->message,sizeof(e->message),"%s",s);}return 0;}
static int identifier(const char* s) {
    if(!s || !*s || strlen(s)>128)return 0;
    for(const unsigned char* p=(const unsigned char*)s;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||strchr("_.-",*p)))return 0;
    return 1;
}
static int functionName(const char* s) {
    if(!s||!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||*s=='_'))return 0;
    for(const char* p=s;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'))return 0;
    return strlen(s)<=128;
}
static int relative(const char* s) {
    if(!s || !*s || s[0]=='/' || s[0]=='\\' || strchr(s,':') || strchr(s,'\\'))return 0;
    const char* p=s;
    while(*p){const char* end=strchr(p,'/');size_t n=end ? (size_t)(end-p):strlen(p);if(!n || (n==1&&p[0]=='.') || (n==2&&!strncmp(p,"..",2)))return 0;p=end ? end+1:p+n;}
    return 1;
}
void xideTemplateDefinitionClear(xide_template_definition* d) {
    if(!d)return;
    free(d->id);free(d->title);free(d->description);free(d->author);free(d->version);free(d->icon);free(d->category);free(d->root);
    free(d->module);free(d->validate);free(d->create);free(d->language);xrtValueRelease(d->fields);xrtValueRelease(d->files);memset(d,0,sizeof(*d));
}
int xideTemplateDefinitionCopy(xide_template_definition* d,const xide_template_definition* s) {
    memset(d,0,sizeof(*d));
#define COPY(k) do {d->k=duplicate(s->k ? s->k:"");if(!d->k)goto failed;}while(0)
    COPY(id);COPY(title);COPY(description);COPY(author);COPY(version);COPY(icon);COPY(category);COPY(root);COPY(module);COPY(validate);COPY(create);COPY(language);
#undef COPY
    d->project=s->project;d->builtin=s->builtin;d->fields=s->fields ? xrtValueDeepClone(s->fields):xrtValueArray();d->files=s->files ? xrtValueDeepClone(s->files):xrtValueArray();
    if(d->fields && d->files)return 1;
failed:xideTemplateDefinitionClear(d);return 0;
}
void xideTemplateRegistryClear(xide_template_registry* r) {if(r){for(size_t i=0;i<r->count;++i)xideTemplateDefinitionClear(&r->items[i]);free(r->items);memset(r,0,sizeof(*r));}}
static int append(xide_template_registry* r,const xide_template_definition* d) {
    if(r->count>=256)return 0;
    for(size_t i=0;i<r->count;++i)if(!strcmp(r->items[i].id,d->id))return 0;
    xide_template_definition copy={0};if(!xideTemplateDefinitionCopy(&copy,d))return 0;
    void* next=realloc(r->items,(r->count+1)*sizeof(*r->items));if(!next){xideTemplateDefinitionClear(&copy);return 0;}
    r->items=next;r->items[r->count++]=copy;return 1;
}
int xideTemplateFieldActive(const xvalue* field,const char* name,const xvalue* values) {
    xvalue* condition=get(field,name);if(!condition)return 1;
    const char* id=xideTemplateString(condition,"field","");xvalue* actual=get(values,id),*expected=get(condition,"equals");
    return actual && expected && xrtValueEqual(actual,expected);
}
static int fieldsValid(const xvalue* fields,xide_model_error* e) {
    if(!fields || xrtValueType(fields)!=XVALUE_ARRAY || xrtValueCount(fields)>64)return fail(e,"fields 必须为不超过 64 项的数组。");
    for(size_t i=0;i<xrtValueCount(fields);++i){
        xvalue* f=xrtValueArrayGet(fields,i);const char* id=xideTemplateString(f,"id",""),*type=xideTemplateString(f,"type","");
        if(strncmp(id,"custom.",7)||!identifier(id)||!strlen(xideTemplateString(f,"label","")))return fail(e,"自定义字段必须具有唯一 custom.* ID 和标题。");
        const char* types[]={"text","multiline","boolean","choice","integer","directory","file"};int known=0;for(size_t j=0;j<7;++j)if(!strcmp(type,types[j]))known=1;
        if(!known)return fail(e,"模板包含未注册的字段类型。");
        for(size_t j=0;j<i;++j)if(!strcmp(id,xideTemplateString(xrtValueArrayGet(fields,j),"id","")))return fail(e,"模板字段 ID 重复。");
        if(!strcmp(type,"choice")){xvalue* options=get(f,"options");if(!options||xrtValueType(options)!=XVALUE_ARRAY||!xrtValueCount(options)||xrtValueCount(options)>64)return fail(e,"choice 需要 1–64 个选项。");
            for(size_t j=0;j<xrtValueCount(options);++j){xvalue* o=xrtValueArrayGet(options,j);const char* value=xideTemplateString(o,"value","");if(!*value||!*xideTemplateString(o,"label",""))return fail(e,"choice 选项需要 value 和 label。");for(size_t k=0;k<j;++k)if(!strcmp(value,xideTemplateString(xrtValueArrayGet(options,k),"value","")))return fail(e,"choice 的 value 重复。");}}
        xvalue* initial=get(f,"default"),*required=get(f,"required");
        if(required&&!xrtValueIs(required,XVALUE_BOOL))return fail(e,"required 必须是布尔值。");
        if(!strcmp(type,"integer")){if((get(f,"min")&&!xrtValueIs(get(f,"min"),XVALUE_INT))||(get(f,"max")&&!xrtValueIs(get(f,"max"),XVALUE_INT)))return fail(e,"整数范围必须为整数。");}
        if(initial){int okay=!strcmp(type,"boolean") ? xrtValueIs(initial,XVALUE_BOOL):!strcmp(type,"integer") ? xrtValueIs(initial,XVALUE_INT):xrtValueIs(initial,XVALUE_STRING);if(!okay)return fail(e,"字段默认值与类型不一致。");}
        const char* conditions[]={"visibleWhen","enabledWhen"};for(size_t j=0;j<2;++j){xvalue* c=get(f,conditions[j]);if(!c)continue;const char* dep=xideTemplateString(c,"field","");int found=0;const char* common[]={"project.name","project.location","project.subdirectory","project.startup","workspace.mode","workspace.name","workspace.subdirectory"};for(size_t key=0;key<7;++key)if(!strcmp(dep,common[key]))found=1;for(size_t k=0;k<i;++k)if(!strcmp(dep,xideTemplateString(xrtValueArrayGet(fields,k),"id","")))found=1;if(!found||!get(c,"equals"))return fail(e,"字段条件仅允许依赖通用字段或前面已声明的字段，不能形成循环。");}
    }
    return 1;
}
static int validateFields(const xvalue* fields,const xvalue* values,xvalue** result,xide_model_error* e) {
    if(result){*result=NULL;}if(!result||!fieldsValid(fields,e))return 0;xvalue* out=xrtValueObject();if(!out)return fail(e,"无法分配字段结果。");
    for(size_t i=0;i<xrtValueCount(fields);++i){xvalue* f=xrtValueArrayGet(fields,i);if(!xideTemplateFieldActive(f,"visibleWhen",values)||!xideTemplateFieldActive(f,"enabledWhen",values))continue;
        const char* id=xideTemplateString(f,"id",""),*type=xideTemplateString(f,"type","");xvalue* v=get(values,id);if(!v)v=get(f,"default");bool required=false;if(xrtValueIs(get(f,"required"),XVALUE_BOOL))xrtValueGetBool(get(f,"required"),&required);int valid=1;
        if(!v){if(!required)continue;valid=0;}
        else if(!strcmp(type,"boolean")){bool n=false;valid=xrtValueIs(v,XVALUE_BOOL)&&xrtValueGetBool(v,&n);}
        else if(!strcmp(type,"integer")){int64 n=0,min=INT64_MIN,max=INT64_MAX;valid=xrtValueIs(v,XVALUE_INT)&&xrtValueGetInt(v,&n);if(get(f,"min"))valid=valid&&xrtValueIs(get(f,"min"),XVALUE_INT)&&xrtValueGetInt(get(f,"min"),&min);if(get(f,"max"))valid=valid&&xrtValueIs(get(f,"max"),XVALUE_INT)&&xrtValueGetInt(get(f,"max"),&max);valid=valid&&n>=min&&n<=max;}
        else {xstrview s={0};valid=xrtValueIs(v,XVALUE_STRING)&&xrtValueGetString(v,&s)&&!memchr(s.Data,0,s.Size)&&s.Size<=65536&&(!required||s.Size);if(valid&&!strcmp(type,"choice")){valid=0;xvalue* options=get(f,"options");for(size_t j=0;j<xrtValueCount(options);++j)if(!strcmp(s.Data,xideTemplateString(xrtValueArrayGet(options,j),"value","")))valid=1;}}
        if(!valid){if(e){e->code=XIDE_MODEL_INVALID;snprintf(e->message,sizeof(e->message),"%s：值缺失、类型错误或超出允许范围。",xideTemplateString(f,"label",id));}xrtValueRelease(out);return 0;}
        if(!xrtValueObjectSet(out,sv(id),v)){xrtValueRelease(out);return fail(e,"无法保存字段结果。");}
    }
    *result=out;return 1;
}
static int loadOne(xide_template_registry* r,const char* root,xide_model_error* e) {
    char* path=xidePathResolve(root,"template.json");size_t n=0;unsigned char* data=path ? xideIoReadAllLimit(path,1024*1024,&n):NULL;free(path);if(!data)return fail(e,"无法读取 template.json。");
    xvalue* json=xrtJsonParse((xstrview){(const char*)data,n});xrtFree(data);if(!json)return fail(e,"模板 JSON 无效。");
    xide_template_definition d={0};d.id=(char*)xideTemplateString(json,"id","");d.title=(char*)xideTemplateString(json,"title","");d.description=(char*)xideTemplateString(json,"description","");d.author=(char*)xideTemplateString(json,"author","");d.version=(char*)xideTemplateString(json,"version","");d.icon=(char*)xideTemplateString(json,"icon","");d.category=(char*)xideTemplateString(json,"category","");d.root=(char*)root;d.language=(char*)xideTemplateString(json,"language","xlang");
    const char* kind=xideTemplateString(json,"kind","");d.project=!strcmp(kind,"project");xvalue* gen=get(json,"generator"),*form=get(json,"form");d.module=(char*)xideTemplateString(gen,"module","");d.validate=(char*)xideTemplateString(gen,"validate","");d.create=(char*)xideTemplateString(gen,"create","");d.fields=get(form,"fields");d.files=get(json,"files");
    xvalue* empty=xrtValueArray();if(!d.fields)d.fields=empty;if(!d.files)d.files=empty;
    int okay=empty&&!strcmp(xideTemplateString(json,"format",""),"xlang-ide-template-1")&&identifier(d.id)&&strncmp(d.id,"builtin.",8)&&*d.title&&(!strcmp(kind,"project")||!strcmp(kind,"file"))&&(!*d.icon||relative(d.icon))&&fieldsValid(d.fields,e);
    if(okay&&*d.module)okay=relative(d.module)&&functionName(d.create)&&(!*d.validate||functionName(d.validate))&&!strcmp(xideTemplateString(gen,"kind",""),"xlang");
    if(okay)okay=xrtValueType(d.files)==XVALUE_ARRAY&&append(r,&d);
    if(!okay&&e&&!e->message[0]){fail(e,"模板格式、ID、路径或回调声明无效（或 ID 重复）。");}xrtValueRelease(empty);xrtValueRelease(json);return okay;
}
static int compare(const void* aa,const void* bb){const xide_template_definition *a=aa,*b=bb;return strcmp(a->id,b->id);}
static int loadRegistry(xide_template_registry* r,const char* userRoot,xide_model_error* e) {
    memset(r,0,sizeof(*r));if(e)memset(e,0,sizeof(*e));
    const char* icons[]={"blank","console","mixed","dll"};
    for(size_t i=0;i<xideProjectTemplateCount();++i){const xide_project_template* p=xideProjectTemplateAt(i);xide_template_definition d={0};d.id=(char*)p->id;d.title=(char*)p->title;d.description=(char*)p->description;d.author="xlang";d.version="1.0.0";d.icon=(char*)icons[i];d.builtin=1;d.project=1;if(!append(r,&d))goto memory;}
    for(int i=0;i<2;++i){xide_template_definition d={0};d.id=i ? "builtin.file.c":"builtin.file.xlang";d.title=i ? "C 文件（.c）":"xlang 文件（.xl）";d.description=i ? "创建独立的 C 文件（.c）；保存时选择文件名与位置。":"创建独立的 xlang 文件（.xl）；保存时选择文件名与位置。";d.author="xlang";d.version="1.0.0";d.icon=i ? "c-source":"script";d.language=i ? "c":"xlang";d.builtin=1;if(!append(r,&d))goto memory;}
    if(!userRoot||!xidePathExists(userRoot))return 1;
    char* mask=xidePathResolve(userRoot,"*");wchar_t* wide=mask ? xidePathNativeWide(mask):NULL;free(mask);WIN32_FIND_DATAW entry={0};HANDLE find=wide ? FindFirstFileW(wide,&entry):INVALID_HANDLE_VALUE;free(wide);
    if(find==INVALID_HANDLE_VALUE){snprintf(r->issues,sizeof(r->issues),"无法枚举用户模板目录；内置模板仍可使用。");return 1;}
    do{if(!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(entry.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||entry.cFileName[0]==L'.')continue;
        char name[1024];if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,entry.cFileName,-1,name,sizeof(name),NULL,NULL))continue;
        char* dir=xidePathResolve(userRoot,name);xide_model_error issue={0};if(dir&&!loadOne(r,dir,&issue)){size_t used=strlen(r->issues);snprintf(r->issues+used,sizeof(r->issues)-used,"%s%s：%s",used ? "\n":"",name,issue.message);}free(dir);
    }while(FindNextFileW(find,&entry));DWORD last=GetLastError();FindClose(find);if(last!=ERROR_NO_MORE_FILES)return fail(e,"枚举模板目录失败。");
    if(r->count>6){qsort(r->items+6,r->count-6,sizeof(*r->items),compare);}return 1;
memory:xideTemplateRegistryClear(r);return fail(e,"无法加载模板目录。");
}

int xideTemplateFieldsValidate(const xvalue* fields,const xvalue* values,xvalue** result,xide_model_error* error) {
    xerror* previous=xrtTakeError();
    int okay=validateFields(fields,values,result,error);
    xrtErrorFree(xrtTakeError());xrtSetErrorTake(previous);return okay;
}
int xideTemplateRegistryLoad(xide_template_registry* registry,const char* root,xide_model_error* error) {
    xerror* previous=xrtTakeError();
    xideTemplateRegistryClear(registry);
    int okay=loadRegistry(registry,root,error);
    xrtErrorFree(xrtTakeError());xrtSetErrorTake(previous);return okay;
}
