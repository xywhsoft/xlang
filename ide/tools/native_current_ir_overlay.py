"""Private current raw-function IR qualification; normal IR admission stays closed."""
from pathlib import Path
import hashlib
from native_index_overlay import profile as index_profile
from native_ir_memo_overlay import derive as raw_memo

PINS={
    'src/ir/xl6_ir.c':'fb1c73cb62d95540fac8f524b74b518f928e1668f531c87f7cd37f31e3f0353e',
    'src/ir/xl6_ir.h':'f06775de0768d65c1b96d88fcc79abc4a800dddb088d3b36b165a2471ef99651',
    'src/ir/xl6_ir_class_identity.c':'78c3c7d0baec527c30a6dcf3c3d76f90c2c5485354c6df084cf4be2cf679eb7d',
    'src/type/xl6_type_model.c':'3f70de039edcef2d6d6345457e388d015610cb5dd0acde360e4e45df94db5f8a',
    'src/type/xl6_type_id.h':'662ae3b34d48ef626c5973db765d7d6007475692aa2448d82245ee74a9ffbde7',
}


def derive(frontend:Path,output:Path):
    audited=index_profile(frontend)
    ast=frontend/'src/ast/xl6_ast.h'
    if audited[str(ast)]!='5ebb5cc003e51d954d02ad229307406a4990142d7813f755e0df3bbc9838fbb1':
        raise ValueError('current IR unit requires its current AST/parser/type/XRT cohort')
    for rel,digest in PINS.items():
        p=frontend/rel
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:
            raise ValueError('current IR asset changed; audit representation and phase contracts: '+rel)
        audited[str(p)]=digest
    original=frontend/'src/ir/xl6_ir.c'
    source=raw_memo(original.read_text(encoding='utf8'),current=True)
    compiled=output/'frontend/xl6_ir.c';compiled.parent.mkdir(parents=True,exist_ok=True)
    compiled.write_text(source,encoding='utf8',newline='\n')
    return original,compiled,dict(source=str(original),sourceSha256=PINS['src/ir/xl6_ir.c'],
        compiledSource=str(compiled),compiledSha256=hashlib.sha256(compiled.read_bytes()).hexdigest(),
        auditedDependencies=audited,scope='private current raw memo/owner units; original module passes/verifier; old scan/initializer/cancellation overlays are not admitted')
