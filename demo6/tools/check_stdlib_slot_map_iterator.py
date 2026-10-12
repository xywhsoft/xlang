"""Resident SlotMap cursor foundation: one capture, direct complete key copy,
four minimal snapshot lanes, checked failure/EOF, real owning edges, phased
retirement, full allocation prefixes and bad implementations. Not source
foreach acceptance. Optional ASan/UBSan instruments the full selected closure.
"""
from pathlib import Path
import argparse,ast,hashlib,json,os,re,shutil,subprocess,sys
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true'); a=parser.parse_args()
    root,out=a.root.resolve(),a.out.resolve(); cc=shutil.which('gcc')
    assert cc and not out.exists() and (not a.sanitize or os.name!='nt')
    population=root/'tools/check_stdlib_dict_native.py'; tree=ast.parse(population.read_text(encoding='utf-8'))
    sources=ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign) and
        any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets)))
    sources[0]='test/cases/stdlib_slot_map_iterator_host.c'; sources[1]='test/support/slot_map_iterator_native.c'
    sources=[s for s in sources if s!='src/runtime/xl6_dict.c']
    sources+=['src/runtime/xl6_slot_map.c','src/runtime/xl6_slot_map_key.c','src/runtime/xl6_slot_map_key_value.c',
        'src/runtime/xl6_native_leaf_value.c','src/runtime/xl6_array_native_value_admission.c','src/runtime/xl6_slot_map_keys.c',
        'src/runtime/xl6_slot_map_pairs.c','src/runtime/xl6_string_value.c','src/runtime/xl6_array_iterator.c',
        'src/runtime/xl6_array_value_iterator.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_ownership_plan.c',
        'src/runtime/xl6_slot_map_iterator.c','src/runtime/xl6_slot_map_iterator_key.c',
        'src/runtime/xl6_slot_map_iterator_pair.c','src/runtime/xl6_slot_map_iterator_keys.c',
        'src/runtime/xl6_slot_map_iterator_values.c','src/runtime/xl6_slot_map_iterator_count.c']
    files={Path(__file__).resolve(),population,Path(sys.executable),Path(cc),*[root/s for s in sources]}
    for folder in ('src/runtime','src/api','src/type','lib/xruntime','lib/xrt','release/include','test/support','test/cases'):
        files.update(f for f in (root/folder).rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(); rows=[]; objects=[]; products=[]; controls=[]; prefixes=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Objects=objects,Products=products,
        NegativeControls=controls,Prefixes=prefixes,Sanitized=a.sanitize,SelectedNativeClosureInstrumented=a.sanitize,
        LanguagePairedIterationAcceptance=False,Full53Acceptance=False,ShippingArtifact=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=['-Wl,--wrap='+name for name in ('xrtTypedArrayPush','xrtSlotMapIterNext','xrtSlotMapIterBegin',
        'xrtTypeCopyValue','xrtTypedArrayGetValue','xrtTypedArrayClone','xl6SlotMapKeyBoxV1')]
    env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
    if a.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,command,rejection=None):
        r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=240)
        data=r.stdout+r.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Name=label,Command=list(map(str,command)),Exit=r.returncode,ExpectedRejection=bool(rejection),
            Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if rejection else r.returncode==0),(label,data[-6500:].decode(errors='replace'))
        if rejection: assert rejection.encode() in data,(label,data[-2000:])
        return data.decode(errors='replace')
    try:
        compiled={}; suffix='.exe' if os.name=='nt' else '.host'
        for opt in (0,2):
            current=[]
            for i,source in enumerate(sources):
                obj=out/(str(opt)+'-'+str(i)+'.o'); run('O'+str(opt)+'.compile'+str(i),[cc,*flags,'-O'+str(opt),'-c',source,'-o',obj])
                current.append(obj); objects.append(dict(Path=str(obj),Sha256=sha(obj)))
            compiled[opt]=current; binary=out/('O'+str(opt)+suffix)
            run('O'+str(opt)+'.link',[cc,*current,*links,'-o',binary]); products.append(dict(Path=str(binary),Sha256=sha(binary)))
            assert 'lifetimes balanced' in run('O'+str(opt)+'.vectors',[binary,'vectors'])
            text=run('O'+str(opt)+'.prefixes',[binary,'prefixes'])
            matches=re.findall(r'cursor mode=(\d+) prefixes=(\d+) first-no-hit=(\d+) balanced',text)
            assert len(matches)==4
            for mode,count,last in matches:
                assert int(count)==int(last)+1
                prefixes.append(dict(Optimization=opt,Mode=int(mode),Count=int(count),FirstNoHit=int(last)))
        mutations=[
            ('premature-eof','src/runtime/xl6_slot_map_iterator.c',
                'bool available=success && ordinal<snapshot->count;',
                'bool available=false;','api->Advance(cursor,&found) && found'),
            ('wrong-key-row','src/runtime/xl6_slot_map_iterator_key.c',
                'xrtTypedArrayConstGet(xl6SequenceStorageV1(pinned),ordinal)',
                'xrtTypedArrayConstGet(xl6SequenceStorageV1(pinned),0)','xl6SlotMapKeyEqualV1(key,b)'),
            ('missing-key-edge','src/runtime/xl6_value_iterator.c',
                '(!state->pair.keys || visit(xrtValueOwnership(state->pair.keys),context)) &&',
                'true &&','edges(cursor)=='),
            ('key-failure-not-terminal','src/runtime/xl6_value_iterator.c',
                'state->failed=true; state->currentValid=false; state->view=(XL6_ValueIteratorView){0};',
                'state->currentValid=false; state->view=(XL6_ValueIteratorView){0};','!api->View(cursor)')]
        for name,source,before,after,oracle in mutations:
            body=(root/source).read_text(encoding='utf-8-sig'); assert body.count(before)==1,name
            bad=out/(name+'.c'); bad.write_text(body.replace(before,after),encoding='utf-8')
            obj=out/(name+'.o'); run(name+'.compile',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj])
            linked=compiled[0].copy(); linked[sources.index(source)]=obj
            binary=out/(name+suffix); run(name+'.link',[cc,*linked,*links,'-o',binary]); run(name+'.refused',[binary,'vectors'],oracle)
            controls.append(dict(Name=name,Compiled=True,Rejected=True,Source=str(bad),Sha256=sha(bad),Product=str(binary),ProductSha256=sha(binary)))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs+objects+products)
        report['Complete']=report['InputsUnchanged'] and len(controls)==4
    finally:
        report['PhaseCount']=len(rows)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('SlotMap native cursors: '+str(len(rows))+' phases, '+str(sum(p['Count'] for p in prefixes))+
        ' full allocation prefixes, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
