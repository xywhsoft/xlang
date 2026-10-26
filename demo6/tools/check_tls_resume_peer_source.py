"""Actual-source resume-chain facade, normal offline handshakes, IR/JIT/AOT.
No sockets or security campaign. Not shipping/full-goal acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, time
from tls_resume_peer_fixtures import fixtures
from xlir_public_build_inputs import verify_public_build


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['root','build','out']:parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve()
    assert not out.exists();out.mkdir(parents=True)
    build,pinned,_=verify_public_build(root,args.build.resolve().parent)
    fixture=out/'fixtures';fixture.mkdir();cases=fixtures(root,fixture)
    driver=Path(build['Driver']);compiler=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    pinned.update([Path(__file__).resolve(),compiler,*cases])
    pinned.update(p for p in fixture.iterdir() if p.is_file())
    pinned.update(Path(__file__).parent/name for name in ['tls_resume_peer_fixtures.py',
        'tls_identity_fixtures.py','tls_revocation_fixtures.py','tls_verifier_fixtures.py',
        'x509_path_fixtures.py','x509_identity_fixtures.py','check_key_format_wire.py'])
    report=dict(Complete=False,Inputs={str(p):sha(p) for p in sorted(pinned)},Rows=[],Failures=[],
        NetworkTraffic=False,SecurityTests=False,ShippingAccepted=False,FullTlsAccepted=False)
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']

    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));executed=command
        if Path(command[0])==compiler:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"'
                for s in command[1:])+'\n',encoding='utf8');executed=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as stream:code=subprocess.run(executed,cwd=root,env=env,stdout=stream,
            stderr=subprocess.STDOUT,timeout=900).returncode
        report['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),
            Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==0,str(log)
        return log.read_text(errors='replace')

    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    for case in cases:
        name=case.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,case,'cgen-default',generated])
            text=generated.read_text(encoding='utf8')
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6TlsResume\w*\([^;]*\)\s*\{',text,re.M)
            for stage in ['typed','lowered']:run(name+'.'+stage,[driver,case,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ['O0','O2']:run(name+'.strict.'+opt,[compiler,*flags,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
            for mode in ['run','module']:run(name+'.'+mode,[driver,case,mode,out/(name+'.'+mode)])
            binary=out/(name+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
            run(name+'.aot.build',[driver,case,'exe',target]);run(name+'.aot.run',[binary])
            imports=run(name+'.aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary]
                if os.name=='nt' else ['readelf','-d',binary])
            assert not any(token in imports.lower() for token in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
            implementation=(Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text(encoding='utf8')
            assert '_MODULE_ALL' not in implementation
            assert ('XRT_MODULE_TLS_CLIENT_RESUME' in implementation)==(case in cases[:4])
            if case==cases[-1]:assert 'XRT_MODULE_TLS' not in implementation
            if case==cases[-2]:assert 'XRT_MODULE_TLS_VERIFY' not in implementation and 'XRT_MODULE_X509' not in implementation
        except Exception as error:
            report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(p)==h for p,h in report['Inputs'].items())
    report['Complete']=report['InputsUnchanged'] and not report['Failures']
    report['Products']={str(p):sha(p) for p in out.rglob('*') if p.is_file() and p.name!='results.json'}
    save();assert report['Complete'],report['Failures']


if __name__=='__main__':main()
