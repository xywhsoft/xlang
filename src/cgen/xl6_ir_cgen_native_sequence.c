/* All metadata is sealed by XLIR. Generated methods still call Native directly. */
static void xl6IrCgenNativeSequenceCursorDeclaration(XL6_Output* out,const XL6_IrInstruction* instruction)
{
 if(instruction->kind==XL6_IR_INSTR_RUNTIME_CALL && instruction->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN)
  xl6OutputPrintf(out,"    xlir_native_sequence_cursor native_cursor_v%u={0};\n",(unsigned)instruction->result);
}
static int xl6IrCgenNativeSequenceSchemas(XL6_Output* out,const XL6_IrModule* m)
{
 xl6OutputPuts("#include <__xl_runtime/xl6_native_sequence.h>\n#ifndef XLIR_NATIVE_ALIGNOF\n#ifdef __cplusplus\n#define XLIR_NATIVE_ALIGNOF(T) alignof(T)\n#else\n#define XLIR_NATIVE_ALIGNOF(T) _Alignof(T)\n#endif\n#endif\n"
  "typedef struct xlir_native_sequence_cursor { XL6_NativeSequenceReadV1 read; size_t index; } xlir_native_sequence_cursor;\n"
  "static void xlir_native_sequence_cursor_drop(xlir_native_sequence_cursor* cursor) { if(cursor && !xl6NativeSequenceReadEndV1(&cursor->read))xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); }\n",out);
 for(size_t n=0;n<m->types.count;++n){XL6_IrTypeId element=xl6IrNativeSequenceElement(m,(XL6_IrTypeId)n+1);if(!element)continue;
  const XL6_IrType* sequence=&m->types.items[n];const XL6_IrType* item=xl6IrTypeGet(&m->types,element);const char* type=xl6IrNativeSequenceStorageCName(m,element);if(!type || !item->typeValueId)return 0;
  xl6OutputPrintf(out,"static const XL6_NativeSequenceSchemaV1 xlir_native_sequence_t%u={sizeof(XL6_NativeSequenceSchemaV1),XL6_NATIVE_SEQUENCE_SCHEMA_V1_TAG,UINT64_C(0x%016llX),UINT64_C(0x%016llX),sizeof(%s),XLIR_NATIVE_ALIGNOF(%s),sizeof(%s)};\n",
   (unsigned)n+1,(unsigned long long)sequence->typeValueId,(unsigned long long)item->typeValueId,type,type,type);
 }
 return !xl6OutputHasFailed(out);
}
static int xl6IrCgenNativeSequenceOutputBegin(XL6_Output* out,const XL6_IrModule* m,const XL6_IrCallTarget* target,const char* errors)
{
 const XL6_ForeignNativeSequenceContract* row=xl6IrForeignNativeSequenceGet(m,target);if(!row)return 0;
 const XL6_IrType* owner=xl6IrTypeGet(&m->types,row->ownerType);
 xl6OutputPrintf(out,"    { XL6_NativeSequenceResultV1 native_sequence_result={0};\n    if(!%s.failed && !xl6NativeSequenceResultReserveV1(&native_sequence_result,&xlir_native_sequence_t%u,&%s,",errors,row->sequenceType,owner->foreignPolicy);
 if(owner->foreignDomain)xl6OutputPrintf(out,"&%s",owner->foreignDomain);else xl6OutputPuts("NULL",out);
 xl6OutputPrintf(out,",xlir_code_owner.size ? &xlir_code_owner : NULL)) xl6ErrorScopeCaptureFailure(&%s);\n    if(!%s.failed && !xrtGetError()) {\n",errors,errors);return 1;
}
static void xl6IrCgenNativeSequenceOutputAdmit(XL6_Output* out,const XL6_IrModule* m,const XL6_IrCallTarget* target,const char* place,const char* errors)
{
 const XL6_ForeignNativeSequenceContract* row=xl6IrForeignNativeSequenceGet(m,target);
 const XL6_IrType* owner=xl6IrTypeGet(&m->types,row->ownerType);const XL6_IrCallTarget* hold=xl6IrCallTargetGet(m,row->retainSymbol);
 if(row->countAbi==XL6_FOREIGN_SPAN_COUNT_INT)xl6OutputPrintf(out,"    if(!%s.failed && native_raw.e1<0){ xrtSetErrorKind(XERR_RANGE);xl6ErrorScopeCaptureFailure(&%s); }\n",errors,errors);
 xl6OutputPrintf(out,"    if(!%s.failed && !xrtGetError() && !xl6NativeSequenceResultSpanV1(&native_sequence_result,native_raw.e0,(uint64_t)native_raw.e1)) xl6ErrorScopeCaptureFailure(&%s);\n"
  "    if(!%s.failed && !xrtGetError()) {\n        int native_sequence_hold=%s((%s)native_view_%u.raw);\n        bool native_sequence_held=native_sequence_hold%s;\n"
  "        if(native_sequence_held && !xl6NativeSequenceResultAdoptV1(&native_sequence_result,native_view_%u.raw)) xl6ErrorScopeCaptureFailure(&%s);\n"
  "        if(!native_sequence_held){if(!xrtGetError())xl6ForeignStatusFailureV1(native_sequence_hold,\"%s\");xl6ErrorScopeCaptureFailure(&%s);}\n"
  "        if(xrtGetError())xl6ErrorScopeCaptureFailure(&%s);\n    }\n"
  "    if(!%s.failed && !xrtGetError()){ %s=xl6NativeSequenceResultTakeV1(&native_sequence_result);if(!%s)xl6ErrorScopeCaptureFailure(&%s); }\n",
  errors,errors,errors,hold->cName,owner->cName,row->ownerParameter,row->retainSuccess==XL6_FOREIGN_RETAIN_ZERO ? "==0":">0",row->ownerParameter,errors,hold->cName,errors,errors,errors,place,place,errors);
}
/* The logical value contains only declared scalar fields. Never memcpy the
 * backing record, including padding or private pointer-bearing members. */
