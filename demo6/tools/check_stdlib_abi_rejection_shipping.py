"""Reject the complete scoped legacy ABI/privacy/default declaration population
with actual shipping CLI C emission and compiler DLL/SO C AND header emission.
No compiler/source objects or provider override; never execute invalid calls.
Frontend-only success is not verified-IR acceptance. Positive/oracle controls
reject blanket failure, unrelated diagnostics, crashes, timeouts and partial
output. This is declaration safety, not runtime/function/full53 acceptance.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys,threading,time

ABI = re.compile(r'(?:bad_xlir_(?:text|number|unicode|encoding|html|buffer)_|'
                 r'xlir_(?:time|environment|signal|process|regex|pattern)(?:_\w+)?_bad_)')
ABI_ERROR = r'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI|C ABI extern product requires a closed primitive product profile'
PRIVACY = ('xlir_regex_private_consume','xlir_regex_private_split_consume',
           'xlir_pattern_private_code','xlir_pattern_private_builder','xlir_time_datetime_private')
POSITIVE = ('stdlib_buffer_core_only','stdlib_text_lower_only','stdlib_time_core_only',
            'stdlib_signal_subscription_only','stdlib_process_command_only','stdlib_regex_core_only',
            'stdlib_number_integer_only','stdlib_encoding_detect_only','stdlib_pattern_core_only','stdlib_containers_guide')
DEFAULTS = {'bad_extern_default_type':'extern default expression does not match parameter type',
            'bad_extern_default_order':'required extern parameter follows an optional parameter',
            'bad_native_extern_default_type':'默认值表达式类型和参数类型不匹配',
            'bad_native_extern_default_order':'必选参数不能出现在可选参数之后'}


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def population(directory):
    found={p.stem:('privacy' if re.search(r'(?:iterator|validator)_handle_',p.stem) else 'abi',
                  'private' if re.search(r'(?:iterator|validator)_handle_',p.stem) else ABI_ERROR)
           for p in directory.glob('*.xl') if ABI.match(p.stem) and not p.stem.startswith('bad_xlir_text_output_')}
    for name in PRIVACY:found[name]=('privacy',r'unknown class/union field|private')
    for name in ('sem_native_class_extern_bad_type','sem_native_class_extern_return_bad'):
        found[name]=('undeclared-container','does not support typed container direct C ABI')
    for name,marker in DEFAULTS.items():found[name]=('default',re.escape(marker))
    for kind in ('spsc_queue','mpsc_queue','mpmc_queue','wait_queue','queue_alias'):
        found['sem_'+kind+'_capacity_required']=('capacity','队列构造函数必须显式提供一个正容量参数')
        found['sem_'+kind+'_initializer_required']=('initializer','不可空队列声明必须包含初始化表达式')
    # The current verifier rejects these before scalar ABI matching: neither
    # raw point/int input carries the required native owner/field borrow.
    for name in ('bad_xlir_buffer_text_result','xlir_process_file_bad_input'):
        found[name]=('native-borrow','XLIR VFS resource parameter has no exact native field borrow')
    assert len(found)>=142
    assert all((directory/(name+'.xl')).is_file() for name in found)
    return found


def rejected(code,data,marker,output):
    return code==1 and re.search(marker,data.decode('utf-8',errors='replace')) is not None and not output.exists()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=2)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert root.is_dir() and not out.exists() and 1<=args.jobs<=4
    task=Path(__file__).resolve().parents[1];win=os.name=='nt';cc=shutil.which('gcc');assert cc
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/n for n in (('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so'))]
    recorded={Path(r['Path']).resolve():r['Sha256'] for r in build['Products']}
    assert all(recorded.get(f)==sha(f) for f in products)
    negative=population(root/'test/cases');assert negative==population(task/'test/cases')
    names=set(negative)|set(POSITIVE)
    sources={name:task/'test/cases'/(name+'.xl') for name in names}
    assert all(f.read_bytes()==(root/'test/cases'/f.name).read_bytes() for f in sources.values())
    host=task/'test/cases/stdlib_abi_rejection_product_host.c'
    files={Path(__file__).resolve(),Path(sys.executable).resolve(),Path(cc),args.build_report.resolve(),host,*sources.values()}
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256'],row['Path'];files.add(Path(row['Path']))
    out.mkdir();isolated=out/'isolated';release=isolated/'release';staged=isolated/'inputs';tmp=isolated/'tmp';sdk=out/'consumer-sdk'
    for directory in (release,staged,tmp,sdk):directory.mkdir(parents=True)
    for product in products:
        shutil.copy2(product,release/product.name);files.add(release/product.name)
    if win:
        # Import-by-name uses the observer's directory, NOT options.tccDllPath.
        # Never let PATH choose an unrelated installed xlc.dll.
        for product in products[1:]:
            shutil.copy2(product,isolated/product.name);files.add(isolated/product.name)
    for source in sources.values():
        shutil.copy2(source,staged/source.name);files.add(staged/source.name)
    for header in (root/'release/include').glob('*.h'):files.add(header);shutil.copy2(header,sdk/header.name)
    assert not any((isolated/n).exists() for n in ('src','lib','res','include','sdk'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[];cases=[];controls=[];failures=[];lock=threading.Lock()
    objects=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Cases=cases,Controls=controls,Objects=objects,
        Failures=failures,Scope=__doc__,CandidateCount=len(negative),PositiveCount=len(POSITIVE),
        ExplicitSubset=False,NativeAbiRejectionAcceptance=False,ProviderOverrides=False,
        CompilerSourceObjects=False,CompilerImageIdentityChecked=True,
        InvalidProgramsExecuted=False,RuntimeExecutionAcceptance=False,Full53Acceptance=False)
    def save():(out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        start=time.monotonic();command=list(map(str,command))
        try:
            process=subprocess.run(command,cwd=isolated,env=env,capture_output=True,timeout=120)
            code,data=process.returncode,process.stdout+process.stderr
        except subprocess.TimeoutExpired as error:code,data='timeout',(error.stdout or b'')+(error.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        row=dict(Name=name,Command=command,Exit=code,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start)
        with lock:rows.append(row)
        return row,data
    compiler=(isolated if win else release)/products[2].name;binary=isolated/('observer'+('.exe' if win else ''))
    def emit(name,source,mode):
        directory=out/name/mode;directory.mkdir(parents=True)
        output=directory/('output.h' if mode=='header' else 'output.c')
        if mode=='cli':command=[release/products[0].name,'cgen',source,'--emit',output,'--emit-header',directory/'output.h']
        else:command=[binary,compiler,source,mode,directory/'tmp',output]
        row,data=run(name+'.'+mode,command)
        return row,data,output
    def exercise(item):
        name,(kind,marker)=item;source=staged/(name+'.xl');records=[];passed=True
        for mode in ('cli','c','header'):
            row,data,output=emit(name,source,mode);row=dict(row,ExpectedDiagnostic=marker)
            safe=rejected(row['Exit'],data,marker,output)
            if mode!='cli':safe=safe and b'ABI observer compiler verified: ' in data
            # CLI also requests a header: neither product may publish a partial C/header pair.
            safe=safe and not any(output.parent.glob('*.h')) and not any(output.parent.glob('*.c'))
            records.append(dict(row,ExpectedRejection=safe,NoGeneratedOutput=not any(output.parent.glob('output.*'))))
            passed=passed and safe
        return dict(Name=name,Category=kind,Source=str(sources[name]),SourceSha256=sha(sources[name]),Passed=passed,Rows=records)
    try:
        links=[compiler] if win else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
        row,data=run('observer.build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2','-DXL_STATIC_EMBED',
                                     *([] if win else ['-D_GNU_SOURCE']),'-I'+str(sdk),host,*links,'-o',binary])
        assert row['Exit']==0,data.decode(errors='replace')[-3000:]
        objects.append(dict(Path=str(binary),Sha256=sha(binary)))
        for name in POSITIVE:
            for mode in ('cli','c','header'):
                row,data,output=emit(name,staged/(name+'.xl'),mode)
                ok=row['Exit']==0 and output.is_file() and output.stat().st_size>0
                if mode!='cli':ok=ok and b'ABI observer compiler verified: ' in data
                controls.append(dict(Name=name+'.'+mode,PositiveAccepted=ok,Generated=dict(Path=str(output),Sha256=sha(output)) if output.is_file() else None))
                if not ok:failures.append(dict(Name=name+'.'+mode,Diagnostic=data.decode(errors='replace')))
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for case in pool.map(exercise,sorted(negative.items())):
                cases.append(case)
                if not case['Passed']:failures.append(dict(Name=case['Name'],Error='Expected diagnostic or output refusal mismatch'))
                save();print(case['Name']+': '+('passed' if case['Passed'] else 'FAILED'),flush=True)
        missing=out/'never-produced.c'
        assert not missing.exists()
        # A real frontend-only pass is deliberately NOT classified as IR/Cgen acceptance.
        source=staged/'bad_xlir_number_parse_no_raise.xl';directory=out/'frontend-control';directory.mkdir()
        row,data=run('frontend-only-control',[binary,compiler,source,'front',directory/'tmp',missing])
        assert row['Exit']==0 and not rejected(row['Exit'],data,ABI_ERROR,missing)
        controls.append(dict(Name='frontend-only-not-abi-check',ActualFrontendAccepted=True,OracleRejected=True))
        genuine=next(c for c in cases if c['Name']=='bad_xlir_number_parse_no_raise')
        data=Path(genuine['Rows'][0]['Log']).read_bytes();assert genuine['Passed']
        for name,code,diagnostic in (('unrelated-diagnostic',1,b'unrelated syntax failure'),('crash',3221225477,data),('timeout','timeout',data)):
            assert not rejected(code,diagnostic,ABI_ERROR,missing)
            controls.append(dict(Name=name,OracleRejected=True))
        partial=out/'partial-control.c';partial.write_text('/* deliberately partial generated output */\n',encoding='ascii')
        assert not rejected(1,data,ABI_ERROR,partial);controls.append(dict(Name='partial-output',OracleRejected=True,Path=str(partial),Sha256=sha(partial)))
        syntax=isolated/'syntax-control.xl';syntax.write_text('func (\n',encoding='ascii')
        for mode in ('cli','c','header'):
            row,diagnostic,output=emit('syntax-control',syntax,mode)
            assert row['Exit']==1 and not rejected(row['Exit'],diagnostic,ABI_ERROR,output)
            controls.append(dict(Name='syntax-not-abi-'+mode,ActualSyntaxRejected=True,OracleRejected=True))
        alias=out/('other-'+products[2].name);shutil.copy2(compiler,alias)
        assert sha(alias)==sha(compiler)
        directory=out/'image-identity-control';directory.mkdir();output=directory/'output.c'
        row,data=run('wrong-compiler-image',[binary,alias,source,'c',directory/'tmp',output])
        assert row['Exit']==4 and b'loaded compiler does not match requested image' in data and not output.exists()
        controls.append(dict(Name='wrong-compiler-image',SameBytesDifferentPath=True,ActualImageMismatchRejected=True,Path=str(alias),Sha256=sha(alias)))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['NativeAbiRejectionAcceptance']=len(cases)==len(negative) and all(c['Passed'] for c in cases) and len(controls)==39 and not failures
        report['Complete']=report['InputsUnchanged'] and report['NativeAbiRejectionAcceptance']
    except Exception as error:failures.append(dict(Name='gate',Error=repr(error)))
    finally:report['PhaseCount']=len(rows);save()
    print(json.dumps({k:report[k] for k in ('Complete','CandidateCount','PhaseCount','Failures')},ensure_ascii=False),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
