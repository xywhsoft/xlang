/* Included once by Sequence: read leases share its participating owner gate.
 * The token exposes only stable const storage to compiler-generated C. */
bool xl6SequenceReadPinBeginV1(const xvalue* value,uint64_t identity,uint64_t elementIdentity,
    unsigned int transfer,size_t size,const xrttype* type,XL6_SequenceReadPinV1* output)
{
    if(!output || output->self || output->owner || output->storage || output->count || output->thread){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    xrtownershipscope mutation={0};mutationBegin(&mutation);
    XL6_Sequence* owner=borrow(value,identity,elementIdentity,transfer,size,type);bool ok=false;
    if(owner){
        if(owner->role!=XL6_SEQUENCE_ARRAY_V1 || owner->iterator.prepared ||
            xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE) || owner->readPins==UINT32_MAX)
            xrtSetErrorKind(XERR_STATE);
        else if(xrtRefRetain(&owner->refs)>0){
            ++owner->readPins;
            *output=(XL6_SequenceReadPinV1){output,owner,owner->storage,xrtTypedArrayCount(owner->storage),xrtThreadCurrentId()};
            ok=true;
        }else xrtSetErrorKind(XERR_STATE);
    }
    mutationEnd(&mutation);return ok;
}
bool xl6SequenceReadPinEndV1(XL6_SequenceReadPinV1* pin)
{
    if(!pin || pin->self!=pin || !pin->owner || pin->thread!=xrtThreadCurrentId()){
        xrtSetErrorKind(XERR_STATE);return false;
    }
    xrtownershipscope mutation={0};mutationBegin(&mutation);
    XL6_Sequence* owner=pin->owner;
    if(!owner->readPins || owner->cleared || owner->storage!=pin->storage){
        mutationEnd(&mutation);xrtSetErrorKind(XERR_STATE);return false;
    }
    --owner->readPins;*pin=(XL6_SequenceReadPinV1){0};
    int32_t refs=xrtRefRelease(&owner->refs);if(refs<0)abort();
    if(refs){mutationEnd(&mutation);return true;}
    if(owner->retired || owner->readPins || xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE))abort();
    xtypedarray* storage=owner->storage;owner->storage=NULL;owner->cleared=true;
    XL6_SequenceAdmissionV3 policy=owner->admission;owner->admission=(XL6_SequenceAdmissionV3){0};
    mutationEnd(&mutation);
    /* This pin may own the last wrapper after Native reentry clears every
     * source variable. Unlike the legacy void Drop ABI, its checked End must
     * retain both false/NULL and TLS-only cleanup failures. Detach first and
     * consume exactly once even when cleanup reports a failure. */
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBegin(&cleanup))abort();
    xrtTypedArrayDestroy(storage);if(xrtGetError())xl6ErrorScopeCaptureFailure(&cleanup);
    if(policy.ops){policy.ops->Drop(policy.data);if(xrtGetError())xl6ErrorScopeCaptureFailure(&cleanup);}
    xrtFree(owner);if(xrtGetError())xl6ErrorScopeCaptureFailure(&cleanup);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
const void* xl6SequenceReadPinSlotV1(const XL6_SequenceReadPinV1* pin,size_t index)
{
    if(!pin || pin->self!=pin || !pin->owner || pin->thread!=xrtThreadCurrentId() || index>=pin->count){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    const XL6_Sequence* owner=pin->owner;
    if(owner->cleared || owner->storage!=pin->storage){xrtSetErrorKind(XERR_STATE);return NULL;}
    const void* slot=xrtTypedArrayConstGet(pin->storage,index);
    if(!slot || (owner->admission.ops && !owner->admission.ops->Slot(owner->admission.data,slot))){
        if(!xrtGetError())xrtSetErrorKind(XERR_TYPE);
        return NULL;
    }
    return slot;
}
