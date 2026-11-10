"""Enumerate set parameter bits in the pinned native verifier, in native order.

No rule, node, row, diagnostic or module pass is omitted. Keep the original
function in the private TU for independent C checks. Compiler sources stay intact.
"""
from pathlib import Path
import hashlib
from check_native_bridge_catalog import function
from native_ir_derived_overlay import current_pair, current_contract
from native_c_interop_profile import pins_for


def derive(source: str, frontend: Path, source_digest: str, header_digest: str):
    pins=pins_for(frontend)
    if not current_pair(source_digest,header_digest) or not current_contract(pins):
        return source,dict(enabled=False,scope='Unreviewed parameter-mask dependency cohort; original enumeration retained')
    path=frontend/'src/ir/xl6_ir_foreign_parameter.c'
    digest=hashlib.sha256(path.read_bytes()).hexdigest()
    if digest!=pins['src/ir/xl6_ir_foreign_parameter.c']:
        raise ValueError('Native parameter verifier changed; audit its enumeration')
    original=function(path.read_text(encoding='utf-8'),'xl6IrVerifyForeignParameters')
    loop='for(size_t p=0;p<64;++p)if(mask&(UINT64_C(1)<<p)) {'
    if original.count(loop)!=1:raise ValueError('Native parameter-mask loop changed')
    updated=original.replace(loop,'for(uint64 xlsParameterBits=mask;xlsParameterBits;xlsParameterBits&=xlsParameterBits-1) {\n            size_t p=xlsIrMemoWordBit(xlsParameterBits);\n            XLS_IR_PARAMETER_BIT_HIT(p);',1)
    point='#include "src/ir/xl6_ir_foreign_parameter.c"'
    if source.count(point)!=1:raise ValueError('Native parameter verifier include seam changed')
    source=source.replace(point,'#define xl6IrVerifyForeignParameters xlsIrParameterVerifyOriginal\n'+point+'\n#undef xl6IrVerifyForeignParameters\n'+updated,1)
    source='#define XLS_NATIVE_IR_PARAMETER_BITS 1\n#ifndef XLS_IR_PARAMETER_BIT_HIT\n#define XLS_IR_PARAMETER_BIT_HIT(p) ((void)0)\n#endif\n'+source
    return source,dict(enabled=True,source=str(path),sourceSha256=digest,originalBodySha256=hashlib.sha256(original.encode()).hexdigest(),scope='Only ascending set-bit enumeration; reuse raw IR bit scan; full native row/node verifier and original control retained; no allocations or retained state')
