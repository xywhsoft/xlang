"""Pinned XRT/xruntime provider cache publication; no traffic or security tests.

Uses the upstream fixture without requiring an upstream checkout. This is
selected native snapshot acceptance, not fresh shipping DLL/SO/VFS acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--compiler',action='append',required=True)
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve();assert not out.exists()
    receipt=root/'lib/xrt/backports/net_socket_message_cache.json'
    provenance=json.loads(receipt.read_text(encoding='utf8'))
    fixture=root/'test/support/net_socket_message_cache.c'
    normalized=fixture.read_text(encoding='utf8').rstrip()+'\n'
    assert hashlib.sha256(normalized.encode()).hexdigest()==provenance['NormalizedFixtureSha256']
    headers=[root/row['Path'] for row in provenance['Consumers']]
    for row in provenance['Consumers']:assert sha(root/row['Path'])==row['AfterSha256']
    compilers=[Path(shutil.which(name) or name).resolve() for name in args.compiler]
    files=[Path(__file__).resolve(),receipt,fixture,*headers,*compilers,
           root/'lib/xrt/xrt.snapshot.json',root/'lib/xruntime/xruntime.snapshot.json']
    inputs={str(f):sha(f) for f in files};out.mkdir(parents=True);rows=[];products=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,
                Products=products,NetworkTraffic=False,NetworkSecurityTests=False,
                ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        log=out/(name+'.log');command=list(map(str,command));started=time.monotonic()
        with log.open('wb') as stream:
            result=subprocess.run(command,cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Exit=result.returncode,Command=command,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started))
        save();print(name+': '+str(result.returncode),flush=True);assert not result.returncode,str(log)
        return log.read_text(encoding='utf8',errors='replace')
    libraries=['-lws2_32','-liphlpapi'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    save()
    try:
        for index,cc in enumerate(compilers):
            flags=[cc,'-std=c11','-Wall','-Werror',*['-DXRT_MODULE_'+m for m in ('NET_SOCKET','THREAD','ATOMIC','TIME')]]
            if cc.stem.lower()!='tcc':flags+=['-Wextra']
            if os.name=='nt':flags+=['-DWIN32_LEAN_AND_MEAN','-D_WIN32_WINNT=0x0A00']
            else:flags+=['-D_GNU_SOURCE']
            for opt in ('O0','O2'):
                for mode,header in enumerate(headers):
                    name=f'{index}.{opt}.{mode}';binary=out/(name+('.exe' if os.name=='nt' else ''))
                    options=['-DTEST_SOCKET_CACHE_HEADER="'+str(header).replace('\\','/')+'"']
                    if header.name=='xruntime.h':options+=['-DXRUNTIME_IMPLEMENTATION','-DXRT_IMPLEMENTATION',
                        '-include',str(root/'lib/xruntime/xruntime_features.h'),'-include',str(root/'lib/xrt/xrt.h')]
                    run(name+'.build',[*flags,'-'+opt,*options,fixture,*libraries,'-o',binary])
                    products.append(dict(Path=str(binary),Sha256=sha(binary)));save()
                    proof=run(name+'.run',[binary]);assert 'eight simultaneous first users, canonical publication; no traffic' in proof
        report['InputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert report['InputsUnchanged'] and report['LogsUnchanged']
        assert all(sha(row['Path'])==row['Sha256'] for row in products);report['Complete']=True
    except BaseException as error:report['Error']=repr(error);raise
    finally:save()

if __name__=='__main__':main()
