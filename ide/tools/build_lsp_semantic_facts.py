"""Qualify semantic facts,native queries and IR against a verified semantic base.

Current frontend derivation is an explicit private unit probe. This target does
not open normal semantic/IR server guards,build a service,or publish binaries.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import time
from build_lsp_core import dependencies,prepare_native_sem


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    ide=Path(__file__).resolve().parents[1]
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--base-source-revision')
    ap.add_argument('--ir-phases',action='store_true',help='Qualify current IR phase optimizations and cooperative cancellation in the private IR fixture')
    ap.add_argument('--ir-source',type=Path,help='Audited shared IR source with the C-vararg arena-bounds correction; private phase qualification only')
    ap.add_argument('--fixture',default='analysis_sem_metadata_probe.inc',choices=['analysis_sem_metadata_probe.inc','analysis_sem_effects_probe.inc','analysis_sem_queries_probe.inc','analysis_ir_current_probe.inc'])
    args=ap.parse_args()
    if args.ir_phases and args.fixture!='analysis_ir_current_probe.inc':ap.error('--ir-phases requires analysis_ir_current_probe.inc')
    if bool(args.ir_source)!=args.ir_phases:ap.error('--ir-phases and --ir-source must be provided together')
    base=args.base.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    real_query=args.fixture in ('analysis_sem_queries_probe.inc','analysis_ir_current_probe.inc')
    original=json.loads((base/'build-result.json').read_text('utf8'))
    if original['status']!='passed':raise ValueError('base must have passed its complete build')
    archive=base/'xlang-semantic-substrate.a'
    if sha(archive)!=original['outputs'][archive.name]:raise ValueError('base archive changed')
    overrides={};review=base.parent/'review-v2.json'
    if review.is_file():overrides=json.loads(review.read_text('utf8')).get('historicalInputOverrides',{})
    report=dict(status='running',accepted=False,scope=__doc__,commands=[],inputs={},historicalBaseInputs={},outputs={})
    start=time.monotonic()
    def publish():
        (out/'build-result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
    env=dict(os.environ)
    command=next(c.copy() for c in original['commands'] if '-c' in c and Path(c[c.index('-c')+1]).name=='xl6_sem.c')
    cc=command[0];env['PATH']=str(Path(cc).parent)+os.pathsep+env.get('PATH','')
    def run(cmd,name):
        report['commands'].append(cmd)
        p=subprocess.run(cmd,cwd=ide,env=env,capture_output=True,timeout=240)
        (out/(name+'.log')).write_bytes(p.stdout+p.stderr);publish()
        if p.returncode:raise RuntimeError(name+' failed: '+str(p.returncode))
        return p
    try:
        for raw,digest in original['inputs'].items():
            p=Path(raw)
            if p.is_file() and sha(p)==digest:continue
            if raw in overrides and sha(overrides[raw])==digest:
                report['historicalBaseInputs'][raw]=overrides[raw];continue
            if p.suffix=='.py' and p.is_relative_to(ide/'tools') and args.base_source_revision:
                rel=p.relative_to(ide.parent).as_posix()
                data=subprocess.run(['git','show',args.base_source_revision+':'+rel],cwd=ide.parent,capture_output=True,check=True).stdout
                if hashlib.sha256(data).hexdigest()==digest:
                    target=out/'base-inputs'/p.name;target.parent.mkdir(exist_ok=True);target.write_bytes(data)
                    report['historicalBaseInputs'][raw]=str(target);continue
            raise ValueError('base input changed; rebuild the base: '+raw)
        report['baseReceipt']=str(base/'build-result.json');report['baseArchiveSha256']=sha(archive)
        frontend=Path(original['sourceOverlay']['source']).parents[2]
        (out/'frontend').mkdir()
        _,sem,overlay=prepare_native_sem(frontend,out,_current_unit_probe=True)
        report['semanticOverlay']=overlay
        if overlay.get('declarationEffects'):
            report['inputs'].update(overlay['declarationEffects']['auditedDependencies'])
        wrapper=out/'semantic_facts_probe.c'
        fixture=ide/'tests'/args.fixture
        wrapper.write_text('#include "'+sem.as_posix()+'"\n#include "'+fixture.as_posix()+'"\n',encoding='utf8',newline='\n')
        pool=out/'completion_pool.c'
        if not real_query:pool.write_text('#include "analysis/frontend_completion_pool.inc"\n#include <stdio.h>\n#include <stdlib.h>\nstatic int xlsCompletionAppendPool(xls_completion_pool_scope* s,const char* n,const char* k,XL6_SymbolId id,const XL6_AstNode* a,char* c){(void)s;(void)n;(void)k;(void)id;(void)a;(void)c;fputs("unit probe entered unqualified completion pool capture\\n",stderr);abort();}\n',encoding='utf8',newline='\n')
        fence=out/'signature_fence.c'
        if not real_query:fence.write_text('#include "analysis/signature_internal.h"\n#include <stdio.h>\n#include <stdlib.h>\nint xlsSignatureAppend(xls_signature_scope* s,xls_signature_record r){(void)s;(void)r;fputs("unit probe entered unqualified signature capture\\n",stderr);abort();}\n',encoding='utf8',newline='\n')
        # Construct fixture bindings with the real index TU's private layout
        # and XRT arrays. The encoder still calls the real lookup function.
        names=sorted(set(original['privateIndexProbe']['namespacedExports'])|{'xl6SemCBridgeMapJson'})
        factory=out/'entity_fixture.c'
        factory.write_text(''.join('#define '+name+' xlsMetadata_'+name+'\n' for name in names)+
            '#include "'+(base/'frontend/xl6_sem_index.c').as_posix()+'"\n'+
            ''.join('#undef '+name+'\n' for name in names)+
            'xls_entity_view* xlsProbeEntityBinding(uint32_t n,uint64_t id){xls_entity_view* v=xrtCalloc(1,sizeof(*v));if(!v)return NULL;xrtAtomic32Init(&v->references,1);xls_entity_binding b={.native=n,.stable=id};if(!xrtArrayInit(&v->symbols,sizeof(b)) || !xrtArrayPush(&v->symbols,&b) || !xrtArrayInit(&v->reverseSymbols,sizeof(b)) || !xrtArrayPush(&v->reverseSymbols,&b)){xlsMetadata_xlsEntityViewRelease(v);return NULL;}return v;}\n'
            'void xlsProbeEntityBindingDrop(xls_entity_view* v){xlsMetadata_xlsEntityViewRelease(v);}\n',encoding='utf8',newline='\n')
        sources=[wrapper,pool,fence,factory,*[ide/'src/analysis'/name for name in ('frontend_context.c','frontend_raw_body.c','frontend_symbols.c')]]
        if real_query:
            from native_context_overlay import completion
            from build_lsp_core import prepare_native_completion
            _,namespaced,namespace=completion(frontend,out)
            _,emitter,emission=prepare_native_completion(frontend,out,namespaced,_current_unit_probe=True)
            report['completionOverlays']=[namespace,emission]
            report['inputs'].update(namespace['auditedDependencies'])
            signature=out/'signature_query.c'
            signature.write_text('#include "analysis/signature_internal.h"\n#include "analysis/frontend_internal.h"\n#include "analysis/frontend_raw_body.h"\n#include "analysis/frontend_completion_path.h"\n#include <stdio.h>\n#include <string.h>\n#include "analysis/signature_data.inc"\n#include "analysis/signature_parse.inc"\n',encoding='utf8',newline='\n')
            sources=[wrapper,emitter,signature,factory,*sources[4:]]
            references={}
            for name in ('xl6_sem.c','xl6_completion.c'):
                original_source=frontend/'src/sem'/name
                reference=out/('reference_'+name);reference.write_bytes(original_source.read_bytes())
                sources.append(reference);references[str(reference)]=dict(original=str(original_source),sha256=sha(original_source))
            report['unmodifiedQueryReferences']=references
        if args.fixture=='analysis_ir_current_probe.inc':
            import re
            from native_current_ir_overlay import derive
            manifest_path=frontend.parent/'source-manifest.json'
            manifest=json.loads(manifest_path.read_text(encoding='utf8'))
            if manifest.get('sourceRevision')!='9192a99f96ecd83bd57ca6e53df6dfcdda7d2f15':raise ValueError('current IR fixture requires frozen committed frontend manifest')
            for rel,digest in manifest['files'].items():
                if sha(frontend/rel)!=digest:raise ValueError('frozen frontend changed: '+rel)
            report['inputs'][str(manifest_path)]=sha(manifest_path)
            report['frozenFrontendFiles']=len(manifest['files'])
            selected_ir,ir,ir_overlay=derive(frontend,out,phases=args.ir_phases,ir_source=args.ir_source);report['irOverlay']=ir_overlay
            report['inputs'].update(ir_overlay['auditedDependencies'])
            wrapper.write_text('#include "'+sem.as_posix()+'"\n',encoding='utf8',newline='\n')
            probe=out/'native_ir_probe.c';probe.write_text('#include "'+ir.as_posix()+'"\n#include "'+fixture.as_posix()+'"\n',encoding='utf8',newline='\n')
            names=sorted(set(re.findall(r'\b(xl6Ir[A-Za-z_0-9]+)\s*\(',(frontend/'src/ir/xl6_ir.h').read_text(encoding='utf8'))))
            header=out/'reference_ir_names.h';header.write_text(''.join('#define '+n+' xlsReference'+n.removeprefix('xl6')+'\n' for n in names),encoding='utf8',newline='\n')
            ir_references=[];ir_originals={};ir_copies={}
            for relative in ['src/ir/xl6_ir.c','src/ir/xl6_ir_class_identity.c']:
                original_ir=frontend/relative;reference=out/('reference_'+original_ir.name)
                compiled_ir=original_ir
                if args.ir_phases and relative=='src/ir/xl6_ir.c':
                    original_ir=selected_ir;compiled_ir=out/'reference_original_ir.c';compiled_ir.write_bytes(original_ir.read_bytes());ir_copies[str(compiled_ir)]=dict(source=str(original_ir),sha256=sha(original_ir))
                ir_originals[str(original_ir)]=sha(original_ir)
                reference.write_text('#include "reference_ir_names.h"\n#include "'+compiled_ir.as_posix()+'"\n',encoding='utf8',newline='\n');ir_references.append(reference)
            sources += [probe,frontend/'src/ir/xl6_ir_class_identity.c',ide/'src/analysis/typed_module.c',*ir_references,ide/'tests/analysis_ir_current_test.c']
            if args.ir_phases:
                sources += [ide/'src/analysis/frontend_ir_regions.c',ide/'tests/analysis_ir_phases_test.c',ide/'tests/analysis_ir_scan_test.c',ide/'tests/analysis_ir_vararg_bounds_test.c']
            report['irReferenceNamespace']=dict(names=names,header=str(header),originalSources=ir_originals,unmodifiedCopies=ir_copies)
        flags=command[:command.index('-c')]
        flags[1:1]=['-I'+str(ide/'tests')]
        if args.fixture=='analysis_ir_current_probe.inc':flags+=['-I'+str(frontend/'src/ir')]
        if args.ir_phases:flags+=['-DXLS_TEST_CURRENT_IR_PHASES=1']
        for p in [Path(__file__),ide/'tools/build_lsp_core.py',ide/'tools/native_index_overlay.py',
                  ide/'tools/native_type_source_overlay.py',ide/'tools/native_cancellation_overlay.py',
                  ide/'tools/native_semantic_effects_overlay.py',ide/'tools/native_context_overlay.py',
                  ide/'tools/native_signature_overlay.py',Path(cc),archive]:
            report['inputs'][str(p)]=sha(p)
        if args.fixture=='analysis_ir_current_probe.inc':
            for p in [ide/'tools/native_current_ir_overlay.py',ide/'tools/native_ir_memo_overlay.py',ide/'tools/native_ir_scan_overlay.py',ide/'tools/native_initializer_overlay.py']:report['inputs'][str(p)]=sha(p)
        def source_flags(source):
            renames=[]
            if source.name=='analysis_ir_scan_test.c':renames=['-Dmain=xlsCurrentIrScanTestMain']
            if source.name=='analysis_ir_vararg_bounds_test.c':renames=['-Dmain=xlsCurrentIrVarargBoundsMain']
            if source.name in ('reference_xl6_sem.c','reference_xl6_completion.c'):
                names=['xl6SemCheckModule','xl6SemCheckModuleWithIndex','xl6SemCheckModuleWithQuery',
                       'xl6CompletionPathEqual','xl6CompletionTarget','xl6CompletionAdd','xl6CompletionFree','xl6CompletionJson']
                renames=['-D'+n+'=xlsReference'+n.removeprefix('xl6') for n in names]
            return flags+renames
        for source in sources:
            dep=run(source_flags(source)+['-MM','-MT','xlsObject',str(source)],source.stem+'-dependencies')
            for p in dependencies(dep.stdout.decode('utf8')):report['inputs'][str(p)]=sha(p)
        objects=[]
        for source in sources:
            obj=out/(source.stem+'.o');run(source_flags(source)+['-c',str(source),'-o',str(obj)],source.stem);objects.append(obj);report['outputs'][obj.name]=sha(obj);publish()
        if args.fixture=='analysis_ir_current_probe.inc':
            nm=Path(cc).with_name('nm.exe' if os.name=='nt' else 'nm');report['inputs'][str(nm)]=sha(nm)
            exports={}
            for source in ir_references:
                result=run([str(nm),'--defined-only','--extern-only',str(out/(source.stem+'.o'))],source.stem+'-exports')
                found=re.findall(r'(?m)^[0-9a-fA-F]+\s+T\s+(\S+)$',result.stdout.decode('utf8').replace('\r',''))
                if not found or any(not n.startswith('xlsReferenceIr') for n in found):raise ValueError('IR reference public exports not fully namespaced')
                exports[source.name]=found
            report['irReferenceExports']=exports
        program=out/('semantic-facts-test.exe' if os.name=='nt' else 'semantic-facts-test')
        libs=['-lm']+(['-lws2_32','-lwinhttp','-liphlpapi','-lgdi32','-luser32','-lshell32','-lole32','-lwinmm','-lavrt','-lversion','-lbcrypt','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl'])
        wraps=['-Wl,--wrap=xrtStrDupN']
        if args.ir_phases:wraps+=['-Wl,--wrap=xrtArrayReserve']
        if args.fixture in ('analysis_sem_effects_probe.inc','analysis_sem_queries_probe.inc'):wraps+=['-Wl,--wrap=xrtStrBufReserve','-Wl,--wrap=xrtArrayReserve']
        run([cc,*[str(p) for p in objects],str(archive),'-Wl,--gc-sections',*wraps,*libs,'-o',str(program)],'link')
        p=run([str(program)],'run');print(p.stdout.decode('utf8'),flush=True)
        if any(sha(p)!=h for p,h in report['inputs'].items()):raise ValueError('probe input changed during qualification')
        report['outputs']={p.name:sha(p) for p in [*objects,program]};report['status']='passed'
    except BaseException as error:
        report.update(status='failed',error=repr(error));raise
    finally:
        report['durationSeconds']=time.monotonic()-start;publish()


if __name__=='__main__':
    main()
