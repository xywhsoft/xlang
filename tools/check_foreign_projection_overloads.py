"""Check direct synchronous projection aliases and refuse conflicting contracts.

The caller supplies the actual compiler and separately built sealed-IR checker.
No compiler build or shipping publication is inferred from these process runs.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--ir-checker',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();root,compiler,ir,out=[p.resolve() for p in (args.root,args.compiler,args.ir_checker,args.out)]
    runtime=compiler.parent;source=root/'test/cases/foreign_projection_overloads.xl'
    files=[Path(__file__).resolve(),compiler,ir,source,
           *[root/'test/cases'/('foreign_projection_overloads'+suffix) for suffix in ('.c','.h','_ir.c')]]
    files.extend(p for p in runtime.glob('*.dll') if p.is_file())
    files.extend(root/p for p in ['src/sem/xl6_sem.c','src/sem/xl6_sem_foreign_projection_compat.c',
        'src/ir/xl6_ir_foreign_compat.c','src/ir/xl6_ir_foreign_projection_compat.c',
        'src/runtime/xl6_foreign_record_abi.h','src/runtime/xl6_foreign_view_abi.h',
        'src/cgen/xl6_ir_cgen_foreign_array.c'])
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs={str(path):sha(path) for path in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(passed=False,shippingDelivery=False,compilerBuild='caller supplied; independently qualify the compiler cohort',
                inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,expected=0):
        command=list(map(str,command));result=subprocess.run(command,cwd=root/'release/win_x64' if os.name=='nt' else root/'release/linux_x64',
            capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=result.returncode,logSha256=sha(log)))
        save()
        if (result.returncode==0)!=(expected==0):raise RuntimeError(name+' failed: '+str(log))
        return result
    cfile,hfile=out/'module.c',out/'module.h'
    run('emit',[compiler,'cgen',source,'--emit',cfile,'--emit-header',hfile])
    assert '#include <__xl_runtime/xl6_foreign_record_abi.h>' in hfile.read_text(encoding='utf-8')
    # The matching native .c sidecar is discovered by the ordinary CLI; do not
    # pass the same source twice through explicit helper and automatic discovery.
    include=['--inc-path',root,'--inc-path',runtime/'include']
    run('jit',[compiler,'run',source,*include])
    binary=out/('projection.exe' if os.name=='nt' else 'projection')
    run('aot-build',[compiler,'build',source,'-o',binary,*include])
    run('aot-run',[binary])
    result=run('sealed-ir',[ir,source])
    count=re.search(rb'Projection overload sealed IR mutations: (\d+)',result.stdout)
    assert count and int(count[1])>=10
    report['irMutations']=int(count[1])
    text=source.read_text(encoding='utf-8')
    variants={
        'pointer':text.replace('@c_span(0,"const ProjectionPair*","int")','@c_span(0,"const int*","int")'),
        'writable':text.replace('@c_span(0,"const ProjectionPair*","int")','@c_span(0,"ProjectionPair*","int")'),
        'count':text.replace('@c_span(0,"const ProjectionPair*","int")','@c_span(0,"const ProjectionPair*","size_t")'),
        'record':text.replace('@c_pack(0,"ProjectionDescriptor",0,2)','@c_pack(0,"ProjectionOtherDescriptor",0,2)',1),
        'effect':text.replace('func scale(c_int value,c_int multiplier)','@pure func scale(c_int value,c_int multiplier)'),
        'success':text.replace('@c_result("status",0) @c_pack(0,"ProjectionDescriptor",0,2)','@c_result("status",1) @c_pack(0,"ProjectionDescriptor",0,2)'),
        'size-index':text.replace('@c_array_group(0,0,1,"int",1)','@c_array_group(0,0,1,"int",63)'),
    }
    for name,body in variants.items():
        assert body!=text
        case=out/(name+'.xl');case.write_text(body,encoding='utf-8');target=out/(name+'.c')
        run('reject-'+name,[compiler,'cgen',case,'--emit',target],1)
        assert not target.exists()
    for name in ('foreign_pack','foreign_pack_pod','foreign_array_projection','foreign_value_projection',
                 'foreign_decl_static_callback','foreign_nested_pack_null','foreign_null_compat','foreign_nullable_span'):
        run('regression-'+name,[compiler,'cgen',root/'test/cases'/(name+'.xl'),'--emit',out/(name+'.c')])
    assert all(sha(path)==expected for path,expected in inputs.items())
    report.update(passed=True,inputsUnchanged=True);save()
    print(json.dumps(dict(passed=True,rows=len(report['rows']),irMutations=report['irMutations'])))


if __name__=='__main__':main()
