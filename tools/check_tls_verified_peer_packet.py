"""Private post-verification snapshot preparation against real runtime products.

The new packet and its unchanged resident TLS diagnostic helper are compiled as
the subject. Existing runtime providers are loaded from ordinary products, not
replaced. This is not public callback, CRL, TLS, or shipping acceptance.
No socket, handshake, trust decision or malformed crypto input is exercised.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('products','build-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    products=args.products.resolve();out=args.out.resolve();assert not out.exists()
    out.mkdir(parents=True);windows=os.name=='nt'
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    read=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
    build=read(args.build_report);assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    recorded={str(Path(row['Path']).resolve()):row['Sha256'] for row in build['Products']}
    names=['xl.dll','xlc.dll'] if windows else ['libxl.so','libxlc.so']
    original=[products/name for name in names]
    assert all(recorded.get(str(p))==sha(p) for p in original)
    helper=root/'src/runtime/xl6_tls_value.c'
    assert sha(helper)==sha(products.parent/'src/runtime/xl6_tls_value.c')
    subject=root/'src/runtime/xl6_tls_verified_peer_packet.c'
    observer=root/'test/cases/xlir_tls_verified_peer_packet.c'
    inputs=[Path(__file__).resolve(),args.build_report.resolve(),subject,helper,observer,
        root/'src/runtime/xl6_tls_verified_peer_packet.h',root/'test/support/tls_verified_peer_packet_data.h',
        root/'release/include/xruntime_decl.h',root/'release/include/xrt_decl.h',*original]
    for name in ('compiler_dll.rsp.in','runtime_dll.rsp.in'):
        template=root/'tools'/name
        entries=[line.strip().replace('\\','/') for line in template.read_text(encoding='utf-8-sig').splitlines()]
        assert entries.count('src/runtime/xl6_tls_verified_peer_packet.c')==1
        inputs.append(template)
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    configurations=[('O0',cc,['-O0']),('O2',cc,['-O2'])]
    if windows:
        tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
        configurations.append(('tcc',tcc,[]))
    else:configurations.append(('san',cc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']))
    inputs += [compiler for _,compiler,_ in configurations]
    report=dict(Complete=False,Scope=__doc__,Rows=[],Failures=[],Products=[],
        Inputs={str(p):sha(p) for p in set(inputs)},ActualBaselineProducts=True,
        RecompiledSubjectSources=[str(subject),str(helper)],PublicTlsAcceptance=False,
        PublicCallbacksImplemented=False,RevocationImplemented=False,FullGoalComplete=False)
    environment=dict(os.environ)
    for variable in list(environment):
        if variable.upper() in ('CPATH','C_INCLUDE_PATH','LIBRARY_PATH','LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT'):
            del environment[variable]
    environment.update(TMPDIR=str(out),TEMP=str(out),TMP=str(out))
    if not windows:environment.update(ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    def save():
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,reject=False):
        command=list(map(str,command));log=out/(name+'.log');start=time.monotonic()
        def disable_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        with log.open('wb') as stream:
            result=subprocess.run(command,cwd=out,env=environment,stdout=stream,stderr=subprocess.STDOUT,
                timeout=180,**({'preexec_fn':disable_core} if reject and not windows else {}))
        report['Rows'].append(dict(Name=name,Command=command,Exit=result.returncode,ExpectedRejection=reject,
            Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        assert (result.returncode!=0 if reject else result.returncode==0),(name,str(log))
        return log.read_text(encoding='utf-8',errors='replace')
    save()
    try:
        for name in names:
            directory=out/name;directory.mkdir();library=directory/name
            shutil.copy2(products/name,library);assert sha(library)==sha(products/name)
            for label,compiler,options in configurations:
                binary=directory/(label+('.exe' if windows else ''))
                strict=['-Wall','-Wextra','-Werror'] if label!='tcc' else ['-Wall','-Werror']
                flags=['-std=c11','-I'+str(root),'-I'+str(root/'release')]
                if not windows:flags+=['-D_GNU_SOURCE','-Wl,-rpath,$ORIGIN']
                run(name+'.'+label+'.build',[compiler,*flags,*strict,*options,subject,helper,observer,library,'-o',binary])
                output=run(name+'.'+label+'.run',[binary])
                assert '4000 shared reads and 32 allocation prefixes balanced: ok' in output
        # A real ownership error must fail the ordinary observer. This changes
        # only a private test copy, never a workspace provider or input data.
        if not windows:
            bad=out/'borrowed-packet.c'
            original_source=subject.read_text(encoding='utf-8-sig')
            needle='self->certificates[i]=(xbytesview){cursor,span.Size};'
            assert original_source.count(needle)==1
            bad.write_text(original_source.replace(needle,'self->certificates[i]=span;'),encoding='utf-8')
            binary=out/names[0]/'borrowed-packet'
            run('borrowed-packet.build',[cc,'-std=c11','-O0','-D_GNU_SOURCE','-Wall','-Wextra','-Werror',
                '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'src/runtime'),
                '-Wl,-rpath,$ORIGIN',bad,helper,observer,out/names[0]/names[0],'-o',binary])
            rejected=run('borrowed-packet.rejected',[binary],reject=True)
            assert 'span.Size==size' in rejected
        report['InputsUnchanged']=all(sha(p)==h for p,h in report['Inputs'].items())
        report['Configurations']=len(configurations)*len(names)
        report['AllocationPrefixes']=32*report['Configurations']
        report['SharedReads']=4000*report['Configurations']
        report['RejectedBorrowedImplementation']=not windows
        report['Complete']=report['InputsUnchanged'] and report['Configurations']==6
    except Exception as error:report['Failures'].append(repr(error))
    report['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file() and p.name!='results.json']
    save();print(json.dumps(dict(Complete=report['Complete'],Failures=report['Failures'])),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
