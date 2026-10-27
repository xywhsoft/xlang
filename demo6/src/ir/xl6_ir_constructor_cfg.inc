/* Independent proof over actual executable CFG. Typed declaration facts do
 * not prove that lowering emitted a write, took its success edge, or withheld
 * a receiver from user code. Only actual operands and control edges do. */
static int xl6IrConstructorGetOp(XL6_IrRuntimeOp op)
{
    switch(op){
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME:return 1;
    default:return 0;
    }
}
static int xl6IrConstructorStorageAlias(const XL6_IrModule* m,const unsigned char* aliases,XL6_SymbolId symbol)
{
    const XL6_IrStorage* storage=xl6IrStorageGet(m,symbol);
    return storage&&aliases[storage-m->storages];
}
static int xl6IrConstructorBoolean(const XL6_IrModule* m,XL6_IrValueId id,unsigned depth)
{
    if(depth>=256)return -1;
    const XL6_IrInstruction* value=xl6IrValueDefinition(m,id,NULL);
    const XL6_IrType* type=value?xl6IrTypeGet(&m->types,value->type):NULL;
    if(!value||!type||type->cAbi!=XL6_IR_C_ABI_BOOL)return -1;
    if(value->kind==XL6_IR_INSTR_CONST&&value->immediateText){
        if(!strcmp(value->immediateText,"1")||!strcmp(value->immediateText,"true"))return 1;
        if(!strcmp(value->immediateText,"0")||!strcmp(value->immediateText,"false"))return 0;
    }
    if(value->operandCount==1&&(value->kind==XL6_IR_INSTR_VALUE||
        (value->kind==XL6_IR_INSTR_UNARY&&value->opcode==XL6_IR_OPCODE_NOT))){
        int truth=xl6IrConstructorBoolean(m,m->operands[value->firstOperand],depth+1);
        return truth<0?-1:value->kind==XL6_IR_INSTR_UNARY?!truth:truth;
    }
    return -1;
}
static int xl6IrConstructorAliases(const XL6_IrModule* m,const XL6_IrFunction* fn,
    unsigned char* values,unsigned char* storages,XL6_IrValueId allocation)
{
    const XL6_IrStorage* storage;
    if(allocation)values[allocation]=1;
    else {
        XL6_SymbolId receiver=m->functionParameters[fn->firstParameter];
        storage=xl6IrStorageGet(m,receiver);if(!storage)return 0;
        storages[storage-m->storages]=1;
        if(fn->parameterCells&&fn->parameterCells[0]){
            storage=xl6IrStorageGet(m,fn->parameterCells[0]);if(!storage)return 0;
            storages[storage-m->storages]=1;
        }
    }
    int changed=1;
    while(changed){
        changed=0;
        for(size_t i=fn->firstInstruction;i<fn->firstInstruction+fn->instructionCount;++i){
            const XL6_IrInstruction* ins=&m->instructions[i];unsigned hasSelf=0;
            for(size_t j=0;j<ins->operandCount;++j)hasSelf|=values[m->operands[ins->firstOperand+j]];
            int propagate=(ins->kind==XL6_IR_INSTR_LOAD||ins->kind==XL6_IR_INSTR_STORAGE_TAKE||ins->kind==XL6_IR_INSTR_CELL_LOAD||
                ins->kind==XL6_IR_INSTR_CELL_RETAIN)&&xl6IrConstructorStorageAlias(m,storages,ins->targetSymbol);
            if(hasSelf&&(ins->kind==XL6_IR_INSTR_VALUE||ins->kind==XL6_IR_INSTR_RETAIN||
                ins->kind==XL6_IR_INSTR_CELL_LOAD||ins->kind==XL6_IR_INSTR_CELL_RETAIN||
                (ins->kind==XL6_IR_INSTR_RUNTIME_CALL&&
                 (ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_RETAIN||
                  ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPE_BIND||
                  ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND))))propagate=1;
            if(propagate&&ins->result&&!values[ins->result]){values[ins->result]=1;changed=1;}
            if(hasSelf&&(ins->kind==XL6_IR_INSTR_STORE||ins->kind==XL6_IR_INSTR_CELL_STORE||ins->kind==XL6_IR_INSTR_CELL_INIT)){
                storage=xl6IrStorageGet(m,ins->targetSymbol);
                /* Construction-site aliases stay within compiler-owned
                 * temporaries. A published local/global holds a completed
                 * object, possibly from an earlier loop iteration. A store
                 * into it while pending is independently rejected below. */
                if(storage&&(!allocation||storage->kind==XL6_IR_STORAGE_TEMPORARY)&&
                    !storages[storage-m->storages]){storages[storage-m->storages]=1;changed=1;}
            }
        }
    }
    return 1;
}
static int xl6IrConstructorCfgField(const XL6_IrModule* m,const XL6_IrFunction* fn,
    const XL6_IrClassField* field,const unsigned char* aliases,XL6_DiagList* diags,
    XL6_IrValueId allocation)
{
    unsigned char* states=(unsigned char*)xrtCalloc(fn->blockCount,1);
    size_t* queue=fn->blockCount<=(SIZE_MAX/sizeof(size_t)-1)/2?
        (size_t*)xrtMalloc((2*fn->blockCount+1)*sizeof(size_t)):NULL;
    char* key=xl6IrCanonicalCStringBytes(field->name);
    size_t head=0,tail=0,failure=fn->firstInstruction;int ok=0;
    if(!states||!queue||!key||!fn->blockCount)goto done;
    states[fn->entryBlock-1-fn->firstBlock]=1;queue[tail++]=fn->entryBlock-1-fn->firstBlock;
    while(head<tail){
        size_t b=queue[head++];const XL6_IrBlock* block=&m->blocks[fn->firstBlock+b];
        unsigned state=states[b];XL6_IrValueId pendingWrite=0;int trapped=0;
        if(!block->instructionCount)goto done;
        for(size_t i=block->firstInstruction;i<block->firstInstruction+block->instructionCount;++i){
            const XL6_IrInstruction* ins=&m->instructions[i];failure=i;
            /* One static allocation may execute repeatedly in a loop. Each
             * new object starts a fresh obligation, independent of the
             * previous object's completed construction. */
            if(allocation&&ins->result==allocation)state=1;
            if(xl6IrInstructionAlwaysTraps(m,ins)){trapped=1;break;}
            unsigned selfOperands=0;
            for(size_t j=0;j<ins->operandCount;++j)if(aliases[m->operands[ins->firstOperand+j]])selfOperands|=j?2:1;
            if(!selfOperands)continue;
            int writes=0,allowed=0;
            if(ins->kind==XL6_IR_INSTR_RUNTIME_CALL&&
               (ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW||
                ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_CLOSURE)){
                allowed=selfOperands==1;
                writes=allowed&&ins->immediateText&&!strcmp(ins->immediateText,key);
            }else if(ins->kind==XL6_IR_INSTR_RUNTIME_CALL&&ins->runtimeOp==XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW){
                /* Declaration defaults initialize only other slots. A
                 * pending resource cannot be discharged by a dummy default. */
                allowed=selfOperands==1&&ins->immediateText&&strcmp(ins->immediateText,key);
            }else if(ins->kind==XL6_IR_INSTR_RUNTIME_CALL&&xl6IrConstructorGetOp(ins->runtimeOp)){
                allowed=selfOperands==1;
                if((state&1)&&ins->immediateText&&!strcmp(ins->immediateText,key))goto done;
            }else if(ins->kind==XL6_IR_INSTR_CALL){
                const XL6_IrNode* target=xl6IrSymbolNodeGet(m,ins->targetSymbol);
                if(target&&target->sourceKind==XL6_AST_CREATE&&target->text&&!strcmp(target->text,"create")&&selfOperands==1){
                    writes=xl6IrConstructorContainsField(m,target->declarationOwnerType,field->declaration);
                    XL6_IrConstructorView view={m,field,target->constructorReceiverSymbol,diags,target->declarationOwnerType};
                    allowed=writes||(target->childCount>1&&!xl6IrConstructorExposes(&view,
                        xl6IrNodeGet(m,m->children[target->firstChild+1]),0));
                }
            }else if(ins->kind==XL6_IR_INSTR_STORE||ins->kind==XL6_IR_INSTR_CELL_STORE||ins->kind==XL6_IR_INSTR_CELL_INIT){
                const XL6_IrStorage* storage=xl6IrStorageGet(m,ins->targetSymbol);
                allowed=storage&&(storage->kind==XL6_IR_STORAGE_TEMPORARY||
                    (!allocation&&(ins->targetSymbol==m->functionParameters[fn->firstParameter]||
                     (fn->parameterCells&&ins->targetSymbol==fn->parameterCells[0]))));
            }else if(ins->kind==XL6_IR_INSTR_VALUE||ins->kind==XL6_IR_INSTR_RETAIN||
                ins->kind==XL6_IR_INSTR_MOVE||ins->kind==XL6_IR_INSTR_RELEASE||
                ins->kind==XL6_IR_INSTR_DROP||ins->kind==XL6_IR_INSTR_CELL_RETAIN||
                ins->kind==XL6_IR_INSTR_CELL_LOAD||ins->kind==XL6_IR_INSTR_DISCARD||
                (allocation&&(ins->kind==XL6_IR_INSTR_ERROR_TEST||ins->kind==XL6_IR_INSTR_NULL_TEST))||
                (ins->kind==XL6_IR_INSTR_RUNTIME_CALL&&
                 (ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_RETAIN||
                  (allocation&&(ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPE_BIND||
                   ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND)))))allowed=1;
            if((state&1)&&!allowed)goto done;
            if(writes){
                if(xl6IrCallInstructionErrorAbi(m,ins)==XL6_IR_ERROR_ABI_MAY_RAISE){
                    if(!ins->result||pendingWrite)goto done;
                    pendingWrite=ins->result;
                }else state=2;
            }
        }
        if(trapped)continue;
        const XL6_IrInstruction* last=&m->instructions[block->firstInstruction+block->instructionCount-1];
        if(last->kind==XL6_IR_INSTR_RETURN){if(!allocation&&(state&1))goto done;continue;}
        if(last->kind==XL6_IR_INSTR_THROW)continue;
        if(last->kind!=XL6_IR_INSTR_BRANCH&&last->kind!=XL6_IR_INSTR_ERROR_TEST&&last->kind!=XL6_IR_INSTR_JUMP)goto done;
        if(pendingWrite&&(last->kind!=XL6_IR_INSTR_ERROR_TEST||last->operandCount!=1||
            m->operands[last->firstOperand]!=pendingWrite))goto done;
        int truth=-1;
        if(last->kind==XL6_IR_INSTR_BRANCH&&last->operandCount==1)
            truth=xl6IrConstructorBoolean(m,m->operands[last->firstOperand],0);
        for(unsigned edge=0;edge<(last->kind==XL6_IR_INSTR_JUMP?1u:2u);++edge){
            if((truth==1&&edge)||(truth==0&&!edge))continue;
            XL6_IrBlockId target=edge?last->falseTarget:last->trueTarget;
            if(target<=fn->firstBlock||target>fn->firstBlock+fn->blockCount)goto done;
            /* ERROR_TEST means operation succeeded on trueTarget. */
            size_t index=target-1-fn->firstBlock;unsigned next=pendingWrite&&!edge?2:state;
            if((states[index]|next)!=states[index]){
                if(tail>=2*fn->blockCount+1)goto done;
                states[index]|=(unsigned char)next;queue[tail++]=index;
            }
        }
    }
    ok=1;
done:
    xrtFree(states);xrtFree(queue);xrtFree(key);
    if(!ok){
        const XL6_IrInstruction* ins=&m->instructions[failure];char message[384];
        snprintf(message,sizeof(message),"XLIR constructor CFG obligation '%s' failed at i%lu (%s/runtime-op=%u)",
            field->name,(unsigned long)failure,xl6IrInstructionKindName(ins->kind),(unsigned)ins->runtimeOp);
        return xl6IrDiag(diags,xl6IrNodeGet(m,ins->origin),message);
    }
    return 1;
}
static int xl6IrVerifyConstructorCfg(const XL6_IrModule* m,XL6_DiagList* diags)
{
    for(size_t f=0;f<m->functionCount;++f){
        const XL6_IrFunction* fn=&m->functions[f];const XL6_IrNode* ctor=xl6IrNodeGet(m,fn->declaration);
        if(fn->kind!=XL6_IR_FUNCTION_USER||!ctor||ctor->sourceKind!=XL6_AST_CREATE||
            !ctor->text||strcmp(ctor->text,"create"))continue;
        unsigned char* values=NULL;unsigned char* storages=NULL;
        for(size_t i=0;i<m->classFieldCount;++i){
            const XL6_IrClassField* field=&m->classFields[i];
            if(field->initializerAbi!=XL6_IR_INITIALIZER_CONSTRUCTOR_REQUIRED||
               !xl6IrConstructorContainsField(m,ctor->declarationOwnerType,field->declaration))continue;
            if(!fn->parameterCount||!ctor->constructorReceiverSymbol||
                m->functionParameters[fn->firstParameter]!=ctor->constructorReceiverSymbol)goto bad;
            if(!values){
                values=(unsigned char*)xrtCalloc(m->valueCount+1,1);storages=(unsigned char*)xrtCalloc(m->storageCount,1);
                if(!values||!storages||!xl6IrConstructorAliases(m,fn,values,storages,0))goto bad;
            }
            if(!xl6IrConstructorCfgField(m,fn,field,values,diags,0))goto bad;
        }
        xrtFree(values);xrtFree(storages);continue;
bad:
        xrtFree(values);xrtFree(storages);return xl6IrDiag(diags,ctor,"XLIR constructor receiver/definite-assignment CFG is invalid");
    }
    /* Also prove the caller's transaction. A correct create body is not a
     * proof that an actual call (and its success edge) precedes COMMIT. */
    for(size_t f=0;f<m->functionCount;++f){
        const XL6_IrFunction* fn=&m->functions[f];
        for(size_t i=fn->firstInstruction;i<fn->firstInstruction+fn->instructionCount;++i){
            const XL6_IrInstruction* ins=&m->instructions[i];
            if(ins->kind!=XL6_IR_INSTR_RUNTIME_CALL||ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_CLASS_NEW)continue;
            unsigned char* values=NULL;unsigned char* storages=NULL;int valid=1;
            for(size_t j=0;j<m->classFieldCount;++j){
                const XL6_IrClassField* field=&m->classFields[j];
                if(field->initializerAbi!=XL6_IR_INITIALIZER_CONSTRUCTOR_REQUIRED||
                   !xl6IrConstructorContainsField(m,ins->type,field->declaration))continue;
                if(!values){
                    values=(unsigned char*)xrtCalloc(m->valueCount+1,1);storages=(unsigned char*)xrtCalloc(m->storageCount,1);
                    if(!values||!storages||!xl6IrConstructorAliases(m,fn,values,storages,ins->result)){valid=0;break;}
                }
                if(!xl6IrConstructorCfgField(m,fn,field,values,diags,ins->result)){valid=0;break;}
            }
            xrtFree(values);xrtFree(storages);
            if(!valid)return xl6IrDiag(diags,xl6IrNodeGet(m,ins->origin),"XLIR construction commits or exposes a pending resource object");
        }
    }
    return 1;
}
