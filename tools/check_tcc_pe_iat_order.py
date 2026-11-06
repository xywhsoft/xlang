"""Exact production PE import-chain ordering, zero-offset thunk, DATA and GC gates."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ['root','implementation-root','distribution','out']:
        ap.add_argument('--'+name,required=True,type=Path)
    ap.add_argument('--baseline-root',type=Path)
    ap.add_argument('--execute-pe',action='store_true')
    a=ap.parse_args();assert os.name=='nt';root=a.root.resolve();impl=a.implementation_root.resolve();out=a.out.resolve();out.mkdir(exist_ok=False,parents=True)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    frozen=[];source=out/'source'
    for p in sorted((impl/'lib/tcc').rglob('*')):
        if p.is_file() and '__pycache__' not in p.parts:
            q=source/p.relative_to(impl);q.parent.mkdir(exist_ok=True,parents=True);shutil.copyfile(p,q);h=sha(q);assert h==sha(p);frozen.append({'path':p.relative_to(impl).as_posix(),'sha256':h})
    for name in ['test/cases/tcc_pe_iat_order.c','test/cases/tcc_pe_iat_order_native.c','tools/check_tcc_pe_iat_order.py']:
        p=root/name;q=source/name;q.parent.mkdir(exist_ok=True,parents=True);shutil.copyfile(p,q);frozen.append({'path':name,'sha256':sha(q)})
    report={'scope':__doc__,'implementationRoot':str(impl),'sourceInputs':frozen,'rows':[],'complete':False,'passed':False,'executePeRequested':a.execute_pe,'peExecuted':False,'shippingDelivery':False}
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,cmd,expected=0):
        t=time.monotonic()
        try:p=subprocess.run([str(x)for x in cmd],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        except OSError as e:report['rows'].append({'name':name,'error':str(e),'winerror':getattr(e,'winerror',None)});save();raise
        log=out/(name+'.log');log.write_bytes(p.stdout);row={'name':name,'command':[str(x)for x in cmd],'exitCode':p.returncode,'rawStatusU32':p.returncode&0xffffffff,'seconds':time.monotonic()-t,'logSha256':sha(log),'passed':(p.returncode==expected)};report['rows'].append(row);save();print(name,p.returncode,flush=True)
        if not row['passed']:raise RuntimeError(name+' failed: '+str(log))
        return p.stdout.decode(errors='replace'),row
    try:
        kernel=out/'kernel.def';kernel.write_text('LIBRARY kernel32.dll\nEXPORTS\nTlsGetValue\nTlsQuery=TlsGetValue\nTlsAlloc\nTlsSetValue\nTlsFree\n')
        zero=out/'zero.def';zero.write_text('LIBRARY msvcrt.dll\nEXPORTS\n_stricmp\n')
        data=out/'data.def';data.write_text('LIBRARY msvcrt.dll\nEXPORTS\nhuge_alias=_HUGE DATA\n')
        obj=out/'native.o';run('gcc-native',['gcc','-std=c11','-O2','-ffunction-sections','-fdata-sections','-c',source/'test/cases/tcc_pe_iat_order_native.c','-o',obj]);run('coff-symbols',['objdump','-t','-r',obj])
        report['definitions']=[{'path':str(p),'sha256':sha(p)}for p in [kernel,zero,data]]
        for opt in ['O0','O2']:
            dest=out/opt;dest.mkdir();exe=dest/'host.exe';run(opt+'-build',['gcc','-std=c11','-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(source/'lib/tcc'),source/'test/cases/tcc_pe_iat_order.c',source/'lib/tcc/tcc_utf8_io.c','-lws2_32','-o',exe])
            text,row=run(opt+'-api',[exe,a.distribution.resolve(),obj,kernel,zero,data,dest]);m=re.search(r'TCC_PE_IAT_CHECKS=(\d+) FAILURES=(\d+)',text);assert m and not int(m[2]);row['checks']=int(m[1]);save()
            for gc in [0,1]:
                for order in [0,1]:
                    program=dest/('gc%d-order%d.exe'%(gc,order));description,_=run(opt+'-gc%d-order%d-imports'%(gc,order),['objdump','-p',program]);assert 'TlsGetValue'in description and 'TlsQuery'not in description
                    if a.execute_pe:run(opt+'-gc%d-order%d-execute'%(gc,order),[program])
        if a.baseline_root:
            baseline=a.baseline_root.resolve();f=out/'baseline-inputs.json';rows=[]
            for p in sorted((baseline/'lib/tcc').rglob('*')):
                if p.is_file() and '__pycache__'not in p.parts:rows.append({'path':p.relative_to(baseline).as_posix(),'sha256':sha(p)})
            f.write_text(json.dumps({'root':str(baseline),'inputs':rows},indent=2)+'\n');report['baselineInputs']={'path':str(f),'sha256':sha(f)}
            dest=out/'baseline';dest.mkdir();exe=dest/'host.exe';run('baseline-build',['gcc','-std=c11','-O0','-DTCC_DISABLE_BUILTIN_VFS','-I'+str(baseline/'lib/tcc'),source/'test/cases/tcc_pe_iat_order.c',baseline/'lib/tcc/tcc_utf8_io.c','-lws2_32','-o',exe]);text,_=run('baseline-regression-rejected',[exe,a.distribution.resolve(),obj,kernel,zero,data,dest],expected=1);assert 'unresolved reference' in text and 'two spellings share valid thunk'in text
        report['frozenInputsUnchanged']=all(sha(source/x['path'])==x['sha256']for x in frozen);report['peExecuted']=a.execute_pe;report['complete']=True;report['passed']=report['frozenInputsUnchanged'] and all(x['passed']for x in report['rows'])
    except Exception as e:report['error']=str(e)
    save();print(json.dumps({'passed':report['passed'],'report':str(out/'results.json'),'error':report.get('error')}));raise SystemExit(0 if report['passed']else 1)
if __name__=='__main__':main()
