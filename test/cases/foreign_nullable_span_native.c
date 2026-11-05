#include "foreign_nullable_span.h"
#include <assert.h>
static int calls;
int nullable_span_create(NullableSpanRoot** out){*out=xrtCalloc(1,sizeof(**out));if(!*out)return -1;(*out)->marker=17;return 0;}
void nullable_span_drop(NullableSpanRoot* root){xrtFree(root);}
static bool destroy(void* raw){nullable_span_drop(raw);return true;}
const XL6_ForeignViewDescriptorV1 nullableSpanPolicy={sizeof(nullableSpanPolicy),XL6_FOREIGN_VIEW_V1_TAG,"nullable.span",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
static int sum(const void* data,size_t bytes){assert(!bytes || data);int value=0;for(size_t i=0;i<bytes;++i)value+=((const uint8_t*)data)[i];return value;}
int nullable_span_read(NullableSpanRoot* root,const void* data,size_t count,int* out){assert(root && root->marker==17);++calls;*out=sum(data,count);return 0;}
int nullable_span_probe(NullableSpanRoot* root,const void* data,size_t count,int* out){return nullable_span_read(root,data,count,out);}
int nullable_span_words(NullableSpanRoot* root,const uint32_t* data,size_t count,int* out){assert(root && (!count || data));++calls;*out=(int)count;return 0;}
int nullable_span_write(NullableSpanRoot* root,void* data,int count,int fail,int* out){assert(root && count>=0 && (!count || data));++calls;if(count)((uint8_t*)data)[0]='c';if(fail)return -7;*out=count;return 0;}
int nullable_span_pack(NullableSpanRoot* root,const NullableSpanDesc* desc,int* out){assert(desc);return nullable_span_read(root,desc->data,desc->count,out);}
int nullable_span_pack_optional(NullableSpanRoot* root,const NullableSpanDesc* desc,int* out){assert(desc);return nullable_span_read(root,desc->data,desc->count,out);}
int nullable_span_callback(NullableSpanRoot* root,const void* data,size_t count,int(*callback)(void*),void* user,int* out){assert(root && callback);++calls;int before=sum(data,count);int code=callback(user);if(code)return code;assert(sum(data,count)==before);*out=before;return 0;}
int nullable_span_text_callback(NullableSpanRoot* root,const void* data,size_t count,int(*callback)(void*),void* user,int* out){return nullable_span_callback(root,data,count,callback,user,out);}
int nullable_span_calls(void){return calls;}
void nullable_span_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
void nullable_span_reset(void){assert(xrtMemDebugFailAfter(-1));xrtClearError();}
