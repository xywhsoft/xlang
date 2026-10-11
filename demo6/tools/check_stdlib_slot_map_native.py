"""Owned typed SlotMap foundation using actual supplier slot/generation storage.

Strict O0/O2, exact complete domains, independent value reads, instance-record
lifetimes, duplicate/self/Future edges, retryable phased retirement, whole
logical allocation prefixes, and compiled broken implementation controls.
Optional ASan/UBSan covers this native closure only. No language/shipping/full53
claim; generation exhaustion is a white-box actual supplier-record seam.
"""
from pathlib import Path
import argparse,ast,hashlib,json,os,re,shutil,subprocess,sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true'); args=parser.parse_args()
    root=args.root.resolve(); out=args.out.resolve(); cc=shutil.which('gcc'); assert cc and not out.exists()
    assert not args.sanitize or os.name!='nt'
    population=root/'tools/check_stdlib_dict_native.py'; tree=ast.parse(population.read_text(encoding='utf-8'))
    sources=ast.literal_eval(next(node.value for node in ast.walk(tree) if isinstance(node,ast.Assign)
        and any(isinstance(target,ast.Name) and target.id=='sources' for target in node.targets)))
    replacements={'test/cases/stdlib_dict_native_host.c':'test/cases/stdlib_slot_map_native_host.c',
        'test/support/dict_native.c':'test/support/slot_map_native.c','src/runtime/xl6_dict.c':'src/runtime/xl6_slot_map.c'}
    sources=[replacements.get(source,source) for source in sources]
    files={Path(__file__).resolve(),population,Path(cc),Path(sys.executable),*[root/source for source in sources]}
    for folder in ('src/runtime','src/api','release/include','lib/xrt','lib/xruntime','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; products=[]; objects=[]; prefixes=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Objects=objects,
        Prefixes=prefixes,NegativeControls=controls,Platform=sys.platform,SelectedNativeClosureSanitized=args.sanitize,
        LanguageSlotMapCutover=False,ShippingLibraryAcceptance=False,Full53Acceptance=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=['-Wl,--wrap='+name for name in ('xl6ErrorScopeBegin','xl6ErrorScopeBeginCleanup','xrtTypeCopyValue','xrtSlotMapInsert')]
    env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,command,reject=False):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Label=label,Command=command,Exit=result.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
        assert result.returncode!=0 if reject else result.returncode==0,(label,data[-6000:])
        if reject: assert b'Assertion' in data or b'assertion' in data,(label,data[-3000:])
        return data.decode(errors='replace')
    try:
        compiled={}
        for opt in ('O0','O2'):
            current=[]
            for index,source in enumerate(sources):
                obj=out/(opt+'-'+str(index)+'.o'); run(opt+'-compile-'+str(index),[cc,*flags,'-'+opt,'-c',source,'-o',obj]); current.append(obj)
                objects.append(dict(Path=str(obj),Sha256=sha(obj)))
            compiled[opt]=current; product=out/(opt+'-native'+suffix)
            run(opt+'-link',[cc,*current,*links,'-o',product]); products.append(dict(Path=str(product),Sha256=sha(product)))
            run(opt+'-vectors',[product,'vectors']); data=run(opt+'-prefixes',[product,'prefixes'])
            found=re.findall(r'slot-map (\S+) runs=(\d+) first-no-hit=(\d+)',data); assert len(found)==7
            for operation,runs,last in found:
                assert int(runs)==int(last)+1
                prefixes.append(dict(Optimization=opt,Operation=operation,Runs=int(runs),FirstNoHit=int(last)))
        source='src/runtime/xl6_slot_map.c'; body=(root/source).read_text(encoding='utf-8')
        mutations=[('instance-binding-omitted',
            'return key.instance==owner->instance && xrtSlotMapContains(&owner->storage,key.slot);',
            'return xrtSlotMapContains(&owner->storage,key.slot);'),
            ('stale-key-admitted',
            'return key.instance==owner->instance && xrtSlotMapContains(&owner->storage,key.slot);',
            'return key.instance==owner->instance && key.slot!=0;'),
            ('read-copy-omitted','success=xrtTypeCopyValue(type,output,xrtSlotMapGet(&owner->storage,key.slot));','success=true;'),
            ('failed-read-owner-leaked','xrtTypeDropValue(type,output); memset(output,0,size);','(void)type; (void)size;')]
        for name,before,after in mutations:
            assert body.count(before)==(2 if name=='failed-read-owner-leaked' else 1),name
            bad=out/(name+'.c'); bad.write_text(body.replace(before,after),encoding='utf-8')
            obj=out/(name+'.o'); product=out/(name+suffix)
            run(name+'-compile',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj])
            current=compiled['O0'].copy(); current[sources.index(source)]=obj
            run(name+'-link',[cc,*current,*links,'-o',product]); run(name+'-refused',[product,'vectors'],True)
            controls.append(dict(Name=name,Compiled=True,Rejected=True,Source=str(bad),SourceSha256=sha(bad),Product=str(product),ProductSha256=sha(product)))
        report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']
    print('Owned SlotMap native foundation complete: '+str(len(rows))+' phases, '+str(sum(row['Runs'] for row in prefixes))+' full-prefix runs')
if __name__=='__main__': main()
