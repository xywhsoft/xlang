#include "xge.h"
#include <limits.h>
#include <string.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdlib.h>
#include "xge_data_lifetime_internal.h"

#define XGE_DATA_OBJECT_MAGIC 0x5847444Fu

/* A slot owns one immutable payload generation. A provider invocation owns a
 * second real reference until its complete C return; replacement never leaves
 * a reader holding a pointer into the movable slot array. Mutable operations
 * retain their existing caller-serialized C contract. Atomic references only
 * protect actual lifetime, not arbitrary concurrent container mutations. */
typedef struct xge_data_generation_t {
	atomic_uint iRefs;
	unsigned iFlags;
	char* sFormat;
	void* pValue;
	union {size_t valueBytes;size_t formatBytes;} tSize;
	union { xge_data_provider_proc raw; xge_data_provider_owned_proc owned; } tRead;
	xge_data_provider_free_proc onFree;
	void* pUser;
} xge_data_generation_t;

struct xge_data_object_t {
	uint32_t iMagic;
	atomic_uint iRefCount;
	xge_data_generation_t** pSlots;
	int iCount;
	int iCapacity;
	atomic_uint iActivity;
	atomic_uint bDestroying;
	uint64_t iRevision; /* Nonwrapping snapshot publication identity. */
};

enum { XGE_DATA_MANAGED=2u,XGE_DATA_OWNED_PROVIDER=4u };
typedef struct xge_data_operation_t { xge_data_object pObject;xerror* pAmbient;bool bOwned; } xge_data_operation_t;
static xge_data_lifetime_t* __xgeDataObjectRoot(xge_data_object object)
{return object && (atomic_load_explicit(&object->bDestroying,memory_order_relaxed)&XGE_DATA_MANAGED) ? (xge_data_lifetime_t*)(object+1):NULL;}
static xge_data_lifetime_t* __xgeDataGenerationRoot(xge_data_generation_t* generation)
{return generation && (generation->iFlags&XGE_DATA_MANAGED) ? (xge_data_lifetime_t*)(generation+1):NULL;}
static xge_callback_owner_t* __xgeDataGenerationOwner(xge_data_generation_t* generation)
{return generation && (generation->iFlags&XGE_DATA_OWNED_PROVIDER) ? (xge_callback_owner_t*)(__xgeDataGenerationRoot(generation)+1):NULL;}
static bool __xgeDataObjectReadable(const void* data)
{const struct xge_data_object_t* object=data;return !(atomic_load_explicit(&object->bDestroying,memory_order_acquire)&1u);}
static bool __xgeDataObjectTrace(const void*,xrtownershipvisitor,void*);
static bool __xgeDataGenerationTrace(const void*,xrtownershipvisitor,void*);
static int __xgeDataObjectFinalize(void*,bool*);
static int __xgeDataGenerationFinish(void*);
static const xge_data_lifetime_type_t __xgeDataObjectType={sizeof(struct xge_data_object_t),offsetof(struct xge_data_object_t,iRefCount),__xgeDataObjectReadable,__xgeDataObjectTrace,__xgeDataObjectFinalize,NULL};
static const xge_data_lifetime_type_t __xgeDataGenerationType={sizeof(xge_data_generation_t),offsetof(xge_data_generation_t,iRefs),NULL,__xgeDataGenerationTrace,NULL,__xgeDataGenerationFinish};
#define DO_FN(name) __xgeDataOwned##name
#define DO_PUBLIC(name) xgeDataObject##name
#define DO_STATUS(name) XGE_##name
#define DO_API 
#define DO_ROOT(object) __xgeDataObjectRoot(object)
#define DO_HANDLE xge_data_object
#define DO_OBJECT_TYPE __xgeDataObjectType
#define DO_GENERATION_TYPE __xgeDataGenerationType
#include "xge_data_lifetime.inl"
#undef DO_FN
#undef DO_PUBLIC
#undef DO_STATUS
#undef DO_API
#undef DO_ROOT
#undef DO_HANDLE
#undef DO_OBJECT_TYPE
#undef DO_GENERATION_TYPE

