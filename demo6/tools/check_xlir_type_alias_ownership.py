"""Check canonical alias ownership, ordinary entry modes and every allocation prefix."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler-directory','case-directory','work-directory','baseline-directory'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--native-root',type=Path)
    parser.add_argument('--cc',default='gcc');parser.add_argument('--ar',default='ar')
    args=parser.parse_args()
    root,compiler,work=args.root.resolve(),args.compiler_directory.resolve(),args.work_directory.resolve()
    if work.exists():parser.error('Choose a fresh work directory')
    work.mkdir(parents=True)
    windows=os.name=='nt';suffix='.exe'if windows else''
    cc,ar=shutil.which(args.cc),shutil.which(args.ar)
    if not cc or not ar:parser.error('C compiler and archive tool are required')
    env=dict(os.environ)
    if not windows:
        if args.native_root is None:parser.error('Linux requires the genuine pinned TCC dependency')
        native=args.native_root.resolve();env['XL6_SOURCE_NATIVE_ROOT']=str(native)
    def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
    manifest=compiler/'build.json';b=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert b['Complete']and not b.get('RuntimeOnly',False)
    driver=Path(b['Driver']);assert sha(driver).lower()==b['DriverSha256'].lower()
    pins={str(p):sha(p)for p in(manifest,driver,Path(__file__),Path(cc),Path(ar))}
    for section in('Inputs','Headers','Objects'):
        for item in b.get(section,[]):
            assert sha(item['Path']).lower()==item['Sha256'].lower(),item['Path']
            pins[item['Path']]=sha(item['Path'])
    cases=work/'inputs';cases.mkdir();name='xlir_type_alias_ownership'
    for filename in(name+'.xl',name+'_contract.c','xlir_callable_container_results_host.c'):
        source,target=args.case_directory.resolve()/filename,cases/filename
        shutil.copyfile(source,target);pins.update({str(source):sha(source),str(target):sha(target)})
    oom_host=root/'test/cases/stdlib_concurrent_queue_language_oom_host.c'
    memory=root/'test/support/module_product_memory.c'
    for p in(oom_host,memory,memory.with_suffix('.h')):pins[str(p)]=sha(p)
    rows=[]
    def run(label,command,marker=None,expected=0,quiet=False):
        command=list(map(str,command));start=time.monotonic();log=work/(label+'.log')
        try:
            p=subprocess.run(command,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=240)
            code,data=p.returncode,p.stdout
        except subprocess.TimeoutExpired as e:code,data=None,e.output or b''
        except OSError as e:code,data=-e.errno,str(e).encode('utf-8')
        log.write_bytes(data);text=data.decode('utf-8',errors='replace')
        passed=code==expected and(marker is None or marker in text)
        rows.append(dict(Name=label,Command=command,Exit=code,ExpectedExit=expected,Passed=passed,
            Marker=marker,Seconds=time.monotonic()-start,Log=str(log),Sha256=sha(log)))
        (work/'progress.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
        if not quiet or not passed:print(f'[alias-ownership] {label}: {"passed"if passed else"FAILED"}',flush=True)
        if not passed:print(text[-2600:],flush=True)
        return passed,text
    def build(label,arguments):
        rsp=work/(label+'.rsp');rsp.write_text('\n'.join('"'+str(v).replace('\\','/').replace('"','\\"')+'"'for v in arguments)+'\n',encoding='utf-8')
        return run(label,[cc,'@'+str(rsp)])[0]
    old_manifest=args.baseline_directory.resolve()/'build.json'
    old=json.loads(old_manifest.read_text(encoding='utf-8-sig'));old_driver=Path(old['Driver'])
    assert old['Complete']and sha(old_driver).lower()==old['DriverSha256'].lower()
    pins.update({str(old_manifest):sha(old_manifest),str(old_driver):sha(old_driver)})
    source=cases/(name+'.xl')
    run('before-refusal',[old_driver,source,'cgen-default',work/'before.c'],
        'XLIR for loop has no admitted canonical iterator ABI',expected=1)
    objects=[v['Path']for v in b['Objects']if Path(v['Path']).name not in('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    archive=work/'resident.a';rsp=work/'archive.rsp'
    rsp.write_text('\n'.join('"'+v.replace('\\','/')+'"'for v in objects)+'\n',encoding='utf-8')
    archive_ready=run('resident-archive',[ar,'rcs',archive,'@'+str(rsp)])[0]
    flags=[v for v in b['CompileFlags']if v not in('-O0','-O1','-O2','-O3','-Os','-Og')]
    flags+=['-I'+str(root),'-I'+str(compiler/'include'),'-I'+str(root/'release/include'),
        '-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation']
    libs=[root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']if windows else[
        '-L'+str(native/'release'),'-lxl','-Wl,-rpath,'+str(native/'release'),'-Wl,--export-dynamic','-pthread','-ldl','-lm']
    if windows:shutil.copyfile(root/'release/xl.dll',work/'xl.dll')
    contract=work/('alias-contract'+suffix)
    if archive_ready and build('contract-build',[*flags,'-O2',cases/(name+'_contract.c'),archive,*libs,'-o',contract]):
        run('contract',[contract,source],'30 mutations refused before C/public/native output')
    generated=work/'alias.c';emitted=run('emit',[driver,source,'cgen-default',generated])[0]
    repeat=work/'alias.repeat.c'
    if run('emit-repeat',[driver,source,'cgen-default',repeat])[0]and emitted:assert generated.read_bytes()==repeat.read_bytes()
    for mode in('run','module'):run(mode,[driver,source,mode,work/mode])
    exe=work/('alias-aot'+suffix)
    if run('aot-build',[driver,source,'exe',exe.with_suffix('')if windows else exe])[0]:run('aot',[exe])
    prefixes={}
    if emitted and archive_ready:
        memory_object=work/'memory.o';memory_ready=build('memory-build',[*flags,'-O2',memory,'-c','-o',memory_object])
        for opt in('-O0','-O2'):
            exe=work/('native'+opt+suffix)
            if build('native'+opt+'-build',[*flags,opt,'-include',generated,cases/'xlir_callable_container_results_host.c',archive,*libs,'-o',exe]):
                run('native'+opt,[exe],'three complete module lifetimes balanced')
            exe=work/('oom'+opt+suffix)
            if memory_ready and build('oom'+opt+'-build',[*flags,opt,'-DXL6_PRODUCT_USER_ENTRY','-include',generated,oom_host,memory_object,archive,*libs,'-o',exe]):
                hits=0;complete=False
                for point in range(512):
                    passed,text=run(f'oom{opt}-{point:03d}',[exe,str(point)],'balanced',quiet=point%32!=0)
                    match=re.search(r'point='+str(point)+r' hit=([01]) balanced',text)
                    if not passed or not match:break
                    if match[1]=='0':complete=True;break
                    hits+=1
                prefixes[opt]=dict(Complete=complete,ActualOom=hits,FirstNoFailure=point if complete else None)
    changed=[p for p,h in pins.items()if sha(p)!=h]
    complete=bool(rows)and not changed and all(v['Passed']for v in rows)and len(prefixes)==2 and all(v['Complete']and v['ActualOom']>20 for v in prefixes.values())
    report=dict(Complete=complete,Scope=__doc__,Rows=rows,Inputs=pins,ChangedInputs=changed,AllocationPrefixes=prefixes,ShippingAccepted=False,Full74GuardsPassed=False)
    (work/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(dict(Complete=complete,Checks=len(rows),AllocationPrefixes=prefixes),flush=True)
    return 0 if complete else 1


if __name__=='__main__':sys.exit(main())