static int xl6IrCgenNativeProjectionFields(XL6_Output* out,const XL6_IrModule* m,
 XL6_IrTypeId id,const char* target,const char* source,size_t depth)
{
 const XL6_IrType* type=xl6IrTypeGet(&m->types,id);if(!type || depth>16)return 0;
 if(type->kind!=XL6_IR_TYPE_C_RECORD){xl6OutputPrintf(out,"    %s=%s;\n",target,source);return !xl6OutputHasFailed(out);}
 const XL6_IrAggregateLayout* layout=xl6IrUnionLayoutGet(m,id);if(!layout)return 0;
 for(size_t i=0;i<layout->fieldCount;++i){
  const XL6_IrAggregateField* field=&m->aggregateFields[layout->firstField+i];char to[2048],from[2048];
  int a=snprintf(to,sizeof(to),"%s.%s",target,field->cName),b=snprintf(from,sizeof(from),"%s.%s",source,field->cName);
  if(a<0 || (size_t)a>=sizeof(to) || b<0 || (size_t)b>=sizeof(from) ||
   !xl6IrCgenNativeProjectionFields(out,m,field->type,to,from,depth+1))return 0;
 }
 return 1;
}
static int xl6IrCgenNativeSequenceLoad(XL6_Output* out,const XL6_IrModule* m,XL6_IrTypeId element,unsigned result,const char* source)
{
 const XL6_IrType* type=xl6IrTypeGet(&m->types,element);if(!type)return 0;
 if(type->foreignRecordAbi!=XL6_IR_FOREIGN_RECORD_PROJECTED){xl6OutputPrintf(out,"    v%u=%s;\n",result,source);return !xl6OutputHasFailed(out);}
 char target[32];snprintf(target,sizeof(target),"v%u",result);
 xl6OutputPrintf(out,"    memset(&v%u,0,sizeof(v%u));\n",result,result);
 return xl6IrCgenNativeProjectionFields(out,m,element,target,source,0);
}
static int xl6IrCgenNativeSequenceInstruction(XL6_Output* out,const XL6_IrModule* m,const XL6_IrInstruction* i)
{
 if(i->kind!=XL6_IR_INSTR_RUNTIME_CALL || i->runtimeOp<XL6_IR_RUNTIME_NATIVE_SEQUENCE_CREATE || i->runtimeOp>XL6_IR_RUNTIME_NATIVE_SEQUENCE_ORDINAL)return 0;
 if(!xl6IrNativeSequenceInstructionCompatible(m,i))return -1;
 unsigned r=(unsigned)i->result,a=i->operandCount ? (unsigned)m->operands[i->firstOperand]:0;
 const XL6_IrInstruction* source=a ? xl6IrCgenValueDefinition(m,a,NULL):NULL;
 XL6_IrTypeId sequence=i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_CREATE ? i->type:
  i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN ? xl6IrTypeGet(&m->types,i->type)->params[0]:
  i->runtimeOp>=XL6_IR_RUNTIME_NATIVE_SEQUENCE_ADVANCE ? xl6IrTypeGet(&m->types,source->type)->params[0]:source->type;
 const XL6_IrType* s=xl6IrTypeGet(&m->types,sequence);XL6_IrTypeId e=xl6IrNativeSequenceElement(m,sequence);const XL6_IrType* element=xl6IrTypeGet(&m->types,e);
 switch(i->runtimeOp){
 case XL6_IR_RUNTIME_NATIVE_SEQUENCE_CREATE:
  xl6OutputPrintf(out,"    v%u=xl6NativeSequenceEmptyV1(&xlir_native_sequence_t%u);\n    if(!v%u)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});\n",r,sequence,r);break;
 case XL6_IR_RUNTIME_NATIVE_SEQUENCE_COUNT:
  xl6OutputPrintf(out,"    if(!xl6NativeSequenceLengthV1(v%u,UINT64_C(0x%016llX),UINT64_C(0x%016llX),&v%u))xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});\n",a,(unsigned long long)s->typeValueId,(unsigned long long)element->typeValueId,r);break;
 case XL6_IR_RUNTIME_NATIVE_SEQUENCE_SLICE:
  xl6OutputPrintf(out,"    v%u=xl6NativeSequenceSliceV1(v%u,UINT64_C(0x%016llX),UINT64_C(0x%016llX),v%u,",r,a,(unsigned long long)s->typeValueId,(unsigned long long)element->typeValueId,(unsigned)m->operands[i->firstOperand+1]);
  if(i->operandCount==3)xl6OutputPrintf(out,"v%u",(unsigned)m->operands[i->firstOperand+2]);else xl6OutputPuts("-1",out);
  xl6OutputPrintf(out,");\n    if(!v%u)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});\n",r);break;
 case XL6_IR_RUNTIME_NATIVE_SEQUENCE_INDEX:{
  unsigned index=(unsigned)m->operands[i->firstOperand+1];const char* type=xl6IrNativeSequenceStorageCName(m,e);char nativeItem[256];
  int length=snprintf(nativeItem,sizeof(nativeItem),"((const %s*)native_read.data)[(size_t)v%u]",type,index);
  if(length<0 || (size_t)length>=sizeof(nativeItem))return -1;
  xl6OutputPrintf(out,"    { XL6_NativeSequenceReadV1 native_read={0};\n      if(!xl6NativeSequenceReadBeginV1(v%u,&xlir_native_sequence_t%u,&native_read))xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});\n"
   "      else { if(v%u<0 || (uint64_t)v%u>=native_read.count){xrtSetErrorKind(XERR_RANGE);xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}\n"
   "        else {\n",a,sequence,index,index);
  if(!xl6IrCgenNativeSequenceLoad(out,m,e,r,nativeItem))return -1;
  xl6OutputPuts("        }\n        if(!xl6NativeSequenceReadEndV1(&native_read))xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});\n      }\n    }\n",out);break;}
 case XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN:
  xl6OutputPrintf(out,"    native_cursor_v%u.index=0; v%u=&native_cursor_v%u;\n    if(!xl6NativeSequenceReadBeginV1(v%u,&xlir_native_sequence_t%u,&v%u->read)){v%u=NULL;xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}\n",r,r,r,a,sequence,r,r);break;
 case XL6_IR_RUNTIME_NATIVE_SEQUENCE_ADVANCE:
  xl6OutputPrintf(out,"    v%u=v%u->index<v%u->read.count; if(v%u)++v%u->index;\n",r,a,a,r,a);break;
 case XL6_IR_RUNTIME_NATIVE_SEQUENCE_ITEM:{
  char nativeItem[256];int length=snprintf(nativeItem,sizeof(nativeItem),"((const %s*)v%u->read.data)[v%u->index-1]",xl6IrNativeSequenceStorageCName(m,e),a,a);
  if(length<0 || (size_t)length>=sizeof(nativeItem) || !xl6IrCgenNativeSequenceLoad(out,m,e,r,nativeItem))return -1;
  break;}
 case XL6_IR_RUNTIME_NATIVE_SEQUENCE_ORDINAL:
  xl6OutputPrintf(out,"    v%u=(int64_t)(v%u->index-1);\n",r,a);break;
 default:return -1;
 }
 return xl6OutputHasFailed(out) ? -1:1;
}
