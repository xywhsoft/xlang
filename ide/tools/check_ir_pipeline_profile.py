"""Exercise source-closed generation of the complete native pipeline."""
from pathlib import Path
from unittest.mock import patch
import argparse,hashlib,json
import native_ir_pipeline_overlay as adapter
import native_ir_pipeline_nodes as node_adapter
from check_native_bridge_catalog import function
from build_lsp_core import prepare_native_ir

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--frontend',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);record=dict(status='failed',cases=[])
    try:
        _,compiled,meta=prepare_native_ir(args.frontend.resolve(),out)
        pipeline=meta['nativePipeline'];assert pipeline['enabled']and pipeline['phases']==list(adapter.PHASES)and len(adapter.PHASES)==38
        text=Path(pipeline['generated']).read_text();assert function(text,'xlsIrPipelinePhaseOriginal').count('case ')==38 and text.count('xl6IrVerifyModule(module, diags)')==1
        assert function(text,'xlsIrPipelinePhaseNodesOriginal').count('case ')==15 and pipeline['nodeRanges']['phases']==list(node_adapter.PHASES)
        record['cases'].append('actual-standard-pipeline-38-phases-and-mandatory-complete-verifier')
        source=compiled.read_text();original=source[:source.index('\n#define XLS_NATIVE_IR_PIPELINE_SOURCE 1\n')]
        changed=original.replace('    xlsIrDerivedActivate(module);','    /* changed native ordering */',1)
        try:adapter.derive(changed,args.frontend,out)
        except ValueError:pass
        else:raise AssertionError('Changed native body admitted')
        record['cases'].append('changed-native-body-rejected')
        with patch.object(adapter,'BODY_SHA','0'*64):
            try:adapter.derive(original,args.frontend,out)
            except ValueError:pass
            else:raise AssertionError('Missing builder receipt admitted')
        record['cases'].append('wrong-native-builder-receipt-rejected')
        with patch.object(adapter,'PHASES',tuple(reversed(adapter.PHASES))):
            try:adapter.derive(original,args.frontend,out)
            except ValueError:pass
            else:raise AssertionError('Changed phase order admitted')
        record['cases'].append('changed-phase-order-rejected')
        read_bytes=Path.read_bytes
        for filename in('src/ir/xl6_ir.c','src/ir/xl6_ir.h'):
            target=(args.frontend/filename).resolve()
            with patch.object(Path,'read_bytes',lambda p:read_bytes(p)+(b'\n'if p.resolve()==target else b'')):
                result,policy=adapter.derive(original,args.frontend,out);assert not policy['enabled']and result==original+'\n#include "analysis/frontend_ir_pipeline.inc"\n'
            record['cases'].append('changed-'+filename+'-declined')
        with patch.object(adapter,'qualify',return_value=dict(enabled=False,reason='unreviewed dependency cohort')):
            _,policy=adapter.derive(original,args.frontend,out);assert not policy['enabled']
        record['cases'].append('unknown-complete-dependency-cohort-declined')
        node_name=next(iter(node_adapter.PHASES.values()))[0]
        with patch.object(node_adapter,'PHASES',{2:(node_name,'0'*64)}):
            try:adapter.derive(original,args.frontend,out)
            except ValueError:pass
            else:raise AssertionError('Changed node phase receipt admitted')
        record['cases'].append('changed-node-phase-receipt-rejected')
        body=function(original,node_name);bad=body.replace('{','{ for(;;)break;',1);changed=original.replace(body,bad,1)
        with patch.object(node_adapter,'PHASES',{2:(node_name,hashlib.sha256(bad.encode()).hexdigest())}):
            try:adapter.derive(changed,args.frontend,out)
            except ValueError:pass
            else:raise AssertionError('Multiple dependent node loops admitted')
        record['cases'].append('multiple-node-loops-rejected')
        record.update(status='passed',generatedSha256=hashlib.sha256(Path(pipeline['generated']).read_bytes()).hexdigest());print(json.dumps(dict(status='passed',cases=len(record['cases']))))
    finally:(out/'result.json').write_text(json.dumps(record,indent=2)+'\n')
if __name__=='__main__':main()
