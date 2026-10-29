"""Resident Dict exact-key shared import/restore, not language/shipping cutover.

Real Array/List/Dict native closures, sparse int64 and exact empty/NUL/UTF-8
keys, shared-child memo, JSON/XLON parsers, checked rejection, O0/O2 full OOM
and diagnosticless scope prefixes; compiled wrong implementations must fail.
Optional sanitizers cover this selected native closure only.
"""
from pathlib import Path
import argparse,ast,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
    root=a.root.resolve(); out=a.out.resolve(); cc=shutil.which('gcc'); assert cc and not out.exists()
    assert not a.sanitize or os.name!='nt'
    population=root/'tools/check_stdlib_list_native.py'; tree=ast.parse(population.read_text(encoding='utf-8'))
    sources=ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign)
        and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets)))
    sources[sources.index('test/cases/stdlib_list_native_host.c')]='test/cases/stdlib_dict_import_native_host.c'
    sources.remove('test/cases/stdlib_list_schema_native_host.c')
    sources[sources.index('test/support/list_native.c')]='test/support/dict_import_native.c'
    sources+=['src/runtime/xl6_array_import_chain.c','src/runtime/xl6_array_input.c','src/runtime/xl6_array_erased.c',
        'src/runtime/xl6_list_import_chain.c','src/runtime/xl6_dict.c','src/runtime/xl6_dict_schema.c',
        'src/runtime/xl6_dict_import_chain.c']
    files={Path(__file__).resolve(),population,Path(cc),Path(sys.executable),*[root/s for s in sources],
        root/'test/support/list_restore_native.c',root/'test/support/list_native.c'}
    for folder in ('src/runtime','src/api','lib/xrt','lib/xruntime','release/include','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(parents=True); rows=[]; prefixes=[]; controls=[]; objects=[]; products=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        NegativeControls=controls,Objects=objects,Products=products,ResidentDictRecursiveImport=True,
        ResidentDictRestore=True,LanguageDictImportCutover=False,LanguageDictRestoreCutover=False,
        ShippingAcceptance=False,Full53Acceptance=False,Sanitized=a.sanitize,WholeCompilerSanitized=False)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime']
    links=['-Wl,--wrap='+s for s in ('xl6ErrorScopeBegin','xl6ErrorScopeBeginCleanup','xrtTypedDictIterNext')]
    env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
    if a.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,command,reject=False):
        command=list(map(str,command)); r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        data=r.stdout+r.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Name=label,Command=command,Exit=r.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if reject else r.returncode==0),(label,data[-6500:])
        if reject: assert b'assert' in data.lower()
        return data.decode(errors='replace')
    try:
        cache={}
        for opt in ('O0','O2'):
            objs=[out/(opt+'-'+str(i)+'.o') for i in range(len(sources))]
            def compile_one(pair):
                i,s=pair; run(opt+'.compile.'+str(i),[cc,*flags,'-'+opt,'-c',s,'-o',objs[i]])
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: list(pool.map(compile_one,enumerate(sources)))
            cache[opt]=objs; objects.extend(dict(Path=str(o),Sha256=sha(o)) for o in objs)
            binary=out/(opt+('.exe' if os.name=='nt' else '')); run(opt+'.link',[cc,*objs,*links,'-o',binary])
            assert '[PASS]' in run(opt+'.vectors',[binary,'vectors'])
            for mode in ('oom','scopes'):
                data=run(opt+'.'+mode,[binary,mode]); found=re.findall(r'Dict import closed lane=(\d+) mode='+mode+r' runs=(\d+) first-no-hit=(\d+)',data)
                assert len(found)==16
                for lane,runs,first in found:
                    lane,runs,first=map(int,(lane,runs,first)); assert runs==first+1
                    points=re.findall(r'Dict import lane='+str(lane)+r' mode='+mode+r' point=(\d+) hit=(\d) balanced',data)
                    assert points==[(str(i),str(int(i<first))) for i in range(runs)]
                    prefixes.append(dict(Optimization=opt,Mode=mode,Lane=lane,Runs=runs,FirstNoHit=first))
            products.append(dict(Path=str(binary),Sha256=sha(binary))); print(opt+' native Dict import complete',flush=True)
        for name,source,before,after in (
            ('nul-key-truncated','src/runtime/xl6_dict_import_chain.c','input->keys[input->keyCount++]=(xstrview){copy,key.Size};',
                'input->keys[input->keyCount++]=(xstrview){copy,key.Size && memchr(copy,0,key.Size) ? (size_t)((char*)memchr(copy,0,key.Size)-copy) : key.Size};'),
            ('shared-memo-disabled','src/runtime/xl6_array_import_chain.c','if (prior) return xrtValueRetain(prior->result);',
                'if (false && prior) return xrtValueRetain(prior->result);'),
            ('typed-erasure-authority-forged','src/runtime/xl6_dict_import_chain.c',
                'bool erased=xl6ReferenceAnyAdmissionV1(&policy,layout.value.elementIdentity);',
                'bool erased=true;'),
            ('restore-text-key-wrong','src/runtime/xl6_array_import_chain.c',
                'schema->provider->AppendText(value,family.textKeys[i],item)',
                'schema->provider->AppendText(value,(xstrview){"wrong",5},item)')):
            body=(root/source).read_text(encoding='utf-8'); expected=2 if name=='restore-text-key-wrong' else 1
            assert body.count(before)==expected,name
            if name=='restore-text-key-wrong':
                prefix,suffix=body.rsplit(before,1); mutated=prefix+after+suffix
            else: mutated=body.replace(before,after)
            changed=out/(name+'.c'); changed.write_text(mutated,encoding='utf-8')
            obj=out/(name+'.o'); run(name+'.compile',[cc,*flags,'-O0','-c',changed,'-o',obj])
            objs=list(cache['O0']); objs[sources.index(source)]=obj
            binary=out/(name+('.exe' if os.name=='nt' else '')); run(name+'.link',[cc,*objs,*links,'-o',binary]); run(name+'.reject',[binary,'vectors'],True)
            controls.append(dict(Name=name,Compiled=True,Rejected=True,Source=str(changed),SourceSha256=sha(changed),Product=str(binary),ProductSha256=sha(binary)))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs+objects+products)
        report['Complete']=report['InputsUnchanged'] and len(prefixes)==64 and len(controls)==4
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
