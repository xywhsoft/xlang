#include "xl6_net_vector_input.h"
static bool buffers(XL6_NetVectorInputsV1* input,XL6_NativeClassArrayContextV1 context,const xvalue* array,bool distinct)
{
    if(!xl6NativeClassArrayBeginV1(&input->classes,context,array,xl6BufferHandlePolicyV1Get()))return false;
    size_t count=xrtTypedArrayCount(xl6SequenceStorageV1(input->classes.owner));
    if(count>VECTOR_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
    for(size_t i=0;i<count;++i){
        void* raw=NULL;input->held[i]=xl6NativeClassArrayRetainAtV1(&input->classes,i,&raw);
        if(!input->held[i])return false;
        input->count=i+1;input->buffers[i]=xl6BufferChecked(raw);if(!input->buffers[i])return false;
        for(size_t j=0;distinct&&j<i;++j)if(input->buffers[j]==input->buffers[i]){xrtSetErrorKind(XERR_VALUE);return false;}
        input->spans[i]=(xnetspan){input->buffers[i]->Data,input->buffers[i]->Size};
    }
    return true;
}
bool xl6NetVectorBuffersCaptureV1(XL6_NetVectorInputsV1* i,XL6_NativeClassArrayContextV1 c,const xvalue* a)
{return buffers(i,c,a,false);}
bool xl6NetVectorOutputCaptureV1(XL6_NetVectorInputsV1* i,XL6_NativeClassArrayContextV1 c,const xvalue* a)
{return buffers(i,c,a,true);}
