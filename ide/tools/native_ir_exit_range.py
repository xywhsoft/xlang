"""Extract a selected-node native producer from a fully qualified cohort.

The ordinary producer stays byte-for-byte intact. This adds a fresh-output
primitive, not an incremental eligibility proof or a production cache.
"""
from pathlib import Path
import hashlib
import re
from check_native_bridge_catalog import function
from native_ir_exit_profile import qualify


def derive(source: str, frontend: Path, output: Path):
    policy = qualify(frontend)
    if not policy['enabled']:
        return source + '\n#include "analysis/frontend_ir_exit_range.inc"\n#include "analysis/frontend_ir_function_exit.inc"\n', policy
    from native_ir_type_resource import derive as type_resource
    source,type_resource_metadata=type_resource(source)
    original = (frontend/'src/ir/xl6_ir.c').read_text(encoding='utf8')
    body = function(original, 'xl6IrRefreshExitPlans')
    start = '    for ( i = 0; i < module->nodeCount; ++i ) {'
    end = '    return xl6IrRefreshExitRoutes(module);'
    if body.count(start) != 1 or body.count(end) != 1:
        raise ValueError('Exit selected-node seam changed; audit')
    loop = body[body.index(start):body.index(end)]
    loop = loop.replace(start, '''    for ( i = first; i < end; ++i ) {
        if (xlsIrExitRangeStop(module, limit, cancelled, user, work)) return 0;
        ++work->planNodes;''')
    generated = '''/* Extracted from the hash-qualified original producer. */
static int xlsIrExitRangePlans(XL6_IrModule* module, size_t first, size_t end,
 size_t limit, int(*cancelled)(void*), void* user, xls_ir_exit_range_work* work)
{
    size_t i;
''' + loop + '    return 1;\n}\n'
    target = output/'frontend/ir_exit_range_generated.inc'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(generated, encoding='utf8', newline='\n')
    projection_source=Path(__file__).resolve().parents[1]/'src/analysis/frontend_ir_exit_key.inc'
    projection=projection_source.read_text(encoding='utf8')
    projection=projection[:projection.index('static int exitInputKey(')]
    projection=projection.replace('xls_exit_pending','xls_fn_key')
    projection=re.sub(r'\bexit(\w+)(?=\s*\()',r'xlsFn\1',projection)
    projection_target=output/'frontend/ir_function_exit_projection.inc'
    projection_target.write_text(projection,encoding='utf8',newline='\n')
    return source + '\n#define XLS_NATIVE_IR_RANGE_PRODUCTION 1\n#include "analysis/frontend_ir_exit_range.inc"\n#include "analysis/frontend_ir_function_exit.inc"\n', dict(
        enabled=True, generated=str(target),
        generatedSha256=hashlib.sha256(target.read_bytes()).hexdigest(),
        producerSha256=policy['functions']['xl6IrRefreshExitPlans'],
        dependencyContractSha256=policy['dependencyContractSha256'],
        functionProjectionSource=str(projection_source),
        functionProjectionSourceSha256=hashlib.sha256(projection_source.read_bytes()).hexdigest(),
        functionProjectionGenerated=str(projection_target),
        functionProjectionGeneratedSha256=hashlib.sha256(projection_target.read_bytes()).hexdigest(),
        typeResources=type_resource_metadata,
        scope='Fresh selected-node production only; no cross-generation eligibility or default integration')