static bool __xgeDataOwnerValid(const xge_callback_owner_t* owner)
{return owner && owner->tReference.Data && owner->tReference.Ops && owner->onHold && owner->onDrop;}
static int __xgeDataOwnerHold(const xge_callback_owner_t* owner,bool* held)
{
    *held=false;if(!__xgeDataOwnerValid(owner))return XGE_ERROR_INVALID_ARGUMENT;
    xerror* ambient=xrtTakeError();*held=owner->onHold(owner->tReference.Data);
    xerror* fresh=xrtTakeError();int status=*held && !fresh ? XGE_OK:XGE_ERROR_CALLBACK_FAILED;
    if(ambient){xrtErrorFree(fresh);fresh=ambient;}xrtSetErrorTake(fresh);return status;
}
static int __xgeDataOwnerDrop(xge_callback_owner_t owner)
{return !owner.onDrop || owner.onDrop(owner.tReference.Data) ? XGE_OK:XGE_ERROR_CALLBACK_FAILED;}
static int __xgeDataGenerationReleaseChecked(xge_data_generation_t* generation);
static bool __xgeDataObjectTrace(const void* data,xrtownershipvisitor visitor,void* user)
{
    const struct xge_data_object_t* object=data;
    for(int i=0;i<object->iCount;++i){
        xge_data_lifetime_t* root=__xgeDataGenerationRoot(object->pSlots[i]);
        if(!root || !visitor(__xgeDataOwnedReference(root),user))return false;
    }
    return true;
}
static bool __xgeDataGenerationTrace(const void* data,xrtownershipvisitor visitor,void* user)
{
    xge_callback_owner_t* owner=__xgeDataGenerationOwner((xge_data_generation_t*)data);
    return !owner || !owner->onDrop || visitor(owner->tReference,user);
}
static int __xgeDataObjectFinalize(void* data,bool* complete)
{
    xge_data_object object=data;xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))return XGE_ERROR_INVALID_STATE;
    atomic_fetch_or_explicit(&object->bDestroying,1u,memory_order_release);
    xge_data_generation_t** slots=object->pSlots;int count=object->iCount,capacity=object->iCapacity;
    object->pSlots=NULL;object->iCount=object->iCapacity=0;__xgeDataOwnedMutationEnd(&mutation);
    xge_data_outcome_t result={0};__xgeDataOwnedOutcomeBegin(&result,XGE_OK);int remaining=0;
    for(int i=0;i<count;++i){
        bool consumed=false;int status=__xgeDataOwnedRelease(__xgeDataGenerationRoot(slots[i]),&consumed);
        __xgeDataOwnedOutcomeAdd(&result,status);
        if(!consumed)slots[remaining++]=slots[i];
    }
    if(remaining){
        if(!xrtOwnershipMutationBegin(&mutation))abort();
        object->pSlots=slots;object->iCount=remaining;object->iCapacity=capacity;__xgeDataOwnedMutationEnd(&mutation);
        if(result.iStatus==XGE_OK)result.iStatus=XGE_STATUS_DEFERRED;
    }else xrtFree(slots);
    *complete=remaining==0;int status=result.iStatus;xrtSetErrorTake(result.pError);return status;
}
static int __xgeDataGenerationFinish(void* data)
{
    xge_callback_owner_t* slot=__xgeDataGenerationOwner(data);if(!slot)return XGE_OK;
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))return XGE_ERROR_INVALID_STATE;
    xge_callback_owner_t owner=*slot;memset(slot,0,sizeof(*slot));__xgeDataOwnedMutationEnd(&mutation);
    return __xgeDataOwnerDrop(owner);
}

 bool xgeDataObjectIsOwned(xge_data_object object)
{return object && object->iMagic==XGE_DATA_OBJECT_MAGIC && __xgeDataOwnedKnown(__xgeDataObjectRoot(object));}
 int xgeDataObjectCreateOwned(xge_data_object* output)
{
    if(!output)return XGE_ERROR_INVALID_ARGUMENT;
    *output=NULL;xge_data_object object=xrtCalloc(1,sizeof(*object)+sizeof(xge_data_lifetime_t));
    if(!object)return XGE_ERROR_OUT_OF_MEMORY;
    object->iMagic=XGE_DATA_OBJECT_MAGIC;atomic_init(&object->iRefCount,1u);atomic_init(&object->iActivity,0u);
    atomic_init(&object->bDestroying,XGE_DATA_MANAGED);
    __xgeDataOwnedInit(__xgeDataObjectRoot(object),&__xgeDataObjectType,xrtThreadCurrentId());
    *output=object;return XGE_OK;
}
 int xgeDataObjectShareOwned(xge_data_object object,xge_data_object* output)
{
    if(!output)return XGE_ERROR_INVALID_ARGUMENT;
    *output=NULL;if(!xgeDataObjectIsOwned(object))return XGE_ERROR_INVALID_ARGUMENT;
    int status=__xgeDataOwnedAcquire(__xgeDataObjectRoot(object),true,false);if(status==XGE_OK)*output=object;return status;
}
 int xgeDataObjectReleaseOwnerChecked(xge_data_object object,bool* consumed)
{
    if(!consumed)return XGE_ERROR_INVALID_ARGUMENT;
    *consumed=false;if(!xgeDataObjectIsOwned(object))return XGE_ERROR_INVALID_ARGUMENT;
    return __xgeDataOwnedRelease(__xgeDataObjectRoot(object),consumed);
}
 int xgeDataObjectReleaseChecked(xge_data_object object)
{bool consumed=false;return xgeDataObjectReleaseOwnerChecked(object,&consumed);}

