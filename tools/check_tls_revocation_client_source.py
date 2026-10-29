"""Normal client-certificate CRL callback: empty reference name, no transport."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['root','build','fixtures','out']:parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build=json.loads(args.build.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    fixture=args.fixtures.resolve();original=fixture/'tls.revocation.p256.xl';client=fixture/'p256.client.der';leaf=fixture/'p256.leaf.der'
    literal=lambda data:'b"'+''.join('\\x%02x'%b for b in data)+'"'
    text=original.read_text(encoding='utf8');server=literal(leaf.read_bytes());assert text.count(server)==2
    text=text.replace(server,literal(client.read_bytes())).replace('"localhost"','""')
    assert text.count('verifier.verifyServer(chain, "")')==1
    text=text.replace('verifier.verifyServer(chain, "")','verifier.verifyClient(chain)')
    text+='\nassert retained.peer.role == TlsRole.client and retained.peer.verificationName == ""\n'
    source=out/'tls.revocation.client.xl';source.write_text(text,encoding='utf8')
    compiler=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    files=[Path(__file__),args.build,driver,original,client,leaf,source,compiler]
    report=dict(Complete=False,Inputs={str(p):sha(p) for p in files},Rows=[],NetworkTraffic=False,SecurityTests=False,ShippingAccepted=False)
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def run(name,command):
        command=list(map(str,command));actual=command
        if Path(command[0])==compiler:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n',encoding='utf8');actual=[command[0],'@'+str(rsp)]
        log=out/(name+'.log')
        with log.open('wb') as stream:code=subprocess.run(actual,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=900).returncode
        report['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log)))
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print(name,code,flush=True);assert code==0,log
        return log.read_text(errors='replace')
    generated=out/'client.c';run('emit',[driver,source,'cgen-default',generated])
    for stage in ['typed','lowered']:run(stage,[driver,source,'ir-'+stage,out/(stage+'.json')])
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    for opt in ['O0','O2']:run('strict.'+opt,[compiler,*flags,'-'+opt,'-c',generated,'-o',out/(opt+'.o')])
    for mode in ['run','module']:run(mode,[driver,source,mode,out/mode])
    binary=out/('client.exe' if os.name=='nt' else 'client');target=binary.with_suffix('') if os.name=='nt' else binary
    run('aot.build',[driver,source,'exe',target]);run('aot.run',[binary])
    imports=run('aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
    assert not any(s in imports.lower() for s in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
    report['InputsUnchanged']=all(sha(p)==h for p,h in report['Inputs'].items())
    report['Complete']=report['InputsUnchanged'];report['Products']={str(p):sha(p) for p in out.rglob('*') if p.is_file() and p.name!='results.json'}
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');assert report['Complete']


if __name__=='__main__':main()
