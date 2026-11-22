"""Closed call metadata roles from actual selected native declarations."""
from pathlib import Path
from ir_table_schema import Schema

FIELDS = {'XL6_IrCallSite': 'origin:XL6_IrNodeId:0 callKind:XL6_SemCallKind:0 targetSymbol:XL6_SymbolId:0 resultType:XL6_IrTypeId:0 receiver:XL6_IrNodeId:0 receiverGate:XL6_IrCallReceiverGate:0 preparedGetter:XL6_IrNodeId:0 dispatchKind:XL6_IrCallDispatchKind:0 dispatchSlot:uint32:0 receiverEvaluationIndex:size_t:0 firstArgument:size_t:0 argumentCount:size_t:0 fixedParameterCount:size_t:0 variadicAbi:XL6_IrCallVariadicAbi:0', 'XL6_IrCallArgument': 'source:XL6_IrCallArgumentSource:0 expression:XL6_IrNodeId:0 parameter:size_t:0 evaluationIndex:size_t:0 name:char:1', 'XL6_IrDynamicCallSite': 'origin:XL6_IrNodeId:0 kind:XL6_IrDynamicCallKind:0 memberBindingAbi:XL6_IrDynamicMemberBindingAbi:0 evaluationOrder:XL6_IrDynamicCallEvaluationOrder:0 subject:XL6_IrNodeId:0 subjectSymbol:XL6_SymbolId:0 memberName:char:1 fallbackCollectionOp:XL6_IrDynamicCollectionOp:0 boundCollectionOp:XL6_IrDynamicCollectionOp:0 resultType:XL6_IrTypeId:0 resultAbi:XL6_IrDynamicCallResultAbi:0 errorAbi:XL6_IrErrorAbi:0 firstArgument:size_t:0 argumentCount:size_t:0 positionalCount:size_t:0 keywordCount:size_t:0', 'XL6_IrDynamicCallArgument': 'kind:XL6_IrDynamicCallArgumentKind:0 admission:XL6_IrDynamicCallArgumentAdmission:0 expression:XL6_IrNodeId:0 type:XL6_IrTypeId:0 name:char:1', 'XL6_IrDynamicMethod': 'kind:XL6_IrDynamicCallKind:0 ownerDeclaration:XL6_IrNodeId:0 ownerType:XL6_IrTypeId:0 runtimeTypeTag:uint32:0 declaration:XL6_IrNodeId:0 targetSymbol:XL6_SymbolId:0 name:char:1 returnCount:size_t:0 returnType:XL6_IrTypeId:0 targetErrorAbi:XL6_IrErrorAbi:0 adapterAbi:XL6_IrDynamicMethodAdapterAbi:0 firstParameter:size_t:0 parameterCount:size_t:0 fixedParameterCount:size_t:0 variadicAbi:XL6_IrCallVariadicAbi:0 variadicCarrierAbi:XL6_IrCollectionCarrierAbi:0 variadicElementIdentity:uint64:0 firstScalarResultCodec:size_t:0 scalarResultCodecCount:size_t:0 boundSignatureType:XL6_IrTypeId:0', 'XL6_IrDynamicMethodParameter': 'declaration:XL6_IrNodeId:0 name:char:1 type:XL6_IrTypeId:0 abi:XL6_IrParameterAbi:0 matchAbi:XL6_IrDynamicMethodMatchAbi:0 scalarCodec:size_t:0 defaultExpression:XL6_IrNodeId:0 defaultAbi:XL6_IrDynamicMethodDefaultAbi:0 defaultLiteral:char:1 defaultFunction:uint32:0 variadicAbi:XL6_IrCallVariadicAbi:0', 'XL6_IrIntrinsicCall': 'origin:XL6_IrNodeId:0 kind:XL6_IrIntrinsicKind:0 admission:XL6_IrIntrinsicAdmission:0 resultType:XL6_IrTypeId:0 resultAbi:XL6_IrIntrinsicResultAbi:0 errorAbi:XL6_IrErrorAbi:0 receiver:XL6_IrNodeId:0 receiverType:XL6_IrTypeId:0 collectionAbi:XL6_IrCollectionAbi:0 runtimeResourceOp:XL6_IrRuntimeResourceOp:0 dynamicCollectionOp:XL6_IrDynamicCollectionOp:0 typedCollectionOp:XL6_IrTypedCollectionOp:0 payloadType:XL6_IrTypeId:0 payloadAbi:XL6_IrAsyncResultAbi:0 persistenceSourceOperand:size_t:0 persistenceOptionsOperand:size_t:0 persistenceDestinationOperand:size_t:0 runtimeResourcePayloadAbi:XL6_IrRuntimeResourcePayloadAbi:0 runtimeResourceKeyType:XL6_IrTypeId:0 runtimeResourceKeyPayloadAbi:XL6_IrRuntimeResourcePayloadAbi:0 runtimeResourceKeyOrderAbi:XL6_IrRuntimeResourceKeyOrderAbi:0 sourcePayloadType:XL6_IrTypeId:0 sourcePayloadAbi:XL6_IrAsyncResultAbi:0 typeQueryAbi:XL6_IrTypeQueryAbi:0 typeQueryTarget:XL6_IrTypeId:0 stringConversionAbi:XL6_IrStringConversionAbi:0 stringClassType:XL6_IrTypeId:0 scalarConversionAbi:XL6_IrScalarConversionAbi:0 castAbi:XL6_IrCastAbi:0 firstOperand:size_t:0 operandCount:size_t:0 integerTarget:XL6_IrIntegerTarget:0 integerSourceAbi:XL6_IrIntegerSourceAbi:0', 'XL6_IrIntrinsicOperand': 'expression:XL6_IrNodeId:0 type:XL6_IrTypeId:0 abi:XL6_IrIntrinsicOperandAbi:0'}
TABLES = (
 ('SITE','XL6_IrCallSite','callSites'),('ARGUMENT','XL6_IrCallArgument','callArguments'),
 ('DYNAMIC_SITE','XL6_IrDynamicCallSite','dynamicCallSites'),('DYNAMIC_ARGUMENT','XL6_IrDynamicCallArgument','dynamicCallArguments'),
 ('METHOD','XL6_IrDynamicMethod','dynamicMethods'),('PARAMETER','XL6_IrDynamicMethodParameter','dynamicMethodParameters'),
 ('INTRINSIC','XL6_IrIntrinsicCall','intrinsicCalls'),('OPERAND','XL6_IrIntrinsicOperand','intrinsicOperands'))
