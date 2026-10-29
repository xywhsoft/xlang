"""Before/after ABI and object equivalence for declaration-only TLS schemas.
The old overlay is an explicit comparison control, never a shipping provider.
No TLS traffic, cryptographic/security tests, or shipping acceptance claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

CHANGED=['src/runtime/xl6_tls_metadata_internal.h','src/runtime/xl6_tls_verifier_internal.h',
    'src/runtime/xl6_tls_execution_symbols.inc','src/runtime/xl6_tls_verifier_symbols.inc',
    'src/runtime/xl6_tls_info.c','src/runtime/xl6_tls_server_stats.c',
    'src/runtime/xl6_tls_verification_policy.c','src/runtime/xl6_tls_verifier_create.c',
    'src/runtime/xl6_tls_verifier_context.c','src/stdlib/xl6_stdlib_tls_metadata.inc',
    'src/stdlib/xl6_stdlib_tls_verifier.inc','src/ir/xl6_ir_tls_metadata_contracts.inc',
    'src/ir/xl6_ir_tls_verifier_contracts.inc']
UNITS=['src/runtime/xl6_tls_info.c','src/runtime/xl6_tls_server_stats.c',
    'src/runtime/xl6_tls_verification_policy.c','src/runtime/xl6_tls_verifier_create.c',
    'src/runtime/xl6_tls_verifier_context.c','src/stdlib/xl6_stdlib_data.c',
    'src/compile/xl6_compile_unit.c','src/api/xl_api_symbols.c','src/ir/xl6_ir.c']
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['root','baseline-root','out']:parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();baseline=args.baseline_root.resolve();out=args.out.resolve()
    assert not out.exists();out.mkdir(parents=True);overlay=out/'old-schema-control'
    selected=CHANGED+['src/runtime/xl6_tls_metadata_fields.inc','src/runtime/xl6_tls_verification_policy_fields.inc']
    inputs={Path(__file__).resolve(),*[baseline/p for p in selected]}
    for relative in sorted(set(selected+UNITS)):
        # Only the schema change is reverted in this explicit A/B control.
        # Unrelated current source bytes (including user whitespace) are shared.
        source=baseline/relative if relative in selected else root/relative
        target=overlay/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target);assert sha(source)==sha(target);inputs.update([source,target])
    for folder in ['src','lib','release/include','include']:
        inputs.update(p for p in (root/folder).rglob('*') if p.is_file() and p.suffix in ['.h','.inc'])
    inputs.update(root/p for p in UNITS)
    gcc=Path(shutil.which('gcc')).resolve();inputs.add(gcc)
    report=dict(Complete=False,Inputs={str(p):sha(p) for p in sorted(inputs)},Rows=[],Comparisons=[],Failures=[],
        Scope=__doc__,ShippingAccepted=False,FullGoalComplete=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));rsp=out/(name+'.rsp');actual=command
        if Path(command[0])==gcc:
            rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n',encoding='utf8');actual=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');started=time.monotonic()
        with log.open('wb') as stream:code=subprocess.run(actual,cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=900).returncode
        report['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started))
        save();print(name,code,flush=True);assert code==0,str(log)
        return log.read_text(errors='replace')
    common=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation','-DXL_STATIC_EMBED',
        '-DXL_ENABLE_EXE_COMPILER=1','-DTCC_DISABLE_BUILTIN_VFS',
        '-ffile-prefix-map='+str(root)+'=/project','-ffile-prefix-map='+str(overlay)+'=/project',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc')]
    if os.name!='nt':common+=['-D_GNU_SOURCE']
    for relative in UNITS:
        name=Path(relative).stem
        for opt in ['O0','O2']:
            try:
                objects=[]
                for variant in ['old','new']:
                    obj=out/(name+'.'+opt+'.'+variant+'.o');objects.append(obj)
                    include=[]
                    if variant=='old':include=['-I'+str(overlay),'-I'+str(overlay/'src/runtime')]
                    include+=['-I'+str(root/Path(relative).parent)]
                    source=overlay/relative if variant=='old' else root/relative
                    run(name+'.'+opt+'.'+variant,[gcc,*include,*common,'-'+opt,'-c',source,'-o',obj])
                equal=sha(objects[0])==sha(objects[1]);report['Comparisons'].append(dict(Unit=relative,Optimization=opt,Equal=equal,
                    OldSha256=sha(objects[0]),NewSha256=sha(objects[1])))
                assert equal,(relative,opt,'Object changed; inspect, never normalize away a difference')
            except Exception as error:report['Failures'].append(dict(Unit=relative,Optimization=opt,Error=repr(error)));save()
    # The same 9 actual TUs instantiate layouts, generated public declarations,
    # IR contracts and symbol maps. A field type/order/signature change cannot
    # be excused by changing just a test-side expected sizeof value.
    report['InputsUnchanged']=all(sha(p)==h for p,h in report['Inputs'].items())
    report['Complete']=report['InputsUnchanged'] and not report['Failures'] and len(report['Comparisons'])==18
    report['Products']={str(p):sha(p) for p in out.rglob('*') if p.is_file() and p.name!='results.json'}
    save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
