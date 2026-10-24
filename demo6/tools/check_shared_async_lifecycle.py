"""Shared-CFG lifecycle/invalid-IR gates, using the existing canonical hosts.

Only Watch registration is observed through the existing test include; it
does not replace the resident execution policy. Actual uninstrumented source
JIT/module/AOT acceptance is performed by check_shared_async_program_source.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['root','build','out']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--tests',type=Path,help='Separately hashed canonical test revision')
    a=p.parse_args();root=a.root.resolve();tests=(a.tests or root/'test/cases').resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build=json.loads(a.build.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    objects=[q['Path'] for q in build['Objects'] if Path(q['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']]
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    families=['xlir_async_cancel_watch','xlir_async_frame_completion']
    inputs={Path(__file__).resolve(),gcc}
    inputs.update(tests/(name+suffix) for name in families for suffix in ['.xl','_contract.c','_host.c','_runtime.c'])
    inputs.update(tests/q for q in ['xlir_async_source_sync.c','xlir_async_source_transport.c',
        'xlir_async_source_sync.h','xlir_launch_test.h','xlir_continuation_owner_test.h',
        'xlir_xvalue_array_iterator_projection.xl','xlir_xvalue_array_iterator_projection_host.c'])
    observed=tests.parents[1]/'src/runtime/xl6_continuation_future.c'
    assert sha(observed)==sha(root/'src/runtime/xl6_continuation_future.c');inputs.add(observed)
    r=dict(Complete=False,SourceBuild=str(a.build),SourceBuildSha256=sha(a.build),Inputs={str(q):sha(q) for q in inputs},
        Rows=[],Products=[],Failures=[],WatchRegistrationObserver=True,ResidentExecutionPolicyReplaced=False,
        FullSourceRuntimeAcceptance=False,NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,command):
        command=list(map(str,command));invocation=command
        if Path(command[0])==gcc:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n');invocation=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(invocation,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name,code,flush=True);assert not code,str(log)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-but-set-variable','-Wno-unused-const-variable','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(Path(build['Driver']).parent/'include'),'-I'+str(tests),
        *(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
    try:
        for family in families:
            baseline=None
            for opt in ['O0','O2']:
                name=family+'.'+opt;prefix=out/name;obj=out/(name+'.o');binary=out/(name+('.exe' if os.name=='nt' else ''))
                run(name+'.contract.compile',[gcc,*flags,'-DXL_STATIC_EMBED','-'+opt,'-c',tests/(family+'_contract.c'),'-o',obj])
                run(name+'.contract.link',[gcc,*objects,obj,runtime,*libraries,'-o',binary])
                run(name+'.contract.run',[binary,str(prefix)+'.c',str(prefix)+'.h',str(prefix)+'.json'])
                digests=[sha(str(prefix)+suffix) for suffix in ['.c','.h','.json','.h.bind.h']]
                if baseline:assert baseline==digests
                else:baseline=digests
                original=(tests/(family+'_host.c')).read_text()
                host=out/(name+'.host.c');adapted=original
                for suffix in ['.c','.h.bind.h']:
                    old='../../build/test/'+family+suffix;assert adapted.count(old)==1
                    adapted=adapted.replace(old,(str(prefix)+suffix).replace('\\','/'))
                host.write_text(adapted)
                unitObjects=[]
                # The observer includes the actual production implementation
                # with only xrtFutureWatchAdd mapped to a counting forwarder.
                for index,source in enumerate([tests/(family+'_runtime.c'),host,
                        tests/'xlir_async_source_sync.c',tests/'xlir_async_source_transport.c']):
                    item=out/(name+'.unit.'+str(index)+'.o');unitObjects.append(item)
                    run(name+'.unit.'+str(index)+'.compile',[gcc,*flags,'-'+opt,'-c',source,'-o',item])
                resident=[q['Path'] for q in build['Objects'] if Path(q['Source']).name not in [
                    'xlir_public_source_driver.c','xlir_public_source_linux_driver.c','xl6_continuation_future.c']]
                unit=out/(name+'.lifecycle'+('.exe' if os.name=='nt' else ''))
                run(name+'.unit.link',[gcc,*resident,*unitObjects,runtime,*libraries,'-o',unit]);run(name+'.unit.run',[unit])
        # The same source driver (no observer) exercises an iterator surviving
        # await. Only the pre-existing host's main is removed from the sidecar.
        case=tests/'xlir_xvalue_array_iterator_projection.xl'
        original=(tests/'xlir_xvalue_array_iterator_projection_host.c').read_text()
        assert original.count('int main(void)')==1
        sidecar=out/'iterator.sidecar.c'
        sidecar.write_text('#include <__xl_runtime/xruntime_decl.h>\n'+original.split('int main(void)',1)[0])
        driver=Path(build['Driver']);c=out/'iterator.c';run('iterator.emit',[driver,case,'cgen-default',c])
        text=c.read_text();program=re.search(r'^static void xlir_async_machine_program_\d+\([^\n]+\)\n\{.*?(?=\nstatic void xlir_async_machine_resume_)',text,re.M|re.S)
        assert program
        for token in ['xlir_xvalue_iterator_create(','xlir_xvalue_iterator_advance(','xlir_xvalue_iterator_item_string_copy(',
                'xlir_xvalue_iterator_key_ordinal_i64(','xrtUtf8Count(']:assert token in program[0],token
        assert 'xlir_xvalue_iterator_destroy(' in text and 'xlir_async_machine_release_' in program[0]
        for mode in ['run','module']:run('iterator.'+mode,[driver,case,mode,out/('iterator.'+mode),sidecar])
        target=out/'iterator.aot';binary=Path(str(target)+'.exe') if os.name=='nt' else target
        run('iterator.aot.build',[driver,case,'exe',target,sidecar]);run('iterator.aot.run',[binary])
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items()) and all(sha(q['Path'])==q['Sha256'].lower() for q in build['Inputs']+build['Objects']+build['Headers'])
        r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
        r['Complete']=r['InputsUnchanged'];r['Stages']=len(r['Rows']);save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
