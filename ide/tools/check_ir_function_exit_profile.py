"""Exercise fail-closed generation for the experimental function-exit path."""
from pathlib import Path
from unittest.mock import patch
import argparse,hashlib,json
import native_ir_exit_range as adapter
import native_ir_exit_profile as policy
import native_ir_type_resource as resources
from build_lsp_core import prepare_native_ir
from native_field_bindings_overlay import ir as field_ir


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frontend',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=args.frontend.resolve();out=args.output.resolve()
    out.mkdir(parents=True,exist_ok=False);(out/'frontend').mkdir()
    record=dict(status='failed',cases=[])
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    try:
        _,compiled,metadata=prepare_native_ir(root,out)
        field_ir(compiled,out/'native_fields/xl6_ir.c')
        details=metadata['exitRangeProduction'];assert details['enabled']
        assert len(details['typeResources']['functions'])==len(resources.FUNCTIONS)==81
        projection=Path(details['functionProjectionGenerated']).read_text()
        assert 'src->exitKind'in projection and 'src->xlsFnKind'not in projection
        assert 'static int xlsFnNodeKey'in projection
        record['cases'].append('actual-standard-pipeline-and-81-native-allocation-seams')
        original=(root/'src/ir/xl6_ir.c').read_text(encoding='utf8')
        original_read=Path.read_bytes
        for name in('src/ir/xl6_ir.c','src/ir/xl6_ir.h'):
            target=(root/name).resolve()
            with patch.object(Path,'read_bytes',lambda p:original_read(p)+(b'\n'if p.resolve()==target else b'')):
                source,guard=adapter.derive(original,root,out)
                assert not guard['enabled'] and 'XLS_NATIVE_IR_TYPE_RESOURCE_GUARD'not in source
            record['cases'].append('changed-'+name+'-declined')
        pins=dict(policy.pins_for(root));dependency=next(n for n in pins if n not in('src/ir/xl6_ir.c','src/ir/xl6_ir.h'))
        pins[dependency]='0'*64
        with patch.object(policy,'pins_for',return_value=pins):
            _,guard=adapter.derive(original,root,out);assert not guard['enabled']
        record['cases'].append('changed-complete-dependency-contract-declined')
        with patch.object(adapter,'qualify',return_value=dict(enabled=False,reason='unreviewed')):
            source,guard=adapter.derive(original,root,out)
            assert not guard['enabled'] and source==original+'\n#include "analysis/frontend_ir_exit_range.inc"\n#include "analysis/frontend_ir_function_exit.inc"\n'
        record['cases'].append('unknown-cohort-only-disabled-entrypoints')
        extract=adapter.function
        with patch.object(adapter,'function',lambda text,name:extract(text,name).replace('    for ( i = 0; i < module->nodeCount; ++i ) {','    for (;;) {')):
            try:adapter.derive(original,root,out)
            except ValueError:pass
            else:raise AssertionError('Changed selected-node seam admitted')
        record['cases'].append('changed-selected-node-seam-rejected')
        with patch.object(resources,'FUNCTIONS',(*resources.FUNCTIONS,'missing_required_type_constructor')):
            try:adapter.derive(original,root,out)
            except ValueError:pass
            else:raise AssertionError('Missing type-allocation seam admitted')
        record['cases'].append('missing-required-type-allocation-seam-rejected')
        record['generation']=details
        record['outputs']={str(p):sha(p)for p in out.rglob('*')if p.is_file()}
        record['status']='passed';print(json.dumps(dict(status='passed',cases=len(record['cases']))))
    finally:
        (out/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')


if __name__=='__main__':main()
