"""Native checked char provider, independent status and exact Unicode domain.
Does not claim language C/JIT/AOT or all primitive conversion acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True); parser.add_argument('--sanitize',action='store_true')
    a=parser.parse_args(); root=Path(__file__).resolve().parents[1]; out=a.out.resolve(); cc=shutil.which('gcc')
    assert cc and not out.exists() and (not a.sanitize or os.name!='nt')
    sources=['test/cases/stdlib_char_checked_native_host.c','test/support/char_checked_native.c',
        'src/runtime/xl6_char_checked.c','src/api/xl_foreign_error.c']
    files={Path(__file__),Path(cc),Path(sys.executable),root/'lib/xrt/xrt.h',root/'src/api/xl_foreign_error.h'}
    files.update(root/name for name in sources)
    for folder in ('src/runtime','release/include'): files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(path.resolve()),Sha256=sha(path)) for path in sorted(files)]
    rows=[]; mutations=[]; products=[]; out.mkdir(); env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Mutations=mutations,Products=products,
        RuntimeInstrumented=a.sanitize,LanguageExecution=False,Full53Acceptance=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=['-Wl,--wrap=xrtValueGetChar']+([] if os.name=='nt' else ['-pthread','-lm'])
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if a.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode))
        assert result.returncode==0,(name,data[-4000:].decode(errors='replace'))
        return data
    try:
        for opt in (0,2):
            binary=out/('O'+str(opt)+suffix)
            run('O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),*sources,*links,'-o',binary])
            assert b'domain/identity/status/zero-budget/first-failure balanced' in run('O'+str(opt)+'.run',[binary])
        provider='src/runtime/xl6_char_checked.c'; original=(root/provider).read_text(encoding='utf-8-sig')
        variants=[('lost-null-diagnostic','if (!outcome.ok)','if (!outcome.ok && outcome.error)'),
            ('lost-identity','tag==XVALUE_CHAR && (!strict || !xrtValueTypeId(source) || xrtValueTypeId(source)==expected)','tag==XVALUE_CHAR'),
            ('wrapped-codepoint','code>UINT32_MAX || !xl6CharDomainV1((uint32_t)code)','!xl6CharDomainV1((uint32_t)code)'),
            ('accepted-surrogate','code>UINT32_MAX || !xl6CharDomainV1((uint32_t)code)','code>0x10ffff')]
        for name,before,after in variants:
            assert original.count(before)==1,name
            mutant=out/(name+'.c'); mutant.write_text(original.replace(before,after),encoding='utf-8')
            selected=[str(mutant) if path==provider else path for path in sources]; binary=out/(name+suffix)
            run(name+'.build',[cc,*flags,'-O0',*selected,*links,'-o',binary])
            result=subprocess.run([str(binary)],cwd=root,env=env,capture_output=True,timeout=180)
            data=result.stdout+result.stderr; (out/(name+'.reject.log')).write_bytes(data)
            assert result.returncode and b'stdlib_char_checked_native_host.c' in data and b'assert' in data.lower(),name
            mutations.append(dict(Name=name,Rejected=True,Exit=result.returncode,MutantSha256=sha(mutant)))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir()) if path.is_file())
        report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('char checked native: '+str(len(rows))+' stages, '+str(len(mutations))+' implementation rejections, complete='+str(report['Complete']))
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
