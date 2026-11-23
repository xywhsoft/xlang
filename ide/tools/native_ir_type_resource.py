"""Latch failed required native type allocations in experimental builds.

Only used after the caller verifies the complete original producer cohort.
Default preprocessing maps every renamed allocation back to its native call.
"""
import hashlib,re
from check_native_bridge_catalog import function

FUNCTIONS=(
 'xl6IrTypeFinalizeDisplayName','xl6IrTypeDisplayNameMatches',
 'xl6IrTypeFinalizeRepresentation','xl6IrTypeInitParameterAbis',
 'xl6IrTypePoolInternTuple','xl6IrTypeInternArray','xl6IrInternAstType',
 'xl6IrInternDeclaredClassTypes','xl6IrBuildSubstituteTypeDepth',
 'xl6IrBuildInternClassSubstitutedTypeDepth','xl6IrBuildResultListType',
 'xl6IrNamedCallableAppliedType','xl6IrInternNamedCallableSignature',
 'xl6IrTypeInternCell','xl6IrTypeInternValueIterator',
 'xl6IrTypeInternCFunctionPointer','xl6IrTypeInternStorageRef',
 'xl6IrTypeInternPersistenceWire')
REPLACE={'xrtMalloc':'xlsIrTypeMalloc','xrtCalloc':'xlsIrTypeCalloc',
 'xrtRealloc':'xlsIrTypeRealloc','xrtStrDup':'xlsIrTypeStrDup',
 'xrtStrDupN':'xlsIrTypeStrDupN','xl6IrGrow':'xlsIrTypeGrow'}

def derive(source):
    grow=function(source,'xl6IrGrow')
    assert source.count(grow)==1
    header='#include "src/ir/xl6_ir.h"'
    if source.count(header)!=1:raise ValueError('Native type-resource declaration seam changed')
    source=source.replace(header,header+'\n#include "analysis/frontend_ir_type_resource.inc"',1)
    rows=[]
    for name in FUNCTIONS:
        before=function(source,name)
        calls=re.findall(r'\b('+'|'.join(REPLACE)+r')(?=\s*\()',before)
        if not calls:raise ValueError('Required type-allocation seam changed: '+name)
        after=re.sub(r'\b('+'|'.join(REPLACE)+r')(?=\s*\()',lambda m:REPLACE[m[0]],before)
        assert source.count(before)==1
        source=source.replace(before,after,1)
        rows.append(dict(name=name,beforeSha256=hashlib.sha256(before.encode()).hexdigest(),
                         afterSha256=hashlib.sha256(after.encode()).hexdigest(),calls=len(calls)))
    return '#define XLS_NATIVE_IR_TYPE_RESOURCE_GUARD 1\n'+source,dict(
        enabled=True,functions=rows,scope='Required type allocations only;experimental profile;default calls expand to the original native names')
