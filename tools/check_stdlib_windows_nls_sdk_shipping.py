"""Windows NLS declarations and strict UTF-16 through actual product VFS.
System GCC builds public API hosts only; both real DLLs compile/run the C
consumer through embedded TCC, with no include/provider fallback. Compiler
AOT is relocated and checked for xl/xlc imports. Six missing/wrong/lenient
controls are distinct from an optional old-product missing-flag rejection.
This does not prove nested xlang compiler integration or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expect-missing-flag',action='store_true',help='Old-product rejection control only, never product acceptance')
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();assert os.name=='nt' and not out.exists()
    cc,inspector=[shutil.which(n) for n in ('gcc','objdump')];assert cc and inspector
    task=Path(__file__).resolve().parents[1]
    host=task/'test/cases/stdlib_windows_nls_sdk_host.c';consumer=task/'test/cases/stdlib_windows_nls_sdk.c'
    build=json.loads(a.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/n for n in ('xl.exe','xl.dll','xlc.dll')]
    recorded={Path(r['Path']).resolve():r['Sha256'] for r in build['Products']}
    assert all(recorded.get(f)==sha(f) for f in products)
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),Path(inspector),a.build_report.resolve(),host,consumer}
    for r in build['Inputs']+build['Products']:assert sha(r['Path'])==r['Sha256'];files.add(Path(r['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();isolated=out/'isolated';isolated.mkdir();tmp=out/'tmp';tmp.mkdir();moved=out/'relocated';moved.mkdir()
    for f in products:shutil.copy2(f,isolated/f.name)
    source=isolated/consumer.name;shutil.copy2(consumer,source)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
        ('CPATH','C_INCLUDE_PATH','LIBRARY_PATH','LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT')}
    env.update(TEMP=str(tmp),TMP=str(tmp),TMPDIR=str(tmp))
    rows=[];proofs=[];controls=[];outputs=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Proofs=proofs,Controls=controls,
        Products=outputs,Failures=failures,Scope=__doc__,OldProductControl=a.expect_missing_flag,
        EmbeddedNlsSdkAcceptance=False,NestedCompilerAcceptance=False,ProviderOverrides=False,Full53Acceptance=False)
    for f in (*[isolated/p.name for p in products],source):outputs.append(dict(Path=str(f),Sha256=sha(f)))
    def run(name,command):
        r=subprocess.run(list(map(str,command)),cwd=isolated,env=env,capture_output=True,timeout=180)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
        return r.returncode,data
    try:
        for library in products[1:]:
            for opt in ('O0','O2'):
                name=library.stem+'-'+opt;compiler=library.stem=='xlc';binary=isolated/(name+'-host.exe')
                image=isolated/(name+'-consumer.exe');temp=tmp/name;temp.mkdir()
                command=[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,'-DXL_STATIC_EMBED',
                    '-I'+str(root/'release/include'),*(['-DXL6_NLS_COMPILER_HOST'] if compiler else []),
                    host,isolated/library.name,'-o',binary]
                code,data=run(name+'.host-build',command);assert code==0,data[-5000:]
                outputs.append(dict(Path=str(binary),Sha256=sha(binary)))
                code,data=run(name+'.jit-module',[binary,isolated/library.name,source,image,temp])
                if a.expect_missing_flag:
                    assert code==1 and b'WC_ERR_INVALID_CHARS' in data and b'missing' in data,data[-5000:]
                    assert not image.exists();proofs.append(dict(Name=name,HostOptimization=opt,MissingFlagRejected=True));continue
                assert code==0 and b'Windows NLS SDK: PASS' in data and b'actual product JIT/module' in data,data[-5000:]
                if compiler:
                    target=moved/image.name;shutil.move(image,target)
                    code,data=run(name+'.imports',[inspector,'-p',target]);assert code==0,data[-5000:]
                    deps=re.findall(rb'DLL Name:\s*(\S+)',data)
                    assert deps and not any(n.lower() in (b'xl.dll',b'xlc.dll') for n in deps),deps
                    code,data=run(name+'.relocated',[target]);assert code==0 and b'Windows NLS SDK: PASS' in data,data[-5000:]
                    outputs.append(dict(Path=str(target),Sha256=sha(target)))
                proofs.append(dict(Name=name,HostOptimization=opt,ConsumerCompiler='embedded TCC',Jit=True,Module=True,RelocatedAot=compiler))
                if opt=='O0':
                    body=consumer.read_text(encoding='utf-8-sig');anchor='#ifndef WC_ERR_INVALID_CHARS'
                    mutations=(('missing',body.replace(anchor,'#undef WC_ERR_INVALID_CHARS\n'+anchor,1),b'missing'),
                        ('wrong',body.replace(anchor,'#undef WC_ERR_INVALID_CHARS\n#define WC_ERR_INVALID_CHARS 0\n'+anchor,1),b'Windows SDK value'),
                        ('lenient',body.replace('CP_UTF8, WC_ERR_INVALID_CHARS,','CP_UTF8, 0,'),b'Windows NLS SDK failure at'))
                    for label,changed,diagnostic in mutations:
                        assert changed!=body
                        mutation=isolated/(name+'-'+label+'.c');mutation.write_text(changed,encoding='utf-8')
                        code,data=run(name+'.reject-'+label,[binary,isolated/library.name,mutation,image,temp])
                        assert code==1 and diagnostic in data,(name,label,data[-5000:])
                        outputs.append(dict(Path=str(mutation),Sha256=sha(mutation)))
                        controls.append(dict(Name=name+'-'+label,Rejected=True))
        assert len(proofs)==4 and (a.expect_missing_flag or len(controls)==6)
        assert not any((isolated/n).exists() for n in ('src','lib','res','include'))
    except Exception as e:failures.append(repr(e))
    report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
    report['Complete']=report['InputsUnchanged'] and len(proofs)==4 and not failures
    report['EmbeddedNlsSdkAcceptance']=report['Complete'] and not a.expect_missing_flag
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Proofs=len(proofs),Controls=len(controls),Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
