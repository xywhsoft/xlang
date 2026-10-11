"""Owning native record storage, not source SlotMapKey/IR-frame completion.

Real XRT typed Array/List/Set/Dict/slot-map/value/hash. O0/O2 authenticated
projection, equality without ordering, occupied-output refusal, partial-copy
rollback, explicit graph-policy capability and whole logical OOM prefixes.
"""
from pathlib import Path
import argparse,ast,hashlib,json,os,re,shutil,subprocess,sys
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','out'): p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
    root=a.root.resolve(); out=a.out.resolve(); cc=shutil.which('gcc')
    assert cc and not out.exists() and (not a.sanitize or os.name!='nt')
    population=root/'tools/check_stdlib_dict_native.py'; tree=ast.parse(population.read_text(encoding='utf-8'))
    sources=ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign)
        and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets)))
    sources[0]='test/cases/stdlib_native_value_host.c'; sources[1]='test/support/native_value_native.c'
    sources+=['src/runtime/xl6_slot_map.c','src/runtime/xl6_slot_map_key.c','src/runtime/xl6_slot_map_key_value.c',
        'src/runtime/xl6_array_native_value_admission.c','src/runtime/xl6_array.c',
        'src/runtime/xl6_list.c','src/runtime/xl6_set.c','src/runtime/xl6_set_key_policy.c','src/runtime/xl6_native_leaf_value.c']
    files={Path(__file__).resolve(),population,Path(cc),Path(sys.executable),*[root/s for s in sources],
        root/'test/support/slot_map_key_value_native.c',root/'src/ir/xl6_ir.h'}
    for folder in ('src/runtime','src/api','release/include','lib/xrt','lib/xruntime','test/support','src/type','src/common','src/ast','src/diag'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]; out.mkdir(parents=True)
    rows=[]; objects=[]; products=[]; prefixes=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Objects=objects,Products=products,
        Prefixes=prefixes,NegativeControls=controls,SelectedNativeClosureSanitized=a.sanitize,
        LanguageSlotMapKeyCutover=False,GeneralErasedSetKeyCutover=False,ShippingLibraryAcceptance=False,Full53Acceptance=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']; links=['-Wl,--wrap=xrtTypeCopyValue']
    env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
    if a.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']; links+=['-fsanitize=address,undefined','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,command,reject=False):
        command=list(map(str,command)); r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=r.stdout+r.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Label=label,Command=command,Exit=r.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if reject else r.returncode==0),(label,data[-7000:])
        if reject: assert b'assert' in data.lower(),(label,data[-1500:])
        return data.decode(errors='replace')
    try:
        compiled={}
        for opt in ('O0','O2'):
            current=[]
            for i,source in enumerate(sources):
                obj=out/(opt+'-'+str(i)+'.o'); run(opt+'-compile-'+str(i),[cc,*flags,'-'+opt,'-c',source,'-o',obj]); current.append(obj)
                objects.append(dict(Path=str(obj),Sha256=sha(obj)))
            compiled[opt]=current; product=out/(opt+suffix)
            run(opt+'-link',[cc,*current,*links,'-o',product]); products.append(dict(Path=str(product),Sha256=sha(product)))
            run(opt+'-vectors',[product,'vectors']); data=run(opt+'-prefixes',[product,'prefixes'])
            found=re.findall(r'native-value (\S+) runs=(\d+) first-no-hit=(\d+)',data); assert len(found)==7
            for operation,runs,last in found:
                assert int(runs)==int(last)+1
                prefixes.append(dict(Optimization=opt,Operation=operation,Runs=int(runs),FirstNoHit=int(last)))
        changes=[('failed-native-copy-owner-leaked','src/runtime/xl6_native_leaf_value.c',
            'xrtTypeDropValue(type,output); copied=false;','copied=false;'),
            ('native-copy-omitted','src/runtime/xl6_native_leaf_value.c',
            'copied=xrtTypeCopyValue(type,output,input);','copied=true;'),
            ('record-address-equality','src/runtime/xl6_sequence.c','equal=content->Equal(left,right,content->UserData);',
            'equal=left==right; (void)content;'),
            ('record-set-content-omitted','src/runtime/xl6_set.c',
            'layout->value.transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1 ? xl6SequenceNativeValuePolicyV1(&policy)->content : NULL;',
            'layout->value.transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1 ? NULL : NULL;'),
            ('foreign-result-owner-omitted','src/runtime/xl6_slot_map_key_value.c',
            '    return result;\n}\nbool xl6SlotMapKeyValueAdmitsV1',
            '    xl6SlotMapKeyDropValueV1(result); return input;\n}\nbool xl6SlotMapKeyValueAdmitsV1')]
        for name,source,before,after in changes:
            body=(root/source).read_text(encoding='utf-8'); assert body.count(before)==1,name
            bad=out/(name+'.c'); bad.write_text(body.replace(before,after),encoding='utf-8'); obj=out/(name+'.o'); product=out/(name+suffix)
            run(name+'-compile',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj]); current=compiled['O0'].copy(); current[sources.index(source)]=obj
            run(name+'-link',[cc,*current,*links,'-o',product]); run(name+'-refused',[product,'vectors'],True)
            controls.append(dict(Name=name,Compiled=True,Rejected=True,Source=str(bad),SourceSha256=sha(bad),Product=str(product),ProductSha256=sha(product)))
        report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']
    print('Owning native value storage complete: '+str(len(rows))+' phases, '+str(sum(r['Runs'] for r in prefixes))+' full-prefix runs')
if __name__=='__main__': main()