REFS = {
 'SITE':dict(origin='NODE',targetSymbol='SYMBOL',resultType='TYPE',receiver='NODE',preparedGetter='NODE'),
 'ARGUMENT':dict(expression='NODE'),
 'DYNAMIC_SITE':dict(origin='NODE',subject='NODE',subjectSymbol='SYMBOL',resultType='TYPE'),
 'DYNAMIC_ARGUMENT':dict(expression='NODE',type='TYPE'),
 'METHOD':dict(ownerDeclaration='NODE',ownerType='TYPE',runtimeTypeTag='RUNTIME_TAG',declaration='NODE',targetSymbol='SYMBOL',returnType='TYPE',boundSignatureType='TYPE'),
 'PARAMETER':dict(declaration='NODE',type='TYPE',scalarCodec='CODEC',defaultExpression='NODE',defaultFunction='FUNCTION'),
 'INTRINSIC':dict(origin='NODE',resultType='TYPE',receiver='NODE',receiverType='TYPE',payloadType='TYPE',runtimeResourceKeyType='TYPE',sourcePayloadType='TYPE',typeQueryTarget='TYPE',stringClassType='TYPE'),
 'OPERAND':dict(expression='NODE',type='TYPE')}
SLICES = {'SITE':dict(firstArgument=('argumentCount','ARGUMENT')),
 'DYNAMIC_SITE':dict(firstArgument=('argumentCount','DYNAMIC_ARGUMENT')),
 'METHOD':dict(firstParameter=('parameterCount','PARAMETER'),firstScalarResultCodec=('scalarResultCodecCount','CODEC')),
 'INTRINSIC':dict(firstOperand=('operandCount','OPERAND'))}