static int __xgeDataObjectValid(xge_data_object pData)
{
	return pData != NULL && pData->iMagic == XGE_DATA_OBJECT_MAGIC &&
		atomic_load_explicit(&pData->iRefCount,memory_order_acquire)>0u &&
		!(atomic_load_explicit(&pData->bDestroying,memory_order_acquire)&1u) &&
		(!__xgeDataObjectRoot(pData) || __xgeDataOwnedReadable(__xgeDataObjectRoot(pData)));
}

static char* __xgeDataStringCopy(const char* sText)
{
	if(!sText || !sText[0])return NULL;
	size_t length=strlen(sText);
	if(length==SIZE_MAX)return NULL;
	char* copy=(char*)xrtMalloc(length+1u);
	if(copy)memcpy(copy,sText,length+1u);
	return copy;
}

static bool __xgeDataGenerationHold(xge_data_generation_t* generation)
{
	if(__xgeDataGenerationRoot(generation))return __xgeDataOwnedAcquire(__xgeDataGenerationRoot(generation),true,true)==XGE_OK;
	unsigned refs=atomic_load_explicit(&generation->iRefs,memory_order_relaxed);
	do {if(!refs || refs==UINT_MAX)return false;}
	while(!atomic_compare_exchange_weak_explicit(&generation->iRefs,&refs,refs+1u,memory_order_relaxed,memory_order_relaxed));
	return true;
}

static int __xgeDataGenerationReleaseChecked(xge_data_generation_t* generation)
{
    if(!generation)return XGE_OK;
    if(__xgeDataGenerationRoot(generation)){bool consumed=false;return __xgeDataOwnedRelease(__xgeDataGenerationRoot(generation),&consumed);}
    if(atomic_fetch_sub_explicit(&generation->iRefs,1u,memory_order_acq_rel)!=1u)return XGE_OK;
    if(generation->onFree)generation->onFree(generation->pUser);
    xrtFree(generation);return XGE_OK;
}
static void __xgeDataGenerationDrop(xge_data_generation_t* generation){(void)__xgeDataGenerationReleaseChecked(generation);}
static int __xgeDataGenerationSnapshotEnd(xge_data_generation_t* generation)
{return __xgeDataGenerationRoot(generation) ? __xgeDataOwnedActivityEnd(__xgeDataGenerationRoot(generation)):__xgeDataGenerationReleaseChecked(generation);}

static int __xgeDataSlotFind(xge_data_object pData,const char* format)
{
	if(!__xgeDataObjectValid(pData) || !format)return -1;
	for(int i=0;i<pData->iCount;++i)
		if(strcmp(pData->pSlots[i]->sFormat,format)==0)return i;
	return -1;
}

