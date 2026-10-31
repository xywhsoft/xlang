"""Audited current IR components; normal builds require the corrected phase cohort."""
from pathlib import Path
import hashlib
from native_index_overlay import profile as index_profile
from native_ir_memo_overlay import derive as raw_memo
from native_current_retirement_profile import PINS as RETIREMENT_PINS,IR_DIGESTS as RETIREMENT_IR_DIGESTS
from native_current_ir_profile import PINS as IR_DEPENDENCIES,PROFILES as IR_PROFILES

PINS={
    'src/ir/xl6_ir.c':'fb1c73cb62d95540fac8f524b74b518f928e1668f531c87f7cd37f31e3f0353e',
    'src/ir/xl6_ir.h':'f06775de0768d65c1b96d88fcc79abc4a800dddb088d3b36b165a2471ef99651',
    'src/ir/xl6_ir_class_identity.c':'78c3c7d0baec527c30a6dcf3c3d76f90c2c5485354c6df084cf4be2cf679eb7d',
    'src/type/xl6_type_model.c':'3f70de039edcef2d6d6345457e388d015610cb5dd0acde360e4e45df94db5f8a',
    'src/type/xl6_type_id.h':'662ae3b34d48ef626c5973db765d7d6007475692aa2448d82245ee74a9ffbde7',
}
# Shared bounds correction ee9a87a73; frozen ABI cohort remains independently
# pinned above. The phase target compiles this exact original source as oracle.
PHASE_IR_SHA256='e1ea31c336278b6729369e9f6a1bb4d86a4e945018a88f8c672c0a643e089628'
# Git archive's configured checkout conversion produces this exact CRLF copy.
# Both constants name the same committed correction; arbitrary normalization
# must not admit otherwise unknown source contents.
PHASE_IR_DIGESTS=(PHASE_IR_SHA256,'5c7a46e13b8f774c0fd27c3d66fc9c064aafc73b20386d2e9a448e9df7d78e6a')+RETIREMENT_IR_DIGESTS


def derive(frontend:Path,output:Path,*,phases=False,ir_source=None):
    audited=index_profile(frontend)
    ast=frontend/'src/ast/xl6_ast.h'
    if audited[str(ast)]!='5ebb5cc003e51d954d02ad229307406a4990142d7813f755e0df3bbc9838fbb1':
        raise ValueError('current IR unit requires its current AST/parser/type/XRT cohort')
    selected_pins={**PINS,**RETIREMENT_PINS} if hashlib.sha256((frontend/'src/ir/xl6_ir.c').read_bytes()).hexdigest() in RETIREMENT_IR_DIGESTS else PINS
    for rel,digest in selected_pins.items():
        p=frontend/rel
        if rel=='src/ir/xl6_ir.c' and p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest() in PHASE_IR_DIGESTS:digest=hashlib.sha256(p.read_bytes()).hexdigest()
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:
            raise ValueError('current IR asset changed; audit representation and phase contracts: '+rel)
        audited[str(p)]=digest
    dependency_cohort=next((name for name,values in IR_PROFILES.items()
        if all((frontend/rel).is_file() and hashlib.sha256((frontend/rel).read_bytes()).hexdigest()==digest for rel,digest in values.items())),None)
    if dependency_cohort is None:raise ValueError('current IR include cohort changed or mixed; audit phase contracts')
    for rel,digest in IR_PROFILES[dependency_cohort].items():
        p=frontend/rel
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:
            raise ValueError('current IR included dependency changed; audit phase contracts: '+rel)
        audited[str(p)]=digest
    profile_source=Path(__file__).with_name('native_current_ir_profile.py')
    audited[str(profile_source)]=hashlib.sha256(profile_source.read_bytes()).hexdigest()
    original=frontend/'src/ir/xl6_ir.c'
    selected=original
    if phases:
        if ir_source is None and audited[str(original)] not in PHASE_IR_DIGESTS:raise ValueError('current phase qualification requires the audited shared C-vararg bounds correction')
        selected=Path(ir_source).resolve(strict=True) if ir_source is not None else original
        selected_digest=hashlib.sha256(selected.read_bytes()).hexdigest()
        if selected_digest not in PHASE_IR_DIGESTS:raise ValueError('corrected current IR source changed; audit phase and ownership contracts')
        audited[str(selected)]=selected_digest
    elif ir_source is not None:raise ValueError('IR source override is private to phase qualification')
    source=selected.read_text(encoding='utf8')
    if phases:
        from native_ir_scan_overlay import derive as scans
        from native_cancellation_overlay import ir_cancellation
        from native_initializer_overlay import derive as initializers
        from native_ir_derived_overlay import derive as derived
        source,seams=scans(source,current=True)
        source=ir_cancellation(source,current=True)
        source=initializers(source)
        source,derived_overlay=derived(source,output,hashlib.sha256(selected.read_bytes()).hexdigest(),audited[str(frontend/'src/ir/xl6_ir.h')])
        for name in ('native_ir_derived_overlay.py','check_native_bridge_catalog.py'):
            dependency=Path(__file__).with_name(name)
            audited[str(dependency)]=hashlib.sha256(dependency.read_bytes()).hexdigest()
    source=raw_memo(source,current=True)
    compiled=output/'frontend/xl6_ir.c';compiled.parent.mkdir(parents=True,exist_ok=True)
    compiled.write_text(source,encoding='utf8',newline='\n')
    return selected,compiled,dict(source=str(selected),sourceSha256=hashlib.sha256(selected.read_bytes()).hexdigest(),
        baseFrontendSource=str(original),baseFrontendSourceSha256=audited[str(original)],
        compiledSource=str(compiled),compiledSha256=hashlib.sha256(compiled.read_bytes()).hexdigest(),
        auditedDependencies=audited,phaseQualification=phases,
        irDependencyCohort=dependency_cohort,
        derivedPhases=derived_overlay if phases else None,
        auditedScanSeams=seams if phases else 0,
        originalAncestorContainmentPreserved=phases,
        scope=('private current complete module phases/scans/initializer/cancellation qualification; original bounded ancestor containment retained'
               if phases else 'private current raw memo/owner units; original module passes/verifier; old scan/initializer/cancellation overlays are not admitted'))
