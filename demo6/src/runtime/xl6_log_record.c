#include "src/runtime/xl6_log_record.h"
#include <string.h>
#include <limits.h>

static bool record_view(xstrview text)
{ if (text.Size && !text.Data) { xrtSetErrorKind(XERR_ARGUMENT); return false; } return true; }
static bool record_copy(xstrview source, xstrview* target)
{
    if (!record_view(source)) return false;
    *target = (xstrview){xrtStrDupN(source.Data,source.Size),source.Size};
    return target->Data != NULL;
}
static bool record_size(xstrview* view, int64_t size)
{
    if (size < -1) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (size >= 0 && (uint64_t)size < view->Size) view->Size = (size_t)size;
    return record_view(*view);
}
static bool record_handle(void* handle)
{ if (!handle) xrtSetErrorKind(XERR_ARGUMENT); return handle != NULL; }
static void field_clear(xlogfield* field)
{
    xrtFree((void*)field->Name.Data);
    if (field->Type == XLOG_FIELD_STRING) xrtFree((void*)field->Value.String.Data);
    if (field->Type == XLOG_FIELD_ERROR) xrtErrorFree((xerror*)field->Value.Error);
    memset(field,0,sizeof(*field));
}
void xl6LogRecordDestroyV1(void* handle)
{
    xl6_log_record* owned=handle;
    if (!owned) return;
    for (size_t i=0;i<owned->record.FieldCount;++i) field_clear(&owned->fields[i]);
    xrtFree(owned->fields);
    xrtFree((void*)owned->record.Logger.Data);
    xrtFree((void*)owned->record.Message.Data);
    xrtFree((void*)owned->record.File.Data);
    xrtFree((void*)owned->record.Function.Data);
    xrtFree(owned);
}
void* xl6LogRecordCreateV1(int64_t level,xstrview message,int64_t size)
{
    xl6_log_record* owned;
    if (level<XLOG_TRACE || level>XLOG_FATAL) { xrtSetErrorKind(XERR_RANGE); return NULL; }
    if (!record_size(&message,size)) return NULL;
    owned=xrtMalloc(sizeof(*owned));
    if (!owned) return NULL;
    memset(owned,0,sizeof(*owned));
    owned->record.Level=(xloglevel)level;
    owned->record.Time=xrtNow();
    owned->record.ThreadId=xrtThreadCurrentId();
    if (!record_copy(message,&owned->record.Message)) { xl6LogRecordDestroyV1(owned); return NULL; }
    return owned;
}
static bool record_add(xl6_log_record* owned,const xlogfield* field)
{
    xlogfield copy=*field;
    copy.Name=(xstrview){0};
    if (copy.Type==XLOG_FIELD_STRING) copy.Value.String=(xstrview){0};
    if (copy.Type==XLOG_FIELD_ERROR) copy.Value.Error=NULL;
    if (!record_copy(field->Name,&copy.Name)) return false;
    if (copy.Type==XLOG_FIELD_STRING && !record_copy(field->Value.String,&copy.Value.String)) { field_clear(&copy); return false; }
    if (copy.Type==XLOG_FIELD_ERROR) copy.Value.Error=xrtErrorRef(field->Value.Error);
    if (owned->record.FieldCount==owned->capacity) {
        size_t next=owned->capacity ? owned->capacity*2 : 8;
        xlogfield* fields;
        if (next<owned->capacity || next>SIZE_MAX/sizeof(*fields)) { field_clear(&copy); xrtSetErrorKind(XERR_RANGE); return false; }
        fields=xrtRealloc(owned->fields,next*sizeof(*fields));
        if (!fields) { field_clear(&copy); return false; }
        owned->fields=fields; owned->capacity=next; owned->record.Fields=fields;
    }
    owned->fields[owned->record.FieldCount++]=copy;
    return true;
}
void* xl6LogRecordCloneV1(void* handle)
{
    const xl6_log_record* source=handle;
    xl6_log_record* owned;
    if (!record_handle(handle)) return NULL;
    owned=xl6LogRecordCreateV1(source->record.Level,source->record.Message,-1);
    if (!owned) return NULL;
    owned->record.Time=source->record.Time; owned->record.ThreadId=source->record.ThreadId;
    owned->record.Line=source->record.Line;
    if (!record_copy(source->record.Logger,&owned->record.Logger) ||
        !record_copy(source->record.File,&owned->record.File) ||
        !record_copy(source->record.Function,&owned->record.Function)) goto fail;
    for (size_t i=0;i<source->record.FieldCount;++i)
        if (!record_add(owned,&source->fields[i])) goto fail;
    return owned;
fail: xl6LogRecordDestroyV1(owned); return NULL;
}
void* xl6LogRecordHandleV1(void* handle) { return record_handle(handle) ? handle : NULL; }
int64_t xl6LogRecordLevelV1(void* handle)
{ return record_handle(handle) ? ((xl6_log_record*)handle)->record.Level : XLOG_OFF; }
xstrview xl6LogRecordStringV1(void* handle,int64_t index)
{
    xstrview copy={0}, source;
    if (!record_handle(handle)) return copy;
    const xlogrecord* record=&((xl6_log_record*)handle)->record;
    switch(index) {
    case 0: source=record->Message; break;
    case 1: source=record->Logger; break;
    case 2: source=record->File; break;
    case 3: source=record->Function; break;
    default: xrtSetErrorKind(XERR_RANGE); return copy;
    }
    (void)record_copy(source,&copy); return copy;
}
int64_t xl6LogRecordTimeV1(void* handle)
{ return record_handle(handle) ? ((xl6_log_record*)handle)->record.Time : 0; }
uint64_t xl6LogRecordNumberV1(void* handle,int64_t index)
{
    if (!record_handle(handle)) return 0;
    const xlogrecord* record=&((xl6_log_record*)handle)->record;
    switch(index) { case 0:return record->Line; case 1:return record->ThreadId; case 2:return record->FieldCount;
    default:xrtSetErrorKind(XERR_RANGE); return 0; }
}
bool xl6LogRecordSourceV1(void* handle,xstrview file,int64_t line,xstrview function)
{
    xstrview fileCopy={0}, functionCopy={0};
    if (!record_handle(handle)) return false;
    if (line<0 || (uint64_t)line>UINT32_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!record_copy(file,&fileCopy) || !record_copy(function,&functionCopy)) {
        xrtFree((void*)fileCopy.Data); return false;
    }
    xlogrecord* record=&((xl6_log_record*)handle)->record;
    xrtFree((void*)record->File.Data); xrtFree((void*)record->Function.Data);
    record->File=fileCopy; record->Function=functionCopy; record->Line=(uint32_t)line;
    return true;
}
bool xl6LogRecordMetadataV1(void* handle,xstrview logger,int64_t time,uint64_t thread)
{
    xstrview copy={0};
    if (!record_handle(handle) || !record_copy(logger,&copy)) return false;
    xlogrecord* record=&((xl6_log_record*)handle)->record;
    xrtFree((void*)record->Logger.Data); record->Logger=copy; record->Time=time; record->ThreadId=thread;
    return true;
}
bool xl6LogRecordFieldV1(void* handle,xstrview name,xvalue* value,int64_t size)
{
    xlogfield field; memset(&field,0,sizeof(field)); field.Name=name;
    if (!record_handle(handle) || !value || !record_view(name)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (size < -1) { xrtSetErrorKind(XERR_RANGE); return false; }
    switch(xrtValueType(value)) {
    case XVALUE_NULL: field.Type=XLOG_FIELD_NULL; break;
    case XVALUE_BOOL: field.Type=XLOG_FIELD_BOOL; if (!xrtValueGetBool(value,&field.Value.Boolean)) return false; break;
    case XVALUE_INT: field.Type=XLOG_FIELD_INT; if (!xrtValueGetInt(value,&field.Value.Integer)) return false; break;
    case XVALUE_UINT: field.Type=XLOG_FIELD_UINT; if (!xrtValueGetUInt(value,&field.Value.Unsigned)) return false; break;
    case XVALUE_FLOAT: field.Type=XLOG_FIELD_FLOAT; if (!xrtValueGetFloat(value,&field.Value.Float)) return false; break;
    case XVALUE_TIME: field.Type=XLOG_FIELD_TIME; if (!xrtValueGetTime(value,&field.Value.Time)) return false; break;
    case XVALUE_CHAR: { uint32 scalar; if (!xrtValueGetChar(value,&scalar)) return false; field.Type=XLOG_FIELD_UINT; field.Value.Unsigned=scalar; break; }
    case XVALUE_STRING: field.Type=XLOG_FIELD_STRING; if (!xrtValueGetString(value,&field.Value.String) || !record_size(&field.Value.String,size)) return false; break;
    default: xrtSetErrorKind(XERR_TYPE); return false;
    }
    return record_add(handle,&field);
}
bool xl6LogRecordErrorV1(void* handle,xstrview name,xerror* error)
{
    if (!record_handle(handle) || !error) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    xlogfield field=xrtLogFieldError(name,error);
    return record_add(handle,&field);
}
static const xlogfield* record_field(void* handle,int64_t index)
{
    if (!record_handle(handle)) return NULL;
    const xl6_log_record* record=handle;
    if (index<0 || (uint64_t)index>=record->record.FieldCount) { xrtSetErrorKind(XERR_RANGE); return NULL; }
    return &record->fields[index];
}
xstrview xl6LogRecordFieldNameV1(void* handle,int64_t index)
{
    const xlogfield* field=record_field(handle,index); xstrview copy={0};
    if (field) (void)record_copy(field->Name,&copy);
    return copy;
}
int64_t xl6LogRecordFieldTypeV1(void* handle,int64_t index)
{ const xlogfield* field=record_field(handle,index); return field ? (int64_t)field->Type:-1; }
xvalue* xl6LogRecordFieldValueV1(void* handle,int64_t index)
{
    const xlogfield* field=record_field(handle,index); if (!field) return NULL;
    switch (field->Type) {
    case XLOG_FIELD_NULL:return xrtValueNull();
    case XLOG_FIELD_BOOL:return xrtValueBool(field->Value.Boolean);
    case XLOG_FIELD_INT:return xrtValueInt(field->Value.Integer);
    case XLOG_FIELD_UINT:return xrtValueUInt(field->Value.Unsigned);
    case XLOG_FIELD_FLOAT:return xrtValueFloat(field->Value.Float);
    case XLOG_FIELD_STRING:return xrtValueString(field->Value.String);
    case XLOG_FIELD_TIME:return xrtValueTime(field->Value.Time);
    default:xrtSetErrorKind(XERR_TYPE); return NULL;
    }
}
xerror* xl6LogRecordFieldErrorV1(void* handle,int64_t index)
{
    const xlogfield* field=record_field(handle,index); if (!field) return NULL;
    if (field->Type!=XLOG_FIELD_ERROR) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    return xrtErrorRef(field->Value.Error);
}
int64_t xl6LogWriteRecordV1(void* logger,void* handle)
{
    if (!record_handle(logger) || !record_handle(handle)) return XLOG_RESULT_ERROR;
    return xrtLogSubmit(logger,&((xl6_log_record*)handle)->record);
}
int64_t xl6LogDefaultRecordV1(void* handle)
{
    if (!record_handle(handle)) return XLOG_RESULT_ERROR;
    xlogger* logger=xrtLogDefault();
    if (!logger) return XLOG_RESULT_SKIPPED;
    int64_t result=xl6LogWriteRecordV1(logger,handle);
    xrtLogFree(logger); return result;
}