static int __xgeDataSlotReserve(xge_data_object pData,int capacity)
{
	if(capacity<=pData->iCapacity)return XGE_OK;
	if(capacity<0 || (size_t)capacity>SIZE_MAX/sizeof(*pData->pSlots))return XGE_ERROR_OUT_OF_MEMORY;
	int next=pData->iCapacity>0 ? pData->iCapacity:4;
	while(next<capacity){if(next>INT_MAX/2){next=capacity;break;}next*=2;}
	if((size_t)next>SIZE_MAX/sizeof(*pData->pSlots))return XGE_ERROR_OUT_OF_MEMORY;
	xge_data_generation_t** slots=(xge_data_generation_t**)xrtRealloc(pData->pSlots,sizeof(*slots)*(size_t)next);
	if(!slots)return XGE_ERROR_OUT_OF_MEMORY;
	pData->pSlots=slots;pData->iCapacity=next;
	return XGE_OK;
}

 int xgeDataObjectCreate(xge_data_object* output)
{
	if(!output)return XGE_ERROR_INVALID_ARGUMENT;
	*output=NULL;
	xge_data_object object=(xge_data_object)xrtCalloc(1u,sizeof(*object));
	if(!object)return XGE_ERROR_OUT_OF_MEMORY;
	object->iMagic=XGE_DATA_OBJECT_MAGIC;
	atomic_init(&object->iRefCount,1u);atomic_init(&object->iActivity,0u);atomic_init(&object->bDestroying,0u);
	*output=object;return XGE_OK;
}

 int xgeDataObjectAddRef(xge_data_object object)
{
	if(__xgeDataObjectRoot(object))return __xgeDataOwnedAcquire(__xgeDataObjectRoot(object),false,false);
	if(!__xgeDataObjectValid(object))return XGE_ERROR_INVALID_ARGUMENT;
	unsigned refs=atomic_load_explicit(&object->iRefCount,memory_order_relaxed);
	do {if(!refs || refs>=INT_MAX)return XGE_ERROR_INVALID_ARGUMENT;}
	while(!atomic_compare_exchange_weak_explicit(&object->iRefCount,&refs,refs+1u,memory_order_relaxed,memory_order_relaxed));
	return XGE_OK;
}

 void xgeDataObjectRelease(xge_data_object object)
{
	if(__xgeDataObjectRoot(object)){bool consumed=false;(void)__xgeDataOwnedRelease(__xgeDataObjectRoot(object),&consumed);return;}
	if(!__xgeDataObjectValid(object))return;
	if(atomic_fetch_sub_explicit(&object->iRefCount,1u,memory_order_acq_rel)!=1u)return;
	/* No owning field remains attached while provider cleanup can reenter. A
	 * real active operation still owns a reference and cannot reach this tail. */
	atomic_store_explicit(&object->bDestroying,1u,memory_order_release);
	xge_data_generation_t** slots=object->pSlots;
	int count=object->iCount;
	object->pSlots=NULL;object->iCount=0;object->iCapacity=0;
	for(int i=0;i<count;++i)__xgeDataGenerationDrop(slots[i]);
	xrtFree(slots);object->iMagic=0u;xrtFree(object);
}

static int __xgeDataOperationBegin(xge_data_object object,xge_data_operation_t* call)
{
    memset(call,0,sizeof(*call));
    xge_data_lifetime_t* root=__xgeDataObjectRoot(object);
    if(root){
        call->bOwned=true;call->pAmbient=xrtTakeError();int status=__xgeDataOwnedAcquire(root,true,true);
        if(status!=XGE_OK){xrtErrorFree(call->pAmbient);call->pAmbient=NULL;return status;}
    }else{
        int status=xgeDataObjectAddRef(object);if(status!=XGE_OK)return status;
        atomic_fetch_add_explicit(&object->iActivity,1u,memory_order_relaxed);
    }
    call->pObject=object;return XGE_OK;
}
static int __xgeDataOperationEnd(xge_data_operation_t* call,int status)
{
    xge_data_object object=call->pObject;call->pObject=NULL;
    if(call->bOwned){
        xge_data_outcome_t result={0};__xgeDataOwnedOutcomeBegin(&result,status);
        __xgeDataOwnedOutcomeAdd(&result,__xgeDataOwnedActivityEnd(__xgeDataObjectRoot(object)));
        return __xgeDataOwnedOutcomeEnd(&result,call->pAmbient);
    }
    xerror* first=status!=XGE_OK ? xrtTakeError():NULL;
    atomic_fetch_sub_explicit(&object->iActivity,1u,memory_order_release);xgeDataObjectRelease(object);
    if(status!=XGE_OK){xrtClearError();xrtSetErrorTake(first);}
    return status;
}

