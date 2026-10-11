"""Immutable SlotMap key projection using genuine XRT Value/typed-set/hash.

Strict O0/O2, genuine key ownership, semantic boxed equality, authenticated
unbox, checked/no-diagnostic partial-copy rollback, native typed content policy
and complete allocation prefixes. Native foundation only: language SlotMapKey,
direct language SlotMapKey/IR/frames and shipping/full53 acceptance are NOT
claimed. The generic erased native Set key policy is exercised explicitly.
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
    replacements={'test/cases/stdlib_dict_native_host.c':'test/cases/stdlib_slot_map_key_value_host.c',
        'test/support/dict_native.c':'test/support/slot_map_key_value_native.c','src/runtime/xl6_dict.c':'src/runtime/xl6_slot_map.c'}
    sources=[replacements.get(source,source) for source in sources]
    sources+=['src/runtime/xl6_slot_map_key.c','src/runtime/xl6_slot_map_key_value.c',
        'src/runtime/xl6_native_leaf_value.c','src/runtime/xl6_set_key_policy.c']
    files={Path(__file__).resolve(),population,Path(cc),Path(sys.executable),*[root/source for source in sources]}
    for folder in ('src/runtime','src/api','release/include','lib/xrt','lib/xruntime','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; products=[]; objects=[]; prefixes=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Objects=objects,
        Prefixes=prefixes,NegativeControls=controls,Platform=sys.platform,SelectedNativeClosureSanitized=args.sanitize,
        LanguageSlotMapKeyCutover=False,ErasedSetKeyCutover=True,ShippingLibraryAcceptance=False,Full53Acceptance=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=['-Wl,--wrap='+name for name in ('xl6ErrorScopeBegin','xrtTypeCopyValue','xrtValueTypeIdBind','xrtValueRelease')]
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
        if reject: assert b'Assertion' in data or b'assertion' in data or b'runtime error:' in data,(label,data[-3000:])
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
            found=re.findall(r'slot-key (\S+) runs=(\d+) first-no-hit=(\d+)',data); assert len(found)==5
            for operation,runs,last in found:
                assert int(runs)==int(last)+1
                prefixes.append(dict(Optimization=opt,Operation=operation,Runs=int(runs),FirstNoHit=int(last)))
        native='src/runtime/xl6_native_leaf_value.c'
        mutations=[('producer-authentication-omitted',native,
            'return box && box->profile==profile;','(void)box; return profile && xrtValueTypeId(value)==profile->identity;'),
            ('box-address-equality',native,
            'return a->profile==b->profile && a->profile->content->Equal(a->value,b->value,a->profile->content->UserData);',
            '(void)a; (void)b; return left==right;'),
            ('unbox-copy-omitted',native,
            'copied=xrtTypeCopyValue(type,output,input);','copied=true;'),
            ('failed-copy-owner-leaked',native,
            'xrtTypeDropValue(type,output); copied=false;',
            'copied=false;'),
            ('occupied-direct-output-accepted',native,
            'xrtMemRangesOverlap(input,type->Size,output,type->Size) || !profile->Empty(output)',
            'xrtMemRangesOverlap(input,type->Size,output,type->Size)'),
            ('misaligned-direct-output-accepted',native,
            '!aligned(input,type) || !aligned(output,type) ||','false ||'),
            ('overlapping-direct-output-accepted',native,
            'xrtMemRangesOverlap(input,type->Size,output,type->Size) ||',''),
            ('late-unbox-owner-published',native,
            'if (copied && (xrtGetError() || scope.failed)) {','if (false && copied && (xrtGetError() || scope.failed)) {'),
            ('erased-value-treated-as-reference','src/runtime/xl6_set_key_policy.c',
            'if (xl6NativeLeafValueIsV1(value)) return true;','if (false && xl6NativeLeafValueIsV1(value)) return true;')]
        for name,source,before,after in mutations:
            body=(root/source).read_text(encoding='utf-8')
            assert body.count(before)==1,name
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
    print('Immutable SlotMap key projection complete: '+str(len(rows))+' phases, '+str(sum(row['Runs'] for row in prefixes))+' full-prefix runs')
if __name__=='__main__': main()
