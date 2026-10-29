"""Exact TLS selector policies in the full actual resident ownership catalog.

No hard-coded counts for unrelated growing policy families. The fixture still
instantiates EVERY registered domain, refuses duplicate identities/policies,
checks both exact selector capabilities and exhausts catalog OOM prefixes.
No network or shipping-product acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build,pinned,_=verify_public_build(root,a.build.resolve().parent)
    assert build['Complete'] and build['InputsUnchanged']
    controls=Path(__file__).resolve().parents[1];contract=controls/'test/cases/xlir_tls_selector_catalog.c'
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    objects=[q['Path'] for q in build['Objects'] if Path(q['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-missing-field-initializers',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    pinned.update((Path(__file__).resolve(),contract,cc,runtime))
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(pinned)},Rows=[],Failures=[],ShippingAccepted=False,FullTlsAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args):
        command=list(map(str,args));actual=command
        if command[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/')+'"' for s in command[1:]),encoding='utf8');actual=[str(cc),'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(actual,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==0,str(log)
        return log.read_text(encoding='utf8',errors='replace')
    try:
        for opt in ('O0','O2'):
            obj=out/(opt+'.o');binary=out/(opt+('.exe' if os.name=='nt' else ''))
            run(opt+'.compile',[cc,*flags,'-'+opt,'-c',contract,'-o',obj]);run(opt+'.link',[cc,obj,*objects,runtime,*links,'-o',binary])
            assert 'TLS selector full catalog: exact independent policies,' in run(opt+'.run',[binary])
        r['InputsUnchanged']=all(sha(q)==h for q,h in r['Inputs'].items());assert r['InputsUnchanged'];r['Complete']=True
    except Exception as e:r['Failures'].append(repr(e))
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