static xge_data_generation_t* __xgeDataGenerationCreate(xge_data_object object,const char* format,const void* value,size_t size,bool provider)
{
    xge_data_lifetime_t* owner=__xgeDataObjectRoot(object);size_t extra=owner ? sizeof(xge_data_lifetime_t)+(provider ? sizeof(xge_callback_owner_t):0u):0u;
    size_t length=strlen(format);size_t header=sizeof(xge_data_generation_t)+extra;
    if(length>=SIZE_MAX-header || size>SIZE_MAX-header-length-1u)return NULL;
    xge_data_generation_t* generation=xrtCalloc(1,header+length+1u+size);if(!generation)return NULL;
    atomic_init(&generation->iRefs,1u);
    if(owner){generation->iFlags=XGE_DATA_MANAGED|(provider ? XGE_DATA_OWNED_PROVIDER:0u);__xgeDataOwnedInit(__xgeDataGenerationRoot(generation),&__xgeDataGenerationType,owner->iCreator);}
    generation->sFormat=(char*)generation+header;memcpy(generation->sFormat,format,length+1u);
    if(size){generation->pValue=generation->sFormat+length+1u;memcpy(generation->pValue,value,size);}
    if(provider)generation->tSize.formatBytes=length;else generation->tSize.valueBytes=size;return generation;
}

static int __xgeDataPublish(xge_data_object object,xge_data_generation_t* next,bool* published)
{
    *published=false;xge_data_lifetime_t* root=__xgeDataObjectRoot(object);xrtownershipscope mutation={0};
    if(root){if(!xrtOwnershipMutationBegin(&mutation))return XGE_ERROR_INVALID_STATE;int status=__xgeDataOwnedAdmit(root);if(status!=XGE_OK){__xgeDataOwnedMutationEnd(&mutation);return status;}}
    if(object->iRevision==UINT64_MAX){if(root)__xgeDataOwnedMutationEnd(&mutation);return XGE_ERROR_INVALID_STATE;}
    int index=__xgeDataSlotFind(object,next->sFormat);xge_data_generation_t* old=NULL;int status=XGE_OK;
    if(index<0){
        if(object->iCount==INT_MAX)status=XGE_ERROR_OUT_OF_MEMORY;
        else status=__xgeDataSlotReserve(object,object->iCount+1);
        if(status==XGE_OK)index=object->iCount++;
    }else old=object->pSlots[index];
    if(status==XGE_OK){object->pSlots[index]=next;object->iRevision++;*published=true;}
    if(root)__xgeDataOwnedMutationEnd(&mutation);
    if(status==XGE_OK)status=__xgeDataGenerationReleaseChecked(old);
    return status;
}

 int xgeDataObjectSet(xge_data_object object,const char* format,const void* value,size_t size)
{
	if(!format || !format[0] || (size && !value))return XGE_ERROR_INVALID_ARGUMENT;
	xge_data_operation_t call={0};int status=__xgeDataOperationBegin(object,&call);
	if(status!=XGE_OK)return status;
	xge_data_generation_t* next=__xgeDataGenerationCreate(object,format,value,size,false);
	if(!next)status=XGE_ERROR_OUT_OF_MEMORY;
	else {
		bool published=false;status=__xgeDataPublish(object,next,&published);
		if(!published)__xgeDataGenerationDrop(next);
	}
	return __xgeDataOperationEnd(&call,status);
}

 int xgeDataObjectSetProvider(xge_data_object object,const char* format,
	xge_data_provider_proc read,xge_data_provider_free_proc release,void* user)
{
	if(!format || !format[0] || !read)return XGE_ERROR_INVALID_ARGUMENT;
	if(__xgeDataObjectRoot(object))return XGE_ERROR_INVALID_STATE;
	xge_data_operation_t call={0};int status=__xgeDataOperationBegin(object,&call);
	if(status!=XGE_OK)return status;
	xge_data_generation_t* next=__xgeDataGenerationCreate(object,format,NULL,0u,false);
	if(!next)status=XGE_ERROR_OUT_OF_MEMORY;
	else {
		next->tRead.raw=read;next->onFree=release;next->pUser=user;
		bool published=false;status=__xgeDataPublish(object,next,&published);
		if(!published){next->onFree=NULL;__xgeDataGenerationDrop(next);}
	}
	return __xgeDataOperationEnd(&call,status);
}

 int xgeDataObjectFormatCount(xge_data_object object)
{return __xgeDataObjectValid(object) ? object->iCount:0;}

 const char* xgeDataObjectFormatAt(xge_data_object object,int index)
{
	return __xgeDataObjectValid(object) && index>=0 && index<object->iCount ? object->pSlots[index]->sFormat:NULL;
}

 int xgeDataObjectHas(xge_data_object object,const char* format)
{return __xgeDataSlotFind(object,format)>=0;}

