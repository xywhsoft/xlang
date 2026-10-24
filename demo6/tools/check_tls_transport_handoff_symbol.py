from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--support',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest();b=json.loads(a.build.read_text(encoding='utf-8-sig'));assert b['Complete'] and b['SourceXrt'] and not b['RuntimeOnly']
for q in b['Inputs']+b['Objects']+b['Headers']:assert sha(q['Path'])==q['Sha256'].lower()
runtime=root/'release/xl.dll' if os.name=='nt' else Path(b['NativeRoot'])/'release/libxl.so'
libraries=root/'res/tcc/win64/x86_64' if os.name=='nt' else Path(b['NativeRoot'])/'build/linux/tcc-dist'
driver=Path(b['Driver']);assert sha(driver)==b['DriverSha256'].lower()
if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
gcc=shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe';objects=[q['Path'] for q in b['Objects'] if Path(q['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']];assert len(objects)==len(b['Objects'])-1
r=dict(Complete=False,Inputs={str(q):sha(q) for q in [Path(__file__),a.support]},Rows=[],Failures=[],NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False)
def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
def run(name,args):
    args=list(map(str,args));command=args
    if args[0]==gcc:
        rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
    log=out/(name+'.log');start=time.monotonic()
    with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
    r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True);assert not code,str(log)
for opt in ['O0','O2']:
    try:
        obj=out/(opt+'.o');binary=out/(opt+('.exe' if os.name=='nt' else ''))
        run(opt+'.compile',[gcc,'-std=c11','-DXL_STATIC_EMBED','-I'+str(root/'release/include'),'-Wall','-Wextra','-Werror','-'+opt,'-c',a.support,'-o',obj])
        libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
        run(opt+'.link',[gcc,*objects,obj,runtime,*libs,'-o',binary]);run(opt+'.run',[binary,root,runtime,libraries,driver.parent/'include',out/(opt+'.temps')])
    except Exception as e:r['Failures'].append(repr(e));save()
r['Stages']=len(r['Rows']);r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items()) and all(sha(q['Path'])==q['Sha256'].lower() for q in b['Inputs']+b['Objects']+b['Headers'])
r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json'];r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']
