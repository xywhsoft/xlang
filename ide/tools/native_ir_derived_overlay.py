"""Preserve audited native pure-pass bodies; enumerate uncached function ranges.

The current IR cohort and all includes are pinned by native_current_ir_overlay.
Reference bodies are extracted before any enumeration change. Legacy C is not
adapted. Query/type/output/ownership boundaries require a fresh source audit
when this cohort changes; a raw memo identity alone is never a final-IR proof.
"""
from pathlib import Path
import hashlib
from check_native_bridge_catalog import function

# The reviewed current retirement cohort,including its exact LF checkout.
# Other already-supported compiler cohorts keep their original pure passes.
COHORTS={
    ('a5cb13b8ea19560a592717c5799c9a2e89ebb74fe331a6f5918d1ca46e8f7b53','2673e4889b4cd4cd68a49f0d87b9991a01959addc005136df4cd18e10997f2b4'),
    ('10ebdce6803d96609ea7aac51e9b395b0d04a15fb8190d39b8d285295c2975d4','1e77f92bffbd498d676077921a872444b934f78cf1b6f1a8ee33b33982ff40c0'),
}

def derive(source: str, output: Path, source_digest: str, header_digest: str):
    if (source_digest,header_digest) not in COHORTS:
        return source,dict(enabled=False,scope='Unreviewed derived-phase compiler pair;retain original native pure passes')
    references=[];bodies={}
    for name,renamed,phase,loops in (
        ('xl6IrBuildOperatorErrorAbis','xlsIrBuildOperatorAbisOriginal','OPERATOR',1),
        ('xl6IrBuildIndexWriteAbis','xlsIrBuildIndexWriteAbisOriginal','INDEX',2),
    ):
        original=function(source,name);body=original
        signature='static int '+name+'(XL6_IrModule* module)'
        if body.count(signature)!=1:raise ValueError('native pure-pass declaration changed: '+name)
        loop='for ( i = 0; i < module->nodeCount; ++i )'
        if body.count(loop)!=loops:raise ValueError('native pure-pass enumeration changed: '+name)
        references.append(body.replace(signature,'static int xlsDerivedReference'+phase+'(XL6_IrModule* module)',1))
        body=body.replace(signature,'static int '+renamed+'(XL6_IrModule* module)',1)
        body=body.replace(loop,'for ( i = xlsIrDerivedNext(module,XLS_IR_DERIVED_'+phase+',0); i < module->nodeCount && (XLS_IR_DERIVED_BODY_HIT(XLS_IR_DERIVED_'+phase+'),1); i = xlsIrDerivedNext(module,XLS_IR_DERIVED_'+phase+',i+1) )')
        source=source.replace(original,body,1);bodies[name]=hashlib.sha256(original.encode()).hexdigest()
    point='    if ( xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1) || !xl6IrInternAsyncInvocationTypes(module, root, root) )'
    if source.count(point)!=1:raise ValueError('native complete raw-module fence changed')
    source=source.replace(point,'    xlsIrDerivedActivate(module);\n'+point,1)
    point='''        if ( node->type == XL6_IR_TYPE_INVALID ) node->type = signature;
        if ( node->storageType == XL6_IR_TYPE_INVALID ) node->storageType = signature;'''
    named=function(source,'xl6IrBuildNamedCallableAbis')
    references.append(named.replace('static int xl6IrBuildNamedCallableAbis(', 'static int xlsDerivedReferenceNAMED(',1))
    bodies['xl6IrBuildNamedCallableAbis']=hashlib.sha256(named.encode()).hexdigest()
    if source.count(point)!=1:raise ValueError('native named-callable type writes changed')
    source=source.replace(point,'''        int xlsDerivedInputChanged=node->type==XL6_IR_TYPE_INVALID || node->storageType==XL6_IR_TYPE_INVALID;
'''+point+'''
        if(xlsDerivedInputChanged)xlsIrDerivedInputPatch(module,i);''',1)
    reference=output/'frontend/ir_derived_reference.inc'
    reference.parent.mkdir(parents=True,exist_ok=True)
    reference.write_text('\n'.join(references),encoding='utf8',newline='\n')
    source='#define XLS_NATIVE_IR_DERIVED_PHASES 1\n#ifndef XLS_IR_DERIVED_BODY_HIT\n#define XLS_IR_DERIVED_BODY_HIT(phase) ((void)0)\n#endif\n'+source
    return source,dict(enabled=True,sourceSha256=source_digest,headerSha256=header_digest,originalBodies=bodies,reference=str(reference),referenceSha256=hashlib.sha256(reference.read_bytes()).hexdigest(),
        scope='Pure operator/index ABI outputs only;immutable per-function assets;exact type/runtime/union context and actual raw cleanliness;original bodies on misses;full original verifier retained')