static int __xgeDataGenerationRead(xge_data_generation_t* generation,void* output,size_t capacity,size_t* outputSize)
{
    int status=XGE_OK;
        if(generation->iFlags&XGE_DATA_OWNED_PROVIDER){
            xge_callback_owner_t owner=*__xgeDataGenerationOwner(generation);bool held=false;
            status=__xgeDataOwnerHold(&owner,&held);xge_data_outcome_t result={0};__xgeDataOwnedOutcomeBegin(&result,status);
            if(status==XGE_OK){
                xstrview view={generation->sFormat,generation->tSize.formatBytes};
                xge_data_output_t sink={NULL,output,capacity,0u};sink._pSelf=&sink;
                int callback=generation->tRead.owned(&view,&sink,generation->pUser);sink._pSelf=NULL;*outputSize=sink.iRequired;
                __xgeDataOwnedOutcomeAdd(&result,callback==0 ? XGE_OK:XGE_ERROR_CALLBACK_FAILED);
                if(result.iStatus==XGE_OK && output && sink.iRequired>capacity)
                    __xgeDataOwnedOutcomeAdd(&result,XGE_ERROR_BUFFER_TOO_SMALL);
            }
            if(held)__xgeDataOwnedOutcomeAdd(&result,__xgeDataOwnerDrop(owner));
            status=result.iStatus;xrtSetErrorTake(result.pError);
        }else if(generation->tRead.raw)status=generation->tRead.raw(generation->sFormat,output,capacity,outputSize,generation->pUser);
        else {
            *outputSize=generation->tSize.valueBytes;if(output && capacity<generation->tSize.valueBytes)status=XGE_ERROR_BUFFER_TOO_SMALL;
            else if(output && generation->tSize.valueBytes)memcpy(output,generation->pValue,generation->tSize.valueBytes);
        }
    return status;
}

 int xgeDataObjectGet(xge_data_object object,const char* format,void* output,size_t capacity,size_t* outputSize)
{
    if(!outputSize || (capacity && !output))return XGE_ERROR_INVALID_ARGUMENT;
    *outputSize=0u;int index=__xgeDataSlotFind(object,format);
    xge_data_generation_t* generation=index<0 ? NULL:object->pSlots[index];
    bool owned=__xgeDataObjectRoot(object)!=NULL;
    bool provider=generation && ((generation->iFlags&XGE_DATA_OWNED_PROVIDER) || generation->tRead.raw!=NULL);
    if(generation && !provider){
        *outputSize=generation->tSize.valueBytes;
        if(output && capacity<generation->tSize.valueBytes)return XGE_ERROR_BUFFER_TOO_SMALL;
        if(output && generation->tSize.valueBytes)memcpy(output,generation->pValue,generation->tSize.valueBytes);
        return XGE_OK;
    }
    xge_data_operation_t call={0};int status=__xgeDataOperationBegin(object,&call);if(status!=XGE_OK)return status;
    if(!generation)status=XGE_ERROR_NOT_FOUND;
    else if(!__xgeDataGenerationHold(generation))status=XGE_ERROR_INVALID_STATE;
    else {
        status=__xgeDataGenerationRead(generation,output,capacity,outputSize);
        if(owned){
            xge_data_outcome_t result={0};__xgeDataOwnedOutcomeBegin(&result,status);
            __xgeDataOwnedOutcomeAdd(&result,__xgeDataGenerationSnapshotEnd(generation));status=result.iStatus;xrtSetErrorTake(result.pError);
        }else {
            xerror* first=status!=XGE_OK ? xrtTakeError():NULL;__xgeDataGenerationSnapshotEnd(generation);
            if(status!=XGE_OK){xrtClearError();xrtSetErrorTake(first);}
        }
    }
    return __xgeDataOperationEnd(&call,status);
}


 int xgeDataOutputWrite(xge_data_output_t* output,size_t offset,const void* data,size_t size)
{
    if(!output || output->_pSelf!=output || (size && !data) || offset>SIZE_MAX-size)return XGE_ERROR_INVALID_ARGUMENT;
    size_t previous=output->iRequired,required=offset+size;if(required>previous)output->iRequired=required;
    if(!output->_pDestination)return XGE_OK;
    if(required>output->iCapacity)return XGE_OK;
    if(size)memmove((char*)output->_pDestination+offset,data,size);
    if(offset>previous)memset((char*)output->_pDestination+previous,0,offset-previous);
    return XGE_OK;
}
 int xgeDataOutputSet(xge_data_output_t* output,const void* data,size_t size)
{
    if(!output || output->_pSelf!=output || (size && !data))return XGE_ERROR_INVALID_ARGUMENT;
    output->iRequired=size;if(!output->_pDestination)return XGE_OK;
    if(size>output->iCapacity)return XGE_OK;
    if(size)memmove(output->_pDestination,data,size);
    return XGE_OK;
}
 int xgeDataObjectSetProviderOwned(xge_data_object object,const char* format,xge_data_provider_owned_proc read,void* user,const xge_callback_owner_t* incoming)
{
    if(!format || !format[0] || !read || !__xgeDataOwnerValid(incoming) || !xgeDataObjectIsOwned(object))return XGE_ERROR_INVALID_ARGUMENT;
    xge_callback_owner_t owner=*incoming;xge_data_operation_t call={0};int status=__xgeDataOperationBegin(object,&call);if(status!=XGE_OK)return status;
    xge_data_generation_t* next=__xgeDataGenerationCreate(object,format,NULL,0u,true);bool published=false;
    if(!next)status=XGE_ERROR_OUT_OF_MEMORY;
    else {
        bool held=false;status=__xgeDataOwnerHold(&owner,&held);if(held)*__xgeDataGenerationOwner(next)=owner;
        if(status==XGE_OK)status=__xgeDataOwnedAdmit(__xgeDataObjectRoot(object));
        if(status==XGE_OK){next->tRead.owned=read;next->pUser=user;status=__xgeDataPublish(object,next,&published);}
        if(!published){xge_data_outcome_t result={0};__xgeDataOwnedOutcomeBegin(&result,status);__xgeDataOwnedOutcomeAdd(&result,__xgeDataGenerationReleaseChecked(next));status=result.iStatus;xrtSetErrorTake(result.pError);}
    }
    return __xgeDataOperationEnd(&call,status);
}


#define DS_FN(name) __xgeDataSnapshot##name
#define DS_PREFIX(name) __xgeData##name
#define DS_STATUS(name) XGE_##name
#define DS_HANDLE xge_data_object
#define DS_GENERATION xge_data_generation_t
#define DS_OPERATION xge_data_operation_t
#define DS_ITEM xge_clipboard_item_t
#define DS_IS_OWNED xgeDataObjectIsOwned
#define DS_PROVIDER XGE_DATA_OWNED_PROVIDER
#include "xge_data_snapshot.inl"
#undef DS_FN
#undef DS_PREFIX
#undef DS_STATUS
#undef DS_HANDLE
#undef DS_GENERATION
#undef DS_OPERATION
#undef DS_ITEM
#undef DS_IS_OWNED
#undef DS_PROVIDER