def generate(headers,directory):
 schema=Schema(headers);metadata=schema.build();lines=['/* Actual native declarations;closed reviewed field roles. */'];roles=[]
 for tag,ty,field in TABLES:
  rows=schema.record(ty);actual=' '.join(r['name']+':'+r['type']+':'+str(r['pointers'])for r in rows)
  if actual!=FIELDS[ty]or any(r['extent']for r in rows):raise ValueError('Audit changed native call record: '+ty)
  table=next(t for t in metadata['tables']if t['field']==field)
  if table['type']!=ty or table['scalar']or not table['capacity']:raise ValueError('Audit call table shape: '+field)
  lines+=['static int equal_'+tag+'(const void* av,const void* bv){const '+ty+' *a=av,*b=bv;']
  for row in rows:
   f=row['name']
   if row['pointers']:
    if row['type']!='char'or row['pointers']!=1:raise ValueError('Audit new call payload: '+ty+'.'+f)
    lines.append('if(!textEqual(a->'+f+',b->'+f+'))return 0;')
   else:lines.append('if(a->'+f+'!=b->'+f+')return 0;')
  lines+=['return 1;}','static int relocate_'+tag+'(call_state* s,void* value,const void* target){'+ty+'* dst=value;const '+ty+'* current=target;uint64_t mapped;']
  for f,domain in REFS[tag].items():
   row=next(v for v in rows if v['name']==f)
   if row['pointers']:raise ValueError('Reference became a pointer: '+f)
   lines.append('if(!mapOne(s,XLS_CALL_'+domain+',"'+f+'",dst->'+f+',&mapped))return 0;dst->'+f+'=mapped;')
   roles.append((tag,f,domain))
  for first,(count,domain)in SLICES.get(tag,{}).items():
   lines.append('if(dst->'+count+'!=current->'+count+')return fail(s,XLS_CALL_DIFFERENT,"'+count+'",XLS_CALL_'+domain+',dst->'+count+');')
   lines.append('if(!mapSlice(s,XLS_CALL_'+domain+',"'+first+'",dst->'+first+',dst->'+count+',current->'+first+',&mapped))return 0;dst->'+first+'=(size_t)mapped;')
   roles.append((tag,first,domain+'-slice'))
  if tag=='SITE':
   lines+=['if(!dispatch(s,dst->dispatchKind,dst->dispatchSlot,&mapped))return 0;dst->dispatchSlot=(uint32)mapped;']
   roles.append((tag,'dispatchSlot','CLASS_SLOT/PROTOCOL_SLOT'))
  lines+=['return equal_'+tag+'(dst,current) || fail(s,XLS_CALL_DIFFERENT,"record",XLS_CALL_'+tag+',0);}',
   'static const void* read_'+tag+'(const XL6_IrModule* m,size_t* count,size_t* capacity){*count=m->'+table['count']+';*capacity=m->'+table['capacity']+';return m->'+field+';}',
   'static void set_'+tag+'(XL6_IrModule* m,void* p,size_t n){m->'+field+'=p;m->'+table['count']+'=m->'+table['capacity']+'=n;}']
 lines+=['static const call_descriptor descriptors[]={']
 for tag,ty,field in TABLES:lines.append('{"'+field+'",sizeof('+ty+'),_Alignof('+ty+'),read_'+tag+',set_'+tag+',relocate_'+tag+'},')
 lines+=['};']
 directory=Path(directory);directory.mkdir(parents=True,exist_ok=True);(directory/'ir-call-records.inc').write_text('\n'.join(lines).replace('return 0;dst->','{return 0;}dst->')+'\n',encoding='utf8')
 return dict(schema=1,tables=[t[2]for t in TABLES],records={ty:schema.record(ty)for _,ty,_ in TABLES},roles=roles,fields=sum(len(schema.record(ty))for _,ty,_ in TABLES),nativeTables=len(metadata['tables']))
